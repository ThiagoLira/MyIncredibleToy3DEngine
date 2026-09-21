// vk_function_table.h — the list of Vulkan entry points we load by hand.
//
// This is an "X-macro" table: each list takes a macro X and applies it to every name.
// The same list is then used (a) to declare the function-pointer globals, and (b) to
// load them, so the two can never go out of sync.
//
// The three lists mirror the three *levels* of the Vulkan dispatch model:
//
//   GLOBAL   — callable before any VkInstance exists (queried with instance = NULL).
//   INSTANCE — need a VkInstance; the loader's trampoline forwards them to the right
//              ICD/layer based on the object's dispatch table.
//   DEVICE   — need a VkDevice; fetched with vkGetDeviceProcAddr, which returns a pointer
//              *directly into the driver*, skipping the loader trampoline (faster).
//
// Adding a Vulkan call to the engine == adding one line to the right list.
#pragma once

#define VK_GLOBAL_FUNCTIONS(X)                    \
    X(vkEnumerateInstanceVersion)                 \
    X(vkEnumerateInstanceLayerProperties)         \
    X(vkEnumerateInstanceExtensionProperties)     \
    X(vkCreateInstance)

#define VK_INSTANCE_FUNCTIONS(X)                  \
    X(vkDestroyInstance)                          \
    X(vkEnumeratePhysicalDevices)                 \
    X(vkGetPhysicalDeviceProperties2)             \
    X(vkGetPhysicalDeviceFeatures2)               \
    X(vkGetPhysicalDeviceQueueFamilyProperties)   \
    X(vkGetPhysicalDeviceMemoryProperties)        \
    X(vkEnumerateDeviceExtensionProperties)       \
    X(vkCreateDevice)                             \
    X(vkGetDeviceProcAddr)                        \
    /* VK_EXT_debug_utils — NULL unless the extension was enabled on the instance */ \
    X(vkCreateDebugUtilsMessengerEXT)             \
    X(vkDestroyDebugUtilsMessengerEXT)

#define VK_DEVICE_FUNCTIONS(X)                    \
    X(vkDestroyDevice)                            \
    X(vkGetDeviceQueue)                           \
    X(vkDeviceWaitIdle)
