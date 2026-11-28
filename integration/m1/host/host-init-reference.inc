// ---- the IQ table contract, installed at init ------------------------------
//
// The descriptor is data: six function pointers plus the provider's own ctx.
// The native CPU provider is defined in ggml/src/ggml-quants.c, next to the
// tables. The reference is weak, so this archive has no hard dependency on
// ggml and links into a host that owns its tables another way.

static const fg_goose_iq_tables * g_iq_tables = NULL;

extern const fg_goose_iq_tables * fg_goose_iq_tables_native(void) __attribute__((weak));

int fg_goose_host_bind_iq_tables(const fg_goose_iq_tables * tables) {
    if (tables == NULL) {
        return -1;
    }
    if (tables->abi != FG_GOOSE_IQ_TABLES_ABI || tables->size != sizeof(fg_goose_iq_tables)) {
        return -2;
    }
    if (tables->iq2_grid == NULL || tables->iq3_grid == NULL || tables->iq2_map == NULL ||
        tables->iq2_nb == NULL || tables->iq3_map == NULL || tables->iq3_nb == NULL) {
        return -3;
    }

    const fg_goose_iq_tables * previous = __atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE);
    if (previous == tables) {
        return 0; // repeated bind of the same provider is a no-op
    }
    if (previous != NULL) {
        return -4; // one provider per process: a second one is a second copy
    }
    if (!__atomic_compare_exchange_n(&g_iq_tables, &previous, tables, 0,
                                     __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
        return previous == tables ? 0 : -4;
    }
    return 0;
}

int fg_goose_host_iq_tables_bound(void) {
    return __atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE) != NULL;
}

#ifdef GOOSEINFER_GOOSE_HOST_QUANT
uint64_t fg_goose_host_iq_tables_pointer(void) {
    return (uintptr_t)__atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE);
}
#endif

// ---- one start-up for the whole runtime ------------------------------------

static pthread_once_t g_host_once = PTHREAD_ONCE_INIT;
static int64_t g_init_runs = 0;

static void fg_goose_host_init_once(void) {
    if (__atomic_load_n(&g_iq_tables, __ATOMIC_ACQUIRE) == NULL && fg_goose_iq_tables_native != NULL) {
        (void) fg_goose_host_bind_iq_tables(fg_goose_iq_tables_native());
    }
    goose_init(0, NULL);
    __atomic_add_fetch(&g_init_runs, 1, __ATOMIC_SEQ_CST);
}

void fg_goose_host_ensure_init(void) {
    pthread_once(&g_host_once, fg_goose_host_init_once);
}

int fg_goose_host_init_with_tables(const fg_goose_iq_tables * tables) {
    const int rc = fg_goose_host_bind_iq_tables(tables);
    if (rc != 0) {
        return rc;
    }
    fg_goose_host_ensure_init();
    return 0;
}

// How many times goose_init() was actually entered. One is the only correct
// value; the two-thread gate in gates/m1/host_smoke.c asserts it.
int64_t fg_goose_host_init_runs(void) {
    return __atomic_load_n(&g_init_runs, __ATOMIC_SEQ_CST);
}
