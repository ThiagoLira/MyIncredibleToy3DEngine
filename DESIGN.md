# DESIGN.md — architecture and decision log

Every decision has: **Options**, **Choice**, **Why**, **Cost** (what we give up), **Status**.
Status is `decided`, `pending` (needs Thiago's call), or `revisit` (decided, but we expect
to reopen it once we know more).

---

## 0. What "portable" means for this engine

Two different things get called "portability" and we must pick which we mean:

- **(A) Portable across operating systems, using Vulkan as the only graphics API.**
  Vulkan itself runs on Linux, Windows, Android, and (via MoltenVK, a translation layer to
  Metal) macOS/iOS. The only OS-specific parts are: creating a window, and creating a
  Vulkan *surface* for that window (the WSI, Window System Integration, extensions such as
  `VK_KHR_wayland_surface` / `VK_KHR_win32_surface`).
- **(B) Portable across graphics APIs**: Vulkan, Direct3D 12, Metal, WebGPU behind one
  abstraction (an RHI, "Render Hardware Interface"). This is what Unreal, Godot, bgfx, wgpu do.
  It multiplies the work by roughly the number of backends.

**Choice (confirmed by Thiago, 2026-09-17):** (A) now, with the code *shaped* for (B). One backend (Vulkan), but the rest of
the engine never calls Vulkan directly; it talks to a small RHI layer that we own. See §2.

**Why:** we want to learn Vulkan deeply, which a multi-backend abstraction would dilute.
But an engine whose renderer, scene, and asset code are full of `vk*` calls can never be
ported, so the boundary must exist from day one.

**Cost:** an abstraction with a single implementation tends to leak the shape of that
implementation. Mitigation: model the RHI on WebGPU's concepts (which were designed as the
intersection of Vulkan/D3D12/Metal), so we know the shape is portable even though we only
implement it once.

**Status:** decided.

---

## 1. How Vulkan gets to the GPU (the runtime stack)

This is not a decision, it's the mechanism, recorded because the design depends on it.

```
 our engine
    │  calls vkCreateInstance, vkCmdDraw, ...
    ▼
 Vulkan loader        libvulkan.so.1 (Linux) / vulkan-1.dll (Windows) / MoltenVK's loader
    │  looks at JSON manifests, picks ICDs and layers
    ├──► layers        e.g. VK_LAYER_KHRONOS_validation (checks our API usage; dev only)
    ▼
 ICD (driver)         nvidia_icd.json → libGLX_nvidia / radeon_icd.json → libvulkan_radeon.so
    ▼
 GPU
```

- We never link against a driver. We link against (or `dlopen`) the **loader**, and the
  loader finds drivers through manifest files. That's why one binary runs on any GPU.
- Each `VkPhysicalDevice` is one ICD's view of one GPU. This machine exposes two:
  the RTX 5090 (NVIDIA ICD) and the Ryzen iGPU (Mesa RADV ICD). Choosing between them is
  our job (§4 lesson: device selection).
- **Layers** sit between us and the driver and can intercept every call. Validation layers
  are how we find bugs; they are off in release builds.

---

## 2. Layering of the engine

```
 app / game code
 ─────────────────────────────────────────
 engine: scene, camera, materials, assets
 ─────────────────────────────────────────
 renderer: frame graph, passes (uses RHI only)
 ─────────────────────────────────────────
 RHI (our API): Device, Buffer, Texture, Pipeline, CommandList, Swapchain
 ─────────────────────────────────────────
 rhi_vulkan: the only implementation for now
 ─────────────────────────────────────────
 platform: window + input + surface creation (SDL3 or GLFW)   ← the OS-specific bits
```

Rule: `vulkan.h` is included only inside `rhi_vulkan/`. Anything above it that needs a
Vulkan type is a design bug.

**Status:** decided.

---

## 3. Minimum Vulkan version and feature set

**Options:** Vulkan 1.0 (max hardware reach, maximum boilerplate: render passes,
framebuffers, old sync API) vs Vulkan 1.3 core (dynamic rendering, synchronization2,
timeline semaphores, buffer device address; far less code, but requires ~2019+ drivers).

**Choice:** Vulkan 1.3 core, no optional extensions required for the base engine.

**Why:** 1.3 removes the two most confusing legacy objects (`VkRenderPass`,
`VkFramebuffer`) and the ambiguous 1.0 barrier API. Every lesson gets shorter and closer
to the real concept. Both GPUs here are 1.4. MoltenVK (macOS) supports 1.3 as of 2024.

**Cost:** old Android phones and pre-2020 drivers are out. Acceptable for a learning engine.

**Status:** decided (revisit only if a target platform demands it).

---

## 4. Loading Vulkan functions

**Options:**
- Link `libvulkan` at build time (simplest; binary fails to start if the loader is absent).
- Load the loader at runtime with `dlopen`/`LoadLibrary` and fetch every function pointer
  via `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr` (what the `volk` library does).

**Choice:** runtime loading, written by hand (a small header, a few hundred lines), not `volk`.

**Why:** (1) the binary starts even on a machine with no Vulkan, so we can print a real
error or fall back; (2) device-level function pointers skip the loader's dispatch
trampoline, which is measurably faster; (3) writing it ourselves is the lesson on how the
loader/ICD/layer dispatch actually works. `volk` would hide exactly that.

