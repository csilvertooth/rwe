#pragma once

#include <variant>

namespace rwe
{
    struct ProjectilePhysicsTypeLineOfSight
    {
    };
    struct ProjectilePhysicsTypeBallistic
    {
    };
    struct ProjectilePhysicsTypeTracking
    {
        /**
         * Rate at which the projectile turns to face its target in world angular units/tick.
         */
        SimScalar turnRate;
    };

    /** Guided missile: travels toward target position with turn rate, affected by gravity if not self-propelled. */
    struct ProjectilePhysicsTypeGuided
    {
        SimScalar turnRate;
        SimScalar acceleration;
    };

    /** Bomb: dropped from aircraft, affected by gravity, inherits aircraft velocity. */
    struct ProjectilePhysicsTypeDropped
    {
    };

    using ProjectilePhysicsType = std::variant<
        ProjectilePhysicsTypeLineOfSight,
        ProjectilePhysicsTypeBallistic,
        ProjectilePhysicsTypeTracking,
        ProjectilePhysicsTypeGuided,
        ProjectilePhysicsTypeDropped>;
}
