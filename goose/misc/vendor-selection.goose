// P10-07 S3: which vendored directory is added, which component links it, and
// where the system copy wins over the in-tree one.
//
// Native sources at the base commit 70de805c, copied byte-identical into
// tests/native/ (upstream llama.cpp 0b5be7e4):
//
//   CMakeLists.txt:51        option(LLAMA_USE_SYSTEM_GGML "Use system libggml" OFF)
//   CMakeLists.txt:204-212   the system branch (find_package + alias) and the
//                            in-tree branch (add_subdirectory, skipped when a
//                            parent project already defines the target)
//   vendor/CMakeLists.txt    five unconditional add_subdirectory calls and
//                            cpp-httplib behind LLAMA_BUILD_COMMON
//   common/CMakeLists.txt:132        vendor::nlohmann vendor::sheredom
//   common/CMakeLists.txt:147        cpp-httplib, unconditionally, for the
//                                    download and http helpers
//   tools/mtmd/CMakeLists.txt:83     vendor::hash vendor::miniaudio vendor::stb
//                                    vendor::sheredom
//   tools/server/CMakeLists.txt:51   cpp-httplib
//
// The slice decides the resolution plan for ggml, the set of added vendor
// directories, the vendor libraries each component links, the closure of a
// given component selection, and the two mismatch masks that P10-07 audits:
// directories added but never linked, and libraries linked but never added.
//
// Not owned: CMake's target graph, find_package, the compiler and the link.

import std;

const SYS_HASH: i64 = 1;
const SYS_MINIAUDIO: i64 = 2;
const SYS_NLOHMANN: i64 = 4;
const SYS_SHEREDOM: i64 = 8;
const SYS_STB: i64 = 16;
const SYS_HTTPLIB: i64 = 32;

// CMakeLists.txt:204-212, evaluated as a plan instead of executed:
//   2  LLAMA_USE_SYSTEM_GGML is on: find_package(ggml REQUIRED) and alias it
//   1  no target yet and the system flag is off: add_subdirectory(ggml)
//   0  a parent project already provides the target: nothing is added here
export fn sys_ggml_plan(use_system: i64, target_exists: i64) -> i64 {
    if use_system != 0 {
        return 2;
    }
    if target_exists != 0 {
        return 0;
    }
    return 1;
}

// vendor/CMakeLists.txt — the five unconditional subdirectories, then
// cpp-httplib only when the common layer is built. The mask keeps file order.
export fn sys_vendor_dirs(build_common: i64) -> i64 {
    var m: i64 = SYS_HASH + SYS_MINIAUDIO + SYS_NLOHMANN + SYS_SHEREDOM + SYS_STB;
    if build_common != 0 {
        m = m + SYS_HTTPLIB;
    }
    return m;
}

// The vendor libraries one component links, from the native
// target_link_libraries lines: 1 llama-common, 2 mtmd, 3 llama-server-impl, 0
// the core library, which links none of them.
export fn sys_component_deps(component: i64) -> i64 {
    if component == 1 {
        return SYS_NLOHMANN + SYS_SHEREDOM + SYS_HTTPLIB;
    }
    if component == 2 {
        return SYS_HASH + SYS_MINIAUDIO + SYS_STB + SYS_SHEREDOM;
    }
    if component == 3 {
        return SYS_HTTPLIB;
    }
    return 0;
}

// Union over the components a build actually enables.
export fn sys_link_closure(common: i64, tools: i64, server: i64) -> i64 {
    // sheredom is linked by both common and mtmd, so the union must not add the
    // masks: addition would carry, a bit-wise or does not
    var m: i64 = 0;
    if common != 0 {
        m = sys_or(m, sys_component_deps(1));
    }
    if tools != 0 {
        m = sys_or(m, sys_component_deps(2));
    }
    if server != 0 {
        m = sys_or(m, sys_component_deps(3));
    }
    return m;
}

// Directories added but never linked: the build compiles a third-party source
// that no product code uses.
export fn sys_added_not_linked(dirs: i64, closure: i64) -> i64 {
    return sys_andnot(dirs, closure);
}

// Libraries linked but never added: a link error waiting for a configuration.
export fn sys_linked_not_added(dirs: i64, closure: i64) -> i64 {
    return sys_andnot(closure, dirs);
}

fn sys_or(a: i64, b: i64) -> i64 {
    var r: i64 = 0;
    var bit: i64 = 1;
    while bit <= SYS_HTTPLIB {
        if sys_has(a, bit) || sys_has(b, bit) {
            r = r + bit;
        }
        bit = bit * 2;
    }
    return r;
}

fn sys_andnot(a: i64, b: i64) -> i64 {
    var r: i64 = 0;
    var bit: i64 = 1;
    while bit <= SYS_HTTPLIB {
        if sys_has(a, bit) && !sys_has(b, bit) {
            r = r + bit;
        }
        bit = bit * 2;
    }
    return r;
}

fn sys_has(mask: i64, bit: i64) -> bool {
    return (mask / bit) % 2 != 0;
}
