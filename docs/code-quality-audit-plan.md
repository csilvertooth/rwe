# Annihilation Engine — Code Quality Audit & Improvement Plan

## Context

This codebase was forked from Robot War Engine (RWE). A thorough audit reveals several categories of issues: a **critical macOS platform bug**, deprecated C++ patterns, DRY violations, missing tests for core systems, weak observability, and a monolithic build structure. This plan prioritizes fixes by impact and risk.

---

## Phase 1: Critical — macOS Platform Bug (BLOCKING)

**Problem:** `CMakeLists.txt:567-571` sets `RWE_PLATFORM_LINUX` on macOS. This means `src/rwe/util.cpp` uses the Linux data path (`~/.rwe`) instead of the correct macOS path (`~/Library/Application Support/RWE`).

**Files to modify:**
- [CMakeLists.txt](CMakeLists.txt) — Add `APPLE` check before the `else()` fallback
- [src/rwe/util.cpp](src/rwe/util.cpp) — Add `#ifdef RWE_PLATFORM_APPLE` block returning `~/Library/Application Support/RWE`
- [src/rwe/util.h](src/rwe/util.h) — No change needed (function signature unchanged)

**Changes:**
```cmake
if(WIN32)
    add_definitions(-DRWE_PLATFORM_WINDOWS)
elseif(APPLE)
    add_definitions(-DRWE_PLATFORM_APPLE)
else()
    add_definitions(-DRWE_PLATFORM_LINUX)
endif()
```

---

## Phase 2: Build System Modernization

### 2a. Migrate `add_definitions()` → `target_compile_definitions()`
- [CMakeLists.txt](CMakeLists.txt) lines 45, 567-571, 585 — Replace all `add_definitions()` with `target_compile_definitions(librwe PRIVATE/PUBLIC ...)` for proper scoping

### 2b. Fix C++20 standard specification
- [CMakeLists.txt](CMakeLists.txt) lines 101-103 — Set `CMAKE_CXX_STANDARD 20` globally (remove the `if(NOT MSVC)` guard), remove the MSVC `/std:c++20` compile option

### 2c. Fix PNG transitive linking
- [CMakeLists.txt](CMakeLists.txt) lines 684-731 — Link PNG to `librwe` once with `PUBLIC` visibility so downstream targets inherit it, removing repeated `target_link_libraries` for PNG on each executable

---

## Phase 3: Deprecated C++ Patterns

### 3a. Replace custom ref-counting in SharedHandle
- [src/rwe/util/SharedHandle.h](src/rwe/util/SharedHandle.h) — Replace `new unsigned int(1)` / manual `delete` with `std::shared_ptr<unsigned int>` or refactor to use `std::shared_ptr` with custom deleter

### 3b. Standardize error handling
- The codebase has `Result<T, E>` ([src/rwe/util/Result.h](src/rwe/util/Result.h)) but many places throw exceptions instead (GraphicsContext.cpp, TdfBlock.h, gui.cpp)
- **Decision needed:** Exceptions are acceptable at initialization/loading boundaries. Document this convention. Audit `.value()` calls in hot paths ([src/rwe/game/GameScene_util.cpp](src/rwe/game/GameScene_util.cpp)) to ensure they can't hit empty optionals

### 3c. Replace visitor boilerplate with C++20 patterns
- [src/rwe/LoadingScene.h](src/rwe/LoadingScene.h) lines 43-65 — Replace visitor classes with `std::visit` + overloaded lambdas pattern. Create a utility:
  ```cpp
  // src/rwe/util/overloaded.h
  template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
  ```

---

## Phase 4: Performance Fixes

### 4a. Cache uppercase keys (HIGH impact)
- [src/rwe/sim/GameSimulation.cpp](src/rwe/sim/GameSimulation.cpp) lines 182, 188, 231, 2137 — `toUpper()` allocates a new string on every lookup. Pre-compute uppercase keys at load time in the definition maps, or use a case-insensitive comparator

