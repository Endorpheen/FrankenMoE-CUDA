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
