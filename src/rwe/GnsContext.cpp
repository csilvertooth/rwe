#include "GnsContext.h"
#include <rwe/util/SimpleLogger.h>
#include <stdexcept>

namespace rwe
{
    static void gnsDebugOutput(ESteamNetworkingSocketsDebugOutputType type, const char* msg)
    {
        switch (type)
        {
            case k_ESteamNetworkingSocketsDebugOutputType_Bug:
            case k_ESteamNetworkingSocketsDebugOutputType_Error:
                LOG_ERROR << "[GNS] " << msg;
                break;
            case k_ESteamNetworkingSocketsDebugOutputType_Important:
            case k_ESteamNetworkingSocketsDebugOutputType_Warning:
                LOG_WARN << "[GNS] " << msg;
                break;
            default:
                LOG_DEBUG << "[GNS] " << msg;
                break;
        }
    }

    GnsContext::GnsContext()
    {
        SteamNetworkingErrMsg errMsg;
        if (!GameNetworkingSockets_Init(nullptr, errMsg))
        {
            throw std::runtime_error(std::string("GameNetworkingSockets_Init failed: ") + errMsg);
        }

        interface = SteamNetworkingSockets();
        if (!interface)
        {
            GameNetworkingSockets_Kill();
            throw std::runtime_error("SteamNetworkingSockets() returned null");
        }

        // Enable GNS debug logging
        SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, gnsDebugOutput);

        LOG_INFO << "GameNetworkingSockets initialized";
    }

    GnsContext::~GnsContext()
    {
        GameNetworkingSockets_Kill();
        LOG_INFO << "GameNetworkingSockets shutdown";
    }
}
