#include "device.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

int fg_vk_load(fg_vk_device *vk) {
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