### 4b. Avoid per-tick object construction
- [src/rwe/sim/GameSimulation.cpp](src/rwe/sim/GameSimulation.cpp) line 1846 — `UnitBehaviorService(this).update(unitId)` creates a temporary every tick for every unit. Make `UnitBehaviorService` a member or create once per tick

---

## Phase 5: DRY Improvements

### 5a. Create `overloaded` utility (see 3c above)
- Eliminates visitor boilerplate across LoadingScene.h, TdfBlock.cpp, and others

### 5b. Extract TDF extract/expect pattern
- [src/rwe/io/tdf/TdfBlock.h](src/rwe/io/tdf/TdfBlock.h) lines 105-139 — The `extract<T>()`/`expect<T>()` pattern is duplicated across multiple database classes. Consider a shared base or free function template

### 5c. Consolidate tryGet() patterns
- `std::optional<std::reference_wrapper<T>> tryGet()` is duplicated in VectorMap.h, SimpleVectorMap.h, GameSimulation.h — ensure one canonical implementation in the collection types and reuse

---

## Phase 6: CI/CD Improvements

**File:** [.github/workflows/build.yml](.github/workflows/build.yml)

- Add ccache for macOS builds (Linux/Windows already have it)
- Add macOS launcher tests (currently skipped — no Node.js setup on macOS job)
- Upload Linux/macOS release artifacts (currently only Windows)
- Replace hardcoded `-j 4` with dynamic core count detection

---

## Phase 7: Observability

### 7a. Structured logging enhancement
- [src/rwe/util/SimpleLogger.h](src/rwe/util/SimpleLogger.h) — Good foundation already exists. Add optional structured key-value pairs for network events and simulation ticks
- Add frame time / tick duration logging at DEBUG level in GameSimulation

### 7b. Network metrics
- [src/rwe/game/GameNetworkService.cpp](src/rwe/game/GameNetworkService.cpp) — RTT tracking already exists. Add packet loss tracking and log periodic network health summaries

---

## Phase 8: Testing Gaps

**Critical untested areas:**
- [src/rwe/sim/GameSimulation.cpp](src/rwe/sim/GameSimulation.cpp) — **DONE**: Added GameSimulation.test.cpp with tests for construction, addPlayer, tryGetFeatureDefinitionId, trySpawnFeature, tick, and Projectile::getDamage
- [src/rwe/sim/UnitBehaviorService.cpp](src/rwe/sim/UnitBehaviorService.cpp) — Unit AI, needs tests (requires full unit/script definitions)
- [src/rwe/game/GameNetworkService.cpp](src/rwe/game/GameNetworkService.cpp) — Network protocol, needs tests (requires async networking setup)

**Approach:** Start with deterministic simulation tests (no rendering dependency). The sim layer is already separated from rendering, making it testable.

---

## Phase 9: Documentation

- Add `CONTRIBUTING.md` with build setup, architecture overview, and coding conventions
- Document the error handling convention (Result vs exceptions)
- Add subsystem READMEs for `sim/`, `io/`, `cob/` explaining the architecture
- Convert inline TODOs/FIXMEs to GitHub issues for tracking

---

## Phase 10: Library Modularization (Future)

The current monolithic `librwe` (393-line file list) could be split into:
- `librwe_sim` — Deterministic simulation (no rendering deps)
- `librwe_io` — File format parsers
- `librwe_render` — OpenGL rendering
- `librwe_net` — Network services

**Note:** This is a large refactor. Recommend deferring until after Phases 1-5 are complete, as it requires careful dependency analysis.

---

## Verification

After each phase:
1. `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(sysctl -n hw.ncpu)` — Builds cleanly
2. `./build/rwe_test` — All tests pass
3. `./build/AnnihilationEngine --data-path <path>` — Game launches and runs
4. CI passes on all platforms (push to branch, check GitHub Actions)
