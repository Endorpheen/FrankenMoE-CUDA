#include "upload.h"

_Static_assert(sizeof(VkCommandBufferSubmitInfo) == 32 &&
    offsetof(VkCommandBufferSubmitInfo, commandBuffer) == 16 &&
    sizeof(VkSemaphoreSubmitInfo) == 48 && offsetof(VkSemaphoreSubmitInfo, semaphore) == 16 &&
    offsetof(VkSemaphoreSubmitInfo, value) == 24 && offsetof(VkSemaphoreSubmitInfo, stageMask) == 32 &&
    sizeof(VkSubmitInfo2) == 64 && offsetof(VkSubmitInfo2, commandBufferInfoCount) == 32 &&
    offsetof(VkSubmitInfo2, pCommandBufferInfos) == 40 && offsetof(VkSubmitInfo2, signalSemaphoreInfoCount) == 48 &&
    offsetof(VkSubmitInfo2, pSignalSemaphoreInfos) == 56 && sizeof(VkSemaphoreWaitInfo) == 40 &&
    offsetof(VkSemaphoreWaitInfo, semaphoreCount) == 20 && offsetof(VkSemaphoreWaitInfo, pSemaphores) == 24 &&
    offsetof(VkSemaphoreWaitInfo, pValues) == 32 && offsetof(fg_vk_device, queue) == 2032 &&
    offsetof(fg_vk_device, device) == 2016 && offsetof(fg_vk_device, QueueSubmit2) == 2232 &&
    offsetof(fg_vk_device, WaitSemaphores) == 2240 && offsetof(fg_vk_device, DeviceWaitIdle) == 2248 &&
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO == 1000314006 &&
    VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO == 1000314005 &&
    VK_STRUCTURE_TYPE_SUBMIT_INFO_2 == 1000314004 && VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO == 1000207004,
    "Goose Vulkan queue ABI");

