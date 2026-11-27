#include "main.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "platform.h"
#include <wchar.h>
typedef SOCKET WebSocket;
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
typedef int WebSocket;
#endif

static bool send_all(WebSocket socket, const void *bytes, size_t count) {
    const char *data = bytes;
    while (count) {
        const int request = count > 65536 ? 65536 : (int)count;
#ifdef _WIN32
        const int written = send(socket, data, request, 0);
#else
        const ssize_t written = send(socket, data, (size_t)request, MSG_NOSIGNAL);
        if (written < 0 && errno == EINTR) continue;
#endif
        if (written <= 0) return false;
        data += written;
        count -= (size_t)written;
    }
    return true;
}

static bool header(WebSocket socket, const char *mime, uint64_t bytes) {
    char text[256];
    const int size = snprintf(text, sizeof text, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
        "Content-Length: %llu\r\nX-Content-Type-Options: nosniff\r\n"
        "Cache-Control: no-cache\r\nConnection: close\r\n\r\n", mime, (unsigned long long)bytes);
    return size > 0 && (size_t)size < sizeof text && send_all(socket, text, (size_t)size);
}

static const char *mime_type(const char *path) {
    const char *extension = strrchr(path, '.');
    if (!extension) return "application/octet-stream";
    if (!strcmp(extension, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(extension, ".js")) return "text/javascript; charset=utf-8";
    if (!strcmp(extension, ".css")) return "text/css; charset=utf-8";
    if (!strcmp(extension, ".svg")) return "image/svg+xml";
    if (!strcmp(extension, ".png")) return "image/png";
    if (!strcmp(extension, ".ico")) return "image/x-icon";
    if (!strcmp(extension, ".woff2")) return "font/woff2";
    if (!strcmp(extension, ".webmanifest")) return "application/manifest+json";
    if (!strcmp(extension, ".json")) return "application/json";
    if (!strcmp(extension, ".wasm")) return "application/wasm";
    return "application/octet-stream";
}

static bool asset_name(sl_u8 request, char path[512]) {
    if (request.len < 1 || request.len >= 512 || !request.data || request.data[0] != '/') return false;
    if (request.len == 1) { strcpy(path, "index.html"); return true; }
    bool start = true;
    for (int64_t i = 1; i < request.len; ++i) {
        const unsigned char c = request.data[i];
        if (c == '/') {
            if (start || request.data[i-1] == '.') return false;
            start = true;
            path[i-1] = '/';
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
    char relative[512];
    if (!asset_name(request, relative)) return 0;
    const WebSocket socket = (WebSocket)fd;
#ifdef _WIN32
    wchar_t root[4096], path[4608], name[512];
    size_t i = 0;
    for (; relative[i]; ++i) name[i] = relative[i] == '/' ? L'\\' : relative[i];
    name[i] = 0;
    DWORD n = GetEnvironmentVariableW(L"GOOSEINFER_WEB_ROOT", root, 4096);
    if (!n) n = GetEnvironmentVariableW(L"FRANKENMOE_WEB_ROOT", root, 4096);
    if (!n || n >= 4096 || swprintf(path, 4608, L"%ls\\%ls", root, name) < 0) return 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size;
    if (GetFileType(file) != FILE_TYPE_DISK || !GetFileSizeEx(file, &size) || size.QuadPart < 0) {
        CloseHandle(file);
        return 0;
    }
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
#else
    const char *root = getenv("GOOSEINFER_WEB_ROOT");
    if (!root) root = getenv("FRANKENMOE_WEB_ROOT");
    if (!root || !*root) return 0;
    int directory = open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) return 0;
    char *name = relative;
    for (char *slash; (slash = strchr(name, '/')) != NULL; name = slash + 1) {
        *slash = 0;
        const int next = openat(directory, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        *slash = '/';
        close(directory);
        if (next < 0) return 0;
        directory = next;
    }
    const int file = openat(directory, name, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    close(directory);
    if (file < 0) return 0;
    struct stat status;
    if (fstat(file, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0) {
        close(file);
        return 0;
    }
    if (header(socket, mime_type(relative), (uint64_t)status.st_size)) {
        unsigned char buffer[16384];
        off_t remaining = status.st_size;
        while (remaining > 0) {
            const size_t chunk = remaining < (off_t)sizeof buffer ? (size_t)remaining : sizeof buffer;
            const ssize_t count = read(file, buffer, chunk);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0 || !send_all(socket, buffer, (size_t)count)) break;
            remaining -= count;
        }
    }
    close(file);
#endif
    return 1;
}

void fg_http_windows_sse(int64_t fd, sl_u8 body) {
    static const char prefix[] = "data: ";
    static const char suffix[] = "\n\ndata: [DONE]\n\n";
    const WebSocket socket = (WebSocket)fd;
    const uint64_t bytes = (sizeof prefix - 1) + (uint64_t)body.len + (sizeof suffix - 1);
    if (header(socket, "text/event-stream; charset=utf-8", bytes) &&
        send_all(socket, prefix, sizeof prefix - 1) && send_all(socket, body.data, (size_t)body.len))
        send_all(socket, suffix, sizeof suffix - 1);
}

static FILE *open_http_log(void) {
#ifdef _WIN32
    return _wfopen(L"gooseinfer-http.log", L"ab");
#else
    return fopen("gooseinfer-http.log", "ab");
#endif
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
        case 36: request_route = "/v1/streams/lookup"; break;
        case 39: request_route = "/tools"; break;
        case 101: request_route = "static-asset"; break;
        default: request_route = "unmatched"; break;
    }
}

void fg_http_windows_response(int64_t status, sl_u8 error_body) {
    FILE *log = open_http_log();
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
    fprintf(stderr, "GooseInfer HTTP: %s is unsupported and ignored\n", name);
    FILE *log = open_http_log();
    if (log) {
        fprintf(log, "warning: %s is unsupported and ignored\n", name);
        fclose(log);
    }
}

void fg_http_console(sl_u8 line) {
    static bool initialized;
    static bool color;
    if (!initialized) {
#ifdef _WIN32
        DWORD mode;
        HANDLE terminal = GetStdHandle(STD_ERROR_HANDLE);
        if (GetConsoleMode(terminal, &mode)) {
            SetConsoleOutputCP(CP_UTF8);
            color = !getenv("NO_COLOR") && SetConsoleMode(terminal, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
#else
        const char *term = getenv("TERM");
        color = isatty(STDERR_FILENO) && term && strcmp(term, "dumb") != 0 && !getenv("NO_COLOR");
#endif
        initialized = true;
    }
    if (line.len <= 0 || !line.data) return;
    const unsigned char *cursor = line.data;
    const unsigned char *end = cursor + line.len;
    while (cursor < end) {
        const unsigned char *lf = memchr(cursor, '\n', (size_t)(end - cursor));
        const unsigned char *next = lf ? lf + 1 : end;
        const unsigned char *label = color ? memchr(cursor, ']', (size_t)(next - cursor)) : NULL;
        if (label) {
            fputs("\033[33m", stderr);
            fwrite(cursor, 1, (size_t)(label + 1 - cursor), stderr);
            fputs("\033[0m", stderr);
            cursor = label + 1;
        }
        fwrite(cursor, 1, (size_t)(next - cursor), stderr);
        cursor = next;
    }
    fflush(stderr);
}
