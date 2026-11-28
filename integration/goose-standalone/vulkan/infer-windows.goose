extern "_setmode" fn vk_setmode(fd: i32, mode: i32) -> i32;

fn vk_cli_binary_stdout() -> bool { return vk_setmode(1, 32768) != -1; }
