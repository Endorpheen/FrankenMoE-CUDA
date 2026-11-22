#include "platform.h"
#include "launcher-resources.h"

#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <winhttp.h>
#include <inttypes.h>
#include <stdio.h>
#include <wchar.h>

#define CAP 4096
#define MIB (1024ULL * 1024)
#define PORT 8080

struct gpu_probe {
    int driver_version, major, minor, status;
    size_t free_bytes, total_bytes;
};

static bool cuda_probe(struct gpu_probe *result) {
    HMODULE module = LoadLibraryExW(L"nvcuda.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) { result->status = -1; return false; }
    int (*init)(unsigned) = (void *)GetProcAddress(module, "cuInit");
    int (*version)(int *) = (void *)GetProcAddress(module, "cuDriverGetVersion");
    int (*device_get)(int *, int) = (void *)GetProcAddress(module, "cuDeviceGet");
    int (*cap)(int *, int *, int) = (void *)GetProcAddress(module, "cuDeviceComputeCapability");
    int (*create)(void **, unsigned, int) = (void *)GetProcAddress(module, "cuCtxCreate_v2");
    int (*memory)(size_t *, size_t *) = (void *)GetProcAddress(module, "cuMemGetInfo_v2");
    int (*destroy)(void *) = (void *)GetProcAddress(module, "cuCtxDestroy_v2");
    int device = 0;
    void *context = NULL;
    if (!init || !version || !device_get || !cap || !create || !memory || !destroy) {
        result->status = -2;
    } else if ((result->status = init(0)) == 0 &&
               (result->status = version(&result->driver_version)) == 0 &&
               (result->status = device_get(&device, 0)) == 0 &&
               (result->status = cap(&result->major, &result->minor, device)) == 0 &&
               (result->status = create(&context, 0, device)) == 0) {
        result->status = memory(&result->free_bytes, &result->total_bytes);
    }
    if (context) destroy(context);
    FreeLibrary(module);
    return result->status == 0;
}

static int fail(const wchar_t *message) {
    fwprintf(stderr, L"FrankenMoE: %ls\n", message);
    MessageBoxW(NULL, message, L"FrankenMoE Windows Alpha", MB_OK | MB_ICONERROR);
    return 1;
}

static bool directory(wchar_t *path, size_t size) {
    const DWORD n = GetModuleFileNameW(NULL, path, (DWORD)size);
    if (!n || n >= size) return false;
    wchar_t *separator = wcsrchr(path, L'\\');
    if (!separator) return false;
    *separator = 0;
    return true;
}

static bool locate_settings(wchar_t *path, size_t size) {
    wchar_t folder[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL,
                                SHGFP_TYPE_CURRENT, folder))) return false;
    wchar_t app[CAP];
    if (FAILED(StringCchPrintfW(app, CAP, L"%ls\\FrankenMoE", folder))) return false;
    if (!CreateDirectoryW(app, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    return SUCCEEDED(StringCchPrintfW(path, size, L"%ls\\model-path.txt", app));
}

enum startup_phase {
    STARTUP_NONE, STARTUP_DRIVER, STARTUP_VRAM, STARTUP_PORT,
    STARTUP_FILES, STARTUP_MODEL, STARTUP_PROCESS, STARTUP_READY, STARTUP_EXIT
};

struct startup_status { uint32_t magic, phase, status; };

static FILE *startup_log(const wchar_t *mode) {
    wchar_t path[CAP];
    if (!locate_settings(path, CAP)) return NULL;
    wchar_t *name = wcsrchr(path, L'\\');
    if (!name || FAILED(StringCchCopyW(name + 1, CAP - (size_t)(name + 1 - path),
                                        L"startup-status.bin"))) return NULL;
    return _wfopen(path, mode);
}

static void remember_status(enum startup_phase phase, DWORD status) {
    const struct startup_status entry = {UINT32_C(0x464d4f45), (uint32_t)phase, (uint32_t)status};
    FILE *out = startup_log(L"wb");
    if (out) {
        fwrite(&entry, sizeof entry, 1, out);
        fclose(out);
    }
}

static void report_startup_status(FILE *out) {
    struct startup_status entry;
    FILE *in = startup_log(L"rb");
    if (!in) return;
    const bool valid = fread(&entry, sizeof entry, 1, in) == 1 &&
        fgetc(in) == EOF && entry.magic == UINT32_C(0x464d4f45) && entry.phase <= STARTUP_EXIT;
    fclose(in);
    if (valid) fprintf(out, "last_startup_phase=%" PRIu32 "\nlast_startup_status=%" PRIu32 "\n",
                       entry.phase, entry.status);
}

static bool verify_model(const wchar_t *path) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    char header[8];
    DWORD got = 0;
    const bool valid = GetFileType(file) == FILE_TYPE_DISK &&
        ReadFile(file, header, sizeof header, &got, NULL) && got == sizeof header &&
        header[0] == 'G' && header[1] == 'G' && header[2] == 'U' && header[3] == 'F';
    CloseHandle(file);
    return valid;
}

