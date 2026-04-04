# Annihilation Engine — Code Quality Audit & Improvement Plan

## Context

This codebase was forked from Robot War Engine (RWE). A thorough audit revealed several categories of issues: a **critical macOS platform bug**, deprecated C++ patterns, DRY violations, missing tests for core systems, weak observability, and a monolithic build structure. This plan prioritizes fixes by impact and risk.

---

## Phase 1: Critical — macOS Platform Bug (BLOCKING) — COMPLETED

**Problem:** `CMakeLists.txt` set `RWE_PLATFORM_LINUX` on macOS, causing wrong data path (`~/.rwe` instead of `~/Library/Application Support/RWE`).

**Changes made:**
- `CMakeLists.txt` — Added `elseif(APPLE)` with `RWE_PLATFORM_APPLE` definition
- `src/rwe/util.cpp` — Added `#ifdef RWE_PLATFORM_APPLE` block returning `~/Library/Application Support/RWE`

---

## Phase 2: Build System Modernization — COMPLETED

**Changes made:**
- Migrated all `add_definitions()` to `target_compile_definitions()` for proper scoping
- Set `CMAKE_CXX_STANDARD 20` globally with `CMAKE_CXX_STANDARD_REQUIRED ON`, removed MSVC `/std:c++20` workaround
- Moved PNG linking into `librwe` (eliminated 6 duplicate `target_link_libraries` calls across test/tool targets)
- Migrated `add_compile_definitions(GL_SILENCE_DEPRECATION)` for Apple
- `ASIO_STANDALONE` now uses `target_compile_definitions`

---

## Phase 3: Deprecated C++ Patterns — COMPLETED

**Changes made:**
- `src/rwe/util/SharedHandle.h` — Replaced manual `new/delete` ref-counting with `std::shared_ptr`
- `src/rwe/LoadingScene.h` — Removed 3 visitor boilerplate classes (`IsHumanVisitor`, `IsComputerVisitor`, `GetNetworkAddressVisitor`)
- `src/rwe/LoadingScene.cpp` — Replaced visitor usage with `std::holds_alternative` and `match()` (utility already existed in `util/match.h`)
- Error handling convention documented in `CONTRIBUTING.md`: exceptions at init/loading, `LOG_ERROR` + graceful degradation at runtime

---

## Phase 4: Performance Fixes — COMPLETED

**Changes made:**
- `src/rwe/sim/GameSimulation.cpp` — Cached `toUpper()` result in `tryCreateWeapon()` (was allocating twice per call)
- `src/rwe/sim/GameSimulation.cpp` — Hoisted `UnitBehaviorService` construction out of per-unit loop (created once per tick instead of once per unit per tick)

---

## Phase 4b: Graceful Error Handling — COMPLETED (critical points)

**Changes made:**
- Created `src/rwe/util/safe_lookup.h` — Utility for safe map lookups with logging
- `src/rwe/game/GameScene.cpp` — Desync now logs error instead of crashing; GUI info returns default instead of throwing
- `src/rwe/game/GameScene_util.cpp` — Flamethrower render gracefully handles missing sprites; missing piece definitions log + return identity matrix; bounds-checked sprite frame index
- `src/rwe/sim/GameSimulation.cpp` — Feature spawning handles unknown types gracefully; weapon `randomDecay` handles missing optional
- `src/rwe/sim/Projectile.cpp` — Missing damage entry returns 0 instead of crashing
- `src/rwe/util/SimpleLogger.h` — Logger fallback: `getLogger()` creates a fallback logger instead of asserting when none is initialized (fixes crash in test context)

**Remaining `.at()` conversion — COMPLETED:**
Converted 99 of 101 dangerous `.at()` calls to graceful error handling across GameSimulation.cpp (27), UnitBehaviorService.cpp (14), GameScene.cpp (35), GameScene_util.cpp (6), and CobExecutionContext.cpp (4). Added safe accessor methods to `GameSimulation`: `tryGetUnitDefinition()`, `tryGetUnitModelDefinition()`, `tryGetWeaponDefinition()`, `tryGetUnitScript()`. The 2 remaining `.at()` calls are safe `players.at()` vector accesses with validated indices.

---

## Phase 5: DRY Improvements — COMPLETED (already addressed)

**Findings:** The `overloaded` utility and `match()` helper already existed in `util/match.h`. The TDF `extract`/`expect` pattern is a template in one place (TdfBlock.h), not actually duplicated. No changes needed.

