const VK_DEV_INSTANCE: u64 = 16;
const VK_DEV_DESTROY_INSTANCE: u64 = 24;
const VK_DEV_PHYSICAL: u64 = 32;
const VK_DEV_PROPERTIES: u64 = 40;
const VK_DEV_DRIVER: u64 = 1384;
const VK_DEV_ID: u64 = 1920;
const VK_DEV_SUBGROUP: u64 = 1984;
const VK_DEV_DESTROY_DEVICE: u64 = 2024;
const VK_DEV_INDEX: u64 = 2040;
const VK_DEV_FAMILY: u64 = 2044;
const VK_DEV_TIMESTAMP_BITS: u64 = 2048;
const VK_DEV_COMMANDS: u64 = 2056;
const VK_DEV_SIZE: u64 = 2392;
const VK_AUTO_DEVICE: u32 = 0xffffffff;
const VK_API_1_3: u32 = 4206592;

type VkGetProc = c_fnptr(u64, u64) -> u64;
type VkDestroy = c_fnptr(u64, u64) -> void;
type VkLoaderVersion = c_fnptr(u64) -> i32;
type VkCreateInstanceCall = c_fnptr(u64, u64, u64) -> i32;
type VkEnumerateDevices = c_fnptr(u64, u64, u64) -> i32;
type VkGetDeviceInfo = c_fnptr(u64, u64) -> void;
type VkGetQueues = c_fnptr(u64, u64, u64) -> void;
type VkGetQueue = c_fnptr(u64, u32, u32, u64) -> void;

extern "memset" fn vk_device_zero(data: c_ptr<void>, value: i32, size: u64) -> c_ptr<void>;
extern "calloc" fn vk_device_alloc(count: u64, size: u64) -> c_ptr<void>;
extern "free" fn vk_device_free(data: c_ptr<void>);
extern "gs_os_write_stderr" fn vk_device_stderr(message: const u8[:]);

struct VkInstanceAPI {
    enumerate: u64,
    properties: u64,
    features: u64,
    queues: u64,
    memory: u64,
    create: u64,
    device_proc: u64
}

fn vk_device_addr(pointer: c_ptr<void>) -> u64 {
    var address: u64 = 0;
    vk_queue_memcpy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn vk_device_clear(vk: u64) {
    vk_device_zero(unsafe_c_ptr<c_ptr<void>>(vk), 0, VK_DEV_SIZE);
}

fn vk_device_field(vk: u64, field: u64, value: u32) {
    vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(vk + field), unsafe_c_address<c_ptr<const void>>(value), 4);
}

fn vk_device_copy(to: u64, from: u64, count: u64) {
    vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(to), unsafe_c_ptr<c_ptr<const void>>(from), count);
}

fn vk_device_message(action: const u8[:], result: i32) -> i32 {
    vk_device_stderr(str("Vulkan: ", action, " (", result, ")\n"));
    return 0;
}

export fn fg_vk_error(action: u64, result: i32) -> i32 {
    var message: u8[>..] = [];
    var at: u64 = 0;
    while true {
        var byte: u8 = 0;
        vk_queue_memcpy(unsafe_c_address<c_ptr<void>>(byte), unsafe_c_ptr<c_ptr<const void>>(action + at), 1);
        if byte == 0 { break; }
        message.push(byte);
        at += 1;
    }
    return vk_device_message(message, result);
}

fn vk_device_proc(get: u64, owner: u64, name: const u8[:]) -> u64 {
    var key: u8[64];
    for i in name.len { key[i] = name[i]; }
    key[name.len] = 0;
    let symbol = unsafe_c_fnptr<VkGetProc>(get)(owner, vk_queue_address(unsafe_c_address<c_ptr<const void>>(key[0])));
    if owner != 0 && symbol == 0 { vk_device_message(str("missing ", name), -3); }
    return symbol;
}