static bool read_model(const wchar_t *settings, wchar_t *selected) {
    FILE *file = _wfopen(settings, L"rb");
    if (!file) return false;
    const size_t n = fread(selected, sizeof(*selected), CAP, file);
    const bool complete = n > 0 && n < CAP && feof(file);
    fclose(file);
    if (!complete) return false;
    selected[n] = 0;
    return wcslen(selected) == n && verify_model(selected);
}

static void save_model(const wchar_t *settings, const wchar_t *selected) {
    wchar_t temp[CAP];
    if (FAILED(StringCchPrintfW(temp, CAP, L"%ls.%lu.tmp", settings, GetCurrentProcessId()))) return;
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    const DWORD bytes = (DWORD)(wcslen(selected) * sizeof(*selected));
    DWORD written = 0;
    const bool saved = WriteFile(file, selected, bytes, &written, NULL) &&
        written == bytes && FlushFileBuffers(file);
    CloseHandle(file);
    if (!saved || !MoveFileExW(temp, settings, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        DeleteFileW(temp);
}

static bool choose_model(wchar_t *selected) {
    selected[0] = 0;
    OPENFILENAMEW picker = {.lStructSize = sizeof(picker),
        .lpstrFilter = L"GGUF model (*.gguf)\0*.gguf\0",
        .lpstrFile = selected, .nMaxFile = CAP,
        .lpstrTitle = L"Choose an Apex GGUF model for FrankenMoE",
        .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT};
    return GetOpenFileNameW(&picker);
}

enum profile { PROFILE_AUTO, PROFILE_ECONOMY, PROFILE_PERFORMANCE };

struct server_options {
    size_t cache;
    unsigned context, batch, threads, batch_threads;
};

static struct server_options server_settings(const struct gpu_probe *gpu, enum profile profile) {
    const size_t free = gpu->free_bytes / MIB;
    const bool plenty = free >= 12288 && fg_win_memory(true) / MIB >= 24576;
    const bool economy = profile == PROFILE_ECONOMY;
    const bool large = profile == PROFILE_PERFORMANCE && plenty;
    struct server_options out = {
        .context = economy ? 8192 : large ? 65536 : free >= 10240 ? 32768 : 16384,
        .batch = economy ? 1024 : free >= 10240 ? 4096 : 2048,
    };
    const size_t reserve = large ? 4096 : 3072;
    out.cache = free > reserve ? free - reserve : 0;
    const size_t cap = economy ? 3584 : 6290;
    if (out.cache > cap) out.cache = cap;
    DWORD_PTR available, system;
    unsigned cpus = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (GetProcessAffinityMask(GetCurrentProcess(), &available, &system) && available)
        cpus = (unsigned)__builtin_popcountll((unsigned long long)available);
    if (!cpus) cpus = 1;
    out.threads = cpus < 8 ? cpus : 8;
    const unsigned batch_max = economy ? 4 : 16;
    out.batch_threads = cpus < batch_max ? cpus : batch_max;
    return out;
}

static enum profile load_profile(void) {
    DWORD value = 0, bytes = sizeof value;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\FrankenMoE", L"WindowsAlphaProfile",
                     RRF_RT_REG_DWORD, NULL, &value, &bytes) != ERROR_SUCCESS ||
        value > PROFILE_PERFORMANCE) return PROFILE_AUTO;
    return (enum profile)value;
}

static bool save_profile(enum profile profile) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\FrankenMoE", 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) return false;
    const DWORD value = (DWORD)profile;
    const LSTATUS status = RegSetValueExW(key, L"WindowsAlphaProfile", 0, REG_DWORD,
                                          (const BYTE *)&value, sizeof value);
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

struct settings_dialog {
    const struct gpu_probe *gpu;
    enum profile profile;
};

static void preview_settings(HWND window, const struct settings_dialog *settings) {
    const struct server_options options = server_settings(settings->gpu, settings->profile);
    wchar_t display[320];
    if (settings->gpu->status != 0) {
        SetDlgItemTextW(window, IDC_PROFILE_DETAILS, L"NVIDIA CUDA driver not available. Settings can still be saved.");
        return;
    }
    if (SUCCEEDED(StringCchPrintfW(display, 320,
        L"Free VRAM: %zu MiB. Expert cache: %zu MiB.\r\nContext: %u tokens. Batch: %u.\r\nCPU threads: %u decode / %u prefill.",
        settings->gpu->free_bytes / MIB, options.cache, options.context, options.batch,
        options.threads, options.batch_threads)))
        SetDlgItemTextW(window, IDC_PROFILE_DETAILS, display);
}

