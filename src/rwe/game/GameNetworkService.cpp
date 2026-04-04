#include "GameNetworkService.h"
#include <algorithm>
#include <rwe/network_util.h>
#include <rwe/proto/serialization.h>
#include <rwe/sim/GameHash.h>
#include <rwe/sim/SimTicksPerSecond.h>
#include <rwe/util/Index.h>
#include <rwe/util/OpaqueId_io.h>
#include <rwe/util/range_util.h>
#include <rwe/util/SimpleLogger.h>
#include <thread>

namespace rwe
{
    GameNetworkService::GameNetworkService(
        PlayerId localPlayerId,
        std::shared_ptr<GnsContext> gnsContext,
        const std::vector<GameNetworkService::EndpointInfo>& endpoints,
        PlayerCommandService* playerCommandService)
        : localPlayerId(localPlayerId),
          gnsContext(std::move(gnsContext)),
          sockets(this->gnsContext ? this->gnsContext->sockets() : nullptr),
          endpoints(endpoints),
          playerCommandService(playerCommandService)
    {
    }

    GameNetworkService::~GameNetworkService()
    {
        running = false;
        if (networkThread.joinable())
        {
            networkThread.join();
        }

        if (!sockets)
        {
            return;
        }

        if (pollGroup != k_HSteamNetPollGroup_Invalid)
        {
            sockets->DestroyPollGroup(pollGroup);
        }

        for (auto& e : endpoints)
        {
            if (e.connection != k_HSteamNetConnection_Invalid)
            {
                sockets->CloseConnection(e.connection, 0, "GameNetworkService shutdown", false);
            }
        }
    }

    void GameNetworkService::start()
    {
        if (!sockets || endpoints.empty())
        {
            // Single-player mode — no network thread needed
            return;
        }
        networkThread = std::thread(&GameNetworkService::run, this);
    }

    void GameNetworkService::submitCommands(SceneTime sceneTime, const GameNetworkService::CommandSet& commands)
    {
        if (endpoints.empty())
        {
            return;
        }
        std::scoped_lock<std::mutex> lock(mutex);
        currentSceneTime = sceneTime;
        pendingCommands.push_back(commands);
    }

    void GameNetworkService::submitGameHash(GameHash hash)
    {
        if (endpoints.empty())
        {
            return;
        }
        std::scoped_lock<std::mutex> lock(mutex);
        pendingHashes.push_back(hash);
    }

    SceneTime GameNetworkService::estimateAvergeSceneTime(SceneTime localSceneTime)
    {
        std::scoped_lock<std::mutex> lock(mutex);
        auto time = getTimestamp();
        auto otherTimes = choose(endpoints, [](const auto& e) { return e.lastKnownSceneTime; });
        auto finalValue = estimateAverageSceneTimeStatic(localSceneTime, otherTimes, time);
        return SceneTime(finalValue);
    }

    float GameNetworkService::getMaxAverageRttMillis()
    {
        std::scoped_lock<std::mutex> lock(mutex);
        auto maxRtt = 0.0f;
        for (const auto& e : endpoints)
        {
            if (e.averageRoundTripTime > maxRtt)
            {
                maxRtt = e.averageRoundTripTime;
            }
        }
        return maxRtt;
    }

