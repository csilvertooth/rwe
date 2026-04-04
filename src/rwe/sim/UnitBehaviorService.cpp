#include "UnitBehaviorService.h"
#include <algorithm>
#include <rwe/cob/CobExecutionContext.h>
#include <rwe/sim/SimTicksPerSecond.h>
#include <rwe/sim/UnitBehaviorService_util.h>
#include <rwe/sim/cob.h>
#include <rwe/sim/movement.h>
#include <rwe/util/Index.h>
#include <rwe/util/match.h>

namespace rwe
{
    SimScalar getTargetAltitude(const GameSimulation& sim, SimScalar x, SimScalar z, const UnitDefinition& unitDefinition)
    {
        auto groundHeight = rweMax(sim.terrain.getHeightAt(x, z), sim.terrain.getSeaLevel());

        // Check for features and buildings nearby that the aircraft must clear
        // Use a 7x7 cell area (112x112 world units) to account for aircraft wingspan
        auto heightmapPos = sim.terrain.worldToHeightmapCoordinate(SimVector(x, 0_ss, z));
        auto gridW = static_cast<int>(sim.occupiedGrid.getWidth());
        auto gridH = static_cast<int>(sim.occupiedGrid.getHeight());

        SimScalar obstacleTop = groundHeight;
        for (int dy = -3; dy <= 3; ++dy)
        {
            for (int dx = -3; dx <= 3; ++dx)
            {
                int cx = heightmapPos.x + dx;
                int cy = heightmapPos.y + dy;
                if (cx < 0 || cx >= gridW || cy < 0 || cy >= gridH)
                {
                    continue;
                }
                const auto& cell = sim.occupiedGrid.get(cx, cy);
                if (cell.featureId)
                {
                    auto featureIt = sim.features.find(*cell.featureId);
                    if (featureIt != sim.features.end())
                    {
                        const auto& featureDef = sim.getFeatureDefinition(featureIt->second.featureName);
                        obstacleTop = rweMax(obstacleTop, featureIt->second.position.y + featureDef.height);
                    }
                }
                if (cell.buildingInfo)
                {
                    auto unitOpt = sim.tryGetUnitState(cell.buildingInfo->unit);
                    if (unitOpt)
                    {
                        const auto& bldgDef = sim.unitDefinitions.at(unitOpt->get().unitType);
                        const auto& modelDef = sim.unitModelDefinitions.at(bldgDef.objectName);
                        obstacleTop = rweMax(obstacleTop, unitOpt->get().position.y + modelDef.height);
                    }
                }
            }
        }

        return obstacleTop + unitDefinition.cruiseAltitude;
    }

    UnitBehaviorService::UnitBehaviorService(GameSimulation* sim)
        : sim(sim)
    {
    }

    void UnitBehaviorService::onCreate(UnitId unitId)
    {
        auto& unit = sim->getUnitState(unitId);
        const auto& unitDefinition = sim->unitDefinitions.at(unit.unitType);

        unit.cobEnvironment->createThread("Create", std::vector<int>());

        // set speed for metal extractors
        if (unitDefinition.extractsMetal != Metal(0))
        {
            auto footprint = sim->computeFootprintRegion(unit.position, unitDefinition.movementCollisionInfo);
            auto metalValue = sim->metalGrid.accumulate(sim->metalGrid.clipRegion(footprint), 0u, std::plus<>());
            unit.cobEnvironment->createThread("SetSpeed", {static_cast<int>(metalValue)});
        }

        runUnitCobScripts(*sim, unitId);

        // measure z distances for ballistics
        for (int i = 0; i < getSize(unit.weapons); ++i)
        {
            auto& weapon = unit.weapons[i];
            if (!weapon)
            {
                continue;
            }
            auto localAimingPoint = getLocalAimingPoint(unitId, i);
            auto localFiringPoint = getLocalFiringPoint(unitId, i);
            weapon->ballisticZOffset = localFiringPoint.z - localAimingPoint.z;
        }
    }

    void UnitBehaviorService::updateWind(SimScalar windGenerationFactor, SimAngle windDirection)
    {
        int cobWindSpeed = static_cast<int>(toCobSpeed(windGenerationFactor).value);
        int cobWindDirection = toCobAngle(windDirection).value;

        for (auto& [id, unit] : sim->units)
        {
            const auto& unitDefinition = sim->unitDefinitions.at(unit.unitType);
            if (unitDefinition.windGenerator != Energy(0))
            {
                unit.cobEnvironment->createThread("SetSpeed", {cobWindSpeed});
                unit.cobEnvironment->createThread("SetDirection", {cobWindDirection});
            }
        }
    }

    void UnitBehaviorService::update(UnitId unitId)
    {
        auto unitInfo = sim->getUnitInfo(unitId);

        // Stunned units cannot act
        if (unitInfo.state->stunned)
        {
            return;
        }

        // Clear steering targets.
        match(
            unitInfo.state->physics,
            [&](UnitPhysicsInfoGround& p) {
                p.steeringInfo = SteeringInfo{
                    unitInfo.state->rotation,
                    0_ss,
                };
            },
            [&](UnitPhysicsInfoAir& p) {
                match(
                    p.movementState,
                    [&](AirMovementStateFlying& s) {
                        s.targetPosition = unitInfo.state->position;
                    },
                    [&](const AirMovementStateTakingOff&) {
                        // do nothing
                    },
                    [&](const AirMovementStateLanding&) {
                        // do nothing
                    });
            });

        // clear navigation targets
        unitInfo.state->navigationState.desiredDestination = std::nullopt;

        // Run unit and weapon AI
        if (!unitInfo.state->isBeingBuilt(*unitInfo.definition))
        {
            // check our build queue
            if (!unitInfo.state->buildQueue.empty())
            {
                auto& entry = unitInfo.state->buildQueue.front();
                if (handleBuild(unitInfo, entry.first))
                {
                    if (entry.second > 1)
                    {
                        --entry.second;
                    }
                    else
                    {
                        unitInfo.state->buildQueue.pop_front();
                    }
                }
            }
            else
            {
                clearBuild(unitInfo);
            }

            // check our orders
            if (!unitInfo.state->orders.empty())
            {
                const auto& order = unitInfo.state->orders.front();

                // process move orders
                if (handleOrder(unitInfo, order))
                {
                    unitInfo.state->orders.pop_front();
                    unitInfo.state->buildOrderUnitId = std::nullopt;
                }
            }
            else if (auto airPhysics = std::get_if<UnitPhysicsInfoAir>(&unitInfo.state->physics); airPhysics != nullptr)
            {
                match(
                    airPhysics->movementState,
                    [&](AirMovementStateFlying& m) {
                        auto terrainHeight = sim->terrain.getHeightAt(unitInfo.state->position.x, unitInfo.state->position.z);
                        if (terrainHeight < sim->terrain.getSeaLevel())
                        {
                            // Over water — drift back and forth along facing direction
                            auto driftDistance = 24_ss;
                            auto driftPhase = sin(SimAngle(sim->gameTime.value * 40));
                            auto driftX = unitInfo.state->position.x + driftDistance * driftPhase * sin(unitInfo.state->rotation);
                            auto driftZ = unitInfo.state->position.z + driftDistance * driftPhase * cos(unitInfo.state->rotation);
                            auto targetHeight = getTargetAltitude(*sim, driftX, driftZ, *unitInfo.definition);
                            m.targetPosition = SimVector(driftX, targetHeight, driftZ);
                        }
                        else if (navigateTo(unitInfo, NavigationGoalLandingLocation()))
                        {
                            m.shouldLand = true;
                        }
                    },
                    [&](const AirMovementStateLanding&) {
                        // do nothing
                    },
                    [&](const AirMovementStateTakingOff&) {
                        // do nothing
                    });
            }
            else
            {
                changeState(*unitInfo.state, UnitBehaviorStateIdle());
            }

            for (Index i = 0; i < getSize(unitInfo.state->weapons); ++i)
            {
                updateWeapon(unitId, i);
            }
        }

        if (unitInfo.definition->isMobile)
        {
            updateNavigation(unitInfo);

            applyUnitSteering(unitInfo);

            auto previouslyWasMoving = !areCloserThan(unitInfo.state->previousPosition, unitInfo.state->position, 0.1_ssf);

            updateUnitPosition(unitInfo);

            auto currentlyIsMoving = !areCloserThan(unitInfo.state->previousPosition, unitInfo.state->position, 0.1_ssf);

            if (currentlyIsMoving && !previouslyWasMoving)
            {
                unitInfo.state->cobEnvironment->createThread("StartMoving");
            }
            else if (!currentlyIsMoving && previouslyWasMoving)
            {
                unitInfo.state->cobEnvironment->createThread("StopMoving");
            }

            // do physics transitions
            match(
                unitInfo.state->physics,
                [&](const UnitPhysicsInfoGround& p) {
                    if (p.steeringInfo.shouldTakeOff)
                    {
                        transitionFromGroundToAir(unitInfo);
                    }
                },
                [&](UnitPhysicsInfoAir& p) {
                    match(
                        p.movementState,
                        [&](const AirMovementStateTakingOff&) {
                            auto targetHeight = getTargetAltitude(*sim, unitInfo.state->position.x, unitInfo.state->position.z, *unitInfo.definition);
                            if (unitInfo.state->position.y == targetHeight)
                            {
                                p.movementState = AirMovementStateFlying();
                            }
                        },
                        [&](AirMovementStateLanding& m) {
                            if (m.shouldAbort)
                            {
                                unitInfo.state->activate();
                                p.movementState = AirMovementStateFlying();
                            }
                            else
                            {
                                auto terrainHeight = sim->terrain.getHeightAt(unitInfo.state->position.x, unitInfo.state->position.z);
                                // Abort landing if trying to land underwater
                                if (terrainHeight < sim->terrain.getSeaLevel())
                                {
                                    unitInfo.state->activate();
                                    p.movementState = AirMovementStateFlying();
                                }
                                else if (unitInfo.state->position.y == terrainHeight)
                                {
                                    if (!tryTransitionFromAirToGround(unitInfo))
                                    {
                                        m.landingFailed = true;
                                    }
                                }
                            }
                        },
                        [&](const AirMovementStateFlying& m) {
                            if (m.shouldLand)
                            {
                                p.movementState = AirMovementStateLanding();
                                unitInfo.state->deactivate();
                            }
                        });
                });
        }
    }