#define FG_VK_ABI_OFFSET(type, field, offset) \
    _Static_assert(offsetof(type, field) == (offset), "Goose Vulkan ABI: " #type "." #field)
FG_VK_ABI_OFFSET(fg_vk_device, index, 2040);
FG_VK_ABI_OFFSET(fg_vk_device, family, 2044);
FG_VK_ABI_OFFSET(fg_vk_device, CreateCommandPool, 2144);
FG_VK_ABI_OFFSET(fg_vk_device, DestroyCommandPool, 2152);
FG_VK_ABI_OFFSET(fg_vk_device, AllocateCommandBuffers, 2160);
FG_VK_ABI_OFFSET(fg_vk_device, ResetCommandPool, 2168);
FG_VK_ABI_OFFSET(fg_vk_device, BeginCommandBuffer, 2176);
FG_VK_ABI_OFFSET(fg_vk_device, EndCommandBuffer, 2184);
FG_VK_ABI_OFFSET(fg_vk_device, CmdCopyBuffer, 2192);
FG_VK_ABI_OFFSET(fg_vk_device, CmdFillBuffer, 2200);
FG_VK_ABI_OFFSET(fg_vk_device, CmdPipelineBarrier2, 2208);
FG_VK_ABI_OFFSET(fg_vk_device, CreateSemaphore, 2216);
FG_VK_ABI_OFFSET(fg_vk_device, DestroySemaphore, 2224);
FG_VK_ABI_OFFSET(fg_vk_upload, vk, 0);
FG_VK_ABI_OFFSET(fg_vk_upload, staging, 8);
FG_VK_ABI_OFFSET(fg_vk_upload, pool, 40);
FG_VK_ABI_OFFSET(fg_vk_upload, cmd, 48);
FG_VK_ABI_OFFSET(fg_vk_upload, done, 56);
FG_VK_ABI_OFFSET(fg_vk_upload, epoch, 64);
FG_VK_ABI_OFFSET(fg_vk_buffer, buffer, 0);
FG_VK_ABI_OFFSET(fg_vk_buffer, usage, 20);
FG_VK_ABI_OFFSET(fg_vk_buffer, size, 24);
FG_VK_ABI_OFFSET(VkCommandPoolCreateInfo, flags, 16);
FG_VK_ABI_OFFSET(VkCommandPoolCreateInfo, queueFamilyIndex, 20);
FG_VK_ABI_OFFSET(VkSemaphoreTypeCreateInfo, semaphoreType, 16);
FG_VK_ABI_OFFSET(VkSemaphoreTypeCreateInfo, initialValue, 24);
FG_VK_ABI_OFFSET(VkSemaphoreCreateInfo, pNext, 8);
FG_VK_ABI_OFFSET(VkSemaphoreCreateInfo, flags, 16);
FG_VK_ABI_OFFSET(VkCommandBufferAllocateInfo, commandPool, 16);
FG_VK_ABI_OFFSET(VkCommandBufferAllocateInfo, commandBufferCount, 28);
FG_VK_ABI_OFFSET(VkCommandBufferBeginInfo, flags, 16);
FG_VK_ABI_OFFSET(VkCommandBufferBeginInfo, pInheritanceInfo, 24);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, srcStageMask, 16);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, srcAccessMask, 24);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, dstStageMask, 32);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, dstAccessMask, 40);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, srcQueueFamilyIndex, 48);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, dstQueueFamilyIndex, 52);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, buffer, 56);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, offset, 64);
FG_VK_ABI_OFFSET(VkBufferMemoryBarrier2, size, 72);
FG_VK_ABI_OFFSET(VkDependencyInfo, bufferMemoryBarrierCount, 32);
FG_VK_ABI_OFFSET(VkDependencyInfo, pBufferMemoryBarriers, 40);
FG_VK_ABI_OFFSET(VkBufferCopy, srcOffset, 0);
FG_VK_ABI_OFFSET(VkBufferCopy, dstOffset, 8);
FG_VK_ABI_OFFSET(VkBufferCopy, size, 16);
FG_VK_ABI_OFFSET(fg_vk_device, memory, 864);
FG_VK_ABI_OFFSET(fg_vk_device, CreateBuffer, 2064);
FG_VK_ABI_OFFSET(fg_vk_device, GetBufferMemoryRequirements, 2072);
FG_VK_ABI_OFFSET(fg_vk_device, AllocateMemory, 2080);
FG_VK_ABI_OFFSET(fg_vk_device, BindBufferMemory, 2088);
FG_VK_ABI_OFFSET(fg_vk_device, DestroyBuffer, 2096);
FG_VK_ABI_OFFSET(fg_vk_device, FreeMemory, 2104);
FG_VK_ABI_OFFSET(fg_vk_device, MapMemory, 2112);
FG_VK_ABI_OFFSET(fg_vk_device, UnmapMemory, 2120);
FG_VK_ABI_OFFSET(fg_vk_device, FlushMappedMemoryRanges, 2128);
FG_VK_ABI_OFFSET(fg_vk_device, InvalidateMappedMemoryRanges, 2136);
FG_VK_ABI_OFFSET(fg_vk_buffer, memory, 8);
FG_VK_ABI_OFFSET(fg_vk_buffer, properties, 16);
FG_VK_ABI_OFFSET(VkPhysicalDeviceMemoryProperties, memoryTypeCount, 0);
FG_VK_ABI_OFFSET(VkPhysicalDeviceMemoryProperties, memoryTypes, 4);
FG_VK_ABI_OFFSET(VkMemoryType, propertyFlags, 0);
FG_VK_ABI_OFFSET(VkBufferCreateInfo, size, 24);
FG_VK_ABI_OFFSET(VkBufferCreateInfo, usage, 32);
FG_VK_ABI_OFFSET(VkBufferCreateInfo, sharingMode, 36);
FG_VK_ABI_OFFSET(VkBufferCreateInfo, pQueueFamilyIndices, 48);
FG_VK_ABI_OFFSET(VkMemoryRequirements, size, 0);
FG_VK_ABI_OFFSET(VkMemoryRequirements, memoryTypeBits, 16);
FG_VK_ABI_OFFSET(VkMemoryAllocateInfo, allocationSize, 16);
FG_VK_ABI_OFFSET(VkMemoryAllocateInfo, memoryTypeIndex, 24);
FG_VK_ABI_OFFSET(VkMappedMemoryRange, memory, 16);
FG_VK_ABI_OFFSET(VkMappedMemoryRange, offset, 24);
FG_VK_ABI_OFFSET(VkMappedMemoryRange, size, 32);
_Static_assert(sizeof(VkMemoryType) == 8 && sizeof(VkBufferCreateInfo) == 56 &&
    sizeof(VkMemoryRequirements) == 24 && sizeof(VkMemoryAllocateInfo) == 32 &&
    sizeof(VkMappedMemoryRange) == 40 && VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO == 12 &&
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO == 5 && VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE == 6,
    "Goose Vulkan memory ABI");
