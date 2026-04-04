#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <network.pb.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingsockets_flat.h>
#include <steam/steamnetworkingtypes.h>
#include <string>
#include <thread>
#include <vector>

namespace rwe
{
    class LoadingNetworkService
    {
    public:
        enum class Status
        {
            Loading,
            Ready
        };

        struct PlayerInfo
        {
            int playerIndex;
            SteamNetworkingIPAddr address;
            HSteamNetConnection connection{k_HSteamNetConnection_Invalid};
            Status status;
            PlayerInfo(int playerIndex, const SteamNetworkingIPAddr& address, Status status)
                : playerIndex(playerIndex), address(address), status(status) {}
        };

    private:
        std::thread networkThread;
        std::atomic<bool> running{false};

        ISteamNetworkingSockets* sockets;

        // state shared between threads
        std::mutex mutex;
        Status loadingStatus{Status::Loading};
        std::vector<PlayerInfo> remoteEndpoints;

        // state owned by the worker thread
        HSteamListenSocket listenSocket{k_HSteamListenSocket_Invalid};
        HSteamNetPollGroup pollGroup{k_HSteamNetPollGroup_Invalid};

    public:
        explicit LoadingNetworkService(ISteamNetworkingSockets* sockets);

        virtual ~LoadingNetworkService();

        void addEndpoint(int playerIndex, const std::string& host, const std::string& port);

        HSteamNetConnection getConnection(int playerIndex);

        void setDoneLoading();

        bool areAllClientsReady();

        void waitForAllToBeReady();

        void start(const std::string& port);

        void releaseConnections();

    private:
        void run(const std::string& port);

        void pollMessages();

        void notifyStatus();

        static void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
    };
}
