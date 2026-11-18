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

static bool read_one_response(int fd, std::string * out);

// Sends the request in two writes with a pause between them, so the server sees a
// fragment first. One read is not one request.
static void client_fragmented(int port, int echo_fd) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) _exit(10);
    struct sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t) port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *) &a, sizeof(a)) != 0) _exit(11);
    // Split INSIDE the request line: a fragment that already contains the whole
    // line parses as a complete request and would not exercise the defect at all.
    const char * first = "GET /hea";
    const char * second = "lth HTTP/1.1\r\nHost: x\r\n\r\n";
    if (write(fd, first, strlen(first)) != (ssize_t) strlen(first)) _exit(12);
    usleep(20000);   // let the server's first read return with a partial request
    if (write(fd, second, strlen(second)) != (ssize_t) strlen(second)) _exit(13);
    std::string resp;
    if (!read_one_response(fd, &resp)) _exit(14);
    if (write(echo_fd, resp.data(), resp.size()) != (ssize_t) resp.size()) _exit(15);
    close(fd);
    _exit(0);
}

static std::string exchange_fragmented(int * serve_rc) {
    int64_t port_out[1] = { 0 };
    const int64_t fd = hl_open((sl_i64){ port_out, 1 });
    check(fd >= 0, "hl_open (fragmented)");
    if (fd < 0) return {};
    int pfd[2];
    if (pipe(pfd) != 0) { hl_close(fd); return {}; }
    const pid_t pid = fork();
    if (pid == 0) { close(pfd[0]); client_fragmented((int) port_out[0], pfd[1]); _exit(20); }
    close(pfd[1]);
    std::vector<uint8_t> req_buf(16384, 0), resp_buf(4096, 0);
    std::vector<int64_t> koff{ 0 };
    *serve_rc = (int) hl_serve_full(fd, (sl_u8){ req_buf.data(), (int64_t) req_buf.size() },
                                    (sl_u8){ resp_buf.data(), (int64_t) resp_buf.size() },
                                    (sl_u8){ (uint8_t *) "", 0 }, (sl_i64){ koff.data(), (int64_t) koff.size() },
                                    (sl_u8){ (uint8_t *) "", 0 }, 0);
    hl_close(fd);
    std::string echo; char buf[4096]; ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf))) > 0) echo.append(buf, (size_t) n);
    close(pfd[0]);
    int status = 0; waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the fragmented client completed");
    return echo;
}

// Reads exactly one HTTP response: headers up to the blank line, then Content-Length
// bytes of body. Returns false on EOF.
static bool read_one_response(int fd, std::string * out) {
    std::string buf;
    char c;
    // headers
    while (buf.find("\r\n\r\n") == std::string::npos) {
        if (read(fd, &c, 1) != 1) return false;
        buf.push_back(c);
        if (buf.size() > 8192) return false;
    }
    size_t head_end = buf.find("\r\n\r\n") + 4;
    size_t cl = 0;
    const std::string lower = buf.substr(0, head_end);
    const size_t at = lower.find("Content-Length: ");
    if (at != std::string::npos) cl = (size_t) strtoul(lower.c_str() + at + 16, nullptr, 10);
    while (buf.size() < head_end + cl) {
        if (read(fd, &c, 1) != 1) return false;
        buf.push_back(c);
    }
    out->append(buf);
    return true;
}

// Connects once and sends each request only after reading the previous answer, which
// is what keep-alive means: the connection stays open and requests are sequential.
static void client_keep(int port, const std::vector<std::string> & requests, int echo_fd) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) _exit(10);
    struct sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t) port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *) &a, sizeof(a)) != 0) _exit(11);
    for (const auto & r : requests) {
        if (write(fd, r.data(), r.size()) != (ssize_t) r.size()) _exit(12);
        std::string resp;
        if (!read_one_response(fd, &resp)) _exit(13);
        if (write(echo_fd, resp.data(), resp.size()) != (ssize_t) resp.size()) _exit(14);
    }
    close(fd);
    _exit(0);
}

// Writes every request in ONE write, then reads the answers in order. This is
// pipelining: the server must buffer the remainder after the first answer.
static void client_pipeline(int port, const std::vector<std::string> & requests,
                            size_t expected_responses, int echo_fd) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) _exit(10);
    struct sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t) port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *) &a, sizeof(a)) != 0) _exit(11);
    std::string all;
    for (const auto & r : requests) all += r;
    if (write(fd, all.data(), all.size()) != (ssize_t) all.size()) _exit(12);
    for (size_t i = 0; i < expected_responses; i++) {
        std::string resp;
        if (!read_one_response(fd, &resp)) _exit(13);
        if (write(echo_fd, resp.data(), resp.size()) != (ssize_t) resp.size()) _exit(14);
    }
    close(fd);
    _exit(0);
}

