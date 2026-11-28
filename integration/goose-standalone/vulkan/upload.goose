const VK_UPLOAD_QUEUE_INDEX: u64 = 2040;
const VK_UPLOAD_CREATE_POOL: u64 = 2144;
const VK_UPLOAD_DESTROY_POOL: u64 = 2152;
const VK_UPLOAD_ALLOCATE_COMMAND: u64 = 2160;
const VK_UPLOAD_RESET_POOL: u64 = 2168;
const VK_UPLOAD_BEGIN_COMMAND: u64 = 2176;
const VK_UPLOAD_END_COMMAND: u64 = 2184;
const VK_UPLOAD_COPY_BUFFER: u64 = 2192;
const VK_UPLOAD_FILL_BUFFER: u64 = 2200;
const VK_UPLOAD_BARRIER: u64 = 2208;
const VK_UPLOAD_CREATE_SEMAPHORE: u64 = 2216;
const VK_UPLOAD_DESTROY_SEMAPHORE: u64 = 2224;

const VK_UPLOAD_STAGING: u64 = 8;
const VK_UPLOAD_POOL: u64 = 40;
const VK_UPLOAD_COMMAND: u64 = 48;
const VK_UPLOAD_DONE: u64 = 56;
const VK_UPLOAD_EPOCH: u64 = 64;
const VK_BUFFER_SIZE: u64 = 24;
const VK_BUFFER_USAGE: u64 = 20;

type VkUploadCreate = c_fnptr(u64, u64, u64, u64) -> i32;
type VkUploadDestroy = c_fnptr(u64, u64, u64) -> void;
type VkUploadAllocate = c_fnptr(u64, u64, u64) -> i32;
type VkUploadReset = c_fnptr(u64, u64, u32) -> i32;
type VkUploadBegin = c_fnptr(u64, u64) -> i32;
type VkUploadEnd = c_fnptr(u64) -> i32;
type VkUploadBarrier = c_fnptr(u64, u64) -> void;
type VkUploadCopy = c_fnptr(u64, u64, u64, u32, u64) -> void;
type VkUploadFill = c_fnptr(u64, u64, u64, u64, u32) -> void;


fn vk_upload_put(address: u64, value: u64) {
    vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 8);
}

fn vk_upload_usage(buffer: u64) -> u32 {
    var flags: u32 = 0;
    vk_queue_memcpy(unsafe_c_address<c_ptr<void>>(flags), unsafe_c_ptr<c_ptr<const void>>(buffer + VK_BUFFER_USAGE), 4);
    return flags;
}

fn vk_upload_clear(upload: u64, vk: u64) {
    let state: u64[9] = [vk, 0, 0, 0, 0, 0, 0, 0, 0];
    vk_queue_memcpy(unsafe_c_ptr<c_ptr<void>>(upload), unsafe_c_address<c_ptr<const void>>(state[0]), 72);
}

export fn fg_vk_upload_close(upload: u64) {
    let vk = vk_queue_word(upload);
    if vk == 0 { return; }
    let device = vk_queue_word(vk + VK_DEVICE_OFFSET);
    let pool = vk_queue_word(upload + VK_UPLOAD_POOL);
    let done = vk_queue_word(upload + VK_UPLOAD_DONE);
    if pool != 0 { unsafe_c_fnptr<VkUploadDestroy>(vk_queue_word(vk + VK_UPLOAD_DESTROY_POOL))(device, pool, 0); }
    if done != 0 { unsafe_c_fnptr<VkUploadDestroy>(vk_queue_word(vk + VK_UPLOAD_DESTROY_SEMAPHORE))(device, done, 0); }
    fg_vk_free_buffer(vk, upload + VK_UPLOAD_STAGING);
    vk_upload_clear(upload, 0);
}

export fn fg_vk_upload_open(upload: u64, vk: u64, capacity: u64) -> i32 {
    vk_upload_clear(upload, vk);
    let device = vk_queue_word(vk + VK_DEVICE_OFFSET);
    let family = vk_queue_word(vk + VK_UPLOAD_QUEUE_INDEX) >> 32;
    let pool: u64[3] = [39, 0, 1 | (family << 32)];
    let timeline: u64[4] = [1000207002, 0, 1, 0];
    let done: u64[3] = [9, vk_queue_address(unsafe_c_address<c_ptr<const void>>(timeline[0])), 0];
    var result = fg_vk_alloc_buffer(vk, upload + VK_UPLOAD_STAGING, capacity, 1, 2, 4);
    if result == 0 {
        result = unsafe_c_fnptr<VkUploadCreate>(vk_queue_word(vk + VK_UPLOAD_CREATE_POOL))(
            device, vk_queue_address(unsafe_c_address<c_ptr<const void>>(pool[0])), 0, upload + VK_UPLOAD_POOL);
    }
    if result == 0 {
        let cmd: u64[4] = [40, 0, vk_queue_word(upload + VK_UPLOAD_POOL), 0x100000000];
        result = unsafe_c_fnptr<VkUploadAllocate>(vk_queue_word(vk + VK_UPLOAD_ALLOCATE_COMMAND))(
            device, vk_queue_address(unsafe_c_address<c_ptr<const void>>(cmd[0])), upload + VK_UPLOAD_COMMAND);
    }
    if result == 0 {
        result = unsafe_c_fnptr<VkUploadCreate>(vk_queue_word(vk + VK_UPLOAD_CREATE_SEMAPHORE))(
            device, vk_queue_address(unsafe_c_address<c_ptr<const void>>(done[0])), 0, upload + VK_UPLOAD_DONE);
    }
    if result == 0 { return 1; }
    fg_vk_upload_close(upload);
    return vk_queue_error("create weight upload queue", result);
}

