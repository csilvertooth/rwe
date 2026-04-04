#include "LoadingNetworkService.h"
#include <algorithm>
#include <rwe/util/SimpleLogger.h>

namespace rwe
{
    // Static instance pointer for the C-style callback.
    // Only one LoadingNetworkService can be active at a time.
    static LoadingNetworkService* g_loadingNetworkServiceInstance = nullptr;

    LoadingNetworkService::LoadingNetworkService(ISteamNetworkingSockets* sockets)
        : sockets(sockets)
    {
    }

    LoadingNetworkService::~LoadingNetworkService()
    {
        running = false;
        if (networkThread.joinable())
        {
            networkThread.join();
        }

        if (pollGroup != k_HSteamNetPollGroup_Invalid)
        {
            sockets->DestroyPollGroup(pollGroup);
        }

        // Close any connections we still own
        std::scoped_lock<std::mutex> lock(mutex);
        for (auto& p : remoteEndpoints)
        {
            if (p.connection != k_HSteamNetConnection_Invalid)
            {
                sockets->CloseConnection(p.connection, 0, "LoadingNetworkService shutdown", false);
                p.connection = k_HSteamNetConnection_Invalid;
            }
        }

        if (listenSocket != k_HSteamListenSocket_Invalid)
        {
            sockets->CloseListenSocket(listenSocket);
            listenSocket = k_HSteamListenSocket_Invalid;
        }

        if (g_loadingNetworkServiceInstance == this)
        {
            g_loadingNetworkServiceInstance = nullptr;
        }
    }

    void LoadingNetworkService::addEndpoint(int playerIndex, const std::string& host, const std::string& port)
    {
        std::scoped_lock<std::mutex> lock(mutex);

        SteamNetworkingIPAddr addr;
        addr.Clear();
        auto portNum = static_cast<uint16>(std::stoi(port));

        if (!SteamAPI_SteamNetworkingIPAddr_ParseString(&addr, (host + ":" + port).c_str()))
        {
            // Try as hostname — resolve to IP first
            // GNS only supports IP addresses, so for hostnames we fall back to "localhost" as IPv6 loopback
            if (host == "localhost" || host == "::1" || host == "127.0.0.1")
            {
                addr.SetIPv6LocalHost(portNum);
            }
            else
            {
                // Try parsing as just an IP
                SteamAPI_SteamNetworkingIPAddr_ParseString(&addr, host.c_str());
                addr.m_port = portNum;
            }
        }

        LOG_INFO << "Adding loading endpoint for player " << playerIndex
            << " at " << host << ":" << port;

        remoteEndpoints.emplace_back(playerIndex, addr, Status::Loading);
    }

    HSteamNetConnection LoadingNetworkService::getConnection(int playerIndex)
    {
        std::scoped_lock<std::mutex> lock(mutex);
        auto it = std::find_if(remoteEndpoints.begin(), remoteEndpoints.end(),
            [&](const auto& e) { return e.playerIndex == playerIndex; });
        if (it == remoteEndpoints.end())
        {
            throw std::runtime_error("Connection not found for player index " + std::to_string(playerIndex));
        }
        return it->connection;
    }

    void LoadingNetworkService::setDoneLoading()
    {
        std::scoped_lock<std::mutex> lock(mutex);
        loadingStatus = Status::Ready;
    }

    bool LoadingNetworkService::areAllClientsReady()
    {
        std::scoped_lock<std::mutex> lock(mutex);
        return std::all_of(remoteEndpoints.begin(), remoteEndpoints.end(),
            [](const auto& p) { return p.status == Status::Ready; });
    }

