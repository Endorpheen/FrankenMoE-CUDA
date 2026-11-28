import re
import sys
from pathlib import Path


header, out = map(Path, sys.argv[1:])
commands = re.findall(r"X\((\w+)\)", header.read_text().split("typedef struct", 1)[0])
if not commands or commands[0] != "GetDeviceQueue":
    raise ValueError("Vulkan dispatch table is missing GetDeviceQueue")

body = ["fn vk_bind_device_commands(vk: u64, get: u64, device: u64) -> bool {"]
for slot, command in enumerate(commands):
    body.append(f'    if !vk_bind_device_command(vk, get, device, {slot}, "vk{command}") {{ return false; }}')
body.extend(["    return true;", "}"])
(out / "device-dispatch.goose").write_text("\n".join(body) + "\n")

checks = ['#include "driver-layout.h"']
for slot, command in enumerate(commands):
    checks.append(f'_Static_assert(offsetof(fg_vk_device, {command}) == {2056 + slot * 8}, "Vulkan dispatch {command}");')
(out / "device-layout.h").write_text("\n".join(checks) + "\n")
