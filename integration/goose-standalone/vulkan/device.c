#include "device.h"

#include <stdio.h>
#include <stdlib.h>

int fg_vk_error(const char *action, VkResult result) {
    fprintf(stderr, "Vulkan: %s (%d)\n", action, (int)result);
    return 0;
}

void fg_vk_close(fg_vk_device *vk) {
    if (vk->device && vk->DestroyDevice) vk->DestroyDevice(vk->device, NULL);
    if (vk->instance && vk->DestroyInstance) vk->DestroyInstance(vk->instance, NULL);
    fg_vk_unload(vk);
    *vk = (fg_vk_device){0};
}

typedef struct {
    PFN_vkEnumeratePhysicalDevices enumerate;
    PFN_vkGetPhysicalDeviceProperties2 properties;
    PFN_vkGetPhysicalDeviceFeatures2 features;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties queues;
    PFN_vkGetPhysicalDeviceMemoryProperties memory;
    PFN_vkCreateDevice create;
    PFN_vkGetDeviceProcAddr device_proc;
} fg_vk_instance_api;

static int pick_queue(const fg_vk_instance_api *api, VkPhysicalDevice device,
                      uint32_t *family, uint32_t *timestamp_bits) {
    uint32_t count = 0;
    api->queues(device, &count, NULL);
    if (!count) return 0;
    VkQueueFamilyProperties *queues = calloc(count, sizeof(*queues));
    if (!queues) return 0;
    api->queues(device, &count, queues);
    int found = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (queues[i].queueCount && (queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            const int dedicated = !(queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT);
            if (!found || dedicated) {
                *family = i;
                *timestamp_bits = queues[i].timestampValidBits;
                found = 1;
            }
            if (dedicated) break;
        }
    }
    free(queues);
    return found;
}

static int choose_device(fg_vk_device *vk, const fg_vk_instance_api *api, uint32_t index, int allow_cpu) {
    uint32_t count;
    VkResult result = api->enumerate(vk->instance, &count, NULL);
    if (result != VK_SUCCESS) return fg_vk_error("enumerate physical devices", result);
    if (!count) return fg_vk_error("no physical device", VK_ERROR_INITIALIZATION_FAILED);
    VkPhysicalDevice *devices = calloc(count, sizeof(*devices));
    if (!devices) return fg_vk_error("physical device list allocation failed", VK_ERROR_OUT_OF_HOST_MEMORY);
    result = api->enumerate(vk->instance, &count, devices);
    if (result != VK_SUCCESS) {
        free(devices);
        return fg_vk_error("enumerate physical devices", result);
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (index != FG_VK_AUTO_DEVICE && i != index) continue;
        VkPhysicalDeviceSubgroupProperties subgroup = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
        VkPhysicalDeviceIDProperties id = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES, .pNext = &subgroup };
        VkPhysicalDeviceDriverProperties driver = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES, .pNext = &id };
        VkPhysicalDeviceProperties2 properties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver };
        api->properties(devices[i], &properties);
        VkPhysicalDeviceProperties p = properties.properties;
        if (p.apiVersion < VK_API_VERSION_1_3) continue;
        if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU && (!allow_cpu || index == FG_VK_AUTO_DEVICE)) continue;
        if (index == FG_VK_AUTO_DEVICE && (p.vendorID != 0x1002 ||
            (p.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
             p.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))) continue;

        VkPhysicalDeviceVulkan13Features f13 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceVulkan12Features f12 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &f13 };
        VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f12 };
        api->features(devices[i], &features);
        if (!f12.timelineSemaphore || !f13.synchronization2) continue;
        if (!pick_queue(api, devices[i], &vk->family, &vk->timestamp_bits)) continue;

        vk->physical = devices[i];
        vk->index = i;
        vk->properties = p;
        vk->driver = driver;
        vk->id = id;
        vk->subgroup = subgroup;
        vk->driver.pNext = vk->id.pNext = vk->subgroup.pNext = NULL;
        api->memory(vk->physical, &vk->memory);
        break;
    }
    free(devices);
    if (vk->physical) return 1;
    return fg_vk_error(index == FG_VK_AUTO_DEVICE ? "no eligible AMD Vulkan 1.3 device" :
                       "requested device is absent or does not meet the Vulkan 1.3 requirements",
                       VK_ERROR_FEATURE_NOT_PRESENT);
}