---

## Phase 6: CI/CD Improvements — COMPLETED

**Changes made to `.github/workflows/build.yml`:**
- Added `ccache` for macOS builds (was missing; Linux/Windows already had it)
- Added macOS launcher tests (Node.js setup, `npm ci`, `npm run tsc`, `npm test`, `npm run lint`)
- Added ccache for macOS cmake invocation (`-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache`)
- Replaced hardcoded `-j 4` / `-j 3` with `$(nproc)` (Linux/MinGW) and `$(sysctl -n hw.ncpu)` (macOS)

**Also completed:** Removed redundant AppVeyor CI (`appveyor.yml`, `appveyor.bat`, `appveyor.bash`) — GitHub Actions already provides full coverage.

---

## Phase 7: Observability — COMPLETED

**Changes made:**
- `src/rwe/sim/GameSimulation.cpp` — Added tick duration measurement with `LOG_WARN` when >50ms threshold exceeded
- `src/rwe/game/GameNetworkService.h` — Added `packetsSent` and `packetsReceived` counters per endpoint
- `src/rwe/game/GameNetworkService.cpp` — Increments counters on send/receive; logs periodic network health summary (sent/recv/avgRTT) every 300 packets

---

## Phase 8: Testing Gaps — PARTIALLY COMPLETED

**Completed:**
- Added `src/rwe/sim/GameSimulation.test.cpp` with 6 test cases (19 assertions):
  - GameSimulation construction
  - `addPlayer` with sequential IDs
  - `tryGetFeatureDefinitionId` (unknown + known features)
  - `trySpawnFeature` (unknown type graceful handling + successful spawn)
  - `tick` (time advancement + 100-tick stability on empty simulation)
  - `Projectile::getDamage` (specific damage, DEFAULT fallback, missing entry graceful return)

**Remaining:**
- `src/rwe/sim/UnitBehaviorService.cpp` — Unit AI tests (requires full unit definitions + COB script setup)
- `src/rwe/game/GameNetworkService.cpp` — Network protocol tests (requires async networking setup)

---

## Phase 9: Documentation — COMPLETED

**Changes made:**
- Created `CONTRIBUTING.md` with:
  - Build prerequisites and instructions for all platforms
  - Architecture overview table (sim, game, render, cob, io, vfs, pathfinding, ui, util)
  - Code conventions (style, strong typing, variant state machines)
  - Error handling policy (exceptions at init, LOG_ERROR at runtime)
  - Logging usage guide
  - Cross-platform development guidelines
  - CI overview

---

## Phase 10: Library Modularization — COMPLETED

**Changes made:**
Split the monolithic 393-file `SOURCE_FILES` list into 5 categorized groups in `CMakeLists.txt`:
- `UTIL_FILES` — Shared utilities, math, geometry, grid, collections (~80 files)
- `IO_FILES` — File format parsers (HPI, GAF, TDF, 3DO, etc.) + VFS (~55 files)
- `SIM_FILES` — Deterministic simulation, COB VM, pathfinding (~95 files)
- `RENDER_FILES` — OpenGL rendering, shaders, sprites (~25 files)
- `GAME_FILES` — Integration layer (scenes, services, UI, SDL, network) (~100 files)

All groups compose into the unified `librwe` static library via `set(SOURCE_FILES ${UTIL_FILES} ${IO_FILES} ${SIM_FILES} ${RENDER_FILES} ${GAME_FILES})`.

**Dependency cleanup:**
- Removed dead cross-module includes (`MapTerrain`→`TextureRegion`, `GraphicsContext`→`MapFeature`/`MapTerrain`)
- Added missing direct includes exposed by removing transitive deps (`Rectangle2x.h`, `rwe_math.h`, `Grid.h`)
- Moved `PlayerColorIndex` from `game/` to `sim/` (eliminated sim→game dependency)

**Remaining work:**
- UnitBehaviorService + GameNetworkService tests (Phase 8 continuation)

---

## Verification

All changes verified:
1. `cmake -B build -DCMAKE_BUILD_TYPE=Release` — Configures cleanly
2. `cmake --build build` — Builds with no errors (only pre-existing warnings)
3. `./build/rwe_test` — **1179 assertions in 94 test cases, all passing**
4. `./build/AnnihilationEngine --data-path <path>` — Game launches, loads, and runs correctly