static std::string exchange_pipeline(const std::vector<std::string> & requests,
                                     size_t expected_responses, int64_t max_requests,
                                     int64_t * served_out) {
    int64_t port_out[1] = { 0 };
    const int64_t fd = hl_open((sl_i64){ port_out, 1 });
    check(fd >= 0, "hl_open (pipeline)");
    if (fd < 0) return {};
    int pfd[2];
    if (pipe(pfd) != 0) { hl_close(fd); return {}; }
    const pid_t pid = fork();
    if (pid == 0) {
        close(pfd[0]);
        client_pipeline((int) port_out[0], requests, expected_responses, pfd[1]);
        _exit(20);
    }
    close(pfd[1]);
    std::vector<uint8_t> req_buf(16384, 0), resp_buf(4096, 0);
    std::vector<int64_t> koff{ 0 };
    *served_out = hl_serve_keep(fd, (sl_u8){ req_buf.data(), (int64_t) req_buf.size() },
                                (sl_u8){ resp_buf.data(), (int64_t) resp_buf.size() },
                                (sl_u8){ (uint8_t *) "", 0 }, (sl_i64){ koff.data(), (int64_t) koff.size() },
                                (sl_u8){ (uint8_t *) "", 0 }, 0, max_requests);
    hl_close(fd);
    std::string echo; char buf[4096]; ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf))) > 0) echo.append(buf, (size_t) n);
    close(pfd[0]);
    int status = 0; waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the pipelining client completed");
    return echo;
}

static std::string exchange_keep(const std::vector<std::string> & requests, int64_t max_requests,
                                 int64_t * served_out) {
    int64_t port_out[1] = { 0 };
    const int64_t fd = hl_open((sl_i64){ port_out, 1 });
    check(fd >= 0, "hl_open (keep-alive)");
    if (fd < 0) return {};
    int pfd[2];
    if (pipe(pfd) != 0) { hl_close(fd); return {}; }
    const pid_t pid = fork();
    if (pid == 0) { close(pfd[0]); client_keep((int) port_out[0], requests, pfd[1]); _exit(20); }
    close(pfd[1]);
    std::vector<uint8_t> req_buf(16384, 0), resp_buf(4096, 0);
    std::vector<int64_t> koff{ 0 };
    *served_out = hl_serve_keep(fd, (sl_u8){ req_buf.data(), (int64_t) req_buf.size() },
                                (sl_u8){ resp_buf.data(), (int64_t) resp_buf.size() },
                                (sl_u8){ (uint8_t *) "", 0 }, (sl_i64){ koff.data(), (int64_t) koff.size() },
                                (sl_u8){ (uint8_t *) "", 0 }, 0, max_requests);
    hl_close(fd);
    std::string echo; char buf[4096]; ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf))) > 0) echo.append(buf, (size_t) n);
    close(pfd[0]);
    int status = 0; waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the keep-alive client completed");
    return echo;
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

