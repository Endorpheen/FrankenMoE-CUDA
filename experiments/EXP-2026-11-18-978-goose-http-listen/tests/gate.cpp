// EXP978 differential gate: the Goose HTTP listener serves a real request over a
// real socket, and what reaches the client is what the Goose formatter produced.
//
// The client runs in a forked child so the parent can block in accept(). The child
// echoes every byte it receives into a pipe, and the parent compares that echo with
// its own response buffer - so the test checks the wire, not just the buffer.
//
// Two cases: a well-formed request must get 200 with body "ok", and a malformed
// request line must get 400.
#include "runtime.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int checks = 0, fails = 0;
static void check(bool cond, const std::string & what) {
    checks++;
    if (!cond) { fails++; std::printf("FAIL %s\n", what.c_str()); }
}

// Connects, sends `request`, echoes the reply into `echo_fd`.
static void client(int port, const std::string & request, int echo_fd) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) _exit(10);
    struct sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t) port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *) &a, sizeof(a)) != 0) _exit(11);
    if (write(fd, request.data(), request.size()) != (ssize_t) request.size()) _exit(12);
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(echo_fd, buf, (size_t) n) != n) _exit(13);
    }
    close(fd);
    _exit(0);
}

// One full exchange with a configured key set; returns what the client received.
static std::string exchange_keyed(const std::string & request, const std::string & keys,
                                  std::vector<int64_t> koff, int * serve_rc) {
    int64_t port_out[1] = { 0 };
    const int64_t fd = hl_open((sl_i64){ port_out, 1 });
    check(fd >= 0, "hl_open (keyed)");
    if (fd < 0) { *serve_rc = -1; return {}; }
    int pfd[2];
    if (pipe(pfd) != 0) { hl_close(fd); *serve_rc = -1; return {}; }
    const pid_t pid = fork();
    if (pid == 0) {
        close(pfd[0]);
        client((int) port_out[0], request, pfd[1]);
        _exit(20);
    }
    close(pfd[1]);
    std::vector<uint8_t> req_buf(8192, 0), resp_buf(4096, 0);
    const int64_t rc = hl_serve_keyed(fd, (sl_u8){ req_buf.data(), (int64_t) req_buf.size() },
                                      (sl_u8){ resp_buf.data(), (int64_t) resp_buf.size() },
                                      (sl_u8){ (uint8_t *) keys.data(), (int64_t) keys.size() },
                                      (sl_i64){ koff.data(), (int64_t) koff.size() });
    *serve_rc = (int) rc;
    hl_close(fd);
    std::string echo; char buf[4096]; ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf))) > 0) echo.append(buf, (size_t) n);
    close(pfd[0]);
    int status = 0; waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the client completed (keyed)");
    const std::string mine((const char *) resp_buf.data(), (size_t) (rc > 0 ? rc : 0));
    check(echo == mine, "wire equals buffer (keyed)");
    return echo;
}

// One full exchange; returns the bytes the client received.
static std::string exchange(const std::string & request, int * serve_rc) {
    int64_t port_out[1] = { 0 };
    const int64_t fd = hl_open((sl_i64){ port_out, 1 });
    check(fd >= 0, "hl_open returned a listening socket");
    check(port_out[0] > 0, "the kernel reported an ephemeral port");
    if (fd < 0) { *serve_rc = -1; return {}; }

    int pfd[2];
    if (pipe(pfd) != 0) { hl_close(fd); *serve_rc = -1; return {}; }
    const pid_t pid = fork();
    if (pid == 0) {
        close(pfd[0]);
        client((int) port_out[0], request, pfd[1]);
        _exit(20);
    }
    close(pfd[1]);

    std::vector<uint8_t> req_buf(8192, 0);
    std::vector<uint8_t> resp_buf(4096, 0);
    const int64_t rc = hl_serve(fd, (sl_u8){ req_buf.data(), (int64_t) req_buf.size() },
                                (sl_u8){ resp_buf.data(), (int64_t) resp_buf.size() });
    *serve_rc = (int) rc;
    hl_close(fd);

    std::string echo;
    char buf[4096];
    ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf))) > 0) echo.append(buf, (size_t) n);
    close(pfd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the client completed");

    const std::string mine((const char *) resp_buf.data(), (size_t) (rc > 0 ? rc : 0));
    check(echo == mine, "what the client received equals the Goose response buffer");
    return echo;
}

