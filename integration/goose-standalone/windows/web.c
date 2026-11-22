#include "platform.h"
#include "main.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static bool send_all(SOCKET socket, const void *bytes, size_t count) {
    const char *data = bytes;
    while (count) {
        const int request = count > 65536 ? 65536 : (int)count;
        const int written = send(socket, data, request, 0);
        if (written <= 0) return false;
        data += written;
        count -= (size_t)written;
    }
    return true;
}

static bool header(SOCKET socket, const char *mime, uint64_t bytes) {
    char text[256];
    const int size = snprintf(text, sizeof text, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
        "Content-Length: %llu\r\nX-Content-Type-Options: nosniff\r\n"
        "Cache-Control: no-cache\r\nConnection: close\r\n\r\n", mime, (unsigned long long)bytes);
    return size > 0 && (size_t)size < sizeof text && send_all(socket, text, (size_t)size);
}

static const char *mime_type(const wchar_t *path) {
    const wchar_t *extension = wcsrchr(path, L'.');
    if (!extension) return "application/octet-stream";
    if (!wcscmp(extension, L".html")) return "text/html; charset=utf-8";
    if (!wcscmp(extension, L".js")) return "text/javascript; charset=utf-8";
    if (!wcscmp(extension, L".css")) return "text/css; charset=utf-8";
    if (!wcscmp(extension, L".svg")) return "image/svg+xml";
    if (!wcscmp(extension, L".png")) return "image/png";
    if (!wcscmp(extension, L".ico")) return "image/x-icon";
    if (!wcscmp(extension, L".woff2")) return "font/woff2";
    if (!wcscmp(extension, L".webmanifest")) return "application/manifest+json";
    if (!wcscmp(extension, L".json")) return "application/json";
    if (!wcscmp(extension, L".wasm")) return "application/wasm";
    return "application/octet-stream";
}

static bool asset_name(sl_u8 request, wchar_t path[512]) {
    if (request.len < 1 || request.len >= 512 || !request.data || request.data[0] != '/') return false;
    if (request.len == 1) { wcscpy(path, L"index.html"); return true; }
    bool start = true;
    for (int64_t i = 1; i < request.len; ++i) {
        const unsigned char c = request.data[i];
        if (c == '/') {
            if (start || request.data[i-1] == '.') return false;
            start = true;
            path[i-1] = L'\\';
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-' || (c == '.' && !start))) return false;
            start = false;
            path[i-1] = c;
        }
    }
    if (start || request.data[request.len-1] == '.') return false;
    path[request.len-1] = 0;
    return true;
}

uint8_t fg_http_windows_asset(int64_t fd, sl_u8 request) {
    wchar_t relative[512], root[4096], path[4608];
    if (!asset_name(request, relative)) return 0;
    const DWORD n = GetEnvironmentVariableW(L"FRANKENMOE_WEB_ROOT", root, 4096);
    if (!n || n >= 4096 || swprintf(path, 4608, L"%ls\\%ls", root, relative) < 0) return 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size;
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileSizeEx(file, &size) || size.QuadPart < 0) {
        CloseHandle(file);
        return 0;
    }
    const SOCKET socket = (SOCKET)fd;
    if (header(socket, mime_type(relative), (uint64_t)size.QuadPart)) {
        unsigned char buffer[16384];
        uint64_t remaining = (uint64_t)size.QuadPart;
        while (remaining) {
            DWORD count = 0;
            const DWORD chunk = remaining < sizeof buffer ? (DWORD)remaining : (DWORD)sizeof buffer;
            if (!ReadFile(file, buffer, chunk, &count, NULL) || !count || !send_all(socket, buffer, count)) break;
            remaining -= count;
        }
    }
    CloseHandle(file);
    return 1;
}

void fg_http_windows_sse(int64_t fd, sl_u8 body) {
    static const char prefix[] = "data: ";
    static const char suffix[] = "\n\ndata: [DONE]\n\n";
    const SOCKET socket = (SOCKET)fd;
    const uint64_t bytes = (sizeof prefix - 1) + (uint64_t)body.len + (sizeof suffix - 1);
    if (header(socket, "text/event-stream; charset=utf-8", bytes) &&
        send_all(socket, prefix, sizeof prefix - 1) && send_all(socket, body.data, (size_t)body.len))
        send_all(socket, suffix, sizeof suffix - 1);
}

static const char *request_method = "unknown";
static const char *request_route = "unmatched";

void fg_http_windows_request(int64_t method, int64_t route) {
    request_method = method == 1 ? "GET" : method == 2 ? "POST" : "unknown";
    switch (route) {
        case 1: request_route = "/health"; break;
        case 3: request_route = "/props"; break;
        case 5: request_route = "/v1/models"; break;
        case 11: request_route = "/completion"; break;
        case 12: request_route = "/v1/completions"; break;
        case 13: request_route = "/v1/chat/completions"; break;
        case 15: request_route = "/v1/responses"; break;
        case 39: request_route = "/tools"; break;
        case 101: request_route = "static-asset"; break;
        default: request_route = "unmatched"; break;
    }
}

void fg_http_windows_response(int64_t status, sl_u8 error_body) {
    FILE *log = _wfopen(L"frankenmoe-http.log", L"ab");
    if (!log) return;
    fprintf(log, "%s %s %lld", request_method, request_route, (long long)status);
    if (status >= 400 && error_body.data && error_body.len > 0 && error_body.len <= 160) {
        // Error payloads originate from fixed server messages.
        fprintf(log, " %.*s", (int)error_body.len, (const char *)error_body.data);
    }
    fputc('\n', log);
    fclose(log);
}

void fg_http_windows_ignored(int64_t field) {
    static unsigned warned;
    if (field < 1 || field > 3) return;
    const unsigned bit = 1u << (unsigned)field;
    if (warned & bit) return;
    warned |= bit;
    const char *name = field == 1 ? "tools" : field == 2 ? "tool_choice" : "response_format";
    fprintf(stderr, "FrankenMoE HTTP: %s is unsupported and ignored\n", name);
    FILE *log = _wfopen(L"frankenmoe-http.log", L"ab");
    if (log) {
        fprintf(log, "warning: %s is unsupported and ignored\n", name);
        fclose(log);
    }
}

void fg_http_windows_timing(int64_t prompt, int64_t generated, double prefill, double decode) {
    fprintf(stderr, "FrankenMoE generation: prefill %lld tokens, %.1f ms, %.2f tok/s; "
        "decode %lld generated tokens, %.1f ms, %.2f tok/s\n",
        (long long)prompt, 1000.0 * prefill, prefill > 0.0 ? prompt / prefill : 0.0,
        (long long)generated, 1000.0 * decode,
        generated > 1 && decode > 0.0 ? (generated - 1) / decode : 0.0);
    fflush(stderr);
}