    SimVector UnitBehaviorService::getUnitPositionWithCache(UnitState& s, UnitId unitId)
    {
        if (s.navigationState.unitPositionCache && s.navigationState.unitPositionCache->unitId == unitId)
        {
            const auto& pos = s.navigationState.unitPositionCache->position;
            const auto& time = s.navigationState.unitPositionCache->cachedAtTime;
            if (sim->gameTime - time < GameTime(SimTicksPerSecond))
            {
                return pos;
            }
        }

        const auto& pos = sim->getUnitState(unitId).position;
        s.navigationState.unitPositionCache = UnitPositionCache{
            unitId,
            pos,
            sim->gameTime,
        };

        return pos;
    }


    void UnitBehaviorService::updateNavigation(UnitInfo unitInfo)
    {
        const auto& goal = unitInfo.state->navigationState.desiredDestination;

        if (!goal)
        {
            unitInfo.state->navigationState.state = NavigationStateIdle();
            return;
        }

        auto resolvedGoal = match(
            *goal,
            [&](const NavigationGoalLandingLocation&) {
                const auto llState = std::get_if<NavigationStateMovingToLandingSpot>(&unitInfo.state->navigationState.state);
                if (llState)
                {
                    return std::make_optional<MovingStateGoal>(llState->landingLocation);
                }
                else
                {
                    auto landingLocation = findLandingLocation(*sim, unitInfo);
                    if (!landingLocation)
                    {
                        return std::optional<MovingStateGoal>();
                    }
                    unitInfo.state->navigationState.state = NavigationStateMovingToLandingSpot{*landingLocation};
                    return std::make_optional<MovingStateGoal>(*landingLocation);
                }
            },
            [&](const SimVector& v) {
                return std::make_optional<MovingStateGoal>(v);
            },
            [&](const DiscreteRect& r) {
                return std::make_optional<MovingStateGoal>(r);
            },
            [&](const UnitId& u) {
                return std::make_optional<MovingStateGoal>(u);
            });

        if (!resolvedGoal)
        {
            unitInfo.state->navigationState.state = NavigationStateIdle();
            return;
        }

        moveTo(unitInfo, *resolvedGoal);
    }

    bool followPath(UnitInfo unitInfo, UnitPhysicsInfoGround& physics, PathFollowingInfo& path)
    {
        const auto& destination = *path.currentWaypoint;
        SimVector xzPosition(unitInfo.state->position.x, 0_ss, unitInfo.state->position.z);
        SimVector xzDestination(destination.x, 0_ss, destination.z);
        auto distanceSquared = xzPosition.distanceSquared(xzDestination);

        auto isFinalDestination = path.currentWaypoint == (path.path.waypoints.end() - 1);

        if (isFinalDestination)
        {
            if (distanceSquared < (8_ss * 8_ss))
            {
                return true;
            }

            physics.steeringInfo = arrive(*unitInfo.state, *unitInfo.definition, physics, destination);
            return false;
        }

        if (distanceSquared < (16_ss * 16_ss))
        {
            ++path.currentWaypoint;
            return false;
        }

        physics.steeringInfo = seek(*unitInfo.state, *unitInfo.definition, destination);
        return false;
    }

    bool hasCategory(const UnitDefinition& unitDef, const std::string& cat)
    {
        return std::find(unitDef.categories.begin(), unitDef.categories.end(), cat) != unitDef.categories.end();
    }

    bool matchesTargetCategories(const WeaponDefinition& weaponDef, const UnitDefinition& targetDef)
    {
        // toAirWeapon: only targets units with VTOL category
        if (weaponDef.toAirWeapon && !hasCategory(targetDef, "VTOL"))
        {
            return false;
        }

        // onlyTargetCategory: target must have at least one of these categories
        if (!weaponDef.onlyTargetCategory.empty())
        {
            bool found = false;
            for (const auto& cat : weaponDef.onlyTargetCategory)
            {
                if (hasCategory(targetDef, cat))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return false;
            }
        }

        // noChaseCategory: target must NOT have any of these categories
        for (const auto& cat : weaponDef.noChaseCategory)
        {
            if (hasCategory(targetDef, cat))
            {
                return false;
            }
        }

        return true;
    }

    void UnitBehaviorService::updateWeapon(UnitId id, unsigned int weaponIndex)
    {
        auto& unit = sim->getUnitState(id);
        auto& weapon = unit.weapons[weaponIndex];
        if (!weapon)
        {
            return;
        }

        auto weaponDefIt = sim->weaponDefinitions.find(weapon->weaponType);
        if (weaponDefIt == sim->weaponDefinitions.end())
        {
            return;
        }
        const auto& weaponDefinition = weaponDefIt->second;

        if (auto idleState = std::get_if<UnitWeaponStateIdle>(&weapon->state); idleState != nullptr)
        {
            // attempt to acquire a target
            if (!weaponDefinition.commandFire && unit.fireOrders == UnitFireOrders::FireAtWill)
            {
                for (const auto& entry : sim->units)
                {
                    auto otherUnitId = entry.first;
                    const auto& otherUnit = entry.second;

                    if (otherUnit.isDead())
                    {
                        continue;
                    }

                    if (otherUnit.isOwnedBy(unit.owner))
                    {
                        continue;
                    }

                    // Can't target units we can't see (fog of war)
                    if (!sim->isUnitVisible(unit.owner, otherUnitId))
                    {
                        continue;
                    }

                    if (unit.position.distanceSquared(otherUnit.position) > weaponDefinition.maxRange * weaponDefinition.maxRange)
                    {
                        continue;
                    }

                    // Category-based targeting filter
                    const auto& otherUnitDef = sim->unitDefinitions.at(otherUnit.unitType);
                    if (!matchesTargetCategories(weaponDefinition, otherUnitDef))
                    {
                        continue;
                    }

                    weapon->state = UnitWeaponStateAttacking(otherUnitId);
                    break;
                }
            }
        }
        else if (auto aimingState = std::get_if<UnitWeaponStateAttacking>(&weapon->state); aimingState != nullptr)
        {
            if (std::holds_alternative<UnitWeaponStateAttacking::FireInfo>(aimingState->attackInfo))
            {
                tryFireWeapon(id, weaponIndex);
                return;
            }

            // If we are not fire-at-will, the target is a unit,
            // and we don't have an explicit order to attack that unit,
            // drop the target.
            // This can happen if we acquired the target ourselves while in fire-at-will,
            // but then the player switched us to another firing mode.
            if (unit.fireOrders != UnitFireOrders::FireAtWill)
            {
                if (auto targetUnit = std::get_if<UnitId>(&aimingState->target); targetUnit != nullptr)
                {
                    if (unit.orders.empty())
                    {
                        unit.clearWeaponTarget(weaponIndex);
                        return;
                    }
                    else if (auto attackOrder = std::get_if<AttackOrder>(&unit.orders.front()); attackOrder != nullptr)
                    {
                        if (auto attackTarget = std::get_if<UnitId>(&attackOrder->target); attackTarget == nullptr || *attackTarget != *targetUnit)
                        {
                            unit.clearWeaponTarget(weaponIndex);
                            return;
                        }
                    }
                }
            }

            auto targetPosition = getTargetPosition(aimingState->target);

            if (!targetPosition || unit.position.distanceSquared(*targetPosition) > weaponDefinition.maxRange * weaponDefinition.maxRange)
            {
                unit.clearWeaponTarget(weaponIndex);
            }
            else if (std::holds_alternative<UnitWeaponStateAttacking::IdleInfo>(aimingState->attackInfo))
            {
                auto aimFromPosition = getAimingPoint(id, weaponIndex);

                auto headingAndPitch = computeHeadingAndPitch(unit.rotation, aimFromPosition, *targetPosition, weaponDefinition.velocity, (112_ss / (30_ss * 30_ss)), weapon->ballisticZOffset, weaponDefinition.physicsType);
                auto heading = headingAndPitch.first;
                auto pitch = headingAndPitch.second;

                auto threadId = unit.cobEnvironment->createThread(getAimScriptName(weaponIndex), {toCobAngle(heading).value, toCobAngle(pitch).value});

                if (threadId)
                {
                    aimingState->attackInfo = UnitWeaponStateAttacking::AimInfo{*threadId, heading, pitch};
                }
                else
                {
                    // We couldn't launch an aiming script (there isn't one),
                    // just go straight to firing.
                    if (sim->gameTime >= weapon->readyTime)
                    {
                        aimingState->attackInfo = UnitWeaponStateAttacking::FireInfo{heading, pitch, *targetPosition, std::nullopt, 0, GameTime(0)};
                        tryFireWeapon(id, weaponIndex);
                    }
                }
            }
            else if (auto aimInfo = std::get_if<UnitWeaponStateAttacking::AimInfo>(&aimingState->attackInfo))
            {
                auto returnValue = unit.cobEnvironment->tryReapThread(aimInfo->thread);
                if (returnValue)
                {
                    // we successfully reaped, clear the thread.
                    aimingState->attackInfo = UnitWeaponStateAttacking::IdleInfo{};

                    if (*returnValue)
                    {
                        // aiming was successful, check the target again for drift
                        auto aimFromPosition = getAimingPoint(id, weaponIndex);

                        auto headingAndPitch = computeHeadingAndPitch(unit.rotation, aimFromPosition, *targetPosition, weaponDefinition.velocity, (112_ss / (30_ss * 30_ss)), weapon->ballisticZOffset, weaponDefinition.physicsType);
                        auto heading = headingAndPitch.first;
                        auto pitch = headingAndPitch.second;

                        // if the target is close enough, try to fire
                        if (angleBetweenIsLessOrEqual(heading, aimInfo->lastHeading, weaponDefinition.tolerance) && angleBetweenIsLessOrEqual(pitch, aimInfo->lastPitch, weaponDefinition.pitchTolerance))
                        {

                            if (sim->gameTime >= weapon->readyTime)
                            {
                                aimingState->attackInfo = UnitWeaponStateAttacking::FireInfo{heading, pitch, *targetPosition, std::nullopt, 0, GameTime(0)};
                                tryFireWeapon(id, weaponIndex);
                            }
                        }
                    }
                }
            }
        }
    }

