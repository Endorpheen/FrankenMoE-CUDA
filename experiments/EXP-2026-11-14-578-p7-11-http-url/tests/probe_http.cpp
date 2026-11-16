// Native oracle for the HTTP url policy slice. Header-only http.h.
#include "http.h"

#include <cstdio>
#include <string>

static const char *SCHEMES[] = {"http", "https", "ftp", "", "HTTP", "h"};
static const char *USERINFO[] = {"", "u", "u:p", "u:p:q", "@", "u:"};
static const char *HOSTS[] = {"example.com", "localhost", "127.0.0.1", "127.1.2.3", "128.0.0.1", "::1", "[::1]", "::ffff", "a.localhost", ".localhost", "", "0.0.0.0", "127.0.0.1.", "sub.Example.COM", "127.00.0.1", "127.0.0.1.5", "0127.0.0.1", "LOCALHOST"};
static const char *PORTS[] = {"", ":80", ":443", ":0", ":65535", ":65536", ":abc", ":99999999999", ":+80", ":-1", ":80x", ":0x10"};
static const char *PATHS[] = {"", "/", "/x", "/a/b?q=1", "/?"};
static const char *NOPROXY[] = {"", "*", "example.com", ".example.com", " localhost , 127.0.0.1 ", "EXAMPLE.COM", "ample.com", "localhost:8080", ",,", "a.b", "sub.example.com, other"};
static const char *BYPASS_HOSTS[] = {"example.com", "sub.example.com", "ample.com", "LOCALHOST", "127.0.0.1", "other", "x.sub.example.com", "b", "a.b", "", "localhost", "Sub.Example.Com"};

static void put(const std::string & s) {
    for (unsigned char c : s) {
        if (c == '|' || c == '\n' || c == '\r') printf("?%02x", c);
        else putchar((char) c);
    }
}

int main() {
    for (int i = 0; i < 20000; i++) {
        const std::string scheme = SCHEMES[i % 6];
        const std::string userinfo = USERINFO[(i / 7) % 6];
        const std::string host_tok = HOSTS[(i / 11) % 18];
        const std::string port_tok = PORTS[(i / 13) % 12];
        const std::string path_tok = PATHS[(i / 17) % 5];
        const std::string noproxy = NOPROXY[(i / 19) % 11];
        const std::string bhost = BYPASS_HOSTS[(i / 23) % 12];

        const int sh = (i / 29) % 5;
        const std::string vscheme = SCHEMES[i % 2];
        std::string url;
        if (sh == 0) {
            url = vscheme + "://";
            if (!userinfo.empty()) url += userinfo + "@";
            url += host_tok + port_tok + path_tok;
        } else if (sh == 1) {
            url = scheme + "://";
            if (!userinfo.empty()) url += userinfo + "@";
            url += host_tok + port_tok + path_tok;
        } else if (sh == 2) {
            if (!userinfo.empty()) url += userinfo + "@";
            url += host_tok + port_tok + path_tok;
        } else if (sh == 3) {
            url = "http://[::1" + port_tok + path_tok;
        } else {
            url = "http://@" + port_tok + path_tok;
        }

        std::string sc, us, pw, ho, pa, fmt, masked;
        long port = 0;
        int status = 0;
        try {
            common_http_url parts = common_http_parse_url(url);
            sc = parts.scheme;
            us = parts.user;
            pw = parts.password;
            ho = parts.host;
            port = parts.port;
            pa = parts.path;
            fmt = common_http_format_host(parts.host);
            masked = common_http_show_masked_url(parts);
        } catch (const std::runtime_error & e) {
            const std::string w = e.what();
            if (w.rfind("invalid URL: no scheme", 0) == 0) status = 1;
            else if (w.rfind("unsupported URL scheme", 0) == 0) status = 2;
            else if (w.rfind("invalid IPv6 URL authority", 0) == 0) status = 3;
            else status = 4;
        } catch (const std::exception &) {
            status = 4;
        }

        const int loop = common_http_is_loopback(ho);
        const int bypass = common_http_proxy_bypass(noproxy, bhost);

        printf("%d|%d|", i, status);
        put(sc); printf("|");
        put(us); printf("|");
        put(pw); printf("|");
        put(ho); printf("|");
        printf("%ld|", port);
        put(pa); printf("|");
        put(fmt); printf("|");
        put(masked); printf("|");
        printf("%d|%d|", loop, bypass);
        put(url);
        printf("|");
        put(noproxy); printf("|");
        put(bhost);
        printf("\n");
    }
    return 0;
}
