#pragma once

#include <optional>
#include <rwe/util/SimpleLogger.h>
#include <string>
#include <unordered_map>

namespace rwe
{
    // Safe map lookup that logs an error and returns a default instead of throwing.
    // Use in hot paths (simulation tick, rendering) where crashing is unacceptable.
    template <typename Map>
    auto safeLookup(const Map& map, const typename Map::key_type& key, const char* context)
        -> std::optional<std::reference_wrapper<const typename Map::mapped_type>>
    {
        auto it = map.find(key);
        if (it == map.end())
        {
            LOG_ERROR << context << ": key not found: " << key;
            return std::nullopt;
        }
        return std::cref(it->second);
    }

    // Safe optional access that logs an error instead of throwing.
    template <typename T>
    std::optional<T> safeValue(const std::optional<T>& opt, const char* context)
    {
        if (!opt)
        {
            LOG_ERROR << context << ": unexpected empty optional";
            return std::nullopt;
        }
        return *opt;
    }
}
