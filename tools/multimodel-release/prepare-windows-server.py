"""Add Win32 startup and a CUDA-free tokenizer probe to the frozen server host."""
from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"checkpoint does not contain one instance of {old!r}")
    return text.replace(old, new)


host_path, cli_path, output = map(Path, sys.argv[1:])
host = host_path.read_text()
host = replace(host, "void hose_host_load_context(const char * path, int64_t capacity) {",
               "static void load_model_vocabulary(const char *path) {")
host = replace(host, "    load_vocab();\n    if (H.bonsai_layout) {", """    load_vocab();
}

void hose_host_load_context(const char *path, int64_t capacity) {
    load_model_vocabulary(path);
    if (H.bonsai_layout) {""")
host += r'''

int hose_win_model_check(const char *path, const char *prompt) {
    load_model_vocabulary(path);
    const char *model = H.bonsai_layout ? "bonsai" :
        metadata_is("general.architecture", "inkling2_loop") ? "inkling" :
        metadata_is("general.architecture", "qwen35moe") ? "apex" : NULL;
    if (!model || (H.inkling_tokenizer != (strcmp(model, "inkling") == 0))) {
        fputs("Unrecognized model architecture or tokenizer\n", stderr);
        return 2;
    }
    const size_t length = strlen(prompt);
    int64_t ids[256];
    unsigned char decoded[1024];
    if (!length || length >= sizeof(decoded)) return 2;
    const int64_t count = hose_tokenize((const unsigned char *)prompt, (int64_t)length, ids, 256);
    if (count <= 0 || count > 256 ||
        hose_detokenize(ids, count, decoded, sizeof(decoded)) != (int64_t)length ||
        memcmp(prompt, decoded, length) != 0) {
        fputs("Tokenizer round trip failed\n", stderr);
        return 1;
    }
    printf("%s %lld", model, (long long)H.n_tok);
    for (int64_t i = 0; i < count; ++i) printf(" %lld", (long long)ids[i]);
    putchar('\n');
    return 0;
}
'''
cli = cli_path.read_text()
cli = replace(cli, "int main(int argc, char ** argv) {", r'''extern int hose_win_model_check(const char *path, const char *prompt);

static int infer_main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--model-check") == 0) {
        fg_goose_host_ensure_init();
        return hose_win_model_check(argv[2], argv[3]);
    }''')
cli += r'''

int wmain(int argc, wchar_t **wide) {
    char **argv = calloc((size_t)argc + 1, sizeof(*argv));
    if (!argv) return 1;
    int count = 0;
    while (count < argc && (argv[count] = fg_win_to_utf8(wide[count])) != NULL) ++count;
    int result = count == argc ? infer_main(argc, argv) : 1;
    for (int index = 0; index < count; ++index) free(argv[index]);
    free(argv);
    return result;
}
'''
(output / "model-host-multimodel.c").write_text(host)
(output / "infer-main-multimodel.c").write_text(cli)
