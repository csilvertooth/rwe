# Annihilation Engine — Multi-Threading & GPU Optimization Plan

## Context

The engine is almost entirely single-threaded (only networking has worker threads). The GPU is underutilized at ~30-50% — OpenGL 3.2 with no instancing, per-piece draw calls (200-500/frame), per-frame VBO recreation, and all transforms computed on CPU. 

**Strategy:** Two-stage GPU modernization:
1. **Now:** Upgrade to OpenGL 4.1 with a clean render abstraction boundary
2. **Later:** Migrate to SDL3 GPU API (already vendored) which compiles to Vulkan/Metal/D3D12

This approach gets us immediate performance gains while building the architecture for a future backend swap.

---

## Stage 1: OpenGL 4.1 + Multi-Threading (This Plan)

### Phase 1: GLSL 410 Shader Upgrade (Foundation, Low Risk)

Upgrade all 16 shaders from `#version 150` to `#version 410 core`. Request OpenGL 4.1 context with 3.2 fallback.

**Files:**
- `src/main.cpp` — Change `OpenGlVersionInfo(3, 2, ...)` to `OpenGlVersionInfo(4, 1, ...)`
- All 16 files in `shaders/` — Update version, add `layout(location = N)` qualifiers
- `src/rwe/render/GraphicsContext.cpp` — Add GL version capability logging

---

### Phase 2: Render Abstraction Boundary (Architecture, Medium Risk)

Introduce a `RenderBackend` interface that isolates all raw GL calls behind an abstraction. This is the key enabler for future SDL3 GPU API migration — the rest of the engine talks to `RenderBackend`, not to OpenGL directly.

**Design:**
```cpp
class RenderBackend {
    virtual MeshHandle createMesh(span<const Vertex> vertices, BufferUsage usage) = 0;
    virtual void drawMesh(const MeshHandle& mesh) = 0;
    virtual void drawMeshInstanced(const MeshHandle& mesh, int count, const UniformBuffer& instanceData) = 0;
    virtual TextureHandle createTexture(int w, int h, const void* data) = 0;
    virtual TextureArrayHandle createTextureArray(...) = 0;
    virtual ShaderHandle loadShader(const std::string& vertPath, const std::string& fragPath) = 0;
    virtual UniformBufferHandle createUniformBuffer(size_t size) = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    // ... etc
};

class OpenGl41Backend : public RenderBackend { ... };
// Future: class Sdl3GpuBackend : public RenderBackend { ... };
```

**Files:**
- New: `src/rwe/render/RenderBackend.h` — Abstract interface
- New: `src/rwe/render/OpenGl41Backend.h/.cpp` — OpenGL 4.1 implementation (wraps existing `GraphicsContext` logic)
- `src/rwe/render/GraphicsContext.h/.cpp` — Refactor to implement `RenderBackend` or delegate to it
- `src/rwe/RenderService.h/.cpp` — Change to use `RenderBackend*` instead of `GraphicsContext*`

---

### Phase 3: Frustum Culling for Units (CPU, Low Risk)

Skip units outside camera viewport. Currently only terrain tiles are culled.

**Files:**
- New: `src/rwe/render/FrustumCuller.h` — 2D AABB test in XZ plane (orthographic camera)
- `src/rwe/game/GameScene.cpp` — Add frustum check before `drawUnit()`/`drawUnitShadow()` loops

---

### Phase 4: Persistent Mapped Buffers / Ring Buffers (GPU Memory, Medium Risk)

Replace per-frame `glBufferData(GL_STREAM_DRAW)` with ring buffers behind the `RenderBackend` interface.

**Files:**
- New: `src/rwe/render/StreamingBuffer.h/.cpp` — Ring buffer with fence sync
  - GL 4.4+: `glBufferStorage` + `GL_MAP_PERSISTENT_BIT`
  - GL 4.1 (macOS): Triple-buffered `glBufferSubData` + `glFenceSync`
- `src/rwe/render/OpenGl41Backend.cpp` — Integrate streaming buffer into mesh creation
- `src/rwe/RenderService.cpp` — Use streaming allocation for dynamic geometry

---

### Phase 5: Instanced Rendering (GPU Draw Calls, High Impact)

Batch unit pieces sharing the same mesh into instanced draw calls. Reduces 200-500 → 20-50 draw calls.

