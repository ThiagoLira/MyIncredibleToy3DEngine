// vk_loader.cpp — see vk_loader.h for the big picture.
#include "vk_loader.h"

#include <cstdio>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

// ---- the globals declared in vk_loader.h, all start as nullptr -------------------
#define VK_DEFINE_FN(name) PFN_##name name = nullptr;
VK_GLOBAL_FUNCTIONS(VK_DEFINE_FN)
VK_INSTANCE_FUNCTIONS(VK_DEFINE_FN)
VK_DEVICE_FUNCTIONS(VK_DEFINE_FN)
#undef VK_DEFINE_FN
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

namespace {

void* g_library = nullptr;

// Candidate library names per OS. The loader is the *only* thing we open by name;
// the drivers (ICDs) are found by the loader itself via its JSON manifests.
const char* const k_library_names[] = {
#if defined(_WIN32)
    "vulkan-1.dll",
#elif defined(__APPLE__)
    "libvulkan.1.dylib",     // LunarG loader on macOS (routes to MoltenVK as an ICD)
    "libMoltenVK.dylib",     // MoltenVK exposes vkGetInstanceProcAddr directly, too
#else
    "libvulkan.so.1",        // Linux / BSD: the soname installed by the loader package
    "libvulkan.so",
#endif
};

void* open_library(const char* name) {
#if defined(_WIN32)
    return (void*)LoadLibraryA(name);
#else
    return dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
}

void* get_symbol(void* lib, const char* name) {
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}

void close_library(void* lib) {
#if defined(_WIN32)
    FreeLibrary((HMODULE)lib);
#else
    dlclose(lib);
#endif
}

} // namespace

namespace vk_loader {

bool load_library() {
    for (const char* name : k_library_names) {
        g_library = open_library(name);
        if (g_library) break;
    }
    if (!g_library) {
        std::fprintf(stderr, "vk_loader: no Vulkan loader library found on this system\n");
        return false;
    }

    vkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)get_symbol(g_library, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr) {
        std::fprintf(stderr, "vk_loader: library has no vkGetInstanceProcAddr\n");
        return false;
    }

    // Global-level functions are queried with instance == VK_NULL_HANDLE.
#define VK_LOAD_GLOBAL_FN(name) \
    name = (PFN_##name)vkGetInstanceProcAddr(VK_NULL_HANDLE, #name);
    VK_GLOBAL_FUNCTIONS(VK_LOAD_GLOBAL_FN)
#undef VK_LOAD_GLOBAL_FN
    return vkCreateInstance != nullptr;
}

void load_instance_functions(VkInstance instance) {
    // These pointers go through the loader's trampoline: the loader looks at the
    // instance's dispatch table and forwards to layers → ICD. Extension functions
    // (…EXT/…KHR) resolve to nullptr if the extension was not enabled — that is why
    // main.cpp must check them before calling.
#define VK_LOAD_INSTANCE_FN(name) \
    name = (PFN_##name)vkGetInstanceProcAddr(instance, #name);
    VK_INSTANCE_FUNCTIONS(VK_LOAD_INSTANCE_FN)
#undef VK_LOAD_INSTANCE_FN
}

void load_device_functions(VkDevice device) {
    // vkGetDeviceProcAddr returns the driver's own function pointer for this device,
    // bypassing the loader trampoline on every subsequent call.
#define VK_LOAD_DEVICE_FN(name) \
    name = (PFN_##name)vkGetDeviceProcAddr(device, #name);
    VK_DEVICE_FUNCTIONS(VK_LOAD_DEVICE_FN)
#undef VK_LOAD_DEVICE_FN
}

void unload_library() {
    if (g_library) close_library(g_library);
    g_library = nullptr;
}

} // namespace vk_loader
