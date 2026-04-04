#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <network.pb.h>
#include <rwe/GnsContext.h>
#include <rwe/game/PlayerCommand.h>
#include <rwe/game/PlayerCommandService.h>
#include <rwe/rwe_time.h>
#include <rwe/sim/GameHash.h>
#include <rwe/sim/GameTime.h>
#include <rwe/sim/PlayerId.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>
#include <thread>
#include <vector>

namespace rwe
{
    class GameNetworkService
    {
    public:
        using CommandSet = std::vector<PlayerCommand>;

        struct EndpointInfo
        {
            PlayerId playerId;
            HSteamNetConnection connection;

            /**
             * The last reported scene time from this peer,
             * adjusted for RTT.
             */
            std::optional<std::pair<SceneTime, Timestamp>> lastKnownSceneTime;

            /**
             * Round trip time in milliseconds, from GNS connection stats.
             */
            float averageRoundTripTime{0};

            unsigned int packetsSent{0};
            unsigned int packetsReceived{0};

            EndpointInfo(const PlayerId& playerId, HSteamNetConnection connection)
                : playerId(playerId), connection(connection)
            {
            }
        };

    private:
        PlayerId localPlayerId;

        std::thread networkThread;
        std::atomic<bool> running{false};

        std::shared_ptr<GnsContext> gnsContext;
        ISteamNetworkingSockets* sockets;
        HSteamNetPollGroup pollGroup{k_HSteamNetPollGroup_Invalid};

        // Shared state protected by mutex
        mutable std::mutex mutex;
        std::vector<EndpointInfo> endpoints;
        SceneTime currentSceneTime{0};
        std::deque<CommandSet> pendingCommands;
        std::deque<GameHash> pendingHashes;

        PlayerCommandService* const playerCommandService;

    public:
        GameNetworkService(
            PlayerId localPlayerId,
            std::shared_ptr<GnsContext> gnsContext,
            const std::vector<EndpointInfo>& endpoints,
            PlayerCommandService* playerCommandService);

        virtual ~GameNetworkService();

        void start();

        void submitCommands(SceneTime currentSceneTime, const CommandSet& commands);

        void submitGameHash(GameHash hash);

        SceneTime estimateAvergeSceneTime(SceneTime localSceneTime);

        float getMaxAverageRttMillis();

    private:
        void run();

        void pollMessages();

        void sendToAll();

        void send(EndpointInfo& endpoint);

        void processMessage(EndpointInfo& endpoint, const void* data, int size);

        void updateRtt(EndpointInfo& endpoint);
    };
}