fn vk_upload_begin(upload: u64, vk: u64, cmd: u64) -> bool {
    var result = unsafe_c_fnptr<VkUploadReset>(vk_queue_word(vk + VK_UPLOAD_RESET_POOL))(
        vk_queue_word(vk + VK_DEVICE_OFFSET), vk_queue_word(upload + VK_UPLOAD_POOL), 0);
    if result == 0 {
        let begin: u64[4] = [42, 0, 1, 0];
        result = unsafe_c_fnptr<VkUploadBegin>(vk_queue_word(vk + VK_UPLOAD_BEGIN_COMMAND))(
            cmd, vk_queue_address(unsafe_c_address<c_ptr<const void>>(begin[0])));
    }
    if result == 0 { return true; }
    vk_queue_error("begin weight upload", result);
    return false;
}

fn vk_upload_finish(upload: u64, vk: u64, cmd: u64, dest: u64, offset: u64, size: u64) -> i32 {
    let barrier: u64[10] = [1000314001, 0, 4096, 4096, 2048, 96, 0xffffffffffffffff, dest, offset, size];
    let deps: u64[8] = [1000314003, 0, 0, 0, 1,
        vk_queue_address(unsafe_c_address<c_ptr<const void>>(barrier[0])), 0, 0];
    unsafe_c_fnptr<VkUploadBarrier>(vk_queue_word(vk + VK_UPLOAD_BARRIER))(
        cmd, vk_queue_address(unsafe_c_address<c_ptr<const void>>(deps[0])));
    let result = unsafe_c_fnptr<VkUploadEnd>(vk_queue_word(vk + VK_UPLOAD_END_COMMAND))(cmd);
    if result != 0 { return vk_queue_error("end weight upload", result); }
    let epoch = vk_queue_word(upload + VK_UPLOAD_EPOCH) + 1;
    vk_upload_put(upload + VK_UPLOAD_EPOCH, epoch);
    return fg_vk_submit_wait(vk, cmd, vk_queue_word(upload + VK_UPLOAD_DONE), epoch);
}

export fn fg_vk_upload_write(upload: u64, dest: u64, offset: u64, data: u64, size: u64) -> i32 {
    let dest_size = vk_queue_word(dest + VK_BUFFER_SIZE);
    let staging = upload + VK_UPLOAD_STAGING;
    if size == 0 || ((size | offset) & 3) != 0 || (vk_upload_usage(dest) & 2) == 0 ||
        offset > dest_size || size > dest_size - offset || size > vk_queue_word(staging + VK_BUFFER_SIZE) ||
        vk_queue_word(upload + VK_UPLOAD_EPOCH) == 0xffffffffffffffff { return 0; }
    let vk = vk_queue_word(upload);
    let result = fg_vk_access_buffer(vk, staging, 0, data, size, 1);
    if result != 0 { return vk_queue_error("stage weight upload", result); }
    let cmd = vk_queue_word(upload + VK_UPLOAD_COMMAND);
    if !vk_upload_begin(upload, vk, cmd) { return 0; }
    let copy: u64[3] = [0, offset, size];
    let buffer = vk_queue_word(dest);
    unsafe_c_fnptr<VkUploadCopy>(vk_queue_word(vk + VK_UPLOAD_COPY_BUFFER))(
        cmd, vk_queue_word(staging), buffer, 1, vk_queue_address(unsafe_c_address<c_ptr<const void>>(copy[0])));
    return vk_upload_finish(upload, vk, cmd, buffer, offset, size);
}

export fn fg_vk_upload_zero(upload: u64, dest: u64) -> i32 {
    let size = vk_queue_word(dest + VK_BUFFER_SIZE);
    if size == 0 || (size & 3) != 0 || (vk_upload_usage(dest) & 2) == 0 ||
        vk_queue_word(upload + VK_UPLOAD_EPOCH) == 0xffffffffffffffff { return 0; }
    let vk = vk_queue_word(upload);
    let cmd = vk_queue_word(upload + VK_UPLOAD_COMMAND);
    if !vk_upload_begin(upload, vk, cmd) { return 0; }
    let buffer = vk_queue_word(dest);
    unsafe_c_fnptr<VkUploadFill>(vk_queue_word(vk + VK_UPLOAD_FILL_BUFFER))(cmd, buffer, 0, size, 0);
    return vk_upload_finish(upload, vk, cmd, buffer, 0, size);
}