#undef FG_VK_ABI_OFFSET
_Static_assert(sizeof(fg_vk_upload) == 72 && sizeof(fg_vk_buffer) == 32, "Goose Vulkan upload ABI");
_Static_assert(offsetof(fg_vk_device, loader) == 0 && offsetof(fg_vk_device, get_instance_proc) == 8,
    "Goose Vulkan loader ABI");

_Static_assert(sizeof(fg_vk_device) == 2392, "Goose Vulkan device ABI: fg_vk_device");
_Static_assert(offsetof(fg_vk_device, instance) == 16, "Goose Vulkan device ABI: fg_vk_device.instance");
_Static_assert(offsetof(fg_vk_device, DestroyInstance) == 24, "Goose Vulkan device ABI: fg_vk_device.DestroyInstance");
_Static_assert(offsetof(fg_vk_device, physical) == 32, "Goose Vulkan device ABI: fg_vk_device.physical");
_Static_assert(offsetof(fg_vk_device, properties) == 40, "Goose Vulkan device ABI: fg_vk_device.properties");
_Static_assert(offsetof(fg_vk_device, driver) == 1384, "Goose Vulkan device ABI: fg_vk_device.driver");
_Static_assert(offsetof(fg_vk_device, id) == 1920, "Goose Vulkan device ABI: fg_vk_device.id");
_Static_assert(offsetof(fg_vk_device, subgroup) == 1984, "Goose Vulkan device ABI: fg_vk_device.subgroup");
_Static_assert(offsetof(fg_vk_device, DestroyDevice) == 2024, "Goose Vulkan device ABI: fg_vk_device.DestroyDevice");
_Static_assert(sizeof(VkApplicationInfo) == 48, "Goose Vulkan device ABI: VkApplicationInfo");
_Static_assert(offsetof(VkApplicationInfo, pApplicationName) == 16, "Goose Vulkan device ABI: VkApplicationInfo.pApplicationName");
_Static_assert(offsetof(VkApplicationInfo, apiVersion) == 44, "Goose Vulkan device ABI: VkApplicationInfo.apiVersion");
_Static_assert(sizeof(VkInstanceCreateInfo) == 64, "Goose Vulkan device ABI: VkInstanceCreateInfo");
_Static_assert(offsetof(VkInstanceCreateInfo, pApplicationInfo) == 24, "Goose Vulkan device ABI: VkInstanceCreateInfo.pApplicationInfo");
_Static_assert(sizeof(VkPhysicalDeviceProperties) == 824, "Goose Vulkan device ABI: VkPhysicalDeviceProperties");
_Static_assert(offsetof(VkPhysicalDeviceProperties, apiVersion) == 0, "Goose Vulkan device ABI: VkPhysicalDeviceProperties.apiVersion");
_Static_assert(offsetof(VkPhysicalDeviceProperties, vendorID) == 8, "Goose Vulkan device ABI: VkPhysicalDeviceProperties.vendorID");
_Static_assert(offsetof(VkPhysicalDeviceProperties, deviceType) == 16, "Goose Vulkan device ABI: VkPhysicalDeviceProperties.deviceType");
_Static_assert(sizeof(VkPhysicalDeviceProperties2) == 840, "Goose Vulkan device ABI: VkPhysicalDeviceProperties2");
_Static_assert(offsetof(VkPhysicalDeviceProperties2, properties) == 16, "Goose Vulkan device ABI: VkPhysicalDeviceProperties2.properties");
_Static_assert(sizeof(VkPhysicalDeviceDriverProperties) == 536, "Goose Vulkan device ABI: VkPhysicalDeviceDriverProperties");
_Static_assert(sizeof(VkPhysicalDeviceIDProperties) == 64, "Goose Vulkan device ABI: VkPhysicalDeviceIDProperties");
_Static_assert(sizeof(VkPhysicalDeviceSubgroupProperties) == 32, "Goose Vulkan device ABI: VkPhysicalDeviceSubgroupProperties");
_Static_assert(sizeof(VkPhysicalDeviceVulkan12Features) == 208, "Goose Vulkan device ABI: VkPhysicalDeviceVulkan12Features");
_Static_assert(offsetof(VkPhysicalDeviceVulkan12Features, timelineSemaphore) == 164, "Goose Vulkan device ABI: VkPhysicalDeviceVulkan12Features.timelineSemaphore");
_Static_assert(sizeof(VkPhysicalDeviceVulkan13Features) == 80, "Goose Vulkan device ABI: VkPhysicalDeviceVulkan13Features");
_Static_assert(offsetof(VkPhysicalDeviceVulkan13Features, synchronization2) == 52, "Goose Vulkan device ABI: VkPhysicalDeviceVulkan13Features.synchronization2");
_Static_assert(sizeof(VkPhysicalDeviceFeatures2) == 240, "Goose Vulkan device ABI: VkPhysicalDeviceFeatures2");
_Static_assert(sizeof(VkQueueFamilyProperties) == 24, "Goose Vulkan device ABI: VkQueueFamilyProperties");
_Static_assert(offsetof(VkQueueFamilyProperties, timestampValidBits) == 8, "Goose Vulkan device ABI: VkQueueFamilyProperties.timestampValidBits");
_Static_assert(sizeof(VkDeviceQueueCreateInfo) == 40, "Goose Vulkan device ABI: VkDeviceQueueCreateInfo");
_Static_assert(offsetof(VkDeviceQueueCreateInfo, queueFamilyIndex) == 20, "Goose Vulkan device ABI: VkDeviceQueueCreateInfo.queueFamilyIndex");
_Static_assert(offsetof(VkDeviceQueueCreateInfo, queueCount) == 24, "Goose Vulkan device ABI: VkDeviceQueueCreateInfo.queueCount");
_Static_assert(offsetof(VkDeviceQueueCreateInfo, pQueuePriorities) == 32, "Goose Vulkan device ABI: VkDeviceQueueCreateInfo.pQueuePriorities");
_Static_assert(sizeof(VkDeviceCreateInfo) == 72, "Goose Vulkan device ABI: VkDeviceCreateInfo");
_Static_assert(offsetof(VkDeviceCreateInfo, queueCreateInfoCount) == 20, "Goose Vulkan device ABI: VkDeviceCreateInfo.queueCreateInfoCount");
_Static_assert(offsetof(VkDeviceCreateInfo, pQueueCreateInfos) == 24, "Goose Vulkan device ABI: VkDeviceCreateInfo.pQueueCreateInfos");
_Static_assert(VK_API_VERSION_1_3 == 4206592 &&
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 == 1000059001 &&
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 == 1000059000 &&
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES == 51 &&
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES == 53 &&
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES == 1000094000 &&
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES == 1000071004 &&
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES == 1000196000 &&
    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == 2 && VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU == 1 &&
    VK_PHYSICAL_DEVICE_TYPE_CPU == 4, "Goose Vulkan device constants");