    SimVector UnitBehaviorService::changeDirectionByRandomAngle(const SimVector& direction, SimAngle maxAngle)
    {
        std::uniform_int_distribution dist(SimAngle(0).value, maxAngle.value);
        std::uniform_int_distribution dist2(0, 1);
        auto& rng = sim->rng;
        auto angle = SimAngle(dist(rng));
        if (dist2(rng))
        {
            angle = SimAngle(0) - angle;
        }

        return rotateDirectionXZ(direction, angle);
    }

    void UnitBehaviorService::tryFireWeapon(UnitId id, unsigned int weaponIndex)
    {
        auto& unit = sim->getUnitState(id);
        auto& weapon = unit.weapons[weaponIndex];

        if (!weapon)
        {
            return;
        }

        auto weaponDefIt2 = sim->weaponDefinitions.find(weapon->weaponType);
        if (weaponDefIt2 == sim->weaponDefinitions.end())
        {
            return;
        }
        const auto& weaponDefinition = weaponDefIt2->second;

        auto attackInfo = std::get_if<UnitWeaponStateAttacking>(&weapon->state);
        if (!attackInfo)
        {
            return;
        }

        auto fireInfo = std::get_if<UnitWeaponStateAttacking::FireInfo>(&attackInfo->attackInfo);
        if (!fireInfo)
        {
            return;
        }

        // Air units can only fire when airborne
        const auto& unitDefinition = sim->unitDefinitions.at(unit.unitType);
        if (unitDefinition.canFly)
        {
            if (auto airPhysics = std::get_if<UnitPhysicsInfoAir>(&unit.physics))
            {
                if (!std::holds_alternative<AirMovementStateFlying>(airPhysics->movementState))
                {
                    return; // not airborne, can't fire
                }

                // Fighters (non-hoverAttack) can only fire when heading is roughly aligned with target
                if (!unitDefinition.hoverAttack)
                {
                    auto toTarget = fireInfo->targetPosition - unit.position;
                    auto targetAngle = UnitState::toRotation(toTarget);
                    auto delta = angleBetween(unit.rotation, targetAngle);
                    // Allow firing within ~30 degrees of forward
                    if (delta.value > 5461)
                    {
                        return;
                    }
                }
            }
        }

        // wait for burst reload
        auto gameTime = sim->gameTime;
        if (gameTime < fireInfo->readyTime)
        {
            return;
        }

        // spawn a projectile from the firing point
        if (!fireInfo->firingPiece)
        {
            auto scriptName = getQueryScriptName(weaponIndex);
            fireInfo->firingPiece = runCobQuery(id, scriptName).value_or(0);
        }

        auto firingPoint = unit.getTransform() * getPieceLocalPosition(id, *fireInfo->firingPiece);

        auto direction = match(
            weaponDefinition.physicsType,
            [&](const ProjectilePhysicsTypeLineOfSight&) {
                return (fireInfo->targetPosition - firingPoint).normalized();
            },
            [&](const ProjectilePhysicsTypeTracking&) {
                return (fireInfo->targetPosition - firingPoint).normalized();
            },
            [&](const ProjectilePhysicsTypeBallistic&) {
                return toDirection(fireInfo->heading + unit.rotation, -fireInfo->pitch);
            },
            [&](const ProjectilePhysicsTypeGuided&) {
                return (fireInfo->targetPosition - firingPoint).normalized();
            },
            [&](const ProjectilePhysicsTypeDropped&) {
                return SimVector(0_ss, -1_ss, 0_ss); // bombs drop straight down
            });


        if (weaponDefinition.sprayAngle != SimAngle(0))
        {
            direction = changeDirectionByRandomAngle(direction, weaponDefinition.sprayAngle);
        }

        // Check energy/metal cost per shot (D-Gun and other expensive weapons)
        if (weaponDefinition.energyPerShot.value > 0 || weaponDefinition.metalPerShot.value > 0)
        {
            if (!sim->addResourceDelta(id, -weaponDefinition.energyPerShot, -weaponDefinition.metalPerShot))
            {
                return; // not enough resources to fire
            }
        }

        auto targetUnit = std::get_if<UnitId>(&attackInfo->target);
        auto targetUnitOption = targetUnit == nullptr ? std::optional<UnitId>() : std::make_optional(*targetUnit);
        sim->spawnProjectile(unit.owner, *weapon, firingPoint, direction, (fireInfo->targetPosition - firingPoint).length(), targetUnitOption, id);

        sim->events.push_back(FireWeaponEvent{weapon->weaponType, fireInfo->burstsFired, firingPoint});

        // If we just started the burst, set the reload timer
        if (fireInfo->burstsFired == 0)
        {
            unit.cobEnvironment->createThread(getFireScriptName(weaponIndex));
            weapon->readyTime = gameTime + deltaSecondsToTicks(weaponDefinition.reloadTime);
        }

        ++fireInfo->burstsFired;
        fireInfo->readyTime = gameTime + deltaSecondsToTicks(weaponDefinition.burstInterval);
        if (fireInfo->burstsFired >= weaponDefinition.burst)
        {
            // we finished our burst, we are reloading now
            attackInfo->attackInfo = UnitWeaponStateAttacking::IdleInfo{};
        }
    }

    void UnitBehaviorService::applyUnitSteering(UnitInfo unitInfo)
    {
        updateUnitRotation(unitInfo);
        updateUnitSpeed(unitInfo);
    }

