#ifndef INKLING_RESIDENT_BRIDGE_H
#define INKLING_RESIDENT_BRIDGE_H
enum { INKLING_BRIDGE_GRAPH = 0x49324252, INKLING_BRIDGE_WIDTH = 2304, INKLING_BRIDGE_ROWS = 11 };

struct inkling_bridge {
    const float *postconv, *query, *initial_history, *input;
    int slot, rows, position;
    bool restart, add_residual;
};

int inkling_bridge_execute(const struct inkling_bridge *bridge, const void *attention, void *output);
#endif
