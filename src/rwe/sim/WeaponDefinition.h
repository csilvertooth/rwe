#pragma once

#include <optional>
#include <rwe/sim/Energy.h>
#include <rwe/sim/GameTime.h>
#include <rwe/sim/Metal.h>
#include <rwe/sim/ProjectilePhysicsType.h>
#include <rwe/sim/SimAngle.h>
#include <rwe/sim/SimScalar.h>
#include <string>
#include <vector>

namespace rwe
{
    struct WeaponDefinition
    {
        ProjectilePhysicsType physicsType;

        SimScalar maxRange;

        SimScalar reloadTime;

        /** The number of shots in a burst. */
        int burst;

        /** The amount of time between shots in the same burst, in seconds. */
        SimScalar burstInterval;

        /** Maximum angle deviation of projectiles shot in a burst. */
        SimAngle sprayAngle;

        SimAngle tolerance;

        SimAngle pitchTolerance;

        /** Projectile velocity in pixels/tick. */
        SimScalar velocity;

        /** If true, the weapon only fires on command and does not auto-target. */
        bool commandFire;

        std::unordered_map<std::string, unsigned int> damage;

        SimScalar damageRadius;

        /** Number of ticks projectiles fired from this weapon live for */
        std::optional<GameTime> weaponTimer;

        /** The range in frames that projectile lifetime may randomly vary by. */
        std::optional<GameTime> randomDecay;

        /** If true, projectile does not explode when hitting the ground but instead continues travelling. */
        bool groundBounce;

        /** If true, weapon targets air units (VTOL category). */
        bool toAirWeapon{false};

        /** Category tags this weapon can only target. Empty means no restriction. */
        std::vector<std::string> onlyTargetCategory;

        /** Category tags this weapon will not chase. */
        std::vector<std::string> noChaseCategory;

        /** If true, weapon paralyzes instead of damaging. */
        bool paralyzer{false};

        /** If true, weapon is a beam/laser — instant hit, rendered as a line. */
        bool beamWeapon{false};

        /** Duration of beam display in seconds. */
        SimScalar beamDuration{0};

        /** Energy cost per shot. Deducted from owner when firing. 0 = free. */
        Energy energyPerShot{0};

        /** Metal cost per shot. Deducted from owner when firing. 0 = free. */
        Metal metalPerShot{0};

        /** If true, weapon uses stockpile ammo system. */
        bool stockpile{false};

        /** Damage falloff from center to edge of AOE. 0 = no falloff, 1 = full falloff. */
        float edgeEffectiveness{0.0f};

        /** Accuracy modifier (lower = less accurate). 0 = default. */
        unsigned int accuracy{0};

        /** If true, weapon is an interceptor (shoots down enemy projectiles). */
        bool interceptor{false};

        /** Interceptor coverage radius. */
        unsigned int coverage{0};

        /** If true, this projectile can be targeted by interceptors. */
        bool targetable{false};
    };
}
