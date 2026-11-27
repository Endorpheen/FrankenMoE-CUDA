#include "upload.h"

#include <stddef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef CreateSemaphore
#else
#include <dlfcn.h>
#endif

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
#undef FG_VK_ABI_OFFSET
_Static_assert(sizeof(fg_vk_upload) == 72 && sizeof(fg_vk_buffer) == 32, "Goose Vulkan upload ABI");

extern void fg_vk_queue_init(int argc, char **argv);

int fg_vk_load(fg_vk_device *vk) {
    fg_vk_queue_init(0, NULL);
#ifdef _WIN32
    HMODULE loader = LoadLibraryExW(L"vulkan-1.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    vk->loader = loader;
    if (loader) {
        FARPROC symbol = GetProcAddress(loader, "vkGetInstanceProcAddr");
        vk->get_instance_proc = (PFN_vkGetInstanceProcAddr)(void (*)(void))symbol;
    }
#else
    vk->loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (vk->loader) {
        vk->get_instance_proc = (PFN_vkGetInstanceProcAddr)dlsym(vk->loader, "vkGetInstanceProcAddr");
    }
#endif
    return vk->get_instance_proc != NULL;
}

void fg_vk_unload(fg_vk_device *vk) {
    if (!vk->loader) return;
#ifdef _WIN32
    FreeLibrary((HMODULE)vk->loader);
#else
    dlclose(vk->loader);
#endif
}