static INT_PTR CALLBACK settings_window(HWND window, UINT message, WPARAM command, LPARAM payload) {
    if (message == WM_INITDIALOG) {
        struct settings_dialog *settings = (void *)payload;
        SetWindowLongPtrW(window, DWLP_USER, (LONG_PTR)settings);
        CheckRadioButton(window, IDC_AUTOMATIC, IDC_PERFORMANCE, IDC_AUTOMATIC + settings->profile);
        preview_settings(window, settings);
        return TRUE;
    }
    if (message != WM_COMMAND) return FALSE;
    struct settings_dialog *settings = (void *)GetWindowLongPtrW(window, DWLP_USER);
    switch (LOWORD(command)) {
        case IDC_AUTOMATIC:
        case IDC_ECONOMY:
        case IDC_PERFORMANCE:
            settings->profile = (enum profile)(LOWORD(command) - IDC_AUTOMATIC);
            preview_settings(window, settings);
            return TRUE;
        case IDC_PICK_MODEL: {
            wchar_t selected[CAP], path[CAP];
            if (!choose_model(selected)) return TRUE;
            if (!verify_model(selected) || !locate_settings(path, CAP)) {
                MessageBoxW(window, L"Choose a readable GGUF file.", L"FrankenMoE", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            save_model(path, selected);
            return TRUE;
        }
        case IDOK:
            if (!save_profile(settings->profile)) {
                MessageBoxW(window, L"Cannot save your profile.", L"FrankenMoE", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            EndDialog(window, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(window, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

static bool port_free(void) {
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) return false;
    const BOOL exclusive = TRUE;
    const struct sockaddr_in address = {.sin_family = AF_INET,
        .sin_port = htons(PORT), .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)}};
    const bool free_port = setsockopt(server, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
        (const char *)&exclusive, sizeof exclusive) == 0 &&
        bind(server, (const struct sockaddr *)&address, sizeof address) == 0;
    closesocket(server);
    return free_port;
}

static bool ready(void) {
    HINTERNET session = WinHttpOpen(L"FrankenMoE Windows Alpha", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    WinHttpSetTimeouts(session, 500, 500, 500, 1000);
    HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", L"/health",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0) : NULL;
    DWORD status = 0, size = sizeof status;
    if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, NULL)) {
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    }
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return status == 200;
}

static void diagnostics(FILE *out, const struct gpu_probe *gpu, const char *stage, DWORD status) {
    fprintf(out, "runtime=FrankenMoE-Pure-Goose-Windows-Alpha\nplatform=windows-x64\n"
        "cuda_driver_api=%d\ncompute_capability=%d.%d\ncuda_status=%d\n"
        "vram_total_mib=%" PRIu64 "\nvram_free_mib=%" PRIu64 "\n"
        "ram_available_mib=%" PRIu64 "\nstage=%s\nstatus=%lu\n",
        gpu->driver_version, gpu->major, gpu->minor, gpu->status,
        (uint64_t)(gpu->total_bytes / MIB), (uint64_t)(gpu->free_bytes / MIB),
        fg_win_memory(true) / MIB, stage, status);
}

int wmain(int argc, wchar_t **argv) {
    struct gpu_probe gpu = {0};
    const bool cuda = cuda_probe(&gpu);
    if (argc == 2 && wcscmp(argv[1], L"--settings") == 0) {
        struct settings_dialog settings = {&gpu, load_profile()};
        return DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_SETTINGS),
                               NULL, settings_window, (LPARAM)&settings) == -1 ? 1 : 0;
    }
    if (argc == 2 && wcscmp(argv[1], L"--diagnostics") == 0) {
        FILE *report = _wfopen(L"frankenmoe-diagnostics.txt", L"wb");
        if (!report) return fail(L"Cannot write the diagnostic report in this folder.");
        diagnostics(report, &gpu, "manual-probe", 0);
        report_startup_status(report);
        if (fclose(report) != 0) return fail(L"Cannot save the diagnostic report.");
        puts("Saved frankenmoe-diagnostics.txt. Review it before sharing.");
        return 0;
    }
    if (!cuda || gpu.major < 8 || gpu.driver_version < 12040) {
        remember_status(STARTUP_DRIVER, (DWORD)gpu.status);
        return fail(
        L"This preview requires an NVIDIA RTX GPU with CUDA 12.4 driver support. "
        L"Install or update the driver from nvidia.com, then start FrankenMoE again.");
    }
    if (gpu.free_bytes / MIB < 8192) {
        remember_status(STARTUP_VRAM, (DWORD)(gpu.free_bytes / MIB));
        return fail(L"At least 8 GiB of free NVIDIA VRAM are required. Close GPU-heavy apps and retry.");
    }
    if (!fg_win_network_ready() || !port_free()) {
        remember_status(STARTUP_PORT, (DWORD)WSAGetLastError());
        return fail(L"Cannot open localhost port 8080. Close the program using that port and retry.");
    }

    wchar_t root[CAP], executable[CAP], site[CAP], settings[CAP], model[CAP];
    if (!directory(root, CAP) ||
        FAILED(StringCchPrintfW(executable, CAP, L"%ls\\frankenmoe-server.exe", root)) ||
        FAILED(StringCchPrintfW(site, CAP, L"%ls\\web\\index.html", root)) ||
        GetFileAttributesW(executable) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(site) == INVALID_FILE_ATTRIBUTES)
        return fail(L"The server executable or web files are missing. Extract the full ZIP and retry.");
    if (!locate_settings(settings, CAP)) return fail(L"Cannot open the per-user settings folder.");
    const bool select_again = argc == 2 && wcscmp(argv[1], L"--select-model") == 0;
    if (select_again || !read_model(settings, model)) {
        if (!choose_model(model)) return 0;
        if (!verify_model(model)) return fail(L"The selected file is not a readable GGUF model.");
    }

    const struct server_options options = server_settings(&gpu, load_profile());
    wchar_t command[2 * CAP + 256];
    if (FAILED(StringCchPrintfW(command, 2 * CAP + 256,
        L"\"%ls\" --ctx-size %u --batch-size %u --ubatch-size %u "
        L"--cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib %zu "
        L"--threads %u --threads-batch %u \"%ls\" --serve 8080", executable,
        options.context, options.batch, options.batch, options.cache, options.threads, options.batch_threads, model)))
        return fail(L"The selected path is too long.");
    SetEnvironmentVariableW(L"GOOSE_CUDA", L"1");
    SetEnvironmentVariableW(L"GOOSE_MMVQ", L"1");
    SetEnvironmentVariableW(L"GOOSE_MMQ", L"1");
    SetEnvironmentVariableW(L"GOOSE_FLASH_MMA", L"1");
    SetEnvironmentVariableW(L"GOOSE_FLASH_VEC", L"1");
    SetEnvironmentVariableW(L"GOOSE_GDN_MMA", L"1");
    wchar_t *separator = wcsrchr(site, L'\\');
    if (!separator) return fail(L"Cannot locate web assets.");
    *separator = 0;
    SetEnvironmentVariableW(L"FRANKENMOE_WEB_ROOT", site);
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION child;
    if (!CreateProcessW(executable, command, NULL, NULL, FALSE, 0,
                        NULL, root, &startup, &child)) {
        const DWORD status = GetLastError();
        remember_status(STARTUP_PROCESS, status);
        diagnostics(stderr, &gpu, "server-create", status);
        return fail(L"Windows could not start the server. Extract the full ZIP and retry.");
    }
    CloseHandle(child.hThread);
    puts("Loading model. Keep this window open.");
    const ULONGLONG start = GetTickCount64();
    DWORD state;
    bool opened = false;
    while ((state = WaitForSingleObject(child.hProcess, 500)) == WAIT_TIMEOUT) {
        if (ready()) {
            save_model(settings, model);
            remember_status(STARTUP_READY, 0);
            if ((INT_PTR)ShellExecuteW(NULL, L"open", L"http://localhost:8080/",
                                        NULL, NULL, SW_SHOWNORMAL) <= 32)
                puts("FrankenMoE is ready at http://localhost:8080/.");
            opened = true;
            break;
        }
        if (GetTickCount64() - start > 300000) break;
    }
    if (!opened) {
        if (state == WAIT_TIMEOUT) {
            TerminateProcess(child.hProcess, 1);
            WaitForSingleObject(child.hProcess, 5000);
        }
        DWORD status = 1;
        GetExitCodeProcess(child.hProcess, &status);
        remember_status(STARTUP_READY, status);
        diagnostics(stderr, &gpu, "server-readiness", status);
        CloseHandle(child.hProcess);
        return fail(L"The model server did not become ready. Check the console and run collect-diagnostics.bat.");
    }
    WaitForSingleObject(child.hProcess, INFINITE);
    DWORD status = 1;
    GetExitCodeProcess(child.hProcess, &status);
    CloseHandle(child.hProcess);
    remember_status(STARTUP_EXIT, status);
    return status == 0 ? 0 : fail(L"The model server stopped. Run collect-diagnostics.bat for a report.");
}
