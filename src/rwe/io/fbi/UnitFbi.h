#pragma once

#include <string>

namespace rwe
{
    struct UnitFbi
    {
        std::string unitName;
        std::string objectName;
        std::string soundCategory;
        std::string movementClass;

        std::string name;
        std::string description;

        unsigned int turnRate;
        float maxVelocity;
        float acceleration;
        float brakeRate;

        unsigned int footprintX;
        unsigned int footprintZ;
        unsigned int maxSlope;
        unsigned int maxWaterSlope;
        unsigned int minWaterDepth;
        unsigned int maxWaterDepth;

        bool canAttack;
        bool canMove;
        bool canGuard;

        bool commander;

        unsigned int maxDamage;

        bool bmCode;

        bool floater;
        bool canHover;

        bool canFly;

        bool hoverAttack{false};

        unsigned int cruiseAlt;

        std::string weapon1;
        std::string weapon2;
        std::string weapon3;

        std::string explodeAs;

        bool builder;
        unsigned int buildTime;
        unsigned int buildCostEnergy;
        unsigned int buildCostMetal;

        unsigned int workerTime;

        unsigned int buildDistance;

        bool onOffable;
        bool activateWhenBuilt;

        float energyMake;
        float metalMake;
        float energyUse;
        float metalUse;

        float makesMetal;
        float extractsMetal;

        unsigned int energyStorage;
        unsigned int metalStorage;

        unsigned int windGenerator;

        bool hideDamage;
        bool showPlayerName;

        std::string yardMap;

        std::string corpse;

        std::string category;

        unsigned int sightDistance{0};

        unsigned int radarDistance{0};
        unsigned int sonarDistance{0};
        unsigned int radarDistanceJam{0};
        unsigned int sonarDistanceJam{0};
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
        std::string damageModifier;
        std::string armorType;

        // Naval
        unsigned int waterline{0};
    };
}