export fn fg_vk_close(vk: u64) {
    let device = vk_queue_word(vk + VK_DEVICE_OFFSET);
    let free_device = vk_queue_word(vk + VK_DEV_DESTROY_DEVICE);
    if device != 0 && free_device != 0 { unsafe_c_fnptr<VkDestroy>(free_device)(device, 0); }
    let instance = vk_queue_word(vk + VK_DEV_INSTANCE);
    let free_instance = vk_queue_word(vk + VK_DEV_DESTROY_INSTANCE);
    if instance != 0 && free_instance != 0 { unsafe_c_fnptr<VkDestroy>(free_instance)(instance, 0); }
    fg_vk_unload(vk);
    vk_device_clear(vk);
}

fn vk_choose_queue(api: VkInstanceAPI, physical: u64, vk: u64) -> bool {
    var count: u32 = 0;
    let count_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(count));
    let queues = unsafe_c_fnptr<VkGetQueues>(api.queues);
    queues(physical, count_addr, 0);
    if count == 0 { return false; }
    let buffer = vk_device_alloc(count as! u64, 24);
    if buffer == null { return false; }
    let list = vk_device_addr(buffer);
    queues(physical, count_addr, list);
    var found = false;
    for i in (count as! i64) {
        let entry = list + (i as! u64) * 24;
        let flags = vk_memory_u32(entry);
        if vk_memory_u32(entry + 4) == 0 || (flags & 2) == 0 { continue; }
        let dedicated = (flags & 1) == 0;
        if !found || dedicated {
            vk_device_field(vk, VK_DEV_FAMILY, i as! u32);
            vk_device_field(vk, VK_DEV_TIMESTAMP_BITS, vk_memory_u32(entry + 8));
            found = true;
        }
        if dedicated { break; }
    }
    vk_device_free(buffer);
    return found;
}

fn vk_select_physical(vk: u64, api: VkInstanceAPI, physical: u64, index: u32, automatic: bool, allow_cpu: bool) -> bool {
    var subgroup: u64[4] = [1000094000, 0, 0, 0];
    let subgroup_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(subgroup[0]));
    var id: u64[8] = [0; 8];
    id[0] = 1000071004;
    id[1] = subgroup_addr;
    let id_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(id[0]));
    var driver: u64[67] = [0; 67];
    driver[0] = 1000196000;
    driver[1] = id_addr;
    let driver_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(driver[0]));
    var properties: u64[105] = [0; 105];
    properties[0] = 1000059001;
    properties[1] = driver_addr;
    let props_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(properties[0]));
    unsafe_c_fnptr<VkGetDeviceInfo>(api.properties)(physical, props_addr);
    let body = props_addr + 16;
    if vk_memory_u32(body) < VK_API_1_3 { return false; }
    let kind = vk_memory_u32(body + 16);
    if kind == 4 && (!allow_cpu || automatic) { return false; }
    if automatic && (vk_memory_u32(body + 8) != 0x1002 || (kind != 2 && kind != 1)) { return false; }

    var f13: u64[10] = [0; 10];
    f13[0] = 53;
    let f13_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(f13[0]));
    var f12: u64[26] = [0; 26];
    f12[0] = 51;
    f12[1] = f13_addr;
    let f12_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(f12[0]));
    var features: u64[30] = [0; 30];
    features[0] = 1000059000;
    features[1] = f12_addr;
    unsafe_c_fnptr<VkGetDeviceInfo>(api.features)(physical, vk_queue_address(unsafe_c_address<c_ptr<const void>>(features[0])));
    if vk_memory_u32(f12_addr + 164) == 0 || vk_memory_u32(f13_addr + 52) == 0 ||
        !vk_choose_queue(api, physical, vk) { return false; }

    vk_upload_put(vk + VK_DEV_PHYSICAL, physical);
    vk_device_field(vk, VK_DEV_INDEX, index);
    vk_device_copy(vk + VK_DEV_PROPERTIES, body, 824);
    subgroup[1] = 0;
    id[1] = 0;
    driver[1] = 0;
    vk_device_copy(vk + VK_DEV_DRIVER, driver_addr, 536);
    vk_device_copy(vk + VK_DEV_ID, id_addr, 64);
    vk_device_copy(vk + VK_DEV_SUBGROUP, subgroup_addr, 32);
    unsafe_c_fnptr<VkGetDeviceInfo>(api.memory)(physical, vk + VK_MEMORY_PROPERTIES);
    return true;
}

