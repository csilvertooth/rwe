#pragma once

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

namespace rwe
{
    class GnsContext
    {
    private:
        ISteamNetworkingSockets* interface{nullptr};

    public:
        GnsContext();
        ~GnsContext();

        GnsContext(const GnsContext&) = delete;
        GnsContext& operator=(const GnsContext&) = delete;
        GnsContext(GnsContext&&) = delete;
        GnsContext& operator=(GnsContext&&) = delete;

        ISteamNetworkingSockets* sockets() const { return interface; }
    };
}
