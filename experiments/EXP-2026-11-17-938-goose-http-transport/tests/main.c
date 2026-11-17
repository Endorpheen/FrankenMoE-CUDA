#include <stdio.h>
#include <stdint.h>
void goose_init(int, char **);
int64_t probe_listen(int64_t);
int main(void) { goose_init(0, 0); int64_t rc = probe_listen(0); printf("LISTEN_rc=%lld\n", (long long) rc); return (int) rc; }
