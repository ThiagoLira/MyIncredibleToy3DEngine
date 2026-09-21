// main.cpp — Phase 1: instance → physical devices → logical device.
//
// Reading order: main() at the bottom, then follow the calls upward.
#include "vk_loader.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Every Vulkan call that returns VkResult goes through this. On failure it
// names the call and the file:line, then aborts. Vulkan errors are programmer
// errors in this engine (out-of-memory aside), so aborting loudly is the right
// default.
#define VK_CHECK(call)                                                         \
  do {                                                                         \
    VkResult vk_check_result_ = (call);                                        \
    if (vk_check_result_ != VK_SUCCESS) {                                      \
      std::fprintf(stderr, "%s:%d: %s failed with VkResult %d\n", __FILE__,    \
                   __LINE__, #call, (int)vk_check_result_);                    \
      std::abort();                                                            \
    }                                                                          \
  } while (0)

// ============================================================================
// Instance
// ============================================================================

// Called by the validation layer (through the loader) whenever it has something
// to say. Returning VK_FALSE means "don't abort the Vulkan call that triggered
// this message".
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void * /*user_data*/) {
  const char *tag =
      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)     ? "ERROR"
      : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN"
                                                                     : "INFO";
  std::fprintf(stderr, "[vulkan %s] %s\n", tag, data->pMessage);
  return VK_FALSE;
}

static bool has_instance_layer(const char *name) {
  uint32_t count = 0;
  VK_CHECK(vkEnumerateInstanceLayerProperties(&count,
                                              nullptr)); // 1st call: how many?
  std::vector<VkLayerProperties> layers(count);
  VK_CHECK(vkEnumerateInstanceLayerProperties(&count,
                                              layers.data())); // 2nd call: fill
  for (const auto &l : layers)
    if (std::strcmp(l.layerName, name) == 0)
      return true;
  return false;
}

static bool has_instance_extension(const char *name) {
  uint32_t count = 0;
  VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
  std::vector<VkExtensionProperties> exts(count);
  VK_CHECK(
      vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data()));
  for (const auto &e : exts)
    if (std::strcmp(e.extensionName, name) == 0)
      return true;
  return false;
}

struct Instance {
  VkInstance handle = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger =
      VK_NULL_HANDLE; // VK_NULL_HANDLE when validation is off
};

static Instance create_instance(bool want_validation) {
  // --- 1. What does this machine's loader support?
  // --------------------------------
  uint32_t loader_version = VK_API_VERSION_1_0;
  VK_CHECK(vkEnumerateInstanceVersion(&loader_version));
  std::printf("loader supports Vulkan %u.%u.%u\n",
              VK_API_VERSION_MAJOR(loader_version),
              VK_API_VERSION_MINOR(loader_version),
              VK_API_VERSION_PATCH(loader_version));
  if (loader_version < VK_API_VERSION_1_3) {
    std::fprintf(stderr, "this engine requires Vulkan 1.3 (DESIGN.md §3)\n");
    std::exit(1);
  }

  // --- 2. Layers and extensions we ask for
  // ----------------------------------------
  std::vector<const char *> layers;
  std::vector<const char *> extensions;

  const bool validation =
      want_validation && has_instance_layer("VK_LAYER_KHRONOS_validation") &&
      has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  if (want_validation && !validation)
    std::fprintf(stderr,
                 "validation requested but layer/extension not installed\n");
  if (validation) {
    layers.push_back("VK_LAYER_KHRONOS_validation");
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  // MoltenVK (macOS) is a *non-conformant* implementation and hides itself
  // unless we opt in with this extension + flag. Harmless everywhere else.
  // (DESIGN.md §0)
  VkInstanceCreateFlags flags = 0;
  if (has_instance_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }

  // --- 3. The create-info structs
  // ------------------------------------------------- Every Vulkan struct
  // starts with sType (what am I) and pNext (chain of extension structs). This
  // is how the API grows without breaking old function signatures.
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "toy3d";
  app.pEngineName = "toy3d";
  app.apiVersion = VK_API_VERSION_1_3; // the version we promise to stay within

  VkDebugUtilsMessengerCreateInfoEXT dbg{};
  dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  dbg.pfnUserCallback = debug_callback;

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.flags = flags;
  ci.pApplicationInfo = &app;
  ci.enabledLayerCount = (uint32_t)layers.size();
  ci.ppEnabledLayerNames = layers.data();
  ci.enabledExtensionCount = (uint32_t)extensions.size();
  ci.ppEnabledExtensionNames = extensions.data();
  // Chaining the messenger info here makes vkCreateInstance/vkDestroyInstance
  // themselves validated; the real messenger object below covers everything
  // between.
  if (validation)
    ci.pNext = &dbg;

  Instance inst;
  VK_CHECK(vkCreateInstance(&ci, nullptr, &inst.handle));
  vk_loader::load_instance_functions(inst.handle);

  if (validation)
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(inst.handle, &dbg, nullptr,
                                            &inst.messenger));
  return inst;
}