// One full exchange with a key set AND a CORS policy.
static std::string exchange_full(const std::string & request, const std::string & keys,
                                 std::vector<int64_t> koff, const std::string & origins,
                                 int64_t credentials, int * serve_rc) {
    int64_t port_out[1] = { 0 };
    const int64_t fd = hl_open((sl_i64){ port_out, 1 });
    check(fd >= 0, "hl_open (full)");
    if (fd < 0) { *serve_rc = -1; return {}; }
    int pfd[2];
    if (pipe(pfd) != 0) { hl_close(fd); *serve_rc = -1; return {}; }
    const pid_t pid = fork();
    if (pid == 0) { close(pfd[0]); client((int) port_out[0], request, pfd[1]); _exit(20); }
    close(pfd[1]);
    std::vector<uint8_t> req_buf(16384, 0), resp_buf(4096, 0);
    const int64_t rc = hl_serve_full(fd, (sl_u8){ req_buf.data(), (int64_t) req_buf.size() },
                                     (sl_u8){ resp_buf.data(), (int64_t) resp_buf.size() },
                                     (sl_u8){ (uint8_t *) keys.data(), (int64_t) keys.size() },
                                     (sl_i64){ koff.data(), (int64_t) koff.size() },
                                     (sl_u8){ (uint8_t *) origins.data(), (int64_t) origins.size() },
                                     credentials);
    *serve_rc = (int) rc;
    hl_close(fd);
    std::string echo; char buf[4096]; ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf))) > 0) echo.append(buf, (size_t) n);
    close(pfd[0]);
    int status = 0; waitpid(pid, &status, 0);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the client completed (full)");
    const std::string mine((const char *) resp_buf.data(), (size_t) (rc > 0 ? rc : 0));
    check(echo == mine, "wire equals buffer (full)");
    return echo;
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
    std::vector<uint8_t> req_buf(16384, 0), resp_buf(4096, 0);
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

    std::vector<uint8_t> req_buf(16384, 0);
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
        const std::string got = exchange("GET /nope HTTP/1.1\r\nHost: x\r\n\r\n", &rc);
        check(rc > 0, "hl_serve answered the unknown path");
        check(got.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0, "unknown path is 404");
        check(got.find("Content-Length: 0\r\n") != std::string::npos, "404 has no body");
    }

    // ---- a query string is stripped before the table sees the path ----
    {
        int rc = 0;
        const std::string got = exchange("GET /metrics?v=1 HTTP/1.1\r\nHost: x\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "path with a query still routes");
    }

    // ---- a method the table does not carry ----
    {
        int rc = 0;
        const std::string got = exchange("PUT /health HTTP/1.1\r\nHost: x\r\n\r\n", &rc);
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
            "GET /props HTTP/1.1\r\nHost: x\r\nAuthorization: Bearer secret\r\n\r\n",
            "secret", { 0, 6 }, &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "Bearer credential is accepted");
    }

    // ---- the same request with the wrong credential is refused ----
    {
        int rc = 0;
        const std::string got = exchange_keyed(
            "GET /props HTTP/1.1\r\nHost: x\r\nAuthorization: Bearer wrong\r\n\r\n",
            "secret", { 0, 6 }, &rc);
        check(got.rfind("HTTP/1.1 401 Unauthorized\r\n", 0) == 0, "wrong credential is 401");
    }

    // ---- a public path passes even with a key configured ----
    {
        int rc = 0;
        const std::string got = exchange_keyed("GET /health HTTP/1.1\r\nHost: x\r\n\r\n",
                                               "secret", { 0, 6 }, &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "health is public, key or not");
    }

    // ---- OPTIONS is a preflight, decided by cors_is_preflight ----
    {
        int rc = 0;
        const std::string got = exchange("OPTIONS /health HTTP/1.1\r\nHost: x\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 204 No Content\r\n", 0) == 0, "preflight is 204");
    }

    // ---- CORS: a literal policy is emitted on the preflight ----
    {
        int rc = 0;
        const std::string got = exchange_full("OPTIONS /health HTTP/1.1\r\nHost: x\r\n\r\n", "",
                                              { 0 }, "*", 0, &rc);
        check(got.rfind("HTTP/1.1 204 No Content\r\n", 0) == 0, "cors preflight is 204");
        check(got.find("Access-Control-Allow-Origin: *\r\n") != std::string::npos,
              "allow-origin is the literal");
        check(got.find("Access-Control-Allow-Credentials: false\r\n") != std::string::npos,
              "credentials false is emitted");
    }

    // ---- CORS with credentials echoes the request Origin ----
    {
        int rc = 0;
        const std::string got = exchange_full(
            "OPTIONS /health HTTP/1.1\r\nHost: x\r\nOrigin: http://example.test\r\n\r\n", "",
            { 0 }, "*", 1, &rc);
        check(got.find("Access-Control-Allow-Origin: http://example.test\r\n") != std::string::npos,
              "the origin is echoed when credentials are on");
        check(got.find("Access-Control-Allow-Credentials: true\r\n") != std::string::npos,
              "credentials true is emitted");
    }

    // ---- the localhost mode reflects a local Origin and refuses a foreign one ----
    {
        int rc = 0;
        const std::string got = exchange_full(
            "OPTIONS /health HTTP/1.1\r\nHost: x\r\nOrigin: http://localhost:8080\r\n\r\n", "",
            { 0 }, "localhost", 1, &rc);
        check(got.find("Access-Control-Allow-Origin: http://localhost:8080\r\n") != std::string::npos,
              "localhost origin is reflected");
        check(got.find("Access-Control-Allow-Credentials: true\r\n") != std::string::npos,
              "localhost mode keeps credentials");
    }
    {
        int rc = 0;
        const std::string got = exchange_full(
            "OPTIONS /health HTTP/1.1\r\nHost: x\r\nOrigin: http://127.0.0.1:3000\r\n\r\n", "",
            { 0 }, "localhost", 0, &rc);
        check(got.find("Access-Control-Allow-Origin: http://127.0.0.1:3000\r\n") != std::string::npos,
              "127.0.0.1 origin is reflected");
    }
    {
        int rc = 0;
        const std::string got = exchange_full(
            "OPTIONS /health HTTP/1.1\r\nHost: x\r\nOrigin: http://evil.example\r\n\r\n", "",
            { 0 }, "localhost", 1, &rc);
        check(got.find("Access-Control-Allow-Origin") == std::string::npos,
              "a foreign origin gets no header in localhost mode");
    }

    // ---- CORS is emitted on an ordinary response, not only the preflight ----
    {
        int rc = 0;
        const std::string got = exchange_full("GET /health HTTP/1.1\r\nHost: x\r\nOrigin: http://x.test\r\n\r\n",
                                              "", { 0 }, "*", 0, &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0, "ordinary response still 200");
        check(got.find("Access-Control-Allow-Origin: *\r\n") != std::string::npos,
              "CORS header on a 200, as native does");
        // server-http.cpp:292-298 puts these inside the OPTIONS branch only. An
        // ordinary 200 carrying them tells a browser the response was a preflight.
        check(got.find("Access-Control-Allow-Credentials") == std::string::npos,
              "no credentials header on an ordinary 200");
        check(got.find("Content-Type: text/html") == std::string::npos,
              "no preflight content type on an ordinary 200");
    }

    // ---- Connection: close is honoured and stops the loop ----
    {
        int64_t served = -1;
        const std::string got = exchange_pipeline(
            { "GET /health HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", "GET /nope HTTP/1.1\r\nHost: x\r\n\r\n" },
            1, 2, &served);
        check(served == 1, "only the first request is served when it asks to close");
        check(got.find("Connection: close\r\n") != std::string::npos, "the response says close");
        check(got.find("404 Not Found") == std::string::npos, "the pipelined second is not served");
    }

    // ---- no policy, no header ----
    {
        int rc = 0;
        const std::string got = exchange_full("OPTIONS /health HTTP/1.1\r\nHost: x\r\n\r\n", "",
                                              { 0 }, "", 0, &rc);
        check(got.find("Access-Control-Allow-Origin") == std::string::npos,
              "no policy means no CORS header");
    }

    // ---- two requests on one connection ----
    {
        int64_t served = -1;
        const std::string got = exchange_keep(
            { "GET /health HTTP/1.1\r\nHost: x\r\n\r\n", "GET /nope HTTP/1.1\r\nHost: x\r\n\r\n" }, 2, &served);
        check(served == 2, "both requests were served on the same connection");
        check(got.find("HTTP/1.1 200 OK\r\n") != std::string::npos, "first answer is 200");
        check(got.find("HTTP/1.1 404 Not Found\r\n") != std::string::npos, "second answer is 404");
        // the second status must come after the first, not instead of it
        check(got.find("200 OK") < got.find("404 Not Found"), "answers arrive in order");
    }

    // ---- two requests in ONE write: the remainder must be buffered ----
    {
        int64_t served = -1;
        const std::string got = exchange_pipeline(
            { "GET /health HTTP/1.1\r\nHost: x\r\n\r\n", "GET /nope HTTP/1.1\r\nHost: x\r\n\r\n" }, 2, 2, &served);
        check(served == 2, "both pipelined requests were served");
        check(got.find("HTTP/1.1 200 OK\r\n") != std::string::npos, "pipelined first is 200");
        check(got.find("HTTP/1.1 404 Not Found\r\n") != std::string::npos, "pipelined second is 404");
        check(got.find("200 OK") < got.find("404 Not Found"), "pipelined answers arrive in order");
    }

    // ---- the loop stops at max_requests ----
    {
        int64_t served = -1;
        exchange_keep({ "GET /health HTTP/1.1\r\nHost: x\r\n\r\n" }, 1, &served);
        check(served == 1, "max_requests bounds the loop");
    }

    // ---- a request split across two writes is one request ----
    {
        int rc = 0;
        const std::string got = exchange_fragmented(&rc);
        check(rc > 0, "hl_serve_full answered the fragmented request");
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0,
              "a request in two fragments is not answered with 400");
    }

    // ---- a request body belongs to its own request ----
    {
        // The body arrives in the same write as the headers, which a client may do.
        // Treating its bytes as the next request line is how a POST broke keep-alive.
        int64_t served = -1;
        const std::string got = exchange_pipeline(
            { "POST /completion HTTP/1.1\r\nHost: x\r\nContent-Length: 7\r\n\r\n{\"a\":1}",
              "GET /health HTTP/1.1\r\nHost: x\r\n\r\n" }, 2, 2, &served);
        check(served == 2, "both requests are served with a body in front of the second");
        // Count the answers, not their absence: a body parsed as a request produces an
        // extra 400, and only the count shows it.
        size_t ok200 = 0;
        for (size_t at = got.find("HTTP/1.1 200 OK"); at != std::string::npos;
             at = got.find("HTTP/1.1 200 OK", at + 1)) ok200++;
        check(ok200 == 2, "both pipeline answers are 200, so the body became no request");
        check(got.find("400 Bad Request") == std::string::npos, "no 400 anywhere in the pipeline");
    }
    {
        // Body smaller than the buffer, sent separately: it has to be drained.
        int64_t served = -1;
        const std::string got = exchange_pipeline(
            { "POST /completion HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nabcde",
              "GET /props HTTP/1.1\r\nHost: x\r\n\r\n" }, 2, 2, &served);
        check(served == 2, "a separate body is drained before the next request");
        size_t ok200 = 0;
        for (size_t at = got.find("HTTP/1.1 200 OK"); at != std::string::npos;
             at = got.find("HTTP/1.1 200 OK", at + 1)) ok200++;
        check(ok200 == 2, "the drained body left both answers intact");
    }
    {
        int rc = 0;
        const std::string got = exchange(
            "POST /completion HTTP/1.1\r\nHost: x\r\nContent-Length: abc\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 400 Bad Request\r\n", 0) == 0,
              "a Content-Length that is not a number is refused");
        check(got.find("Connection: close\r\n") != std::string::npos,
              "and the connection closes, because the body cannot be measured");
    }
    {
        // RFC 9112 §6.1: this listener cannot frame a chunked body, so it says so.
        int rc = 0;
        const std::string got = exchange(
            "POST /completion HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", &rc);
        check(got.find("Connection: close\r\n") != std::string::npos,
              "a Transfer-Encoding closes the connection after the answer");
    }

    // ---- RFC 9112 §3.2, as cpp-httplib applies it ----
    {
        int rc = 0;
        const std::string got = exchange("GET /health HTTP/1.1\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 400 Bad Request\r\n", 0) == 0,
              "HTTP/1.1 without Host is refused");
        check(got.find("Connection: close\r\n") != std::string::npos,
              "a refused request closes the connection");
    }
    {
        int rc = 0;
        const std::string got = exchange("GET /health HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 400 Bad Request\r\n", 0) == 0,
              "two Host fields are refused, not last-wins");
    }
    {
        int rc = 0;
        const std::string got = exchange("GET /health HTTP/1.1\r\nHost: a b\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 400 Bad Request\r\n", 0) == 0,
              "a Host value with a space is refused");
        const std::string ok = exchange("GET /health HTTP/1.1\r\nHost: a-b.c:8080\r\n\r\n", &rc);
        check(ok.rfind("HTTP/1.1 200 OK\r\n", 0) == 0,
              "a Host value with - . and : is accepted");
    }
    {
        int rc = 0;
        const std::string got = exchange("GET /health HTTP/1.0\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0,
              "HTTP/1.0 needs no Host");
    }
    {
        // is_host is all_of over the bytes, so an empty value passes. Reproduced,
        // not corrected: this is what the library does.
        int rc = 0;
        const std::string got = exchange("GET /health HTTP/1.1\r\nHost: \r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 200 OK\r\n", 0) == 0,
              "an empty Host is accepted, as the library accepts it");
    }
    {
        const std::string long_path(8200, 'a');
        int rc = 0;
        const std::string got = exchange("GET /" + long_path + " HTTP/1.1\r\nHost: x\r\n\r\n", &rc);
        check(got.rfind("HTTP/1.1 414 URI Too Long\r\n", 0) == 0 ||
              got.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0,
              "an over-long target is refused or unrouted, never served");
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
