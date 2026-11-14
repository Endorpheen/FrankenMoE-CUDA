// Smoke call for the server set (jsonparse + mcp-core + mcp-stdio +
// decision-core + server-schema + tools-pure): parse an MCP server config.
#include "server.h"

#include <stdio.h>
#include <string.h>

void goose_init_server(int argc, char **argv);
int64_t mcp_config_lines(sl_u8 text, sl_u8 out);

int run_server(void) {
    static char cfg[] =
        "{\"mcpServers\":{"
        "\"zeta\":{\"command\":\"zeta-srv\",\"args\":[\"--a\",\"b\"],\"timeout_ms\":1500},"
        "\"alpha\":{\"command\":\"alpha-srv\",\"cwd\":\"/tmp\",\"env\":{\"K\":\"V\"}}}}";
    static uint8_t out[4096];
    char *argv[] = {(char *)"smoke", NULL};

    goose_init_server(1, argv);
    sl_u8 text = {(uint8_t *)cfg, (int64_t)strlen(cfg)};
    sl_u8 dst = {out, (int64_t)sizeof out};
    int64_t n = mcp_config_lines(text, dst);

    printf("server: mcp_config_lines -> %lld bytes\n", (long long)n);
    if (n <= 0) return 1;
    /* two servers, sorted by name: alpha first */
    printf("server: %.*s", (int)n, (char *)out);
    return strstr((char *)out, "alpha|alpha-srv|/tmp|30000||K=V") ? 0 : 1;
}
