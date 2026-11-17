// EXP920 differential gate: Goose tokenizer vs the native tokenizer on the same
// GGUF. The native library is the independent reference only: it loads the
// vocabulary (vocab_only, no weights, no GPU) and answers llama_tokenize.
#include "runtime.h"

#include <llama.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int failures = 0;
static long long ids_compared = 0;
static int cases = 0;

static void fail(const char * what, const std::string & label) {
    std::printf("FAIL %s: %s\n", what, label.c_str());
    failures++;
}

static sl_u8 S(const void * p, long long n) {
    return sl_u8{const_cast<uint8_t *>(static_cast<const uint8_t *>(p)), n};
}
static sl_i64 I(const std::vector<int64_t> & v) {
    return sl_i64{const_cast<int64_t *>(v.data()), (long long) v.size()};
}
static sl_i64 I(int64_t * p, long long n) { return sl_i64{p, n}; }

struct Case {
    std::string label;
    std::string text;
};

int main(int argc, char ** argv) {
    if (argc < 2) {
        std::printf("usage: gate <model.gguf>\n");
        return 2;
    }
    goose_init(0, nullptr);

    const char * path = argv[1];
    int fd = open(path, O_RDONLY);
    if (fd < 0) { std::perror("open"); return 2; }
    struct stat st{};
    if (fstat(fd, &st) != 0) { std::perror("fstat"); return 2; }
    const long long file_bytes = st.st_size;
    void * base = mmap(nullptr, (size_t) file_bytes, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) { std::perror("mmap"); return 2; }
    const long long hdr = file_bytes < 67108864 ? file_bytes : 67108864;

    std::vector<int64_t> state(8), kvs(52 * 8), tens(753 * 12), kvs_slots(128), ten_slots(2048);
    int64_t rc = gr_parse(S(base, hdr), file_bytes, I(state), I(kvs), I(tens), I(kvs_slots), I(ten_slots));
    if (rc != 0) { std::printf("gr_parse rc=%lld\n", (long long) rc); return 2; }
    std::printf("reader: kv=%lld tensors=%lld data_off=%lld\n", (long long) state[2], (long long) state[3],
                (long long) state[5]);

    auto find_kv = [&](const char * key) -> int64_t {
        return gr_find_kv(S(base, hdr), I(state), I(kvs), I(kvs_slots), S(key, (long long) std::strlen(key)));
    };
    const int64_t ti = find_kv("tokenizer.ggml.tokens");
    const int64_t mi = find_kv("tokenizer.ggml.merges");
    const int64_t tyi = find_kv("tokenizer.ggml.token_type");
    if (ti < 0 || mi < 0 || tyi < 0) { std::printf("missing tokenizer keys\n"); return 2; }
    const int64_t n_tok = kvs[ti * 8 + 5];
    const int64_t n_mrg = kvs[mi * 8 + 5];
    std::printf("vocab: tokens=%lld merges=%lld\n", (long long) n_tok, (long long) n_mrg);

    std::vector<int64_t> tok_off(n_tok), tok_len(n_tok), mrg_off(n_mrg), mrg_len(n_mrg);
    {
        std::vector<int64_t> pairs(2 * n_tok);
        int64_t n = gr_string_array_index(S(base, hdr), I(state), I(kvs), ti, I(pairs));
        if (n != n_tok) { std::printf("tokens index n=%lld\n", (long long) n); return 2; }
        for (int64_t i = 0; i < n_tok; i++) { tok_off[i] = pairs[2 * i]; tok_len[i] = pairs[2 * i + 1]; }
    }
    {
        std::vector<int64_t> pairs(2 * n_mrg);
        int64_t n = gr_string_array_index(S(base, hdr), I(state), I(kvs), mi, I(pairs));
        if (n != n_mrg) { std::printf("merges index n=%lld\n", (long long) n); return 2; }
        for (int64_t i = 0; i < n_mrg; i++) { mrg_off[i] = pairs[2 * i]; mrg_len[i] = pairs[2 * i + 1]; }
    }

    std::vector<int64_t> tok_slots(1 << 20, 0), mrg_slots(1 << 20, 0), stats(4, 0);
    rc = bt_build(S(base, hdr), I(tok_off), I(tok_len), I(tok_slots), I(mrg_off), I(mrg_len), I(mrg_slots), I(stats));
    if (rc != 0) { std::printf("bt_build rc=%lld\n", (long long) rc); return 2; }
    std::printf("tables: tokens=%lld merges=%lld dup_tokens=%lld dup_merges=%lld\n",
                (long long) stats[0], (long long) stats[1], (long long) stats[2], (long long) stats[3]);

    std::vector<int64_t> tok_type(n_tok, 0), attr(n_tok, 0), spl(n_tok, 0);
    for (int64_t i = 0; i < n_tok; i++) {
        int64_t v = 0;
        if (gr_array_int(S(base, hdr), I(state), I(kvs), tyi, i, I(&v, 1)) != 0) {
            std::printf("token_type read failed at %lld\n", (long long) i);
            return 2;
        }
        tok_type[i] = v;
    }
    if (bt_attrs(S(base, hdr), I(tok_off), I(tok_len), I(tok_type), I(attr)) != 0) {
        std::printf("bt_attrs failed\n");
        return 2;
    }
    const int64_t n_spl = bt_special_order(S(base, hdr), I(tok_off), I(tok_len), I(attr), I(spl));
    std::printf("special tokens: %lld\n", (long long) n_spl);

    // ---- native reference ----
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.vocab_only = true;
    mp.n_gpu_layers = 0;
    llama_model * model = llama_model_load_from_file(path, mp);
    if (!model) { std::printf("native model load failed\n"); return 2; }
    const llama_vocab * voc = llama_model_get_vocab(model);
    if (llama_vocab_n_tokens(voc) != n_tok) {
        std::printf("native n_tokens=%d goose=%lld\n", llama_vocab_n_tokens(voc), (long long) n_tok);
        return 2;
    }

    // ---- corpus ----
    std::vector<Case> corpus = {
        {"empty", ""},
        {"ascii", "Hello, world!"},
        {"english", "The quick brown fox jumps over the lazy dog. It's a test, isn't it? We'd've said so."},
        {"contractions", "'s 't 're 've 'm 'll 'd 'S 'T 'RE 'Ve 'M 'LL 'D"},
        {"russian", "Привет, мир! Это проверка токенизации на русском языке. Ёжик в тумане — классика."},
        {"russian-mixed", "Москва, 2026 год: цены выросли на 12,5%."},
        {"code-c", "int main(void) {\n\tfloat x = 1.0f;\n\treturn (int)x;\n}\n"},
        {"code-py", "def f(x: int) -> list[int]:\n    return [i**2 for i in range(x)]  # squares\n"},
        {"code-json", "{\"a\": [1, 2, 3], \"b\": {\"c\": null}, \"d\": true}"},
        {"cjk", "你好，世界！这是一个测试。日本語のテスト、한국어 테스트."},
        {"emoji", "😀👍🏽🇷🇺👨‍👩‍👧‍👦 — emoji and ZWJ sequences"},
        {"combining", "e\u0301 a\u0300 n\u0303 \u0438\u0306 combining marks"},
        {"arabic", "مرحبا بالعالم، هذا اختبار."},
        {"numbers", "0123456789 3.14159 1e-9 0xDEADBEEF 1000000"},
        {"spaces", "  leading and trailing   "},
        {"whitespace", "a  b\t\tc\r\nd\n\ne   f"},
        {"newlines", "\n\n\n\n"},
        {"tabs", "\t\t\tx\ty"},
        {"punctuation", "!!! ,,, ... ;;; ::: ??? --- ___ *** ((())) [[[]]] {{{}}}"},
        {"mixed-punct", " hello ,, . . ! ? "},
        {"special-end", "<|im_end|>"},
        {"special-both", "<|im_start|>user\nhi<|im_end|>"},
        {"special-eot", "<|endoftext|>after"},
        {"special-mid", "a<|im_end|>b<|im_end|>c"},
        {"special-unknown", "<|not_a_token|> text"},
        {"long-word", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
        {"repeat", "The rain in Spain stays mainly in the plain. " + std::string(40, 'x')},
        {"invalid-2byte", std::string("\xC3\x28", 2)},
        {"invalid-cont", std::string("\x80\x80\x80", 3)},
        {"invalid-4byte", std::string("\xF0\x9F\x92", 3)},
        {"nul", std::string("a\0b", 3)},
        {"one-space", " "},
        {"space-word", " word"},
        {"cyrillic-code", "if (значение > 0) { вернуть 1; }"},
        {"crlf-space", "a\r\n b\r\n\r\n c"},
        {"punct-nl", "x = 1;\ny = 2;\n}\n"},
        {"contractions-d", "I'd we'd they'd he'd she'd 'd 'D"},
        {"crlf-code", "int f(void) {\r\n\treturn 0;\r\n}\r\n"},
        {"contraction-extra", "a'sd a'tx a'rex a'vex a'mx a'llx a'dx"},
        {"contraction-apos", "x'y x'z 'x '1 \u2019s"},
    };
    {
        std::string big;
        for (int i = 0; i < 200; i++) {
            big += "Строка " + std::to_string(i) + ": the quick brown fox, 数字 и код `x = 1_i64`;\n";
        }
        corpus.push_back({"large-mixed", big});
    }

    const int64_t cap_ids = 1 << 20;
    std::vector<int64_t> ids(cap_ids, 0);
    std::vector<int64_t> frag_kind(cap_ids), frag_tok(cap_ids), frag_off(cap_ids), frag_len(cap_ids);
    std::vector<llama_token> native_ids(1 << 22);
    std::vector<int64_t> cpts, seg, sym, heap, hc(2, 0), info(8, 0);

    for (bool parse_special : {true, false}) {
        for (const Case & c : corpus) {
            const long long L = (long long) c.text.size();
            cpts.assign(L + 1, 0);
            seg.assign(4 * (L + 1), 0);
            std::vector<uint8_t> enc(8 * (L + 1), 0);
            sym.assign(24 * (L + 1), 0);
            heap.assign(15 * (L + 1) + 40, 0);

            const int64_t nfrag = stp_partition(S(c.text.data(), L), n_tok, I(tok_off), I(tok_len),
                                                S(base, hdr), I(attr), I(spl.data(), n_spl), parse_special ? 1 : 0,
                                                I(frag_kind), I(frag_tok), I(frag_off), I(frag_len));
            if (nfrag < 0) { fail("partition", c.label); continue; }
            int64_t nout = 0;
            bool bad = false;
            for (int64_t f = 0; f < nfrag; f++) {
                if (frag_kind[f] == 0) {
                    ids[nout++] = frag_tok[f];
                } else {
                    const int64_t r = bt_bpe(S(c.text.data() + frag_off[f], frag_len[f]), S(base, hdr),
                                             I(tok_off), I(tok_len), I(tok_slots), I(mrg_off), I(mrg_len), I(mrg_slots),
                                             I(ids), nout, I(cpts), I(seg), S(enc.data(), (long long) enc.size()),
                                             I(sym), I(heap), I(hc), I(info));
                    if (r < 0) {
                        std::printf("  bt_bpe rc=%lld stage=%lld label=%s\n", (long long) r, (long long) info[4], c.label.c_str());
                        bad = true;
                        break;
                    }
                    nout += r;
                }
            }
            if (bad) { fail("bpe", c.label); continue; }

            const int32_t nnat = llama_tokenize(voc, c.text.data(), (int32_t) L, native_ids.data(),
                                                (int32_t) native_ids.size(), false, parse_special);
            if (nnat < 0) { fail("native tokenize", c.label); continue; }
            cases++;
            ids_compared += nnat;
            if (nnat != nout) {
                std::printf("  %s: goose=%lld native=%d\n", c.label.c_str(), (long long) nout, nnat);
                std::printf("    goose :");
                for (int64_t i = 0; i < nout && i < 24; i++) std::printf(" %lld(%s)", (long long) ids[i], llama_vocab_get_text(voc, (llama_token) ids[i]));
                std::printf("\n    native:");
                for (int64_t i = 0; i < nnat && i < 24; i++) std::printf(" %d(%s)", native_ids[i], llama_vocab_get_text(voc, native_ids[i]));
                std::printf("\n");
                fail("length", c.label);
                continue;
            }
            for (int64_t i = 0; i < nout; i++) {
                if (ids[i] != (int64_t) native_ids[i]) {
                    std::printf("  %s: id[%lld] goose=%lld native=%lld\n", c.label.c_str(), (long long) i,
                                (long long) ids[i], (long long) native_ids[i]);
                    fail("id", c.label);
                    break;
                }
            }
        }
    }

    // ---- pieces for every id, both special modes ----
    {
        long long checked = 0;
        std::vector<uint8_t> gout(4096), nout_buf(4096);
        for (int special = 0; special <= 1; special++) {
            for (int64_t id = 0; id < n_tok; id++) {
                const int32_t nr = llama_token_to_piece(voc, (llama_token) id, (char *) nout_buf.data(),
                                                        (int32_t) nout_buf.size(), 0, special != 0);
                const int64_t gr = bt_token_to_piece(S(base, hdr), I(tok_off), I(tok_len), I(attr), id,
                                                    S(gout.data(), (long long) gout.size()), 0, 0, special, 0);
                checked++;
                if (nr != gr || (nr > 0 && std::memcmp(nout_buf.data(), gout.data(), (size_t) nr) != 0)) {
                    std::printf("  piece id=%lld special=%d native=%d goose=%lld\n", (long long) id, special, nr,
                                (long long) gr);
                    std::printf("    native: %.*s\n", nr > 0 ? nr : 0, (const char *) nout_buf.data());
                    std::printf("    goose : %.*s\n", gr > 0 ? (int) gr : 0, (const char *) gout.data());
                    failures++;
                    break;
                }
            }
        }
        std::printf("pieces checked: %lld\n", checked);
    }

    // ---- detokenize of every corpus case, both modes ----
    {
        long long checked = 0;
        std::vector<uint8_t> gout(1 << 20), nout_buf(1 << 20);
        for (int unparse = 0; unparse <= 1; unparse++) {
            for (const Case & c : corpus) {
                const int32_t nnat = llama_tokenize(voc, c.text.data(), (int32_t) c.text.size(), native_ids.data(),
                                                    (int32_t) native_ids.size(), false, true);
                if (nnat < 0) continue;
                for (int remove = 0; remove <= 1; remove++) {
                    const int32_t nr = llama_detokenize(voc, native_ids.data(), nnat, (char *) nout_buf.data(),
                                                        (int32_t) nout_buf.size(), remove != 0, unparse != 0);
                    for (int64_t i = 0; i < nnat; i++) ids[i] = native_ids[i];
                    const int64_t gr = bt_detokenize(S(base, hdr), I(tok_off), I(tok_len), I(attr),
                                                     I(ids), nnat, S(gout.data(), (long long) gout.size()),
                                                     remove, unparse, 0, 0, -1, 0, -1, 0);
                    checked++;
                    if (nr != gr || (nr > 0 && std::memcmp(nout_buf.data(), gout.data(), (size_t) nr) != 0)) {
                        std::printf("  detok %s remove=%d unparse=%d native=%d goose=%lld\n", c.label.c_str(),
                                    remove, unparse, nr, (long long) gr);
                        failures++;
                    }
                }
            }
        }
        std::printf("detokenize checked: %lld\n", checked);
    }

    // ---- end-of-generation set ----
    {
        std::vector<int64_t> eog(n_tok);
        const int64_t n_eog_ids = bt_eog_ids(S(base, hdr), I(tok_off), I(tok_len), 248046, -1, -1, I(eog));
        std::printf("eog ids: %lld\n", (long long) n_eog_ids);
        long long n_eog = 0, mism = 0;
        for (int64_t id = 0; id < n_tok; id++) {
            const int nat = llama_vocab_is_eog(voc, (llama_token) id) ? 1 : 0;
            const int64_t gs = bt_is_eog(I(eog), n_eog_ids, id);
            if (nat) n_eog++;
            if ((nat != 0) != (gs != 0)) {
                mism++;
                if (mism < 10) std::printf("  eog id=%lld native=%d goose=%lld text=%s\n", (long long) id, nat,
                                           (long long) gs, llama_vocab_get_text(voc, (llama_token) id));
            }
        }
        std::printf("eog: native_count=%lld mismatches=%lld\n", n_eog, mism);
        if (mism) failures++;
    }

    std::printf("\ncases=%d ids_compared=%lld failures=%d\n", cases, ids_compared, failures);
    llama_model_free(model);
    llama_backend_free();
    munmap(base, (size_t) file_bytes);
    close(fd);
    return failures == 0 ? 0 : 1;
}
