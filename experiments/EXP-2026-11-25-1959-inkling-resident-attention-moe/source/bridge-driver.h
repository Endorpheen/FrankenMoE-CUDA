#include "bridge.h"
#include "bridge-image.h"

int inkling_bridge_execute(const struct inkling_bridge *bridge, const void *attention, void *output) {
    enum { width = INKLING_BRIDGE_WIDTH, lanes = 128, row_bytes = width * sizeof(float) };
    static struct bridge_cuda {
        Module module;
        Function prepare, score, mix;
        struct device_buffer input, work;
        struct {
            struct device_buffer data;
            int position;
        } slots[INKLING_VISITS];
    } device;

    if (!cuda_inkling_quants || bridge->slot < 0 || bridge->slot >= INKLING_VISITS ||
        bridge->rows < 0 || bridge->rows > INKLING_BRIDGE_ROWS || bridge->position < 0 ||
        start_context() != 1) return -1;
    const int slot = bridge->slot;
    int error = 0;
    if (!device.module) error = cuda.module_load(&device.module, attention_bridge_cubin);
    if (!error && !device.prepare) error = cuda.function_get(&device.prepare, device.module, "i2_bridge_prepare");
    if (!error && !device.score) error = cuda.function_get(&device.score, device.module, "i2_bridge_score");
    if (!error && !device.mix) error = cuda.function_get(&device.mix, device.module, "i2_bridge_mix");

    struct device_buffer *slot_state = &device.slots[slot].data;
    if (!error) error = reserve(slot_state, 8 * row_bytes);
    const DevicePtr weights = slot_state->address, query = weights + 4 * row_bytes, history = query + row_bytes;
    if (!error && bridge->restart) {
        error = upload_packet(weights, bridge->postconv, 4 * row_bytes);
        if (!error) error = upload_packet(query, bridge->query, row_bytes);
        if (!error) error = upload_packet(history, bridge->initial_history, 3 * row_bytes);
    } else if (!error && device.slots[slot].position != bridge->position) {
        error = -1;
    }

    const size_t input_bytes = (bridge->rows + 1) * row_bytes;
    if (!error) error = reserve(&device.input, (INKLING_BRIDGE_ROWS + 1) * row_bytes);
    if (!error) error = reserve(&device.work, row_bytes + (INKLING_BRIDGE_ROWS + 1) * sizeof(float));
    // A bridge is useful only when the attention producer is resident.
    const uint64_t host = (uint64_t)(uintptr_t)output;
    DevicePtr mixed = 0;
    if (!error) error = activation_output(host, 2 * row_bytes, &mixed);
    const DevicePtr src = activation_device((uint64_t)(uintptr_t)attention, row_bytes);
    if (!error && !src) error = -1;
    if (!error) error = upload_packet(device.input.address, bridge->input, input_bytes);
    const DevicePtr residual = device.input.address, past = residual + row_bytes;
    const DevicePtr normalized = device.work.address, scores = normalized + row_bytes;
    const DevicePtr next = mixed + row_bytes;
    const int add = bridge->add_residual;
    void *prepare_args[] = { (void *)&src, (void *)&weights, (void *)&history, (void *)&residual,
        (void *)&next, (void *)&normalized, (void *)&add };
    void *score_args[] = { (void *)&past, (void *)&normalized, (void *)&query, (void *)&scores, (void *)&bridge->rows };
    void *mix_args[] = { (void *)&past, (void *)&normalized, (void *)&scores, &mixed, (void *)&bridge->rows };
    if (!error) error = cuda.launch(device.prepare, 1, 1, 1, lanes, 1, 1, 0, NULL, prepare_args, NULL);
    if (!error) error = cuda.launch(device.score, bridge->rows + 1, 1, 1, lanes, 1, 1, 0, NULL, score_args, NULL);
    if (!error) error = cuda.launch(device.mix, width / lanes, 1, 1, lanes, 1, 1, 0, NULL, mix_args, NULL);
    if (!error) error = activation_publish(host, 2 * row_bytes, mixed, false);
    if (!error) device.slots[slot].position = bridge->position + 1;
    const int popped = finish_context();
    if (error || popped) fprintf(stderr, "Inkling CUDA bridge: slot=%d token=%d status=%d context=%d\n",
                                  slot, bridge->position, error, popped);
    return error || popped ? -1 : 0;
}
