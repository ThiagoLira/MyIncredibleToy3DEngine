# my_toy_3d_engine — working agreement

This is a **learning project**. The goal is not to ship an engine fast; the goal is that
Thiago understands every line and every decision. Speed is secondary to comprehension.

## Directives (do not drift from these)

1. **Every line of code is read by Thiago.** Never dump large files silently. Show the
   important parts in the reply, explain them, then write them to disk. Boilerplate may be
   written directly, but its gist must be explained once.
2. **Every design decision is validated and explained** before it lands. A decision is
   recorded in `DESIGN.md` with: the options considered, the choice, the reason, and what
   it costs us. Nothing is "just how it's done".
3. **Teaching mode** (the `learning` skill): concepts are taught with the mechanism exposed,
   not hidden behind a helper library. Exercises use placeholders for the parts that teach
   something; plumbing is written by Claude. Hints before answers. Run the code, show output.
4. **Keep the docs current.** When a decision changes, update `DESIGN.md` in the same turn.
   When a directive changes, update this file. `scratchpad.md` is Claude's curriculum plan
   and progress log; update it every session.
5. **No abstraction traps.** Do not pull in a library that hides the thing being learned
   (e.g. vk-bootstrap for device setup, VMA before we've written a memory allocator by hand).
   Libraries are allowed for things that are not the lesson (windowing, math, image loading),
   and each one is a recorded decision.
6. **Learning progression is guarded.** If a goal skips a foundation we haven't built, say
   so and propose the order. Switching from "learning project" to "real project" is
   Thiago's explicit call, never assumed.

## Code style (C++20, C-like)

- Structs with public data + free functions. No class hierarchies, no virtual dispatch in
  hot paths. RAII wrappers only where an object owns a Vulkan handle that must be destroyed.
- No exceptions in engine code; functions that can fail return a status/`std::optional`
  and log. Vulkan results are checked with a `VK_CHECK` macro that names the call.
- `vulkan.h` is included only inside the Vulkan backend directory.
- Build with both GCC and Clang, warnings as errors (`-Wall -Wextra -Werror`).

## Files

- `DESIGN.md`   — architecture and the decision log (the "why" of everything).
- `scratchpad.md` — Claude's lesson plan, pre-research notes, progress tracking.
- `CLAUDE.md`   — this file: the rules of engagement.

## Environment (verified 2026-09-17)

- Linux (CachyOS/Arch), Wayland session. GPUs: RTX 5090 (NVIDIA proprietary, Vulkan 1.4)
  and Ryzen 9950X iGPU (RADV, Vulkan 1.4). Two devices → device selection is a real lesson.
- Toolchain: GCC 16, Clang 22, CMake, Ninja, glslc, glslangValidator, vulkaninfo.
- Installed libs: GLFW 3.4, SDL3 3.5.1, SDL2.
- Missing (install with pacman): `vulkan-headers`, `vulkan-validation-layers`.
- Decided: C++20 + CMake/Ninja, SDL3 for the platform layer (DESIGN.md §5, §6).

## Build / run

```
./build.sh                  # configure if needed + build gcc AND clang (-Werror)
./build.sh run              # build, then run: picks best GPU, validation on (Debug)
./build.sh run --gpu 1      # force the AMD iGPU — run every phase on both GPUs
./build.sh run --no-validation
./build.sh clean
BUILD_TYPE=Release ./build.sh
```
Under the hood: `cmake -B build-<cc> -G Ninja -DCMAKE_CXX_COMPILER=<cc>` + `ninja -C build-<cc>`.

Source layout (phase 1): `src/vk_function_table.h` (X-macro list of entry points),
`src/vk_loader.{h,cpp}` (dlopen + three-level function loading), `src/main.cpp`.
