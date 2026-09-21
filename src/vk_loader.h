// vk_loader.h — runtime loading of the Vulkan loader library and its entry points.
//
// VK_NO_PROTOTYPES tells vulkan.h to declare only the types and the PFN_* function-pointer
// typedefs, and NOT the function prototypes. Without it, `vkCreateInstance(...)` would
// refer to a symbol that must be resolved at link time, i.e. we'd be forced to link
// libvulkan. With it, the name `vkCreateInstance` below is *our* global variable holding
// a pointer we fetched at runtime. Call sites look identical either way.
#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "vk_function_table.h"

// Declare one global function pointer per entry point, e.g.:
//   extern PFN_vkCreateInstance vkCreateInstance;
#define VK_DECLARE_FN(name) extern PFN_##name name;
VK_GLOBAL_FUNCTIONS(VK_DECLARE_FN)
VK_INSTANCE_FUNCTIONS(VK_DECLARE_FN)
VK_DEVICE_FUNCTIONS(VK_DECLARE_FN)
#undef VK_DECLARE_FN

// The one symbol we look up by name in the shared library. Everything else is obtained
// by asking it.
extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

namespace vk_loader {

// Step 1: dlopen the loader, fetch vkGetInstanceProcAddr, then the GLOBAL functions.
// Returns false (and prints why) if no Vulkan loader is installed on this machine.
bool load_library();

// Step 2: after vkCreateInstance — resolve the INSTANCE-level functions for `instance`.
void load_instance_functions(VkInstance instance);

// Step 3: after vkCreateDevice — resolve the DEVICE-level functions for `device`.
void load_device_functions(VkDevice device);

// Step 4: after vkDestroyInstance — dlclose.
void unload_library();

} // namespace vk_loader
