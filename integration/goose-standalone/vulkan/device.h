#ifndef FG_VULKAN_DEVICE_H
#define FG_VULKAN_DEVICE_H

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <stddef.h>

#define FG_VK_AUTO_DEVICE UINT32_MAX

#define FG_VK_DEVICE_COMMANDS(X) \
    X(GetDeviceQueue) X(CreateBuffer) X(GetBufferMemoryRequirements) \
    X(AllocateMemory) X(BindBufferMemory) X(DestroyBuffer) X(FreeMemory) \
    X(MapMemory) X(UnmapMemory) X(FlushMappedMemoryRanges) X(InvalidateMappedMemoryRanges) \
    X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandPool) \
    X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdCopyBuffer) X(CmdFillBuffer) X(CmdPipelineBarrier2) \
    X(CreateSemaphore) X(DestroySemaphore) X(QueueSubmit2) X(WaitSemaphores) X(DeviceWaitIdle) \
    X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) X(CreatePipelineLayout) X(DestroyPipelineLayout) \
    X(CreateShaderModule) X(DestroyShaderModule) X(CreateComputePipelines) X(DestroyPipeline) \
    X(CreateDescriptorPool) X(DestroyDescriptorPool) X(ResetDescriptorPool) X(AllocateDescriptorSets) \
    X(UpdateDescriptorSets) X(CmdBindPipeline) X(CmdBindDescriptorSets) X(CmdPushConstants) X(CmdDispatch)

typedef struct {
    void *loader;
    PFN_vkGetInstanceProcAddr get_instance_proc;
    VkInstance instance;
    PFN_vkDestroyInstance DestroyInstance;
    VkPhysicalDevice physical;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory;
    VkPhysicalDeviceDriverProperties driver;
    VkPhysicalDeviceIDProperties id;
    VkPhysicalDeviceSubgroupProperties subgroup;
    VkDevice device;
    PFN_vkDestroyDevice DestroyDevice;
    VkQueue queue;
    uint32_t index;
    uint32_t family;
    uint32_t timestamp_bits;
#define FG_VK_DECLARE(fn) PFN_vk##fn fn;
    FG_VK_DEVICE_COMMANDS(FG_VK_DECLARE)
#undef FG_VK_DECLARE
} fg_vk_device;

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkMemoryPropertyFlags properties;
    VkBufferUsageFlags usage;
    VkDeviceSize size;
} fg_vk_buffer;

int fg_vk_load(fg_vk_device *vk);
void fg_vk_unload(fg_vk_device *vk);
int fg_vk_open(fg_vk_device *vk, uint32_t index, int allow_cpu);
void fg_vk_close(fg_vk_device *vk);
int fg_vk_error(const char *action, VkResult result);

VkResult fg_vk_alloc_buffer(fg_vk_device *vk, fg_vk_buffer *buffer, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred);
void fg_vk_free_buffer(fg_vk_device *vk, fg_vk_buffer *buffer);
VkResult fg_vk_access_buffer(fg_vk_device *vk, const fg_vk_buffer *buffer,
    VkDeviceSize offset, void *data, size_t size, int write);
int fg_vk_submit_wait(fg_vk_device *vk, VkCommandBuffer cmd, VkSemaphore done, uint64_t value);
int fg_vk_roundtrip(fg_vk_device *vk, const void *input, void *output, size_t size);

#endif