fn vk_find_physical(vk: u64, api: VkInstanceAPI, requested: u32, allow_cpu: bool) -> bool {
    let instance = vk_queue_word(vk + VK_DEV_INSTANCE);
    let enumerate = unsafe_c_fnptr<VkEnumerateDevices>(api.enumerate);
    var count: u32 = 0;
    let count_addr = vk_queue_address(unsafe_c_address<c_ptr<const void>>(count));
    var result = enumerate(instance, count_addr, 0);
    if result != 0 { vk_device_message("enumerate physical devices", result); return false; }
    if count == 0 { vk_device_message("no physical device", -3); return false; }
    let buffer = vk_device_alloc(count as! u64, 8);
    if buffer == null { vk_device_message("physical device list allocation failed", -1); return false; }
    let list = vk_device_addr(buffer);
    result = enumerate(instance, count_addr, list);
    if result != 0 {
        vk_device_free(buffer);
        vk_device_message("enumerate physical devices", result);
        return false;
    }
    let automatic = requested == VK_AUTO_DEVICE;
    var found = false;
    for i in (count as! i64) {
        let index = i as! u32;
        if !automatic && index != requested { continue; }
        if vk_select_physical(vk, api, vk_queue_word(list + (index as! u64) * 8), index, automatic, allow_cpu) {
            found = true;
            break;
        }
    }
    vk_device_free(buffer);
    if !found {
        vk_device_message(if automatic { "no eligible AMD Vulkan 1.3 device" } else {
            "requested device is absent or does not meet the Vulkan 1.3 requirements"
        }, -8);
    }
    return found;
}

fn vk_bind_device_command(vk: u64, get: u64, device: u64, slot: u64, name: const u8[:]) -> bool {
    let symbol = vk_device_proc(get, device, name);
    vk_upload_put(vk + VK_DEV_COMMANDS + slot * 8, symbol);
    return symbol != 0;
}

fn vk_create_device(vk: u64, api: VkInstanceAPI) -> bool {
    let priority: f32 = 1.0 as! f32;
    let family = vk_memory_u32(vk + VK_DEV_FAMILY) as! u64;
    let queue: u64[5] = [2, 0, family << 32, 1, vk_queue_address(unsafe_c_address<c_ptr<const void>>(priority))];
    var f13: u64[10] = [0; 10];
    f13[0] = 53;
    f13[6] = 0x100000000;
    var f12: u64[26] = [0; 26];
    f12[0] = 51;
    f12[1] = vk_queue_address(unsafe_c_address<c_ptr<const void>>(f13[0]));
    f12[20] = 0x100000000;
    let info: u64[9] = [3, vk_queue_address(unsafe_c_address<c_ptr<const void>>(f12[0])), 0x100000000,
        vk_queue_address(unsafe_c_address<c_ptr<const void>>(queue[0])), 0, 0, 0, 0, 0];
    let result = unsafe_c_fnptr<VkUploadCreate>(api.create)(vk_queue_word(vk + VK_DEV_PHYSICAL),
        vk_queue_address(unsafe_c_address<c_ptr<const void>>(info[0])), 0, vk + VK_DEVICE_OFFSET);
    if result != 0 { vk_device_message("create compute device", result); return false; }
    let device = vk_queue_word(vk + VK_DEVICE_OFFSET);
    let get = api.device_proc;
    if !vk_bind_device_commands(vk, get, device) { return false; }
    unsafe_c_fnptr<VkGetQueue>(vk_queue_word(vk + VK_DEV_COMMANDS))(
        device, family as! u32, 0, vk + VK_QUEUE_OFFSET);
    return true;
}

