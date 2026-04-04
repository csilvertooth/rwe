#include <catch2/catch_test_macros.hpp>
#include <rwe/sim/GameSimulation.h>
#include <rwe/sim/Projectile.h>
#include <rwe/util/rwe_string.h>

namespace rwe
{
    namespace
    {
        MapTerrain createTestTerrain(int width = 33, int height = 33)
        {
            Grid<unsigned char> heights(width, height, static_cast<unsigned char>(128));
            return MapTerrain(std::move(heights), 50_ss);
        }

        GameSimulation createTestSim(int terrainWidth = 33, int terrainHeight = 33)
        {
            return GameSimulation(createTestTerrain(terrainWidth, terrainHeight), 0, 0, 10);
        }

        FeatureDefinition createTestFeatureDef(const std::string& name)
        {
            FeatureDefinition def{};
            def.name = name;
            def.footprintX = 1;
            def.footprintZ = 1;
            def.height = 10_ss;
            def.reclaimable = false;
            def.autoreclaimable = false;
            def.metal = 0;
            def.energy = 0;
            def.flamable = false;
            def.burnMin = 0;
            def.burnMax = 0;
            def.sparkTime = 0;
            def.spreadChance = 0;
            def.geothermal = false;
            def.hitDensity = 0;
            def.reproduce = false;
            def.reproduceArea = 0;
            def.noDisplayInfo = false;
            def.permanent = false;
            def.blocking = true;
            def.indestructible = false;
            def.damage = 100;
            return def;
        }

        bool vectorMapIsEmpty(const auto& map)
        {
            return map.begin() == map.end();
        }

        int vectorMapCount(const auto& map)
        {
            int count = 0;
            for ([[maybe_unused]] const auto& entry : map)
            {
                count++;
            }
            return count;
        }
    }

    TEST_CASE("GameSimulation construction")
    {
        SECTION("creates with valid terrain dimensions")
        {
            auto sim = createTestSim();
            REQUIRE(sim.gameTime.value == 0);
            REQUIRE(sim.players.empty());
            REQUIRE(vectorMapIsEmpty(sim.units));
            REQUIRE(vectorMapIsEmpty(sim.projectiles));
        }
    }

    TEST_CASE("GameSimulation::addPlayer")
    {
        auto sim = createTestSim();

        SECTION("adds a player and returns sequential IDs")
        {
            GamePlayerInfo info1{"Player1", GamePlayerType::Human, PlayerColorIndex(0), GamePlayerStatus::Alive, "ARM", Metal(1000), Energy(1000), Metal(1000), Energy(1000), Metal(1000), Energy(1000)};
            GamePlayerInfo info2{"Player2", GamePlayerType::Computer, PlayerColorIndex(1), GamePlayerStatus::Alive, "CORE", Metal(1000), Energy(1000), Metal(1000), Energy(1000), Metal(1000), Energy(1000)};

            auto id1 = sim.addPlayer(info1);
            auto id2 = sim.addPlayer(info2);

            REQUIRE(id1.value == 0);
            REQUIRE(id2.value == 1);
            REQUIRE(sim.players.size() == 2);
        }
    }

    TEST_CASE("GameSimulation::tryGetFeatureDefinitionId")
    {
        auto sim = createTestSim();

        SECTION("returns nullopt for unknown feature")
        {
            auto result = sim.tryGetFeatureDefinitionId("NONEXISTENT");
            REQUIRE(!result.has_value());
        }

        SECTION("returns ID for known feature")
        {
            auto def = createTestFeatureDef("TREE1");
            auto id = sim.featureDefinitions.emplace(std::move(def));
            sim.featureNameIndex.insert({"TREE1", id});

            auto result = sim.tryGetFeatureDefinitionId("tree1");
            REQUIRE(result.has_value());
            REQUIRE(result->value == id.value);
        }
    }

    TEST_CASE("GameSimulation::trySpawnFeature")
    {
        auto sim = createTestSim();

        SECTION("does not crash for unknown feature type")
        {
            sim.trySpawnFeature("NONEXISTENT", SimVector(100_ss, 0_ss, 100_ss), SimAngle(0));
            REQUIRE(vectorMapIsEmpty(sim.features));
        }

        SECTION("spawns feature for known type")
        {
            auto def = createTestFeatureDef("ROCK1");
            auto id = sim.featureDefinitions.emplace(std::move(def));
            sim.featureNameIndex.insert({"ROCK1", id});

            sim.trySpawnFeature("rock1", SimVector(100_ss, 0_ss, 100_ss), SimAngle(0));
            REQUIRE(vectorMapCount(sim.features) == 1);
        }
    }

    TEST_CASE("GameSimulation::tick")
    {
        auto sim = createTestSim();
        sim.initFogOfWar();

        SECTION("advances game time")
        {
            REQUIRE(sim.gameTime.value == 0);
            sim.tick();
            REQUIRE(sim.gameTime.value == 1);
            sim.tick();
            REQUIRE(sim.gameTime.value == 2);
        }

        SECTION("runs without units or projectiles")
        {
            for (int i = 0; i < 100; i++)
            {
                sim.tick();
            }
            REQUIRE(sim.gameTime.value == 100);
        }
    }

    TEST_CASE("Projectile::getDamage")
    {
        SECTION("returns specific damage for matching unit type")
        {
            Projectile proj{};
            proj.damage = {{"TANK", 200}, {"DEFAULT", 50}};
            REQUIRE(proj.getDamage("TANK") == 200);
        }

        SECTION("returns DEFAULT damage for unknown unit type")
        {
            Projectile proj{};
            proj.damage = {{"TANK", 200}, {"DEFAULT", 50}};
            REQUIRE(proj.getDamage("AIRCRAFT") == 50);
        }

        SECTION("returns 0 when no matching entry and no DEFAULT")
        {
            Projectile proj{};
            proj.damage = {{"TANK", 100}};
            REQUIRE(proj.getDamage("AIRCRAFT") == 0);
        }
    }
}