static int create_device(fg_vk_device *vk, const fg_vk_instance_api *api) {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk->family, .queueCount = 1, .pQueuePriorities = &priority,
    };
    VkPhysicalDeviceVulkan13Features f13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .synchronization2 = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features f12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &f13, .timelineSemaphore = VK_TRUE,
    };
    VkDeviceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &f12, .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue,
    };
    VkResult result = api->create(vk->physical, &info, NULL, &vk->device);
    if (result != VK_SUCCESS) return fg_vk_error("create compute device", result);
#define FG_VK_LOAD_DEVICE(fn) \
    vk->fn = (PFN_vk##fn)api->device_proc(vk->device, "vk" #fn); \
    if (!vk->fn) return fg_vk_error("missing vk" #fn, VK_ERROR_INITIALIZATION_FAILED);
    FG_VK_DEVICE_COMMANDS(FG_VK_LOAD_DEVICE)
#undef FG_VK_LOAD_DEVICE
    vk->GetDeviceQueue(vk->device, vk->family, 0, &vk->queue);
    return 1;
}

int fg_vk_open(fg_vk_device *vk, uint32_t index, int allow_cpu) {
    *vk = (fg_vk_device){0};
    if (!fg_vk_load(vk)) {
        fg_vk_error("load Vulkan or resolve vkGetInstanceProcAddr", VK_ERROR_INITIALIZATION_FAILED);
        goto fail;
    }
    PFN_vkEnumerateInstanceVersion version = (PFN_vkEnumerateInstanceVersion)
        vk->get_instance_proc(NULL, "vkEnumerateInstanceVersion");
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)vk->get_instance_proc(NULL, "vkCreateInstance");
    uint32_t api_version = 0;
    if (!create || !version || version(&api_version) != VK_SUCCESS || api_version < VK_API_VERSION_1_3) {
        fg_vk_error("Vulkan 1.3 loader required", VK_ERROR_INCOMPATIBLE_DRIVER);
        goto fail;
    }
    const VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "FrankenMoE",
        .apiVersion = VK_API_VERSION_1_3,
    };
    const VkInstanceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
    VkResult result = create(&info, NULL, &vk->instance);
    if (result != VK_SUCCESS) {
        fg_vk_error("create instance", result);
        goto fail;
    }
    fg_vk_instance_api api;
#define FG_VK_LOAD_INSTANCE(dst, fn) \
    dst = (PFN_vk##fn)vk->get_instance_proc(vk->instance, "vk" #fn); \
    if (!dst) { fg_vk_error("missing vk" #fn, VK_ERROR_INITIALIZATION_FAILED); goto fail; }
    FG_VK_LOAD_INSTANCE(vk->DestroyInstance, DestroyInstance)
    FG_VK_LOAD_INSTANCE(vk->DestroyDevice, DestroyDevice)
    FG_VK_LOAD_INSTANCE(api.enumerate, EnumeratePhysicalDevices)
    FG_VK_LOAD_INSTANCE(api.properties, GetPhysicalDeviceProperties2)
    FG_VK_LOAD_INSTANCE(api.features, GetPhysicalDeviceFeatures2)
    FG_VK_LOAD_INSTANCE(api.queues, GetPhysicalDeviceQueueFamilyProperties)
    FG_VK_LOAD_INSTANCE(api.memory, GetPhysicalDeviceMemoryProperties)
    FG_VK_LOAD_INSTANCE(api.create, CreateDevice)
    FG_VK_LOAD_INSTANCE(api.device_proc, GetDeviceProcAddr)
#undef FG_VK_LOAD_INSTANCE
    if (!choose_device(vk, &api, index, allow_cpu) || !create_device(vk, &api)) goto fail;
    return 1;
fail:
    fg_vk_close(vk);
    return 0;
}