fn vk_create_instance(vk: u64) -> bool {
    if fg_vk_load(vk) == 0 {
        vk_device_message("load Vulkan or resolve vkGetInstanceProcAddr", -3);
        return false;
    }
    let get = vk_queue_word(vk + VK_LOADER_ENTRY);
    let version = vk_device_proc(get, 0, "vkEnumerateInstanceVersion");
    let create = vk_device_proc(get, 0, "vkCreateInstance");
    var api_version: u32 = 0;
    if create == 0 || version == 0 ||
        unsafe_c_fnptr<VkLoaderVersion>(version)(vk_queue_address(unsafe_c_address<c_ptr<const void>>(api_version))) != 0 ||
        api_version < VK_API_1_3 {
        vk_device_message("Vulkan 1.3 loader required", -9);
        return false;
    }
    let app_name: u8[11] = [70,114,97,110,107,101,110,77,111,69,0];
    let app: u64[6] = [0, 0, vk_queue_address(unsafe_c_address<c_ptr<const void>>(app_name[0])),
        0, 0, (VK_API_1_3 as! u64) << 32];
    let info: u64[8] = [1, 0, 0, vk_queue_address(unsafe_c_address<c_ptr<const void>>(app[0])), 0, 0, 0, 0];
    let result = unsafe_c_fnptr<VkCreateInstanceCall>(create)(
        vk_queue_address(unsafe_c_address<c_ptr<const void>>(info[0])), 0, vk + VK_DEV_INSTANCE);
    if result != 0 { vk_device_message("create instance", result); return false; }
    return true;
}

fn vk_device_ready(vk: u64, requested: u32, allow_cpu: bool) -> bool {
    if !vk_create_instance(vk) { return false; }
    let instance = vk_queue_word(vk + VK_DEV_INSTANCE);
    let get = vk_queue_word(vk + VK_LOADER_ENTRY);
    let destroy_instance = vk_device_proc(get, instance, "vkDestroyInstance");
    if destroy_instance == 0 { return false; }
    vk_upload_put(vk + VK_DEV_DESTROY_INSTANCE, destroy_instance);
    let destroy_device = vk_device_proc(get, instance, "vkDestroyDevice");
    if destroy_device == 0 { return false; }
    vk_upload_put(vk + VK_DEV_DESTROY_DEVICE, destroy_device);
    let enumerate = vk_device_proc(get, instance, "vkEnumeratePhysicalDevices");
    if enumerate == 0 { return false; }
    let properties = vk_device_proc(get, instance, "vkGetPhysicalDeviceProperties2");
    if properties == 0 { return false; }
    let features = vk_device_proc(get, instance, "vkGetPhysicalDeviceFeatures2");
    if features == 0 { return false; }
    let queues = vk_device_proc(get, instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    if queues == 0 { return false; }
    let memory = vk_device_proc(get, instance, "vkGetPhysicalDeviceMemoryProperties");
    if memory == 0 { return false; }
    let create = vk_device_proc(get, instance, "vkCreateDevice");
    if create == 0 { return false; }
    let device_proc = vk_device_proc(get, instance, "vkGetDeviceProcAddr");
    if device_proc == 0 { return false; }
    let api = VkInstanceAPI { enumerate: enumerate, properties: properties, features: features,
        queues: queues, memory: memory, create: create, device_proc: device_proc };
    return vk_find_physical(vk, api, requested, allow_cpu) && vk_create_device(vk, api);
}

export fn fg_vk_open(vk: u64, requested: u32, allow_cpu: i32) -> i32 {
    vk_device_clear(vk);
    if vk_device_ready(vk, requested, allow_cpu != 0) { return 1; }
    fg_vk_close(vk);
    return 0;
}
