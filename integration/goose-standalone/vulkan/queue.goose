const VK_QUEUE_OFFSET: u64 = 2032;
const VK_DEVICE_OFFSET: u64 = 2016;
const VK_QUEUE_SUBMIT_OFFSET: u64 = 2232;
const VK_WAIT_SEMAPHORES_OFFSET: u64 = 2240;
const VK_DEVICE_WAIT_OFFSET: u64 = 2248;
const VK_STRUCTURE_SUBMIT: u64 = 1000314004;
const VK_STRUCTURE_SIGNAL: u64 = 1000314005;
const VK_STRUCTURE_COMMAND: u64 = 1000314006;
const VK_STRUCTURE_WAIT: u64 = 1000207004;
const VK_STAGE_ALL_COMMANDS: u64 = 65536;

type VkQueueSubmit = c_fnptr(c_ptr<void>, u32, c_ptr<const void>, c_ptr<void>) -> i32;
type VkWaitSemaphores = c_fnptr(c_ptr<void>, c_ptr<const void>, u64) -> i32;
type VkDeviceWait = c_fnptr(c_ptr<void>) -> i32;

extern "memcpy" fn vk_queue_memcpy(destination: c_ptr<void>, source: c_ptr<const void>, bytes: u64) -> c_ptr<void>;
extern fn fg_vk_error(action: c_ptr<const i8>, status: i32) -> i32;

fn vk_queue_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    vk_queue_memcpy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn vk_queue_word(address: u64) -> u64 {
    var word: u64 = 0;
    vk_queue_memcpy(unsafe_c_address<c_ptr<void>>(word), unsafe_c_ptr<c_ptr<const void>>(address), 8);
    return word;
}

fn vk_queue_error(action: const u8[:], status: i32) -> i32 {
    var text: u8[32];
    for i in action.len { text[i] = action[i]; }
    text[action.len] = 0;
    let address = vk_queue_address(unsafe_c_address<c_ptr<const void>>(text[0]));
    return fg_vk_error(unsafe_c_ptr<c_ptr<const i8>>(address), status);
}

export fn fg_vk_submit_wait(vk: u64, cmd: u64, done: u64, value: u64) -> i32 {
    let command: u64[4] = [VK_STRUCTURE_COMMAND, 0, cmd, 0];
    let signal: u64[6] = [VK_STRUCTURE_SIGNAL, 0, done, value, VK_STAGE_ALL_COMMANDS, 0];
    let submit: u64[8] = [VK_STRUCTURE_SUBMIT, 0, 0, 0, 1,
        vk_queue_address(unsafe_c_address<c_ptr<const void>>(command[0])),
        1, vk_queue_address(unsafe_c_address<c_ptr<const void>>(signal[0]))];
    let queue = unsafe_c_ptr<c_ptr<void>>(vk_queue_word(vk + VK_QUEUE_OFFSET));
    let submit_result = unsafe_c_fnptr<VkQueueSubmit>(vk_queue_word(vk + VK_QUEUE_SUBMIT_OFFSET))(
        queue, 1, unsafe_c_address<c_ptr<const void>>(submit[0]), null);
    if submit_result != 0 { return vk_queue_error("submit command buffer", submit_result); }

    let wait: u64[5] = [VK_STRUCTURE_WAIT, 0, 0x100000000,
        vk_queue_address(unsafe_c_address<c_ptr<const void>>(done)),
        vk_queue_address(unsafe_c_address<c_ptr<const void>>(value))];
    let device = unsafe_c_ptr<c_ptr<void>>(vk_queue_word(vk + VK_DEVICE_OFFSET));
    let status = unsafe_c_fnptr<VkWaitSemaphores>(vk_queue_word(vk + VK_WAIT_SEMAPHORES_OFFSET))(
        device, unsafe_c_address<c_ptr<const void>>(wait[0]), 0xffffffffffffffff);
    if status == 0 { return 1; }
    unsafe_c_fnptr<VkDeviceWait>(vk_queue_word(vk + VK_DEVICE_WAIT_OFFSET))(device);
    return vk_queue_error("wait for command buffer", status);
}
