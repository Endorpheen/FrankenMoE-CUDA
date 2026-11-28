// The one read path of the 14 unit entry points below. Reaching it without a
// provider is a programming error (a host that never bound one), so it stops
// with the name of the view instead of dereferencing NULL.
static const fg_goose_iq_tables * iq_tables_required(const char * what) {
    const fg_goose_iq_tables * tables = __atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE);
    if (tables == NULL) {
        fprintf(stderr, "fg-goose: IQ tables (%s) requested before a provider was bound; "
                        "call fg_goose_host_bind_iq_tables() or link ggml-base\n", what);
        abort();
    }
    return tables;
}

// EXP675: gguf-offsets.goose reads and writes through its own binding names.
// Same primitive as fg458_st_i64, bound under the name that module declares.
int64_t fg457_ld_i64(uint64_t p, uint64_t o) {
    return *(const int64_t *) ((const char *) (uintptr_t) p + o);
}

uint32_t fg446_ld_u8(uint64_t p, uint64_t o) {
    return *((const uint8_t *) (uintptr_t) p + o);
}

void fg446_st_i64(uint64_t p, uint64_t o, int64_t value) {
    memcpy((uint8_t *) (uintptr_t) p + o, &value, sizeof value);
}

// EXP684: goose-model-graph.goose reads the native per-layer flags array.
int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    uint32_t v = 0;
    memcpy(&v, (const char *) (uintptr_t) base + index * (int64_t) sizeof v, sizeof v);
    return (int64_t) v;
}

// EXP683: goose-tokenizer.goose binds its i64 store to fg202t_*.
void fg202t_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

void fg457_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

void fg458_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

// File-descriptor I/O. These exist because Goose cannot declare read/write/close
// itself: the generated unit includes unistd.h, which declares them with a
// different prototype, so an extern of that name is a compile error (compiler
// blocker 6). socket/bind/listen/accept/getsockname do NOT collide and are called
// directly from Goose. These three are substrate primitives, the same category as
// qz_st_u8 and qz_ld_u8 - they move bytes, they decide nothing.
int64_t qz_rd(int64_t fd, uint64_t buf, int64_t n) {
#ifdef _WIN32
    return n < 0 || n > INT_MAX ? -1 : recv((SOCKET)fd, (char *)(uintptr_t)buf, (int)n, 0);
#else
    return (int64_t) read((int) fd, (void *) (uintptr_t) buf, (size_t) n);
#endif
}

int64_t qz_wr(int64_t fd, uint64_t buf, int64_t n) {
#ifdef _WIN32
    return n < 0 || n > INT_MAX ? -1 : send((SOCKET)fd, (const char *)(uintptr_t)buf, (int)n, 0);
#else
    return (int64_t) write((int) fd, (const void *) (uintptr_t) buf, (size_t) n);
#endif
}

int64_t qz_cl(int64_t fd) {
#ifdef _WIN32
    return closesocket((SOCKET)fd);
#else
    return (int64_t) close((int) fd);
#endif
}
// ---- the 14 IQ table views the unit declares -------------------------------

// One dispatcher per view; the six provider entries of fg_goose_iq_tables do
// the work. EXP-635 defined these next to the tables; the M1 seam removal
// moved them here, where the descriptor is stored. The names and signatures
// are unchanged, so the composed unit did not have to move.
//
// A view is a (family, kind) pair: grid/qgrid read the compile-time grids,
// map/nb read the run-time tables built by ggml_quantize_init().

int64_t qd_iq2_grid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qd_iq2_grid");
    return tables->iq2_grid(tables->ctx, g, i);
}

int64_t qd_iq3_grid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qd_iq3_grid");
    return tables->iq3_grid(tables->ctx, g, i);
}

int64_t qz2_iq2_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq2_map");
    return tables->iq2_map(tables->ctx, g, u);
}

int64_t qz2_iq2_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq2_nb");
    return tables->iq2_nb(tables->ctx, g, off);
}

int64_t qz2_iq2_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq2_qgrid");
    return tables->iq2_grid(tables->ctx, g, i);
}

int64_t qz2_iq3_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq3_map");
    return tables->iq3_map(tables->ctx, g, u);
}

int64_t qz2_iq3_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq3_nb");
    return tables->iq3_nb(tables->ctx, g, off);
}

int64_t qz2_iq3_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz2_iq3_qgrid");
    return tables->iq3_grid(tables->ctx, g, i);
}

int64_t qz_iq2_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq2_map");
    return tables->iq2_map(tables->ctx, g, u);
}

int64_t qz_iq2_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq2_nb");
    return tables->iq2_nb(tables->ctx, g, off);
}

int64_t qz_iq2_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq2_qgrid");
    return tables->iq2_grid(tables->ctx, g, i);
}

int64_t qz_iq3_map(int64_t g, int64_t u) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq3_map");
    return tables->iq3_map(tables->ctx, g, u);
}

int64_t qz_iq3_nb(int64_t g, int64_t off) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq3_nb");
    return tables->iq3_nb(tables->ctx, g, off);
}

int64_t qz_iq3_qgrid(int64_t g, int64_t i) {
    const fg_goose_iq_tables * tables = iq_tables_required("qz_iq3_qgrid");
    return tables->iq3_grid(tables->ctx, g, i);
}

// A store, and only a store: goose/server/shutdown-policy.goose writes its three
// decision words through this. It is host binding, not logic - the action, the exit
// code and the message flag are chosen in Goose.
void fg448_st_i64(uint64_t base, uint64_t offset, int64_t value) {
    *(int64_t *) (uintptr_t) (base + offset) = value;
}