**Cost:** ~1 hour of plumbing, and we must keep the list of loaded functions in sync with
what we use.

**Status:** decided.

---

## 5. Language and build system

**Options:** C (Vulkan's native API, no hidden control flow), C++20 (RAII for Vulkan
handles, namespaces, `std::span`, concepts), Rust (`ash` crate; safety, but fights the
raw-handle nature of Vulkan and adds a language to learn), Zig.

**Choice (Thiago, 2026-09-17):** **C++20 with a C-like style** (structs + free functions,
RAII only for lifetime-owning wrappers, no inheritance-heavy design), built with
**CMake + Ninja**, compiled with both GCC and Clang from the start.

**Why:** Vulkan is a C API; C++ keeps zero friction with it while giving RAII, which
matters because Vulkan has ~40 object types that all need explicit destruction in the
right order. Compiling with two compilers on day one is the cheapest portability test there is.

**Cost:** C++ has many ways to do things; we constrain ourselves with a style rule in CLAUDE.md.

**Status:** decided.

---

## 6. Windowing / platform layer

**Options:** GLFW 3.4 (tiny, C, desktop only, `glfwCreateWindowSurface` does WSI for us),
SDL3 (bigger, C, desktop + Android + iOS + consoles, also input/audio/gamepad; has
`SDL_Vulkan_CreateSurface`), raw Wayland/Win32 (maximum learning about WSI, maximum pain).

**Choice (Thiago, 2026-09-17):** **SDL3**, confined to the `platform/` layer.

**Why:** reaches more platforms than GLFW, and we will want its input/gamepad/audio later
anyway. It only owns the window and surface; everything else is ours.

**Cost:** larger dependency. We do not use `SDL_GPU` (SDL's own RHI) because it would
hide Vulkan, which is the thing we're learning.

**Status:** decided.

---

## 7. Shaders

**Choice:** GLSL compiled offline to SPIR-V with `glslc` (a CMake step). Later: Slang.

**Why:** SPIR-V is the portable shader IR; every Vulkan driver consumes it. If a second
backend ever exists, SPIRV-Cross can transpile SPIR-V to HLSL/MSL, so the shader source
stays single. GLSL is the least-friction way to learn the pipeline; Slang is the
better long-term language (modules, generics, one source for all targets).

**Status:** decided (revisit: Slang once the basics are in).

---

## 8. GPU memory

**Choice:** write our own allocator first (learn heaps, memory types, alignment, suballocation),
then evaluate switching to VMA (Vulkan Memory Allocator) once we have felt the problem.

**Status:** decided (revisit after the memory lessons).

---

## Dependencies (each is a decision)

| dep | role | status |
|---|---|---|
| Vulkan loader + headers | the API | decided |
| SDL3 | window/surface/input | decided (§6) |
| glslc | GLSL → SPIR-V | decided |
| validation layers | debug only | decided |
| math lib (GLM vs our own) | vectors/matrices | pending, discuss at the "first triangle → first cube" step |

---

## 9. GPU vendor portability (NVIDIA, AMD, Intel, Apple, Qualcomm)

**Requirement (Thiago, 2026-09-17):** the engine must run on AMD GPUs, not just NVIDIA.

**How it works:** Vulkan is vendor-neutral by construction. We talk to the loader; the
loader dispatches to whichever ICD (driver) owns the device. Nothing in our code says
"NVIDIA". So AMD support is not a feature we add; it is a property we must *not break*.
Ways to break it, which become explicit rules:

1. **Never assume a queue family layout.** Measured on this machine (2026-09-17):
   NVIDIA exposes a 16-queue GRAPHICS|COMPUTE|TRANSFER family, an 8-queue COMPUTE family
   and several 1–2 queue TRANSFER families; RADV exposes a *single* graphics queue, a
   4-queue COMPUTE family, and two families with no graphics/compute/transfer bits at all
   (video decode/encode). Family indices, counts and capabilities all differ, so we
   always *search* for families by capability flags and never hardcode an index.
2. **Never assume memory heaps.** Heap count, sizes, and which memory types are
   `DEVICE_LOCAL | HOST_VISIBLE` (the "BAR"/ReBAR heap) differ per vendor and per system.
   We always query `VkPhysicalDeviceMemoryProperties` and pick by flags.
3. **Never assume a format is supported.** Query `vkGetPhysicalDeviceFormatProperties`
   (e.g. depth formats, BC vs ASTC texture compression).
4. **Never hardcode subgroup size.** NVIDIA warps are 32 lanes; AMD waves are 32 or 64
   (RDNA can be either). Query `subgroupSize`; shaders must not depend on it.
5. **Only require features we check for.** Every optional feature/extension is queried,
   and either required with a clear error or made optional with a fallback.
6. **Test on both.** This machine has an RTX 5090 (NVIDIA proprietary ICD) *and* a Ryzen
   9950X iGPU (Mesa RADV ICD). The engine gets a `--gpu <index|name>` switch and every
   phase is run on both devices before it is called done. Vendor-specific bugs
   (usually a missing barrier or a memory-type assumption) show up on one and not the other.

**Cost:** slightly more query code and a rule of running everything twice.

**Status:** decided.
