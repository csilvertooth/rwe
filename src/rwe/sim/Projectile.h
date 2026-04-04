#pragma once

#include <rwe/sim/GameTime.h>
#include <rwe/sim/PlayerId.h>
#include <rwe/sim/ProjectilePhysicsType.h>
#include <rwe/sim/SimVector.h>
#include <rwe/sim/UnitId.h>
#include <variant>

namespace rwe
{
    struct Projectile
    {
        std::string weaponType;

        PlayerId owner;

        SimVector position;
        SimVector previousPosition;

        SimVector origin;

        /** Velocity in game pixels/tick */
        SimVector velocity;

        /** The last time the projectile emitted smoke. */
        GameTime lastSmoke;

        std::unordered_map<std::string, unsigned int> damage;

        std::optional<GameTime> dieOnFrame;

        SimScalar damageRadius;

        bool groundBounce;

        bool isDead{false};

        /** The game time at which the projectile was created. */
        GameTime createdAt;

        /** The unit that this projectile is tracking, if any. */
        std::optional<UnitId> targetUnit;

        /** The unit that fired this projectile (for veterancy credit). */
        std::optional<UnitId> sourceUnit;

        /** If true, this projectile paralyzes instead of damaging. */
        bool paralyzer{false};

        /** Edge effectiveness for AOE damage falloff. 0 = full falloff, 1 = no falloff. */
        float edgeEffectiveness{0.0f};

        SimVector getBackPosition(SimScalar duration) const;

        SimVector getPreviousBackPosition(SimScalar duration) const;

        unsigned int getDamage(const std::string& unitType) const;
    };
}