    void UnitBehaviorService::updateUnitRotation(UnitInfo unitInfo)
    {
        auto turnRateThisFrame = SimAngle(unitInfo.definition->turnRate.value);
        unitInfo.state->previousRotation = unitInfo.state->rotation;

        match(
            unitInfo.state->physics,
            [&](const UnitPhysicsInfoGround& p) {
                unitInfo.state->rotation = turnTowards(unitInfo.state->rotation, p.steeringInfo.targetAngle, turnRateThisFrame);
            },
            [&](const UnitPhysicsInfoAir& p) {
                match(
                    p.movementState,
                    [&](const AirMovementStateTakingOff&) {
                        // do nothing
                    },
                    [&](const AirMovementStateLanding&) {
                        // do nothing
                    },
                    [&](const AirMovementStateFlying& m) {
                        if (!m.targetPosition)
                        {
                            // Smoothly level out when not turning
                            unitInfo.state->bankAngle = unitInfo.state->bankAngle * SimScalar(0.9f);
                            return;
                        }
                        auto direction = *m.targetPosition - unitInfo.state->position;
                        auto targetAngle = UnitState::toRotation(direction);
                        auto prevRotation = unitInfo.state->rotation;
                        unitInfo.state->rotation = turnTowards(unitInfo.state->rotation, targetAngle, turnRateThisFrame);

                        // Compute banking from heading difference to target
                        auto [anticlockwise, headingDelta] = angleBetweenWithDirection(unitInfo.state->rotation, targetAngle);
                        auto headingRad = static_cast<float>(headingDelta.value) / 65536.0f * 6.2832f;
                        if (anticlockwise)
                        {
                            headingRad = -headingRad;
                        }
                        // Bank proportional to heading error, max ~45 degrees (0.78 rad)
                        auto targetBank = SimScalar(std::clamp(headingRad * 3.0f, -0.78f, 0.78f));
                        // Responsive smoothing
                        unitInfo.state->bankAngle = unitInfo.state->bankAngle + (targetBank - unitInfo.state->bankAngle) * SimScalar(0.3f);
                    });
            });
    }

    void UnitBehaviorService::updateUnitSpeed(UnitInfo unitInfo)
    {
        match(
            unitInfo.state->physics,
            [&](UnitPhysicsInfoGround& p) {
                p.currentSpeed = computeNewGroundUnitSpeed(sim->terrain, *unitInfo.state, *unitInfo.definition, p);
            },
            [&](UnitPhysicsInfoAir& p) {
                match(
                    p.movementState,
                    [&](AirMovementStateFlying& m) {
                        m.currentVelocity = computeNewAirUnitVelocity(*unitInfo.state, *unitInfo.definition, m);
                    },
                    [&](const AirMovementStateTakingOff&) {
                        // do nothing
                    },
                    [&](const AirMovementStateLanding&) {
                        // do nothing
                    });
            });
    }

    void UnitBehaviorService::updateGroundUnitPosition(UnitInfo unitInfo, const UnitPhysicsInfoGround& physics)
    {
        auto direction = UnitState::toDirection(unitInfo.state->rotation);

        if (physics.currentSpeed > 0_ss)
        {
            auto newPosition = unitInfo.state->position + (direction * physics.currentSpeed);
            newPosition.y = sim->terrain.getHeightAt(newPosition.x, newPosition.z);
            if (unitInfo.definition->floater || unitInfo.definition->canHover)
            {
                newPosition.y = rweMax(newPosition.y, sim->terrain.getSeaLevel());
            }

            if (!tryApplyMovementToPosition(unitInfo, newPosition))
            {
                unitInfo.state->inCollision = true;

                // if we failed to move, try in each axis separately
                // to see if we can complete a "partial" movement
                const SimVector maskX(0_ss, 1_ss, 1_ss);
                const SimVector maskZ(1_ss, 1_ss, 0_ss);

                SimVector newPos1;
                SimVector newPos2;
                if (direction.x > direction.z)
                {
                    newPos1 = unitInfo.state->position + (direction * maskZ * physics.currentSpeed);
                    newPos2 = unitInfo.state->position + (direction * maskX * physics.currentSpeed);
                }
                else
                {
                    newPos1 = unitInfo.state->position + (direction * maskX * physics.currentSpeed);
                    newPos2 = unitInfo.state->position + (direction * maskZ * physics.currentSpeed);
                }
                newPos1.y = sim->terrain.getHeightAt(newPos1.x, newPos1.z);
                newPos2.y = sim->terrain.getHeightAt(newPos2.x, newPos2.z);

                if (unitInfo.definition->floater || unitInfo.definition->canHover)
                {
                    newPos1.y = rweMax(newPos1.y, sim->terrain.getSeaLevel());
                    newPos2.y = rweMax(newPos2.y, sim->terrain.getSeaLevel());
                }

                if (!tryApplyMovementToPosition(unitInfo, newPos1))
                {
                    tryApplyMovementToPosition(unitInfo, newPos2);
                }
            }
        }
    }

    void UnitBehaviorService::updateUnitPosition(UnitInfo unitInfo)
    {
        unitInfo.state->previousPosition = unitInfo.state->position;
        unitInfo.state->inCollision = false;

        match(
            unitInfo.state->physics,
            [&](const UnitPhysicsInfoGround& p) {
                updateGroundUnitPosition(unitInfo, p);
            },
            [&](const UnitPhysicsInfoAir& p) {
                match(
                    p.movementState,
                    [&](const AirMovementStateFlying& m) {
                        // Apply only XZ velocity; altitude is computed independently
                        auto velocity = m.currentVelocity;
                        velocity.y = 0_ss;
                        auto newPosition = unitInfo.state->position + velocity;

                        // Lookahead altitude: sample ahead along movement direction
                        // so the aircraft starts climbing/descending before reaching obstacles.
                        // Also sample at current position for immediate terrain.
                        auto altitudeHere = getTargetAltitude(*sim, newPosition.x, newPosition.z, *unitInfo.definition);
                        auto targetAlt = altitudeHere;

                        // Sample ahead using the unit's facing direction (more stable than velocity)
                        auto facingDir = UnitState::toDirection(unitInfo.state->rotation);
                        // Sample at 8 points ahead (48, 96, ..., 384 world units)
                        for (int i = 1; i <= 8; ++i)
                        {
                            auto lookX = newPosition.x + facingDir.x * SimScalar(i * 48);
                            auto lookZ = newPosition.z + facingDir.z * SimScalar(i * 48);
                            targetAlt = rweMax(targetAlt, getTargetAltitude(*sim, lookX, lookZ, *unitInfo.definition));
                        }

                        // Smoothly approach the target altitude
                        auto currentY = unitInfo.state->position.y;
                        auto climbRate = 3_ss; // units per tick (90 units/second)
                        if (currentY < targetAlt)
                        {
                            newPosition.y = rweMin(currentY + climbRate, targetAlt);
                        }
                        else if (currentY > targetAlt)
                        {
                            newPosition.y = rweMax(currentY - climbRate, targetAlt);
                        }
                        else
                        {
                            newPosition.y = targetAlt;
                        }

                        tryApplyMovementToPosition(unitInfo, newPosition);
                    },
                    [&](const AirMovementStateTakingOff&) {
                        climbToCruiseAltitude(unitInfo);
                    },
                    [&](const AirMovementStateLanding&) {
                        descendToGroundLevel(unitInfo);
                    });
            });
    }

    bool UnitBehaviorService::tryApplyMovementToPosition(UnitInfo unitInfo, const SimVector& newPosition)
    {
        // No collision for flying units.
        if (isFlying(unitInfo.state->physics))
        {
            unitInfo.state->position = newPosition;
            return true;
        }

        // check for collision at the new position
        auto newFootprintRegion = sim->computeFootprintRegion(newPosition, unitInfo.definition->movementCollisionInfo);

        if (sim->isCollisionAt(newFootprintRegion, unitInfo.id))
        {
            return false;
        }

        // Unlike for pathfinding, TA doesn't care about the unit's actual movement class for collision checks,
        // it only cares about the attributes defined directly on the unitInfo.state->
        // Jam these into an ad-hoc movement class to pass into our walkability check.
        if (!isGridPointWalkable(sim->terrain, sim->getAdHocMovementClass(unitInfo.definition->movementCollisionInfo), newFootprintRegion.x, newFootprintRegion.y))
        {
            return false;
        }

        // we passed all collision checks, update accordingly
        auto footprintRegion = sim->computeFootprintRegion(unitInfo.state->position, unitInfo.definition->movementCollisionInfo);
        sim->moveUnitOccupiedArea(footprintRegion, newFootprintRegion, unitInfo.id);

        auto seaLevel = sim->terrain.getSeaLevel();
        auto oldTerrainHeight = sim->terrain.getHeightAt(unitInfo.state->position.x, unitInfo.state->position.z);
        auto oldPosBelowSea = oldTerrainHeight < seaLevel;

        unitInfo.state->position = newPosition;

        auto newTerrainHeight = sim->terrain.getHeightAt(unitInfo.state->position.x, unitInfo.state->position.z);
        auto newPosBelowSea = newTerrainHeight < seaLevel;

        if (oldPosBelowSea && !newPosBelowSea)
        {
            unitInfo.state->cobEnvironment->createThread("setSFXoccupy", std::vector<int>{4});
        }
        else if (!oldPosBelowSea && newPosBelowSea)
        {
            unitInfo.state->cobEnvironment->createThread("setSFXoccupy", std::vector<int>{2});
        }

        return true;
    }

