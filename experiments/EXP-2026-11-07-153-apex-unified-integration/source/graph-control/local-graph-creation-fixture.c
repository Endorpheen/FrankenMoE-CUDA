// Creation/ownership fixture for the Goose graph layer.
// Parent builds without FG153_LOCAL_ARENA_STUB and runs it against a real
// ggml.c build with graph-arena-adapter.inc. Local check compiles the same
// file with the stub and reports SKIP because ggml_new_object is private.
#include "graph-bridge.h"
#include "goose-replay-wrappers.h"
#include "ggml.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
#ifdef FG153_LOCAL_ARENA_STUB
    printf("SKIP creation: local arena stub cannot call ggml_new_object\n");
    printf("RESULT SKIP\n");
    return 0;
#else
    int ok = 1;
    struct ggml_init_params p = { .mem_size = 4 * 1024 * 1024, .mem_buffer = NULL, .no_alloc = false };
    struct ggml_context * ctx = ggml_init(p);
    if (!ctx) return 1;

    struct ggml_cgraph * g = fg153_goose_gs_new_graph_custom(ctx, 16, false);
    if (!g) { fprintf(stderr, "FAIL create 16\n"); ok = 0; goto cleanup; }
    if (g->size != 16 || g->n_nodes != 0 || g->n_leafs != 0 ||
        g->order != GGML_CGRAPH_EVAL_ORDER_LEFT_TO_RIGHT ||
        g->visited_hash_set.size != 37) {
        fprintf(stderr, "FAIL create metadata size=%d hash=%zu\n", g->size, g->visited_hash_set.size);
        ok = 0;
    }
    if (g->grads != NULL || g->grad_accs != NULL) { fprintf(stderr, "FAIL create grads null\n"); ok = 0; }

    struct ggml_cgraph * gg = fg153_goose_gs_new_graph_custom(ctx, 16, true);
    if (!gg || gg->grads == NULL || gg->grad_accs == NULL) {
        fprintf(stderr, "FAIL create grads arrays\n"); ok = 0;
    }

    struct ggml_cgraph * def = fg153_goose_gs_new_graph(ctx);
    if (!def || def->size != 2048 || def->visited_hash_set.size != 4099) {
        fprintf(stderr, "FAIL create default\n"); ok = 0;
    }

    if (fg153_goose_gs_graph_nbytes(16, false) == 0 ||
        fg153_goose_gs_graph_overhead_custom(16, false) < fg153_goose_gs_graph_nbytes(16, false) ||
        fg153_goose_gs_graph_overhead() != fg153_goose_gs_graph_overhead_custom(2048, false) ||
        fg153_goose_gs_graph_nbytes((size_t) -1, false) != 0 ||
        fg153_goose_gs_graph_overhead_custom((size_t) -1, false) != 0) {
        fprintf(stderr, "FAIL size/overhead contract\n"); ok = 0;
    }
    if (fg153_goose_gs_new_graph_custom(ctx, 0, false) != NULL ||
        fg153_goose_gs_new_graph_custom(ctx, 2147483648ULL, false) != NULL) {
        fprintf(stderr, "FAIL size bounds\n"); ok = 0;
    }

    struct ggml_tensor * x = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * w = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 4);
    struct ggml_tensor * a = ggml_add(ctx, x, w);
    fg153_goose_gt_build_forward_expand(g, a);
    if (g->n_nodes != 1 || g->n_leafs != 2) { fprintf(stderr, "FAIL traversal on created graph\n"); ok = 0; }

    struct ggml_cgraph * d = fg153_goose_gs_graph_dup(ctx, g, false);
    if (!d || d->n_nodes != g->n_nodes || d->n_leafs != g->n_leafs) {
        fprintf(stderr, "FAIL graph dup\n"); ok = 0;
    }
    if (d) {
        struct ggml_cgraph v = fg153_goose_gs_graph_view(d, 0, 1);
        if (v.n_nodes != 1 || v.n_leafs != 0 || v.grads != NULL) {
            fprintf(stderr, "FAIL view on created graph\n"); ok = 0;
        }
    }

cleanup:
    ggml_free(ctx);

    // Context reset: a fresh context owns a fresh graph; no stale state.
    struct ggml_context * ctx2 = ggml_init(p);
    if (!ctx2) { fprintf(stderr, "FAIL reset ctx\n"); ok = 0; }
    else {
        struct ggml_cgraph * g2 = fg153_goose_gs_new_graph_custom(ctx2, 8, false);
        if (!g2 || g2->size != 8 || g2->n_nodes != 0) { fprintf(stderr, "FAIL post-reset create\n"); ok = 0; }
        ggml_free(ctx2);
    }

    // OOM: run in a child because debug ggml aborts on pool exhaustion while
    // release returns NULL. Both are valid native OOM outcomes.
    pid_t pid = fork();
    if (pid == 0) {
        struct ggml_init_params small = { .mem_size = 2048, .mem_buffer = NULL, .no_alloc = false };
        struct ggml_context * csmall = ggml_init(small);
        if (!csmall) { _exit(3); }
        struct ggml_cgraph * oom = fg153_goose_gs_new_graph_custom(csmall, 2048, false);
        ggml_free(csmall);
        _exit(oom == NULL ? 0 : 4);
    }
    if (pid < 0) { fprintf(stderr, "FAIL OOM fork\n"); ok = 0; }
    else {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) { fprintf(stderr, "FAIL OOM wait\n"); ok = 0; }
        else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) { /* release NULL */ }
        else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) { /* debug abort */ }
        else { fprintf(stderr, "FAIL OOM child status=%d\n", status); ok = 0; }
    }

    printf("RESULT %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
#endif
}
