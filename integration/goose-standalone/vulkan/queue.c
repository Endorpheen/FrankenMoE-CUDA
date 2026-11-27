#include "device.h"

int fg_vk_submit_wait(fg_vk_device *vk, VkCommandBuffer cmd, VkSemaphore done, uint64_t value) {
    const VkCommandBufferSubmitInfo command = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = cmd,
    };
    const VkSemaphoreSubmitInfo signal = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = done, .value = value,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    const VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, .commandBufferInfoCount = 1, .pCommandBufferInfos = &command,
        .signalSemaphoreInfoCount = 1, .pSignalSemaphoreInfos = &signal,
    };
    VkResult result = vk->QueueSubmit2(vk->queue, 1, &submit, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) return fg_vk_error("submit command buffer", result);
    const VkSemaphoreWaitInfo wait = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1, .pSemaphores = &done, .pValues = &value,
    };
    result = vk->WaitSemaphores(vk->device, &wait, UINT64_MAX);
    if (result == VK_SUCCESS) return 1;
    (void)vk->DeviceWaitIdle(vk->device);
    return fg_vk_error("wait for command buffer", result);
}