    std::optional<int> UnitBehaviorService::runCobQuery(UnitId id, const std::string& name)
    {
        auto& unit = sim->getUnitState(id);
        auto thread = unit.cobEnvironment->createNonScheduledThread(name, {0});
        if (!thread)
        {
            return std::nullopt;
        }
        CobExecutionContext context(unit.cobEnvironment.get(), &*thread);
        auto status = context.execute();
        if (std::get_if<CobEnvironment::FinishedStatus>(&status) == nullptr)
        {
            throw std::runtime_error("Synchronous cob query thread blocked before completion");
        }

        auto result = thread->returnLocals[0];
        return result;
    }

    SimVector UnitBehaviorService::getAimingPoint(UnitId id, unsigned int weaponIndex)
    {
        const auto& unit = sim->getUnitState(id);
        return unit.getTransform() * getLocalAimingPoint(id, weaponIndex);
    }

    SimVector UnitBehaviorService::getLocalAimingPoint(UnitId id, unsigned int weaponIndex)
    {
        auto scriptName = getAimFromScriptName(weaponIndex);
        auto pieceId = runCobQuery(id, scriptName);
        if (!pieceId)
        {
            return getLocalFiringPoint(id, weaponIndex);
        }

        return getPieceLocalPosition(id, *pieceId);
    }

    SimVector UnitBehaviorService::getLocalFiringPoint(UnitId id, unsigned int weaponIndex)
    {

        auto scriptName = getQueryScriptName(weaponIndex);
        auto pieceId = runCobQuery(id, scriptName);
        if (!pieceId)
        {
            return SimVector(0_ss, 0_ss, 0_ss);
        }

        return getPieceLocalPosition(id, *pieceId);
    }

    SimVector UnitBehaviorService::getSweetSpot(UnitId id)
    {
        auto pieceId = runCobQuery(id, "SweetSpot");
        if (!pieceId)
        {
            return sim->getUnitState(id).position;
        }

        return getPiecePosition(id, *pieceId);
    }

    std::optional<SimVector> UnitBehaviorService::tryGetSweetSpot(UnitId id)
    {
        if (!sim->unitExists(id))
        {
            return std::nullopt;
        }

        return getSweetSpot(id);
    }

    bool UnitBehaviorService::handleOrder(UnitInfo unitInfo, const UnitOrder& order)
    {
        return match(
            order,
            [&](const MoveOrder& o) {
                return handleMoveOrder(unitInfo, o);
            },
            [&](const AttackOrder& o) {
                return handleAttackOrder(unitInfo, o);
            },
            [&](const BuildOrder& o) {
                return handleBuildOrder(unitInfo, o);
            },
            [&](const BuggerOffOrder& o) {
                return handleBuggerOffOrder(unitInfo, o);
            },
            [&](const CompleteBuildOrder& o) {
                return handleCompleteBuildOrder(unitInfo, o);
            },
            [&](const GuardOrder& o) {
                return handleGuardOrder(unitInfo, o);
            },
            [&](const ReclaimOrder& o) {
                return handleReclaimOrder(unitInfo, o);
            },
            [&](const CaptureOrder& o) {
                return handleCaptureOrder(unitInfo, o);
            });
    }

    bool UnitBehaviorService::handleMoveOrder(UnitInfo unitInfo, const MoveOrder& moveOrder)
    {
        if (!unitInfo.definition->isMobile)
        {
            return false;
        }

        if (navigateTo(unitInfo, moveOrder.destination))
        {
            sim->events.push_back(UnitArrivedEvent{unitInfo.id});
            return true;
        }

        return false;
    }

    bool UnitBehaviorService::handleAttackOrder(UnitInfo unitInfo, const AttackOrder& attackOrder)
    {
        return attackTarget(unitInfo, attackOrder.target);
    }

    NavigationGoal attackTargetToNavigationGoal(const AttackTarget& target)
    {
        return match(
            target,
            [&](const UnitId& t) -> NavigationGoal {
                return t;
            },
            [&](const SimVector& t) -> NavigationGoal {
                return t;
            });
    }

    bool UnitBehaviorService::attackTarget(UnitInfo unitInfo, const AttackTarget& target)
    {
        if (!unitInfo.state->weapons[0])
        {
            return true;
        }

        auto atkWeaponIt = sim->weaponDefinitions.find(unitInfo.state->weapons[0]->weaponType);
        if (atkWeaponIt == sim->weaponDefinitions.end())
        {
            return true;
        }
        const auto& weaponDefinition = atkWeaponIt->second;
        auto atkUnitDefIt = sim->unitDefinitions.find(unitInfo.state->unitType);
        if (atkUnitDefIt == sim->unitDefinitions.end())
        {
            return true;
        }
        const auto& unitDefinition = atkUnitDefIt->second;

        auto targetPosition = getTargetPosition(target);
        if (!targetPosition)
        {
            // target has gone away, throw away this order
            return true;
        }

        auto maxRangeSquared = weaponDefinition.maxRange * weaponDefinition.maxRange;
        auto inRange = unitInfo.state->position.distanceSquared(*targetPosition) <= maxRangeSquared;

        // Air units without hoverAttack do strafing runs: always fly toward the target
        // so that their forward direction aligns with the firing direction.
        // Gunships (hoverAttack=true) can stop and fire like ground units.
        if (unitDefinition.canFly && !unitDefinition.hoverAttack)
        {
            // Always navigate toward target for attack run
            navigateTo(unitInfo, attackTargetToNavigationGoal(target));

            // Also aim weapons if in range
            if (inRange)
            {
                for (unsigned int i = 0; i < 2; ++i)
                {
                    match(
                        target,
                        [&](const UnitId& u) { unitInfo.state->setWeaponTarget(i, u); },
                        [&](const SimVector& v) { unitInfo.state->setWeaponTarget(i, v); });
                }
            }
        }
        else
        {
            if (!inRange)
            {
                navigateTo(unitInfo, attackTargetToNavigationGoal(target));
            }
            else
            {
                // Gunships: drift slowly around the target while attacking
                if (unitDefinition.canFly && unitDefinition.hoverAttack)
                {
                    auto driftPhase = sin(SimAngle(sim->gameTime.value * 30));
                    auto driftPerp = SimVector(cos(unitInfo.state->rotation), 0_ss, -sin(unitInfo.state->rotation));
                    auto driftPos = *targetPosition + driftPerp * driftPhase * 40_ss;
                    navigateTo(unitInfo, driftPos);
                }

                // Aim weapons
                for (unsigned int i = 0; i < 2; ++i)
                {
                    match(
                        target,
                        [&](const UnitId& u) { unitInfo.state->setWeaponTarget(i, u); },
                        [&](const SimVector& v) { unitInfo.state->setWeaponTarget(i, v); });
                }
            }
        }

        return false;
    }

    bool UnitBehaviorService::handleBuildOrder(UnitInfo unitInfo, const BuildOrder& buildOrder)
    {
        return buildUnit(unitInfo, buildOrder.unitType, buildOrder.position);
    }

    bool UnitBehaviorService::handleBuggerOffOrder(UnitInfo unitInfo, const BuggerOffOrder& buggerOffOrder)
    {
        auto [footprintX, footprintZ] = sim->getFootprintXZ(unitInfo.definition->movementCollisionInfo);
        return navigateTo(unitInfo, buggerOffOrder.rect.expand((footprintX * 3) - 4, (footprintZ * 3) - 4));
    }

    bool UnitBehaviorService::handleCompleteBuildOrder(UnitInfo unitInfo, const rwe::CompleteBuildOrder& buildOrder)
    {
        return buildExistingUnit(unitInfo, buildOrder.target);
    }

    bool UnitBehaviorService::handleGuardOrder(UnitInfo unitInfo, const GuardOrder& guardOrder)
    {
        auto target = sim->tryGetUnitState(guardOrder.target);
        // TODO: real allied check here
        if (!target || !target->get().isOwnedBy(unitInfo.state->owner))
        {
            // unit is dead or a traitor, abandon order
            return true;
        }
        auto& targetUnit = target->get();


        // assist building
        if (auto bs = std::get_if<UnitBehaviorStateBuilding>(&targetUnit.behaviourState); unitInfo.definition->builder && bs)
        {
            buildExistingUnit(unitInfo, bs->targetUnit);
            return false;
        }

        // assist factory building
        if (auto fs = std::get_if<FactoryBehaviorStateBuilding>(&targetUnit.factoryState); unitInfo.definition->builder && fs)
        {
            if (fs->targetUnit)
            {
                buildExistingUnit(unitInfo, fs->targetUnit->first);
                return false;
            }
        }

        // stay close
        if (unitInfo.definition->canMove && unitInfo.state->position.distanceSquared(targetUnit.position) > SimScalar(200 * 200))
        {
            navigateTo(unitInfo, guardOrder.target);
            return false;
        }

        return false;
    }