    void LoadingNetworkService::waitForAllToBeReady()
    {
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } while (!areAllClientsReady());
    }

    void LoadingNetworkService::start(const std::string& port)
    {
        networkThread = std::thread(&LoadingNetworkService::run, this, port);
    }

    void LoadingNetworkService::releaseConnections()
    {
        // Transfer connection ownership — don't close connections on destruction
        std::scoped_lock<std::mutex> lock(mutex);
        for (auto& p : remoteEndpoints)
        {
            p.connection = k_HSteamNetConnection_Invalid;
        }
        if (pollGroup != k_HSteamNetPollGroup_Invalid)
        {
            sockets->DestroyPollGroup(pollGroup);
            pollGroup = k_HSteamNetPollGroup_Invalid;
        }
        if (listenSocket != k_HSteamListenSocket_Invalid)
        {
            sockets->CloseListenSocket(listenSocket);
            listenSocket = k_HSteamListenSocket_Invalid;
        }
    }

    void LoadingNetworkService::run(const std::string& port)
    {
        try
        {
            g_loadingNetworkServiceInstance = this;

            // Create poll group
            pollGroup = sockets->CreatePollGroup();
            if (pollGroup == k_HSteamNetPollGroup_Invalid)
            {
                throw std::runtime_error("Failed to create GNS poll group");
            }

            // Configure connection options
            SteamNetworkingConfigValue_t opts[3];
            opts[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                (void*)&LoadingNetworkService::onConnectionStatusChanged);
            opts[1].SetInt32(k_ESteamNetworkingConfig_TimeoutConnected, 120);
            opts[2].SetInt32(k_ESteamNetworkingConfig_TimeoutInitial, 60);

            auto localPort = static_cast<uint16>(std::stoi(port));

            // Determine role: lowest port number is the "server" (listens),
            // higher port numbers are "clients" (connect to server).
            bool isServer = true;
            {
                std::scoped_lock<std::mutex> lock(mutex);
                for (const auto& p : remoteEndpoints)
                {
                    if (p.address.m_port < localPort)
                    {
                        isServer = false;
                        break;
                    }
                }
            }

            LOG_INFO << "GNS role: " << (isServer ? "server" : "client") << " (local port " << localPort << ")";

            if (isServer)
            {
                // Server: create listen socket, wait for clients to connect
                SteamNetworkingIPAddr listenAddr;
                listenAddr.Clear();
                listenAddr.m_port = localPort;

                listenSocket = sockets->CreateListenSocketIP(listenAddr, 3, opts);
                if (listenSocket == k_HSteamListenSocket_Invalid)
                {
                    throw std::runtime_error("Failed to create GNS listen socket on port " + port);
                }
                LOG_INFO << "GNS server: listen socket created on port " << port;
            }
            else
            {
                // Client: connect to all peers (which should have lower port numbers)
                std::scoped_lock<std::mutex> lock(mutex);
                for (auto& p : remoteEndpoints)
                {
                    p.connection = sockets->ConnectByIPAddress(p.address, 3, opts);
                    if (p.connection == k_HSteamNetConnection_Invalid)
                    {
                        LOG_ERROR << "Failed to initiate GNS connection to player " << p.playerIndex;
                        continue;
                    }
                    sockets->SetConnectionPollGroup(p.connection, pollGroup);
                    LOG_INFO << "GNS client: connecting to player " << p.playerIndex;
                }
            }

            running = true;
            auto lastSendTime = std::chrono::steady_clock::now();
            auto lastRetryTime = std::chrono::steady_clock::now();

            while (running)
            {
                sockets->RunCallbacks();
                pollMessages();

                auto now = std::chrono::steady_clock::now();
                if (now - lastSendTime >= std::chrono::milliseconds(100))
                {
                    notifyStatus();
                    lastSendTime = now;
                }

                // Client: retry failed connections every 2 seconds
                if (!isServer && now - lastRetryTime >= std::chrono::seconds(2))
                {
                    lastRetryTime = now;
                    std::scoped_lock<std::mutex> lock(mutex);
                    for (auto& p : remoteEndpoints)
                    {
                        if (p.connection == k_HSteamNetConnection_Invalid)
                        {
                            p.connection = sockets->ConnectByIPAddress(p.address, 3, opts);
                            if (p.connection != k_HSteamNetConnection_Invalid)
                            {
                                sockets->SetConnectionPollGroup(p.connection, pollGroup);
                                LOG_INFO << "Retrying GNS connection to player " << p.playerIndex;
                            }
                        }
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR << "Loading network thread died with error: " << e.what();
        }
    }

    void LoadingNetworkService::pollMessages()
    {
        SteamNetworkingMessage_t* messages[16];
        int numMessages = sockets->ReceiveMessagesOnPollGroup(pollGroup, messages, 16);

        for (int i = 0; i < numMessages; ++i)
        {
            auto* msg = messages[i];

            proto::NetworkMessage message;
            if (!message.ParseFromArray(msg->m_pData, msg->m_cbSize))
            {
                LOG_ERROR << "Failed to parse loading network message";
                msg->Release();
                continue;
            }

            std::scoped_lock<std::mutex> lock(mutex);

            // Find the endpoint by connection handle
            auto it = std::find_if(remoteEndpoints.begin(), remoteEndpoints.end(),
                [&](const auto& p) { return p.connection == msg->m_conn; });
            if (it == remoteEndpoints.end())
            {
                // Unknown connection — this is an accepted incoming connection.
                // Assign to the first endpoint that doesn't have a connection yet.
                bool assigned = false;
                for (auto& p : remoteEndpoints)
                {
                    if (p.connection == k_HSteamNetConnection_Invalid)
                    {
                        p.connection = msg->m_conn;
                        it = remoteEndpoints.begin() + (&p - remoteEndpoints.data());
                        assigned = true;
                        LOG_INFO << "GNS: Assigned incoming connection to player " << p.playerIndex
                            << " (handle " << msg->m_conn << ")";
                        break;
                    }
                }
                if (!assigned)
                {
                    LOG_DEBUG << "Received message from truly unknown connection";
                    msg->Release();
                    continue;
                }
            }

            if (!message.has_loading_status())
            {
                LOG_DEBUG << "Sender is already in game";
                it->status = Status::Ready;
                msg->Release();
                continue;
            }

            switch (message.loading_status().status())
            {
                case proto::LoadingStatusMessage_Status_Loading:
                    LOG_DEBUG << "Player " << it->playerIndex << " is loading";
                    it->status = Status::Loading;
                    break;
                case proto::LoadingStatusMessage_Status_Ready:
                    LOG_DEBUG << "Player " << it->playerIndex << " is ready";
                    it->status = Status::Ready;
                    break;
                default:
                    LOG_ERROR << "Unhandled loading status value";
                    break;
            }

            msg->Release();
        }
    }

    void LoadingNetworkService::notifyStatus()
    {
        std::scoped_lock<std::mutex> lock(mutex);

        proto::NetworkMessage outerMessage;
        {
            auto& innerMessage = *outerMessage.mutable_loading_status();
            switch (loadingStatus)
            {
                case Status::Loading:
                    innerMessage.set_status(proto::LoadingStatusMessage_Status_Loading);
                    break;
                case Status::Ready:
                    innerMessage.set_status(proto::LoadingStatusMessage_Status_Ready);
                    break;
                default:
                    LOG_ERROR << "Unhandled loading status in notifyStatus";
                    return;
            }
        }

        auto messageSize = outerMessage.ByteSizeLong();
        std::vector<char> buffer(messageSize);
        if (!outerMessage.SerializeToArray(buffer.data(), buffer.size()))
        {
            LOG_ERROR << "Failed to serialize loading status message";
            return;
        }

        for (const auto& p : remoteEndpoints)
        {
            if (p.connection == k_HSteamNetConnection_Invalid)
            {
                continue;
            }
            auto result = sockets->SendMessageToConnection(p.connection, buffer.data(), buffer.size(),
                k_nSteamNetworkingSend_Reliable, nullptr);
            if (result != k_EResultOK)
            {
                LOG_WARN << "Failed to send loading status to player " << p.playerIndex << ": result=" << result;
            }
            sockets->FlushMessagesOnConnection(p.connection);
        }
    }

    void LoadingNetworkService::onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
    {
        auto* self = g_loadingNetworkServiceInstance;
        if (!self)
        {
            return;
        }

        auto* sockets = self->sockets;

        switch (info->m_info.m_eState)
        {
            case k_ESteamNetworkingConnectionState_Connecting:
            {
                // Only accept incoming connections (from listen socket)
                if (info->m_info.m_hListenSocket == k_HSteamListenSocket_Invalid)
                {
                    // This is our own outgoing connection progressing, ignore
                    LOG_DEBUG << "GNS: Outgoing connection progressing (handle " << info->m_hConn << ")";
                    break;
                }

                LOG_INFO << "GNS: Accepting incoming connection (handle " << info->m_hConn << ")";
                if (sockets->AcceptConnection(info->m_hConn) != k_EResultOK)
                {
                    LOG_ERROR << "GNS: Failed to accept connection";
                    sockets->CloseConnection(info->m_hConn, 0, "Failed to accept", false);
                    break;
                }
                sockets->SetConnectionPollGroup(info->m_hConn, self->pollGroup);
                // Don't assign yet — will be matched when messages arrive or when Connected fires
                break;
            }
            case k_ESteamNetworkingConnectionState_Connected:
            {
                LOG_INFO << "GNS: Connection established (handle " << info->m_hConn << ")";
                // Connection assignment is handled in pollMessages when first data arrives
                break;
            }
            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            {
                LOG_WARN << "GNS: Connection lost (handle " << info->m_hConn << "): " << info->m_info.m_szEndDebug;
                sockets->CloseConnection(info->m_hConn, 0, nullptr, false);

                std::scoped_lock<std::mutex> lock(self->mutex);
                for (auto& p : self->remoteEndpoints)
                {
                    if (p.connection == info->m_hConn)
                    {
                        p.connection = k_HSteamNetConnection_Invalid;
                        break;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}
