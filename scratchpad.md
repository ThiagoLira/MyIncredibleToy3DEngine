# scratchpad.md — Claude's teaching plan (Thiago may read this)

## Learner profile (from memory + this session)
- Comfortable in C (N64 decomp, raylib/WebGL path tracer), Python, JS. Has done a WebGL2
  path tracer, so knows: shaders, framebuffers, uniforms, triangles, projection matrices.
- Has NOT done (assume until shown): explicit GPU APIs (Vulkan/D3D12/Metal), GPU memory
  management, synchronization (fences/semaphores/barriers), descriptor sets, command buffers.
- Learns by reading every line; wants design decisions justified.

## Curriculum (phases) — each phase ends with something on screen or a measurable result

0. **Setup & the runtime stack** — loader/ICD/layers; install headers + validation; build
   system; compile with GCC and Clang. Exercise: none yet, reading + one question.
1. **Instance & device** — hand-written function loading (§4), VkInstance, layers,
   debug messenger, enumerate physical devices (we have two!), pick one, queue families,
   VkDevice. Exercise: write the device-scoring function; write the queue-family search.
   Show queue families of BOTH GPUs side by side (NVIDIA one-family vs AMD split families).
2. **Window & swapchain** — platform layer, VkSurfaceKHR, surface formats/present modes,
   swapchain creation, recreation on resize. Exercise: choose format/present mode; handle
   VK_ERROR_OUT_OF_DATE_KHR.
3. **Command buffers & the frame loop** — command pools, recording, submission,
   fences vs semaphores, frames-in-flight. Clear the screen to a color. Exercise: the
   acquire→record→submit→present loop with correct sync (this is THE Vulkan lesson).
4. **Pipeline & first triangle** — SPIR-V, shader modules, dynamic rendering, pipeline
   state object, vertex input (start with vertices baked in the shader).
5. **Buffers & memory** — heaps/types, our own allocator, staging uploads, vertex/index
   buffers, transfer queue. Exercise: the allocator's `find_memory_type` and suballocation.
6. **Descriptors & uniforms** — descriptor set layouts, pools, UBOs, push constants,
   a spinning cube, depth buffer. Math lib decision.
7. **Textures** — images, layouts, barriers (synchronization2), samplers, mipmaps.
8. **RHI boundary review** — now that Vulkan is understood, refactor into `rhi/` +
   `rhi_vulkan/` (§2) with WebGPU-shaped concepts. (Deliberately AFTER learning Vulkan so
   the abstraction is informed, not guessed.)
9. **Beyond**: bindless, frame graph, glTF loading, PBR, shadows, compute, Slang...

## Phase 1 status
- [x] Headers 357 installed, validation layer present, both ICD manifests seen by Thiago.
- [x] Plumbing built: CMake (gcc+clang, -Werror), X-macro loader, instance + debug
      messenger, device enumeration/printing, create_device with 1.3 features.
- [~] Exercise 1 (score): done enough to pick the 5090. Open: version check is
      `MAJOR<1 && MINOR<3` (wrong), no graphics-family requirement, memory TODO, leftover
      printfs. Thiago said he'll finish later.
- [~] Exercise 2 (families): Thiago wrote the loop; Claude added the fallbacks on request.
      Bug to fix by Thiago: `~(f & BIT)` used as "not has bit" (should be `!(f & BIT)`).
      Works by luck on both GPUs (last match wins).
- [x] Runs on both GPUs, validation silent. First commit 2026-09-21.
- Thiago said "I get the idea of the lesson" → treat queue families as understood.
- Observation to teach: RADV families 2/3 have no flags → video queues; NVIDIA has
  compute-only + transfer-only families too. Both need the fallback logic.
- Loader/ICD question: never answered explicitly; Thiago moved on. Don't nag; revisit
  naturally when we hit layers (phase 3) or MoltenVK.

## Progress log
- 2026-09-21: phase 1 plumbing + exercises, Emacs build workflow (SPC m, .dir-locals.el),
  repo was inside an accidental ~/repos/.git → git init'd its own repo. First commit.
- 2026-09-17: session 0. Thiago requires AMD support → §9, run every phase on both GPUs. Explained portability (A vs B), loader/ICD/layers, wrote docs.
  Decided: C++20 C-style (§5), SDL3 (§6). Asked the understanding-check question (loader/ICD); waiting for the answer. Packages to install: vulkan-headers,
  vulkan-validation-layers.