    bool UnitBehaviorService::handleReclaimOrder(UnitInfo unitInfo, const ReclaimOrder& reclaimOrder)
    {
        if (!unitInfo.definition->builder)
        {
            return true; // non-builders can't reclaim
        }

        return match(
            reclaimOrder.target,
            [&](const FeatureId& featureId) -> bool {
                auto featureIt = sim->features.find(featureId);
                if (featureIt == sim->features.end())
                {
                    return true; // feature gone
                }

                auto& feature = featureIt->second;
                const auto& featureDef = sim->getFeatureDefinition(feature.featureName);
                if (!featureDef.reclaimable)
                {
                    return true;
                }

                // Navigate to the feature
                if (unitInfo.state->position.distanceSquared(feature.position) > SimScalar(100) * SimScalar(100))
                {
                    navigateTo(unitInfo, feature.position);
                    return false;
                }

                // In range — reclaim: give resources to player each tick
                auto metalPerTick = Metal(static_cast<float>(featureDef.metal) * static_cast<float>(unitInfo.definition->workerTimePerTick) / 1000.0f);
                auto energyPerTick = Energy(static_cast<float>(featureDef.energy) * static_cast<float>(unitInfo.definition->workerTimePerTick) / 1000.0f);
                sim->addResourceDelta(unitInfo.id, energyPerTick, metalPerTick);

                // TODO: track reclaim progress and remove feature when complete
                // For now, give resources each tick while near the feature

                return false;
            },
            [&](const UnitId& targetId) -> bool {
                auto targetRef = sim->tryGetUnitState(targetId);
                if (!targetRef)
                {
                    return true; // unit gone
                }

                auto& target = targetRef->get();

                // Navigate to the unit
                if (unitInfo.state->position.distanceSquared(target.position) > SimScalar(100) * SimScalar(100))
                {
                    navigateTo(unitInfo, targetId);
                    return false;
                }

                // In range — drain HP and give resources
                const auto& targetDef = sim->unitDefinitions.at(target.unitType);
                auto metalPerTick = Metal(static_cast<float>(targetDef.buildCostMetal.value) * static_cast<float>(unitInfo.definition->workerTimePerTick) / static_cast<float>(targetDef.buildTime));
                auto energyPerTick = Energy(static_cast<float>(targetDef.buildCostEnergy.value) * static_cast<float>(unitInfo.definition->workerTimePerTick) / static_cast<float>(targetDef.buildTime));
                sim->addResourceDelta(unitInfo.id, energyPerTick, metalPerTick);

                if (target.hitPoints > unitInfo.definition->workerTimePerTick)
                {
                    target.hitPoints -= unitInfo.definition->workerTimePerTick;
                }
                else
                {
                    sim->killUnit(targetId);
                    return true;
                }

                return false;
            });
    }

    bool UnitBehaviorService::handleCaptureOrder(UnitInfo unitInfo, const CaptureOrder& captureOrder)
    {
        if (!unitInfo.definition->builder)
        {
            return true;
        }

        auto targetRef = sim->tryGetUnitState(captureOrder.target);
        if (!targetRef)
        {
            return true;
        }

        auto& target = targetRef->get();

        // Can't capture own units
        if (target.isOwnedBy(unitInfo.state->owner))
        {
            return true;
        }

        // Navigate to the unit
        if (unitInfo.state->position.distanceSquared(target.position) > SimScalar(100) * SimScalar(100))
        {
            navigateTo(unitInfo, captureOrder.target);
            return false;
        }

        // In range — gradually capture (drain HP, then convert when low)
        const auto& targetDef = sim->unitDefinitions.at(target.unitType);
        auto captureRate = unitInfo.definition->workerTimePerTick;

        if (target.hitPoints > targetDef.maxHitPoints / 4)
        {
            // Drain HP during capture
            if (target.hitPoints > captureRate)
            {
                target.hitPoints -= captureRate;
            }
            else
            {
                target.hitPoints = 1;
            }
        }
        else
        {
            // Below 25% HP — captured! Change owner
            target.owner = unitInfo.state->owner;
            target.hitPoints = targetDef.maxHitPoints / 2; // restore some HP
            return true;
        }

        return false;
    }

    bool UnitBehaviorService::handleBuild(UnitInfo unitInfo, const std::string& unitType)
    {
        return match(
            unitInfo.state->factoryState,
            [&](const FactoryBehaviorStateIdle&) {
                sim->activateUnit(unitInfo.id);
                unitInfo.state->factoryState = FactoryBehaviorStateBuilding();
                return false;
            },
            [&](FactoryBehaviorStateCreatingUnit& state) {
                return match(
                    state.status,
                    [&](const UnitCreationStatusPending&) {
                        return false;
                    },
                    [&](const UnitCreationStatusDone& s) {
                        unitInfo.state->cobEnvironment->createThread("StartBuilding");
                        unitInfo.state->factoryState = FactoryBehaviorStateBuilding{std::make_pair(s.unitId, std::optional<SimVector>())};
                        return false;
                    },
                    [&](const UnitCreationStatusFailed&) {
                        unitInfo.state->factoryState = FactoryBehaviorStateBuilding();
                        return false;
                    });
            },
            [&](FactoryBehaviorStateBuilding& state) {
                if (!unitInfo.state->inBuildStance)
                {
                    return false;
                }

                auto buildPieceInfo = getBuildPieceInfo(unitInfo.id);
                // buildPieceInfo.position.y = sim->terrain.getHeightAt(buildPieceInfo.position.x, buildPieceInfo.position.z);
                if (!state.targetUnit)
                {
                    unitInfo.state->factoryState = FactoryBehaviorStateCreatingUnit{unitType, unitInfo.state->owner, buildPieceInfo.position, buildPieceInfo.rotation};
                    sim->unitCreationRequests.push_back(unitInfo.id);
                    return false;
                }

                auto targetUnitOption = sim->tryGetUnitState(state.targetUnit->first);
                if (!targetUnitOption)
                {
                    unitInfo.state->factoryState = FactoryBehaviorStateCreatingUnit{unitType, unitInfo.state->owner, buildPieceInfo.position, buildPieceInfo.rotation};
                    sim->unitCreationRequests.push_back(unitInfo.id);
                    return false;
                }

                auto& targetUnit = targetUnitOption->get();
                const auto& targetUnitDefinition = sim->unitDefinitions.at(targetUnit.unitType);

                if (targetUnit.unitType != unitType)
                {
                    if (targetUnit.isBeingBuilt(targetUnitDefinition) && !targetUnit.isDead())
                    {
                        sim->quietlyKillUnit(state.targetUnit->first);
                    }
                    state.targetUnit = std::nullopt;
                    return false;
                }

                if (targetUnit.isDead())
                {
                    unitInfo.state->cobEnvironment->createThread("StopBuilding");
                    sim->deactivateUnit(unitInfo.id);
                    unitInfo.state->factoryState = FactoryBehaviorStateIdle();
                    return true;
                }

                if (!targetUnit.isBeingBuilt(targetUnitDefinition))
                {
                    if (unitInfo.state->orders.empty())
                    {
                        auto footprintRect = sim->computeFootprintRegion(unitInfo.state->position, unitInfo.definition->movementCollisionInfo);
                        targetUnit.addOrder(BuggerOffOrder(footprintRect));
                    }
                    else
                    {
                        targetUnit.replaceOrders(unitInfo.state->orders);
                    }
                    unitInfo.state->cobEnvironment->createThread("StopBuilding");
                    sim->deactivateUnit(unitInfo.id);
                    unitInfo.state->factoryState = FactoryBehaviorStateIdle();
                    return true;
                }

                if (targetUnitDefinition.floater || targetUnitDefinition.canHover)
                {
                    buildPieceInfo.position.y = rweMax(buildPieceInfo.position.y, sim->terrain.getSeaLevel());
                }

                tryApplyMovementToPosition(sim->getUnitInfo(state.targetUnit->first), buildPieceInfo.position);
                targetUnit.rotation = buildPieceInfo.rotation;

                auto costs = targetUnit.getBuildCostInfo(targetUnitDefinition, unitInfo.definition->workerTimePerTick);
                auto gotResources = sim->addResourceDelta(
                    unitInfo.id,
                    -Energy(targetUnitDefinition.buildCostEnergy.value * static_cast<float>(unitInfo.definition->workerTimePerTick) / static_cast<float>(targetUnitDefinition.buildTime)),
                    -Metal(targetUnitDefinition.buildCostMetal.value * static_cast<float>(unitInfo.definition->workerTimePerTick) / static_cast<float>(targetUnitDefinition.buildTime)),
                    -costs.energyCost,
                    -costs.metalCost);

                if (!gotResources)
                {
                    // we don't have resources available to build -- wait
                    state.targetUnit->second = std::nullopt;
                    return false;
                }
                state.targetUnit->second = getNanoPoint(unitInfo.id);

                if (targetUnit.addBuildProgress(targetUnitDefinition, unitInfo.definition->workerTimePerTick))
                {
                    sim->events.push_back(UnitCompleteEvent{state.targetUnit->first});

                    if (targetUnitDefinition.activateWhenBuilt)
                    {
                        sim->activateUnit(state.targetUnit->first);
                    }
                }

                return false;
            });
    }

