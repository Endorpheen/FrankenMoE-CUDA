#ifndef FRANKENMOE_WINDOWS_PLATFORM_H
#define FRANKENMOE_WINDOWS_PLATFORM_H

#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

wchar_t *fg_win_to_wide(const char *utf8);
char *fg_win_to_utf8(const wchar_t *wide);
void *fg_win_map_model(const char *path, size_t *bytes);
int fg_win_resident(const void *address, size_t bytes, unsigned char *pages);
size_t fg_win_page_size(void);
uint64_t fg_win_memory(bool available);
bool fg_win_network_ready(void);
int fg_win_socket_timeout(int64_t fd);

typedef struct { DWORD_PTR mask; } cpu_set_t;
#define CPU_ZERO(set) ((set)->mask = 0)
#define CPU_SET(cpu, set) do { \
    if ((unsigned)(cpu) < 8 * sizeof(DWORD_PTR)) (set)->mask |= (DWORD_PTR)1 << (cpu); \
} while (0)
#define CPU_ISSET(cpu, set) ((unsigned)(cpu) < 8 * sizeof(DWORD_PTR) && \
    ((set)->mask & ((DWORD_PTR)1 << (cpu))))
#define CPU_COUNT(set) __builtin_popcountll((unsigned long long)(set)->mask)
int sched_getaffinity(int pid, size_t bytes, cpu_set_t *set);
int sched_setaffinity(int pid, size_t bytes, const cpu_set_t *set);
int sched_getcpu(void);
#endif
