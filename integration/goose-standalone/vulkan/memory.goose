const VK_MEMORY_PROPERTIES: u64 = 864;
const VK_MEMORY_CREATE: u64 = 2064;
const VK_MEMORY_REQUIREMENTS: u64 = 2072;
const VK_MEMORY_ALLOCATE: u64 = 2080;
const VK_MEMORY_BIND: u64 = 2088;
const VK_MEMORY_DESTROY: u64 = 2096;
const VK_MEMORY_FREE: u64 = 2104;
const VK_MEMORY_MAP: u64 = 2112;
const VK_MEMORY_UNMAP: u64 = 2120;
const VK_MEMORY_FLUSH: u64 = 2128;
const VK_MEMORY_INVALIDATE: u64 = 2136;
const VK_MEMORY_WHOLE: u64 = 0xffffffffffffffff;

type VkBufferRequirements = c_fnptr(u64, u64, u64) -> void;
type VkBufferBind = c_fnptr(u64, u64, u64, u64) -> i32;
type VkBufferMap = c_fnptr(u64, u64, u64, u64, u32, u64) -> i32;
type VkBufferUnmap = c_fnptr(u64, u64) -> void;
type VkBufferSync = c_fnptr(u64, u32, u64) -> i32;

fn vk_memory_u32(address: u64) -> u32 {
    var value: u32 = 0;
    vk_queue_memcpy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 4);
    return value;
}

fn vk_memory_clear(buffer: u64) {
    let empty: u64[4] = [0, 0, 0, 0];
    vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(buffer), unsafe_c_address<c_ptr<const void>>(empty[0]), 32);
}

export fn fg_vk_free_buffer(vk: u64, buffer: u64) {
    let device = vk_queue_word(vk + VK_DEVICE_OFFSET);
    let handle = vk_queue_word(buffer);
    let memory = vk_queue_word(buffer + 8);
    if handle != 0 { unsafe_c_fnptr<VkUploadDestroy>(vk_queue_word(vk + VK_MEMORY_DESTROY))(device, handle, 0); }
    if memory != 0 { unsafe_c_fnptr<VkUploadDestroy>(vk_queue_word(vk + VK_MEMORY_FREE))(device, memory, 0); }
    vk_memory_clear(buffer);
}

fn vk_memory_type(properties: u64, bits: u32, required: u32, preferred: u32) -> u64 {
    var selected: u64 = 0xffffffff;
    let count = vk_memory_u32(properties) as! u64;
    for i in (count as! i64) {
        let index = i as! u64;
        let flags = vk_memory_u32(properties + 4 + index * 8);
        if (bits & ((1 as! u32) << (index as! u32))) == 0 || (flags & required) != required { continue; }
        if (flags & preferred) == preferred { return index; }
        if selected == 0xffffffff { selected = index; }
    }
    return selected;
}

export fn fg_vk_alloc_buffer(vk: u64, buffer: u64, size: u64, usage: u32, required: u32, preferred: u32) -> i32 {
    vk_memory_clear(buffer);
    if size == 0 { return -3; }
    let device = vk_queue_word(vk + VK_DEVICE_OFFSET);
    let info: u64[7] = [12, 0, 0, size, usage as! u64, 0, 0];
    var result = unsafe_c_fnptr<VkUploadCreate>(vk_queue_word(vk + VK_MEMORY_CREATE))(
        device, vk_queue_address(unsafe_c_address<c_ptr<const void>>(info[0])), 0, buffer);
    if result != 0 { return result; }

    var requirements: u64[3];
    unsafe_c_fnptr<VkBufferRequirements>(vk_queue_word(vk + VK_MEMORY_REQUIREMENTS))(
        device, vk_queue_word(buffer), vk_queue_address(unsafe_c_address<c_ptr<const void>>(requirements[0])));
    let properties = vk + VK_MEMORY_PROPERTIES;
    let index = vk_memory_type(properties, requirements[2] as! u32, required, preferred);
    if index == 0xffffffff {
        fg_vk_free_buffer(vk, buffer);
        return -8;
    }

    let alloc: u64[4] = [5, 0, requirements[0], index];
    result = unsafe_c_fnptr<VkUploadCreate>(vk_queue_word(vk + VK_MEMORY_ALLOCATE))(
        device, vk_queue_address(unsafe_c_address<c_ptr<const void>>(alloc[0])), 0, buffer + 8);
    if result == 0 {
        result = unsafe_c_fnptr<VkBufferBind>(vk_queue_word(vk + VK_MEMORY_BIND))(
            device, vk_queue_word(buffer), vk_queue_word(buffer + 8), 0);
    }
    if result != 0 {
        fg_vk_free_buffer(vk, buffer);
        return result;
    }
    let flags = vk_memory_u32(properties + 4 + index * 8) as! u64;
    vk_upload_put(buffer + 16, flags | ((usage as! u64) << 32));
    vk_upload_put(buffer + VK_BUFFER_SIZE, size);
    return 0;
}

export fn fg_vk_access_buffer(vk: u64, buffer: u64, offset: u64, data: u64, size: u64, write: i32) -> i32 {
    let capacity = vk_queue_word(buffer + VK_BUFFER_SIZE);
    let flags = vk_memory_u32(buffer + 16);
    if data == 0 || size == 0 || offset > capacity || size > capacity - offset || (flags & 2) == 0 { return -5; }
    let coherent = (flags & 4) != 0;
    let device = vk_queue_word(vk + VK_DEVICE_OFFSET);
    let memory = vk_queue_word(buffer + 8);
    var base: u64 = 0;
    var result = unsafe_c_fnptr<VkBufferMap>(vk_queue_word(vk + VK_MEMORY_MAP))(
        device, memory, 0, VK_MEMORY_WHOLE, 0, vk_queue_address(unsafe_c_address<c_ptr<const void>>(base)));
    if result != 0 { return result; }
    let range: u64[5] = [6, 0, memory, 0, VK_MEMORY_WHOLE];
    let range_address = vk_queue_address(unsafe_c_address<c_ptr<const void>>(range[0]));
    let address = base + offset;
    if write != 0 {
        vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_ptr<c_ptr<const void>>(data), size);
        if !coherent {
            result = unsafe_c_fnptr<VkBufferSync>(vk_queue_word(vk + VK_MEMORY_FLUSH))(device, 1, range_address);
        }
    } else {
        if !coherent {
            result = unsafe_c_fnptr<VkBufferSync>(vk_queue_word(vk + VK_MEMORY_INVALIDATE))(device, 1, range_address);
        }
        if result == 0 {
            vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(data), unsafe_c_ptr<c_ptr<const void>>(address), size);
        }
    }
    unsafe_c_fnptr<VkBufferUnmap>(vk_queue_word(vk + VK_MEMORY_UNMAP))(device, memory);
    return result;
}