static void destroy_instance(Instance &inst) {
  if (inst.messenger)
    vkDestroyDebugUtilsMessengerEXT(inst.handle, inst.messenger, nullptr);
  vkDestroyInstance(inst.handle, nullptr);
  inst = {};
}

// ============================================================================
// Physical devices
// ============================================================================

static const char *device_type_name(VkPhysicalDeviceType t) {
  switch (t) {
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
    return "discrete";
  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    return "integrated";
  case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    return "virtual";
  case VK_PHYSICAL_DEVICE_TYPE_CPU:
    return "cpu";
  default:
    return "other";
  }
}

static void print_physical_device(uint32_t index, VkPhysicalDevice pd) {
  // Properties2 + a pNext chain: we hang VkPhysicalDeviceDriverProperties off
  // it and the driver fills both structs in one call. Same mechanism as the
  // instance pNext.
  VkPhysicalDeviceDriverProperties driver{};
  driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
  VkPhysicalDeviceProperties2 props{};
  props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props.pNext = &driver;
  vkGetPhysicalDeviceProperties2(pd, &props);
  const VkPhysicalDeviceProperties &p = props.properties;

  std::printf("\n[%u] %s\n", index, p.deviceName);
  std::printf(
      "    type %s, api %u.%u.%u, driver %s (%s)\n",
      device_type_name(p.deviceType), VK_API_VERSION_MAJOR(p.apiVersion),
      VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion),
      driver.driverName, driver.driverInfo);

  // Queue families: this is where NVIDIA and AMD look different (DESIGN.md §9).
  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &family_count, nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &family_count, families.data());
  for (uint32_t i = 0; i < family_count; ++i) {
    const VkQueueFlags f = families[i].queueFlags;
    std::printf("    queue family %u: %2u queues  %s%s%s%s\n", i,
                families[i].queueCount,
                (f & VK_QUEUE_GRAPHICS_BIT) ? "GRAPHICS " : "",
                (f & VK_QUEUE_COMPUTE_BIT) ? "COMPUTE " : "",
                (f & VK_QUEUE_TRANSFER_BIT) ? "TRANSFER " : "",
                (f & VK_QUEUE_SPARSE_BINDING_BIT) ? "SPARSE " : "");
  }

  // Memory heaps: physical pools of memory. Memory types: ways of accessing a
  // heap.
  VkPhysicalDeviceMemoryProperties mem{};
  vkGetPhysicalDeviceMemoryProperties(pd, &mem);
  for (uint32_t i = 0; i < mem.memoryHeapCount; ++i)
    std::printf("    heap %u: %6.0f MiB %s\n", i,
                (double)mem.memoryHeaps[i].size / (1024.0 * 1024.0),
                (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    ? "DEVICE_LOCAL"
                    : "");
  for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
    const VkMemoryPropertyFlags f = mem.memoryTypes[i].propertyFlags;
    std::printf(
        "    memory type %2u → heap %u: %s%s%s%s\n", i,
        mem.memoryTypes[i].heapIndex,
        (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "DEVICE_LOCAL " : "",
        (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? "HOST_VISIBLE " : "",
        (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? "HOST_COHERENT " : "",
        (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "HOST_CACHED " : "");
  }
}

// ============================================================================
// EXERCISE 1 — score_physical_device
// ============================================================================
// Return 0 if the device is unusable for this engine, otherwise a positive
// score where higher is better. main() picks the highest-scoring device (unless
// --gpu overrides).
//
// Requirements to be usable (return 0 otherwise):
//   - apiVersion >= Vulkan 1.3 (DESIGN.md §3)
//   - at least one queue family with VK_QUEUE_GRAPHICS_BIT
// Preferences (add to the score):
//   - a discrete GPU beats an integrated one beats anything else
//   - more DEVICE_LOCAL memory is better (tie-breaker; keep it small so it
//   never
//     overrides the type preference)
//
// Hint: everything you need is queried exactly the same way
// print_physical_device does it. You may copy from there.
static uint32_t score_physical_device(VkPhysicalDevice pd) {
  (void)pd;
  uint32_t phys_score = 0;
  // Properties2 + a pNext chain: we hang VkPhysicalDeviceDriverProperties off
  // it and the driver fills both structs in one call. Same mechanism as the
  // instance pNext.
  VkPhysicalDeviceDriverProperties driver{};
  driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
  VkPhysicalDeviceProperties2 props{};
  props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props.pNext = &driver;
  vkGetPhysicalDeviceProperties2(pd, &props);
  const VkPhysicalDeviceProperties &p = props.properties;

  if (VK_API_VERSION_MAJOR(p.apiVersion) < 1 &&
      VK_API_VERSION_MINOR(p.apiVersion) < 3) {
    return 0;
  }

  if (strcmp(device_type_name(p.deviceType), "discrete") == 0) {
    phys_score += 1000000;
  } else if (strcmp(device_type_name(p.deviceType), "integrated") == 0) {
    phys_score += 10000;
  } else {
    phys_score += 100;
  }

  // Queue families: this is where NVIDIA and AMD look different (DESIGN.md §9).
  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &family_count, nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &family_count, families.data());
  for (uint32_t i = 0; i < family_count; ++i) {
    const VkQueueFlags f = families[i].queueFlags;
    std::printf("    queue family %u: %2u queues  %s%s%s%s\n", i,
                families[i].queueCount,
                (f & VK_QUEUE_GRAPHICS_BIT) ? "GRAPHICS " : "",
                (f & VK_QUEUE_COMPUTE_BIT) ? "COMPUTE " : "",
                (f & VK_QUEUE_TRANSFER_BIT) ? "TRANSFER " : "",
                (f & VK_QUEUE_SPARSE_BINDING_BIT) ? "SPARSE " : "");
  }

  // Memory heaps: physical pools of memory. Memory types: ways of accessing a
  // heap.
  VkPhysicalDeviceMemoryProperties mem{};
  vkGetPhysicalDeviceMemoryProperties(pd, &mem);
  for (uint32_t i = 0; i < mem.memoryHeapCount; ++i)
    std::printf("    heap %u: %6.0f MiB %s\n", i,
                (double)mem.memoryHeaps[i].size / (1024.0 * 1024.0),
                (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    ? "DEVICE_LOCAL"
                    : "");

//TODO MAKE THE MEMORY ADD TO THE SCORE
  for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
    const VkMemoryPropertyFlags f = mem.memoryTypes[i].propertyFlags;
    std::printf(
        "    memory type %2u → heap %u: %s%s%s%s\n", i,
        mem.memoryTypes[i].heapIndex,
        (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? "DEVICE_LOCAL " : "",
        (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? "HOST_VISIBLE " : "",
        (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? "HOST_COHERENT " : "",
        (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "HOST_CACHED " : "");
  }
  return phys_score;
}

// ============================================================================
// EXERCISE 2 — find_queue_families
// ============================================================================
// Find the queue family indices we will create queues from. UINT32_MAX means
// "none".
//
//   graphics: any family with GRAPHICS (on all drivers it also has
//   COMPUTE+TRANSFER). compute:  a family with COMPUTE but WITHOUT GRAPHICS, so
//   async compute can run in
//             parallel with rendering. AMD exposes one; NVIDIA may not. If
//             none, fall back to the graphics family.
//   transfer: a family with TRANSFER but WITHOUT GRAPHICS and WITHOUT COMPUTE —
//   a
//             pure DMA engine for uploads. Fall back to compute, then graphics.
//
// The fallbacks are what make this vendor-portable (DESIGN.md §9 rule 1).
struct QueueFamilies {
  uint32_t graphics = UINT32_MAX;
  uint32_t compute = UINT32_MAX;
  uint32_t transfer = UINT32_MAX;
};

static QueueFamilies find_queue_families(VkPhysicalDevice pd) {
  (void)pd;
  // Queue families: this is where NVIDIA and AMD look different (DESIGN.md §9).
  QueueFamilies qf = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &family_count, nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &family_count, families.data());
  for (uint32_t i = 0; i < family_count; ++i) {
    const VkQueueFlags f = families[i].queueFlags;
    if (f & VK_QUEUE_GRAPHICS_BIT) {qf.graphics = i;};
    if (~(f & VK_QUEUE_GRAPHICS_BIT) & (f & VK_QUEUE_COMPUTE_BIT)) {qf.compute = i;};
    if (~(f & VK_QUEUE_GRAPHICS_BIT) & ~(f & VK_QUEUE_COMPUTE_BIT) & (f & VK_QUEUE_TRANSFER_BIT) ) {qf.transfer = i;};
  }

  // Fallbacks (DESIGN.md §9 rule 1). A driver may expose no dedicated compute or
  // transfer family; the graphics family can always do both, so degrade toward it.
  // Order matters: transfer falls back to compute, which may itself have fallen
  // back to graphics.
  if (qf.compute  == UINT32_MAX) qf.compute  = qf.graphics;
  if (qf.transfer == UINT32_MAX) qf.transfer = qf.compute;
  return qf;
}

// ============================================================================
// Logical device
// ============================================================================

struct Device {
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice handle = VK_NULL_HANDLE;
  QueueFamilies families;
  VkQueue graphics_queue = VK_NULL_HANDLE;
  VkQueue compute_queue = VK_NULL_HANDLE;
  VkQueue transfer_queue = VK_NULL_HANDLE;
};

static Device create_device(VkPhysicalDevice pd, QueueFamilies families) {
  // One VkDeviceQueueCreateInfo per *distinct* family. Asking for the same
  // family twice is an error, so we deduplicate here.
  const float priority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  for (uint32_t family :
       {families.graphics, families.compute, families.transfer}) {
    bool seen = false;
    for (const auto &q : queue_infos)
      seen |= (q.queueFamilyIndex == family);
    if (seen)
      continue;
    VkDeviceQueueCreateInfo q{};
    q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q.queueFamilyIndex = family;
    q.queueCount = 1;
    q.pQueuePriorities = &priority;
    queue_infos.push_back(q);
  }

  // Features are opt-in: a device is created with everything OFF unless we ask.
  // We ask for the Vulkan 1.3 features that motivated DESIGN.md §3.
  VkPhysicalDeviceVulkan13Features f13{};
  f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;
  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &f13;

  VkDeviceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  ci.pNext = &features;
  ci.queueCreateInfoCount = (uint32_t)queue_infos.size();
  ci.pQueueCreateInfos = queue_infos.data();

  Device dev;
  dev.physical = pd;
  dev.families = families;
  VK_CHECK(vkCreateDevice(pd, &ci, nullptr, &dev.handle));
  vk_loader::load_device_functions(dev.handle);

  // Queue index 0 of each family — we asked for queueCount = 1.
  vkGetDeviceQueue(dev.handle, families.graphics, 0, &dev.graphics_queue);
  vkGetDeviceQueue(dev.handle, families.compute, 0, &dev.compute_queue);
  vkGetDeviceQueue(dev.handle, families.transfer, 0, &dev.transfer_queue);
  return dev;
}

static void destroy_device(Device &dev) {
  vkDeviceWaitIdle(dev.handle); // never destroy a device with work in flight
  vkDestroyDevice(dev.handle, nullptr);
  dev = {};
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char **argv) {
  // --gpu N        force physical device N (for testing both GPUs, DESIGN.md
  // §9)
  // --no-validation  run without the validation layer
  int forced_gpu = -1;
#ifdef NDEBUG
  bool validation = false;
#else
  bool validation = true;
#endif
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--gpu") == 0 && i + 1 < argc)
      forced_gpu = std::atoi(argv[++i]);
    else if (std::strcmp(argv[i], "--no-validation") == 0)
      validation = false;
  }

  if (!vk_loader::load_library())
    return 1;
  Instance inst = create_instance(validation);

  uint32_t pd_count = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(inst.handle, &pd_count, nullptr));
  std::vector<VkPhysicalDevice> pds(pd_count);
  VK_CHECK(vkEnumeratePhysicalDevices(inst.handle, &pd_count, pds.data()));
  std::printf("%u physical device(s)\n", pd_count);

  int best = -1;
  uint32_t best_score = 0;
  for (uint32_t i = 0; i < pd_count; ++i) {
    print_physical_device(i, pds[i]);
    const uint32_t s = score_physical_device(pds[i]);
    std::printf("    score %u\n", s);
    if (s > best_score) {
      best_score = s;
      best = (int)i;
    }
  }
  if (forced_gpu >= 0 && forced_gpu < (int)pd_count)
    best = forced_gpu;
  if (best < 0) {
    std::fprintf(
        stderr, "\nno usable device (is score_physical_device implemented?)\n");
    destroy_instance(inst);
    vk_loader::unload_library();
    return 1;
  }

  const QueueFamilies fam = find_queue_families(pds[best]);
  if (fam.graphics == UINT32_MAX) {
    std::fprintf(
        stderr,
        "\nno graphics queue family (is find_queue_families implemented?)\n");
    destroy_instance(inst);
    vk_loader::unload_library();
    return 1;
  }
  std::printf("\nusing device [%d]; queue families: graphics %u, compute %u, "
              "transfer %u\n",
              best, fam.graphics, fam.compute, fam.transfer);

  Device dev = create_device(pds[best], fam);
  std::printf("logical device created\n");

  // Teardown in reverse order of creation. Vulkan does not do this for us.
  destroy_device(dev);
  destroy_instance(inst);
  vk_loader::unload_library();
  return 0;
}