**Files:**
- `shaders/unitTexture.vert`, `unitBuild.vert`, `unitShadow.vert` — UBO with `mat4 modelMatrices[]`, index via `gl_InstanceID`
- `src/rwe/RenderService.h/.cpp` — Sort batch by `{mesh, texture}`, upload instance data, call instanced draw
- `src/rwe/render/OpenGl41Backend.cpp` — Add UBO create/bind, `drawMeshInstanced()`
  - macOS (UBO): ~256 instances/draw (64KB limit)
  - Linux/Windows (SSBO via GL 4.3): effectively unlimited

---

### Phase 6: Async Pathfinding (Threading, Medium Risk)

Move pathfinding to a dedicated worker thread. Results arrive one tick later (deterministic at 30 ticks/sec).

**Files:**
- New: `src/rwe/pathfinding/AsyncPathFindingService.h/.cpp` — Worker thread, double-buffered request/result queue
- `src/rwe/sim/GameSimulation.cpp` — `tick()`: apply previous results → behavior → submit new requests
- `src/rwe/sim/GameSimulation.h` — Replace `PathFindingService` with `AsyncPathFindingService`

---

### Phase 7: Parallel Simulation Subsystems (Threading, Medium Risk)

Parallelize independent parts of `GameSimulation::tick()`:
- `updateFogOfWar()` / `updateRadarMap()` — Per-player grids → `std::for_each(par)`
- `piece.update()` — Per-piece animation → parallel per-unit
- **NOT** `unitBehaviorService.update()` — Too many cross-unit side effects

**Files:**
- `src/rwe/sim/GameSimulation.cpp` — Parallel execution policies
- `CMakeLists.txt` — Link TBB or pthread for parallel algorithms

---

### Phase 8: Render Thread Separation (Threading, High Risk — Do Last)

Separate simulation and rendering with double-buffered render state snapshots.

**Files:**
- New: `src/rwe/render/RenderSnapshot.h` — All data `renderWorld()` needs
- New: `src/rwe/render/RenderThread.h/.cpp` — Owns GL context, receives snapshots
- `src/rwe/scene/SceneManager.cpp` — Main loop: update → snapshot → submit to render thread
- `src/rwe/game/GameScene.cpp` — Split into `buildRenderSnapshot()` + `renderFromSnapshot()`

---

## Stage 2: SDL3 GPU API Migration (Future Plan)

Once Stage 1 is stable, migrate from `OpenGl41Backend` to a new `Sdl3GpuBackend` implementing the same `RenderBackend` interface. SDL3's GPU API compiles to:
- **Vulkan** (Linux, Windows)
- **Metal** (macOS, iOS)
- **Direct3D 12** (Windows)

This unlocks:
- Compute shaders (particles, FOW, GPU-driven culling)
- Multi-threaded command recording
- Bindless resources
- Full GPU utilization on all platforms

The `RenderBackend` abstraction from Phase 2 makes this a backend swap, not a rewrite.

---

## Implementation Order

| Order | Phase | Risk | Impact | Notes |
|-------|-------|------|--------|-------|
| 1st | Phase 1: GLSL 410 | Low | Foundation | Prerequisite for all GPU work |
| 2nd | Phase 2: Render Abstraction | Medium | Architecture | Enables future SDL3 GPU migration |
| 3rd | Phase 3: Frustum Culling | Low | Medium | Quick win, no API changes |
| 4th | Phase 4: Streaming Buffers | Medium | High | Eliminates VBO churn |
| 5th | Phase 5: Instanced Rendering | High | Very High | Biggest draw call reduction |
| 6th | Phase 6: Async Pathfinding | Medium | Medium | Independent of GPU work |
| 7th | Phase 7: Parallel Sim | Medium | Medium | Independent of GPU work |
| 8th | Phase 8: Render Thread | High | High | Must be last (restructures pipeline) |

Phases 1-3 and 6-7 can proceed in parallel.

## Verification

After each phase:
1. `cmake --build build` — Builds cleanly on all platforms
2. `./build/rwe_test` — All tests pass
3. `./build/AnnihilationEngine` — Game runs, no visual regressions
4. Frame time comparison (before/after) with 200+ units
5. ThreadSanitizer for threading phases (6-8)
6. Multiplayer hash comparison for determinism (phases 6-7)
