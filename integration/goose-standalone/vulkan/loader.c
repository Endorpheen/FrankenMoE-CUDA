#include "device.h"

#include <stddef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
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