    void GameNetworkService::run()
    {
        try
        {
            pollGroup = sockets->CreatePollGroup();
            if (pollGroup == k_HSteamNetPollGroup_Invalid)
            {
                throw std::runtime_error("Failed to create GNS poll group for game");
            }

            for (auto& e : endpoints)
            {
                sockets->SetConnectionPollGroup(e.connection, pollGroup);
            }

            running = true;
            auto lastSendTime = std::chrono::steady_clock::now();

            while (running)
            {
                sockets->RunCallbacks();
                pollMessages();

                auto now = std::chrono::steady_clock::now();
                if (now - lastSendTime >= std::chrono::milliseconds(100))
                {
                    sendToAll();
                    lastSendTime = now;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Game network thread died with error: " << e.what();
        }
    }

    void GameNetworkService::pollMessages()
    {
        SteamNetworkingMessage_t* messages[64];
        int numMessages = sockets->ReceiveMessagesOnPollGroup(pollGroup, messages, 64);

        std::scoped_lock<std::mutex> lock(mutex);

        for (int i = 0; i < numMessages; ++i)
        {
            auto* msg = messages[i];

            auto endpointIt = std::find_if(endpoints.begin(), endpoints.end(),
                [&](const auto& e) { return e.connection == msg->m_conn; });
            if (endpointIt == endpoints.end())
            {
                LOG_DEBUG << "Received message from unknown GNS connection";
                msg->Release();
                continue;
            }

            processMessage(*endpointIt, msg->m_pData, msg->m_cbSize);
            msg->Release();
        }
    }

    void GameNetworkService::processMessage(EndpointInfo& endpoint, const void* data, int size)
    {
        // mutex already held by caller

        proto::NetworkMessage outerMessage;
        if (!outerMessage.ParseFromArray(data, size))
        {
            LOG_ERROR << "Failed to parse game network message";
            return;
        }

        if (!outerMessage.has_game_update())
        {
            LOG_DEBUG << "Received non-game-update message, ignoring";
            return;
        }

        endpoint.packetsReceived++;

        if (endpoint.packetsReceived % 300 == 0)
        {
            LOG_INFO << "Network health [player " << endpoint.playerId.value << "]: sent=" << endpoint.packetsSent
                << " recv=" << endpoint.packetsReceived << " avgRTT=" << endpoint.averageRoundTripTime << "ms";
        }

        const auto& message = outerMessage.game_update();

        if (message.player_id() != endpoint.playerId.value)
        {
            LOG_ERROR << "Player " << endpoint.playerId.value << " endpoint sent wrong player ID: " << message.player_id();
            return;
        }

        // Update peer scene time estimate using RTT from GNS
        updateRtt(endpoint);
        auto extraFrames = static_cast<unsigned int>((endpoint.averageRoundTripTime / 2.0f) * SimTicksPerSecond / 1000.0f);
        auto receiveTime = getTimestamp();
        endpoint.lastKnownSceneTime = std::make_pair(SceneTime(message.current_scene_time() + extraFrames), receiveTime);

        // With GNS reliable delivery, all commands arrive in order.
        // Process all command sets in the message.
        for (int i = 0; i < message.command_set_size(); ++i)
        {
            auto commandSet = deserializeCommandSet(message.command_set(i));
            playerCommandService->pushCommands(endpoint.playerId, commandSet);
        }

        // Process game hashes
        for (int i = 0; i < message.game_hashes_size(); ++i)
        {
            playerCommandService->pushHash(endpoint.playerId, GameHash(message.game_hashes(i)));
        }
    }

    void GameNetworkService::updateRtt(EndpointInfo& endpoint)
    {
        SteamNetConnectionRealTimeStatus_t status;
        if (sockets->GetConnectionRealTimeStatus(endpoint.connection, &status, 0, nullptr) == k_EResultOK)
        {
            if (status.m_nPing >= 0)
            {
                endpoint.averageRoundTripTime = static_cast<float>(status.m_nPing);
            }
        }
    }

    void GameNetworkService::sendToAll()
    {
        std::scoped_lock<std::mutex> lock(mutex);
        for (auto& e : endpoints)
        {
            send(e);
        }
        // Clear pending data after sending to all endpoints
        pendingCommands.clear();
        pendingHashes.clear();
    }

    void GameNetworkService::send(GameNetworkService::EndpointInfo& endpoint)
    {
        // mutex already held by caller

        proto::NetworkMessage outerMessage;
        auto& m = *outerMessage.mutable_game_update();
        m.set_player_id(localPlayerId.value);
        m.set_current_scene_time(currentSceneTime.value);

        // Legacy fields — kept for protocol compatibility, not used by GNS transport
        m.set_packet_id(0);
        m.set_next_command_set_to_send(0);
        m.set_next_command_set_to_receive(0);
        m.set_next_game_hash_to_send(0);
        m.set_next_game_hash_to_receive(0);
        m.set_ack_delay(0);

        // Send any pending commands
        for (const auto& set : pendingCommands)
        {
            auto& setMessage = *m.add_command_set();
            for (const auto& cmd : set)
            {
                auto& cmdMessage = *setMessage.add_command();
                serializePlayerCommand(cmd, cmdMessage);
            }
        }

        // Send any pending hashes
        for (const auto& hash : pendingHashes)
        {
            m.add_game_hashes(hash.value);
        }

        auto messageSize = outerMessage.ByteSizeLong();
        std::vector<char> buffer(messageSize);
        if (!outerMessage.SerializeToArray(buffer.data(), buffer.size()))
        {
            LOG_ERROR << "Failed to serialize game update message";
            return;
        }

        sockets->SendMessageToConnection(endpoint.connection, buffer.data(), buffer.size(),
            k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
        endpoint.packetsSent++;
    }
}