    void UnitBehaviorService::clearBuild(UnitInfo unitInfo)
    {
        match(
            unitInfo.state->factoryState,
            [&](const FactoryBehaviorStateIdle&) {
                // do nothing
            },
            [&](const FactoryBehaviorStateCreatingUnit& state) {
                match(
                    state.status,
                    [&](const UnitCreationStatusDone& d) {
                        sim->quietlyKillUnit(d.unitId);
                    },
                    [&](const auto&) {
                        // do nothing
                    });
                sim->deactivateUnit(unitInfo.id);
                unitInfo.state->factoryState = FactoryBehaviorStateIdle();
            },
            [&](const FactoryBehaviorStateBuilding& state) {
                if (state.targetUnit)
                {
                    sim->quietlyKillUnit(state.targetUnit->first);
                    unitInfo.state->cobEnvironment->createThread("StopBuilding");
                }
                sim->deactivateUnit(unitInfo.id);
                unitInfo.state->factoryState = FactoryBehaviorStateIdle();
            });
    }

    SimVector UnitBehaviorService::getNanoPoint(UnitId id)
    {
        auto pieceId = runCobQuery(id, "QueryNanoPiece");
        if (!pieceId)
        {
            return sim->getUnitState(id).position;
        }

        return getPiecePosition(id, *pieceId);
    }

    SimVector UnitBehaviorService::getPieceLocalPosition(UnitId id, unsigned int pieceId)
    {
        auto& unit = sim->getUnitState(id);

        const auto& pieceName = unit.cobEnvironment->_script->pieces.at(pieceId);
        auto pieceTransform = sim->getUnitPieceLocalTransform(id, pieceName);

        return pieceTransform * SimVector(0_ss, 0_ss, 0_ss);
    }

    SimVector UnitBehaviorService::getPiecePosition(UnitId id, unsigned int pieceId)
    {
        auto& unit = sim->getUnitState(id);

        return unit.getTransform() * getPieceLocalPosition(id, pieceId);
    }

    SimAngle UnitBehaviorService::getPieceXZRotation(UnitId id, unsigned int pieceId)
    {
        auto& unit = sim->getUnitState(id);

        const auto& pieceName = unit.cobEnvironment->_script->pieces.at(pieceId);
        auto pieceTransform = sim->getUnitPieceLocalTransform(id, pieceName);

        auto mat = unit.getTransform() * pieceTransform;

        auto a = Vector2x<SimScalar>(0_ss, 1_ss);
        auto b = mat.mult3x3(SimVector(0_ss, 0_ss, 1_ss)).xz();
        if (b.lengthSquared() == 0_ss)
        {
            return SimAngle(0);
        }

        // angleTo is computed in a space where Y points up,
        // but in our XZ space (Z is our Y here), Z points down.
        // This means we need to negate (and rewrap) the rotation value.
        return -angleTo(a, b);
    }

    UnitBehaviorService::BuildPieceInfo UnitBehaviorService::getBuildPieceInfo(UnitId id)
    {
        auto pieceId = runCobQuery(id, "QueryBuildInfo");
        if (!pieceId)
        {
            const auto& unit = sim->getUnitState(id);
            return BuildPieceInfo{unit.position, unit.rotation};
        }

        return BuildPieceInfo{getPiecePosition(id, *pieceId), getPieceXZRotation(id, *pieceId)};
    }

    std::optional<SimVector> UnitBehaviorService::getTargetPosition(const UnitWeaponAttackTarget& target)
    {
        return match(
            target,
            [](const SimVector& v) { return std::make_optional(v); },
            [this](UnitId id) { return tryGetSweetSpot(id); });
    }

    PathDestination UnitBehaviorService::resolvePathDestination(UnitState& s, const MovingStateGoal& goal)
    {
        return match(
            goal,
            [&](const SimVector& v) -> PathDestination {
                return v;
            },
            [&](const DiscreteRect& r) -> PathDestination {
                return r;
            },
            [&](const UnitId& u) -> PathDestination {
                return getUnitPositionWithCache(s, u);
            });
    }

    void UnitBehaviorService::groundUnitMoveTo(UnitInfo unitInfo, const MovingStateGoal& goal)
    {
        auto movingState = std::get_if<NavigationStateMoving>(&unitInfo.state->navigationState.state);

        if (!movingState || movingState->movementGoal != goal)
        {
            // request a path to follow
            unitInfo.state->navigationState.state = NavigationStateMoving{goal, resolvePathDestination(*unitInfo.state, goal), std::nullopt, true};
            sim->requestPath(unitInfo.id);
            return;
        }

        // check to see if our goal has moved from its original location
        auto resolvedDestination = resolvePathDestination(*unitInfo.state, goal);
        if (resolvedDestination != movingState->pathDestination)
        {
            // The resolved position of our goal has changed.
            // We'll assume that this change isn't too big
            // i.e. we don't need to throw away our previous path,
            // we can still continue following it
            // while we wait for a new path to be computed.
            movingState->pathDestination = resolvedDestination;
            sim->requestPath(unitInfo.id);
            movingState->pathRequested = true;
        }

        // if we are colliding, request a new path
        if (unitInfo.state->inCollision && !movingState->pathRequested)
        {
            // only request a new path if we don't have one yet,
            // or we've already had our current one for a bit
            if (!movingState->path || (sim->gameTime - movingState->path->pathCreationTime) >= GameTime(30))
            {
                sim->requestPath(unitInfo.id);
                movingState->pathRequested = true;
            }
        }

        // if a path is available, attempt to follow it
        if (movingState->path)
        {
            auto groundPhysics = std::get_if<UnitPhysicsInfoGround>(&unitInfo.state->physics);
            if (groundPhysics == nullptr)
            {
                throw std::logic_error("ground unit does not have ground physics");
            }
            if (followPath(unitInfo, *groundPhysics, *movingState->path))
            {
                // We finished following the path.
                // This doesn't necessarily mean we are at the goal.
                // The path might have been a partial path.
                // Request a new path to get us the rest of the way there.
                if (!movingState->path || (sim->gameTime - movingState->path->pathCreationTime) >= GameTime(30))
                {
                    sim->requestPath(unitInfo.id);
                    movingState->pathRequested = true;
                }
            }
        }
    }

    bool UnitBehaviorService::flyingUnitMoveTo(UnitInfo unitInfo, const MovingStateGoal& goal)
    {
        return match(
            unitInfo.state->physics,
            [&](UnitPhysicsInfoGround& p) {
                p.steeringInfo.shouldTakeOff = true;
                return false;
            },
            [&](const UnitPhysicsInfoAir& p) {
                return flyTowardsGoal(unitInfo, goal);
            });
    }

    bool UnitBehaviorService::navigateTo(UnitInfo unitInfo, const NavigationGoal& goal)
    {
        unitInfo.state->navigationState.desiredDestination = goal;

        return hasReachedGoal(*sim, sim->terrain, *unitInfo.state, *unitInfo.definition, goal);
    }

    void UnitBehaviorService::moveTo(UnitInfo unitInfo, const MovingStateGoal& goal)
    {
        if (unitInfo.definition->canFly)
        {
            flyingUnitMoveTo(unitInfo, goal);
        }
        else
        {
            groundUnitMoveTo(unitInfo, goal);
        }
    }

    UnitCreationStatus UnitBehaviorService::createNewUnit(UnitInfo unitInfo, const std::string& unitType, const SimVector& position)
    {
        if (auto s = std::get_if<UnitBehaviorStateCreatingUnit>(&unitInfo.state->behaviourState))
        {
            if (s->unitType == unitType && s->position == position)
            {
                return s->status;
            }
        }

        const auto& targetUnitDefinition = sim->unitDefinitions.at(unitType);
        auto footprintRect = sim->computeFootprintRegion(position, targetUnitDefinition.movementCollisionInfo);
        if (navigateTo(unitInfo, footprintRect))
        {
            // TODO: add an additional distance check here -- we may have done the best
            // we can to move but been prevented by some obstacle, so we are too far away still.
            changeState(*unitInfo.state, UnitBehaviorStateCreatingUnit{unitType, unitInfo.state->owner, position});
            sim->unitCreationRequests.push_back(unitInfo.id);
        }

        return UnitCreationStatusPending();
    }

