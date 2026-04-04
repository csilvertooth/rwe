#pragma once

#include <rwe/grid/Grid.h>
#include <rwe/sim/Energy.h>
#include <rwe/sim/Metal.h>
#include <rwe/sim/MovementClassId.h>
#include <rwe/sim/SimScalar.h>
#include <string>
#include <variant>
#include <vector>

namespace rwe
{
    enum class YardMapCell
    {
        GroundPassableWhenOpen,
        WaterPassableWhenOpen,
        GroundNoFeature,
        GroundGeoPassableWhenOpen,
        Geo,
        Ground,
        GroundPassableWhenClosed,
        Water,
        GroundPassable,
        WaterPassable,
        Passable
    };

    struct UnitDefinition
    {
        struct NamedMovementClass
        {
            MovementClassId movementClassId;
        };
        struct AdHocMovementClass
        {
            unsigned int footprintX;
            unsigned int footprintZ;
            unsigned int maxSlope;
            unsigned int maxWaterSlope;
            unsigned int minWaterDepth;
            unsigned int maxWaterDepth;
        };
        using MovementCollisionInfo = std::variant<NamedMovementClass, AdHocMovementClass>;

        // FIXME: these two things should probably be in unit media info??
        // They are not needed for sim.
        std::string unitName;
        std::string unitDescription;

        std::string objectName;

        MovementCollisionInfo movementCollisionInfo;

        /**
         * Rate at which the unit turns in world angular units/tick.
         */
        SimScalar turnRate;

        /**
         * Maximum speed the unit can travel forwards in game units/tick.
         */
        SimScalar maxVelocity;

        /**
         * Speed at which the unit accelerates in game units/tick.
         */
        SimScalar acceleration;

        /**
         * Speed at which the unit brakes in game units/tick.
         */
        SimScalar brakeRate;

        bool canAttack;
        bool canMove;
        bool canGuard;

        /** If true, the unit is considered a commander for victory conditions. */
        bool commander;

        unsigned int maxHitPoints;

        bool isMobile;

        bool floater;
        bool canHover;

        bool canFly;

        /** True if the air unit can hover in place and fire (gunships). False = must do strafing runs (fighters). */
        bool hoverAttack{false};

        /** Distance above the ground that the unit flies at. */
        SimScalar cruiseAltitude;

        std::string weapon1;
        std::string weapon2;
        std::string weapon3;

        std::string explodeAs;

        bool builder;
        unsigned int buildTime;
        Energy buildCostEnergy;
        Metal buildCostMetal;

        unsigned int workerTimePerTick;

        SimScalar buildDistance;

        bool onOffable;
        bool activateWhenBuilt;

        Energy energyMake;
        Metal metalMake;
        Energy energyUse;
        Metal metalUse;

        Metal makesMetal;
        Metal extractsMetal;

        Energy energyStorage;
        Metal metalStorage;

        Energy windGenerator;

        std::optional<Grid<YardMapCell>> yardMap;
        bool yardMapContainsGeo;

        std::string corpse;

        bool hideDamage;
        bool showPlayerName;

        std::string soundCategory;

        /** Space-separated category tags, e.g. "KBOT WEAPON LEVEL1 NOTAIR" */
        std::vector<std::string> categories;

        /** Line-of-sight distance in world units. */
        unsigned int sightDistance{0};

        /** Radar detection range in world units. 0 = no radar. */
        unsigned int radarDistance{0};

        /** Sonar detection range in world units. 0 = no sonar. */
        unsigned int sonarDistance{0};

        /** Radar jamming range in world units. 0 = no jamming. */
        unsigned int radarDistanceJam{0};

        /** Sonar jamming range in world units. 0 = no sonar jamming. */
        unsigned int sonarDistanceJam{0};

        /** If true, unit is invisible to enemy radar. */
        bool stealth{false};

        // Cloaking
        bool cloakCost{false};
        unsigned int cloakCostMoving{0};
        unsigned int minCloakDistance{0};

        // Transport
        unsigned int transportCapacity{0};
        unsigned int transportSize{1};

        // Kamikaze
        bool kamikaze{false};
        unsigned int kamikazeDistance{0};

        // Misc
        bool isAirBase{false};
        bool teleporter{false};
        std::string armorType;

        // Naval
        unsigned int waterline{0};
    };
}