int main() {
    goose_init(0, nullptr);

    // ---- a route the table carries ----
    {
        int rc = 0;
        const std::string got = exchange("GET /health HTTP/1.1\r\nHost: x\r\n\r\n", &rc);
        check(rc > 0, "hl_serve reported a response");
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "health is 200 OK");
        check(got.find("Content-Length: 2\r\n") != std::string::npos, "Content-Length: 2");
        check(got.size() >= 2 && got.compare(got.size() - 2, 2, "ok") == 0, "body is ok");
    }

    // ---- a path the table does not carry ----
    {
        int rc = 0;
        const std::string got = exchange("GET /nope HTTP/1.1\r\n\r\n", &rc);
        check(rc > 0, "hl_serve answered the unknown path");
        check(got.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0, "unknown path is 404");
        check(got.find("Content-Length: 0\r\n") != std::string::npos, "404 has no body");
    }

    // ---- a query string is stripped before the table sees the path ----
    {
        int rc = 0;
        const std::string got = exchange("GET /metrics?v=1 HTTP/1.1\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "path with a query still routes");
    }

    // ---- a method the table does not carry ----
    {
        int rc = 0;
        const std::string got = exchange("PUT /health HTTP/1.1\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 405 Method Not Allowed\r\n", 0) == 0, "unknown method is 405");
    }

    // ---- a configured key set refuses a request with no credential ----
    {
        int rc = 0;
        // /health is public in the native policy, so the 401 case uses /props.
        const std::string got = exchange_keyed("GET /props HTTP/1.1\r\nHost: x\r\n\r\n",
                                               "secret", { 0, 6 }, &rc);
        check(rc > 0, "hl_serve_keyed answered");
        check(got.rfind("HTTP/1.1 401 Unauthorized\r\n", 0) == 0, "no credential is 401");
    }

    // ---- the same request with the right credential passes ----
    {
        int rc = 0;
        const std::string got = exchange_keyed(
            "GET /props HTTP/1.1\r\nAuthorization: Bearer secret\r\n\r\n",
            "secret", { 0, 6 }, &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "Bearer credential is accepted");
    }

    // ---- the same request with the wrong credential is refused ----
    {
        int rc = 0;
        const std::string got = exchange_keyed(
            "GET /props HTTP/1.1\r\nAuthorization: Bearer wrong\r\n\r\n",
            "secret", { 0, 6 }, &rc);
        check(got.rfind("HTTP/1.1 401 Unauthorized\r\n", 0) == 0, "wrong credential is 401");
    }

    // ---- a public path passes even with a key configured ----
    {
        int rc = 0;
        const std::string got = exchange_keyed("GET /health HTTP/1.1\r\n\r\n",
                                               "secret", { 0, 6 }, &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "health is public, key or not");
    }

    // ---- OPTIONS is a preflight, decided by cors_is_preflight ----
    {
        int rc = 0;
        const std::string got = exchange("OPTIONS /health HTTP/1.1\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 204 No Content\r\n", 0) == 0, "preflight is 204");
    }

    // ---- a malformed request line gets 400 ----
    {
        int rc = 0;
        const std::string got = exchange("GET /\r\n\r\n", &rc);
        check(rc > 0, "hl_serve answered the malformed request");
        check(got.rfind("HTTP/1.1 400 Bad Request\r\n", 0) == 0, "status line is 400");
        check(got.find("Content-Length: 0\r\n") != std::string::npos, "empty body");
    }

    std::printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    return fails ? 1 : 0;
}