    bool UnitBehaviorService::buildUnit(UnitInfo unitInfo, const std::string& unitType, const SimVector& position)
    {
        auto& unit = sim->getUnitState(unitInfo.id);
        if (!unit.buildOrderUnitId)
        {
            auto result = createNewUnit(unitInfo, unitType, position);
            return match(
                result,
                [&](const UnitCreationStatusPending&) { return false; },
                [&](const UnitCreationStatusFailed&) { return true; },
                [&](const UnitCreationStatusDone& d) {
                    unit.buildOrderUnitId = d.unitId;
                    return deployBuildArm(unitInfo, d.unitId);
                });
        }

        return deployBuildArm(unitInfo, *unit.buildOrderUnitId);
    }

    bool UnitBehaviorService::buildExistingUnit(UnitInfo unitInfo, UnitId targetUnitId)
    {
        auto targetUnitRef = sim->tryGetUnitState(targetUnitId);

        if (!targetUnitRef || targetUnitRef->get().isDead() || !targetUnitRef->get().isBeingBuilt(*unitInfo.definition))
        {
            changeState(*unitInfo.state, UnitBehaviorStateIdle());
            return true;
        }
        auto& targetUnit = targetUnitRef->get();

        // FIXME: this distance measure is wrong
        // Experiment has shown that the distance from which a new building
        // can be started (when caged in) is greater than assist distance,
        // and it appears both measures something more advanced than center <-> center distance.
        if (unitInfo.state->position.distanceSquared(targetUnit.position) > (unitInfo.definition->buildDistance * unitInfo.definition->buildDistance))
        {
            navigateTo(unitInfo, targetUnitId);
            return false;
        }

        // we're close enough -- actually build the unit
        return deployBuildArm(unitInfo, targetUnitId);
    }

    void UnitBehaviorService::changeState(UnitState& unit, const UnitBehaviorState& newState)
    {
        if (std::holds_alternative<UnitBehaviorStateBuilding>(unit.behaviourState))
        {
            unit.cobEnvironment->createThread("StopBuilding");
        }
        unit.behaviourState = newState;
    }
    bool UnitBehaviorService::deployBuildArm(UnitInfo unitInfo, UnitId targetUnitId)
    {
        auto targetUnitRef = sim->tryGetUnitState(targetUnitId);
        if (!targetUnitRef || targetUnitRef->get().isDead() || !targetUnitRef->get().isBeingBuilt(sim->unitDefinitions.at(targetUnitRef->get().unitType)))
        {
            changeState(*unitInfo.state, UnitBehaviorStateIdle());
            return true;
        }
        auto& targetUnit = targetUnitRef->get();
        const auto& targetUnitDefinition = sim->unitDefinitions.at(targetUnit.unitType);

        return match(
            unitInfo.state->behaviourState,
            [&](UnitBehaviorStateBuilding& buildingState) {
                if (targetUnitId != buildingState.targetUnit)
                {
                    changeState(*unitInfo.state, UnitBehaviorStateIdle());
                    return buildExistingUnit(unitInfo, targetUnitId);
                }

                if (!unitInfo.state->inBuildStance)
                {
                    // We are not in the correct stance to build the unit yet, wait.
                    buildingState.nanoParticleOrigin = std::nullopt;
                    return false;
                }

                auto costs = targetUnit.getBuildCostInfo(targetUnitDefinition, unitInfo.definition->workerTimePerTick);
                auto gotResources = sim->addResourceDelta(
                    unitInfo.id,
                    -Energy(targetUnitDefinition.buildCostEnergy.value * static_cast<float>(unitInfo.definition->workerTimePerTick) / static_cast<float>(targetUnitDefinition.buildTime)),
                    -Metal(targetUnitDefinition.buildCostMetal.value * static_cast<float>(unitInfo.definition->workerTimePerTick) / static_cast<float>(targetUnitDefinition.buildTime)),
                    -costs.energyCost,
                    -costs.metalCost);

                if (!gotResources)
                {
                    // we don't have resources available to build -- wait
                    buildingState.nanoParticleOrigin = std::nullopt;
                    return false;
                }
                buildingState.nanoParticleOrigin = getNanoPoint(unitInfo.id);

                if (targetUnit.addBuildProgress(targetUnitDefinition, unitInfo.definition->workerTimePerTick))
                {
                    sim->events.push_back(UnitCompleteEvent{buildingState.targetUnit});

                    if (targetUnitDefinition.activateWhenBuilt)
                    {
                        sim->activateUnit(buildingState.targetUnit);
                    }

                    changeState(*unitInfo.state, UnitBehaviorStateIdle());
                    return true;
                }
                return false;
            },
            [&](const auto&) {
                auto nanoFromPosition = getNanoPoint(unitInfo.id);
                auto headingAndPitch = computeLineOfSightHeadingAndPitch(unitInfo.state->rotation, nanoFromPosition, targetUnit.position);
                auto heading = headingAndPitch.first;
                auto pitch = headingAndPitch.second;

                changeState(*unitInfo.state, UnitBehaviorStateBuilding{targetUnitId, std::nullopt});
                unitInfo.state->cobEnvironment->createThread("StartBuilding", {toCobAngle(heading).value, toCobAngle(pitch).value});
                return false;
            });
    }

    bool UnitBehaviorService::climbToCruiseAltitude(UnitInfo unitInfo)
    {
        auto targetHeight = getTargetAltitude(*sim, unitInfo.state->position.x, unitInfo.state->position.z, *unitInfo.definition);

        unitInfo.state->position.y = rweMin(unitInfo.state->position.y + 1_ss, targetHeight);

        return unitInfo.state->position.y == targetHeight;
    }

    bool UnitBehaviorService::descendToGroundLevel(UnitInfo unitInfo)
    {
        auto terrainHeight = sim->terrain.getHeightAt(unitInfo.state->position.x, unitInfo.state->position.z);
        auto seaLevel = sim->terrain.getSeaLevel();

        // Don't land below sea level — abort landing if terrain is underwater
        if (terrainHeight < seaLevel)
        {
            return false;
        }

        unitInfo.state->position.y = rweMax(unitInfo.state->position.y - 1_ss, terrainHeight);

        return unitInfo.state->position.y == terrainHeight;
    }

    void UnitBehaviorService::transitionFromGroundToAir(UnitInfo unitInfo)
    {
        unitInfo.state->activate();

        unitInfo.state->physics = UnitPhysicsInfoAir();
        auto footprintRect = sim->computeFootprintRegion(unitInfo.state->position, unitInfo.definition->movementCollisionInfo);
        auto footprintRegion = sim->occupiedGrid.tryToRegion(footprintRect);
        assert(!!footprintRegion);
        sim->occupiedGrid.forEach(*footprintRegion, [](auto& cell) {
            cell.mobileUnitId = std::nullopt;
        });
        sim->flyingUnitsSet.insert(unitInfo.id);
    }

    bool UnitBehaviorService::tryTransitionFromAirToGround(UnitInfo unitInfo)
    {
        auto footprintRect = sim->computeFootprintRegion(unitInfo.state->position, unitInfo.definition->movementCollisionInfo);
        auto footprintRegion = sim->occupiedGrid.tryToRegion(footprintRect);
        assert(!!footprintRegion);

        if (sim->isCollisionAt(*footprintRegion))
        {
            return false;
        }

        sim->occupiedGrid.forEach(*footprintRegion, [&](auto& cell) {
            cell.mobileUnitId = unitInfo.id;
        });
        sim->flyingUnitsSet.erase(unitInfo.id);

        unitInfo.state->physics = UnitPhysicsInfoGround();

        return true;
    }

    bool UnitBehaviorService::flyTowardsGoal(UnitInfo unitInfo, const MovingStateGoal& goal)
    {
        auto destination = match(
            goal,
            [&](const SimVector& pos) {
                return pos;
            },
            [&](const DiscreteRect& rect) {
                return findClosestPointToFootprintXZ(sim->terrain, rect, unitInfo.state->position);
            },
            [&](const UnitId& u) {
                const auto& unit = sim->getUnitState(u);
                return unit.position;
            });

        SimVector xzPosition(unitInfo.state->position.x, 0_ss, unitInfo.state->position.z);
        SimVector xzDestination(destination.x, 0_ss, destination.z);
        auto distanceSquared = xzPosition.distanceSquared(xzDestination);

        if (distanceSquared < (8_ss * 8_ss))
        {
            return true;
        }

        auto airPhysics = std::get_if<UnitPhysicsInfoAir>(&unitInfo.state->physics);
        if (airPhysics == nullptr)
        {
            throw std::logic_error("cannot fly towards goal because unit does not have air physics");
        }

        match(
            airPhysics->movementState,
            [&](AirMovementStateFlying& m) {
                auto targetHeight = getTargetAltitude(*sim, destination.x, destination.z, *unitInfo.definition);
                SimVector destinationAtAltitude(destination.x, targetHeight, destination.z);

                m.targetPosition = destinationAtAltitude;
            },
            [&](const AirMovementStateTakingOff&) {
                // do nothing
            },
            [&](AirMovementStateLanding& m) {
                m.shouldAbort = true;
            });

        return false;
    }
}
