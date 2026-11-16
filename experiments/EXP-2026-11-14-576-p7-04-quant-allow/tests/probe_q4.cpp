// Native oracle for the quantization allow-cuts slice.
// Includes the translation unit so the file-local policy helpers are callable.
#include "llama-quant.cpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// llama_tensor_get_type's prefix, transcribed: denied, token-embedding type,
// output type, manual override, standard logic. The two real decisions it calls
// (tensor_allows_quantization, regex_search) stay native.
static int prio_source(const std::string & name, int category, int token_type, int output_type,
                       bool is_quant, bool per_layer,
                       const std::vector<std::pair<std::regex, int>> & pats) {
    const int TYPE_COUNT = (int) GGML_TYPE_COUNT;
    const int CAT_TOKEN_EMBD = (int) tensor_category::TOKEN_EMBD;
    const int CAT_OUTPUT = (int) tensor_category::OUTPUT;
    if (token_type < TYPE_COUNT && category == CAT_TOKEN_EMBD) {
        bool named = false;
        if (per_layer) {
            for (auto & pr : pats) if (std::regex_search(name, pr.first)) { named = true; break; }
        }
        if (!named) return 1;
    }
    if (output_type < TYPE_COUNT && category == CAT_OUTPUT) return 2;
    if (is_quant) {
        for (auto & pr : pats) if (std::regex_search(name, pr.first)) return 3;
    }
    return 4;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    llama_model_quantize_params p{};
    { ggml_tensor t{}; std::strcpy(t.name, "blk.0.attn_q.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A0|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "blk.0.attn_norm.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A1|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "output.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A2|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "output.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 1 != 0;
      printf("A3|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "blk.0.ffn_gate_inp.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A4|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "blk.3.ffn_gate_tid2eid.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A5|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "blk.0.ssm_conv1d.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A6|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "position_embd.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A7|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "token_types.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A8|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "blk.0.indexer.attn_k.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A9|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_GLM5_NEXT, &t));
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { ggml_tensor t{}; std::strcpy(t.name, "decision.head.weight");
      for (int d = 0; d < 4; d++) t.ne[d] = d < 2 ? 64 : 1;
      p.only_copy = 0 != 0; p.quantize_output_tensor = 0 != 0;
      printf("A10|allow");
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_VLLM_DECISION2, &t));
      printf("|%d", (int) tensor_allows_quantization(&p, LLM_ARCH_LLAMA, &t));
      printf("\n"); }
    { std::vector<std::pair<std::regex, int>> pats;
      pats.emplace_back(std::regex(".*\\.attn_v.*"), 12);
      pats.emplace_back(std::regex("ffn_down"), 13);
      int hit = -1; int ty = -1;
      for (auto & pr : pats) { if (std::regex_search(std::string("blk.0.attn_v.weight"), pr.first)) { hit = 1; ty = pr.second; break; } }
      printf("O0|override|%d|%d\n", hit, ty); }
    { std::vector<std::pair<std::regex, int>> pats;
      pats.emplace_back(std::regex("attn_[qv]"), 12);
      pats.emplace_back(std::regex("^blk\\.3\\."), 13);
      int hit = -1; int ty = -1;
      for (auto & pr : pats) { if (std::regex_search(std::string("blk.3.attn_k.weight"), pr.first)) { hit = 1; ty = pr.second; break; } }
      printf("O1|override|%d|%d\n", hit, ty); }
    { std::vector<std::pair<std::regex, int>> pats;
      pats.emplace_back(std::regex("^blk\\.0\\."), 12);
      int hit = -1; int ty = -1;
      for (auto & pr : pats) { if (std::regex_search(std::string("blk.10.attn_k.weight"), pr.first)) { hit = 1; ty = pr.second; break; } }
      printf("O2|override|%d|%d\n", hit, ty); }
    { std::vector<std::pair<std::regex, int>> pats;
      pats.emplace_back(std::regex("per_layer.*"), 12);
      int src = prio_source(std::string("per_layer_token_embd.weight"), 0, 11, 43, 1 != 0, 1 != 0, pats);
      printf("P0|priority|%d\n", src); }
    { std::vector<std::pair<std::regex, int>> pats;
      int src = prio_source(std::string("per_layer_token_embd.weight"), 0, 11, 43, 1 != 0, 1 != 0, pats);
      printf("P1|priority|%d\n", src); }
    { int m = -2;
      try { std::regex re(""); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R0|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("abc"); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R1|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("abc"); m = std::regex_search(std::string("xabcx"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R2|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("^abc"); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R3|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("^abc"); m = std::regex_search(std::string("xabc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R4|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("abc$"); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R5|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("abc$"); m = std::regex_search(std::string("abcx"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R6|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a.c"); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R7|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a.c"); m = std::regex_search(std::string("a\012c"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R8|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a\012c"); m = std::regex_search(std::string("a\012c"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R9|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a[bc]d"); m = std::regex_search(std::string("abd"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R10|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a[bc]d"); m = std::regex_search(std::string("acd"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R11|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a[bc]d"); m = std::regex_search(std::string("aed"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R12|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[a-c]+"); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R13|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[a-c]+"); m = std::regex_search(std::string("xyz"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R14|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[^a-c]"); m = std::regex_search(std::string("d"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R15|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[^a-c]"); m = std::regex_search(std::string("a"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R16|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ab*c"); m = std::regex_search(std::string("ac"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R17|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ab*c"); m = std::regex_search(std::string("abbbc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R18|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ab*c"); m = std::regex_search(std::string("adc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R19|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ab+c"); m = std::regex_search(std::string("ac"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R20|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ab+c"); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R21|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ab?c"); m = std::regex_search(std::string("ac"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R22|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ab?c"); m = std::regex_search(std::string("abc"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R23|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a{2,3}"); m = std::regex_search(std::string("aa"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R24|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a{2,3}"); m = std::regex_search(std::string("aaaa"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R25|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a{2}"); m = std::regex_search(std::string("aa"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R26|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a{2}"); m = std::regex_search(std::string("a"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R27|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a{2,}"); m = std::regex_search(std::string("aaaa"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R28|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\d+"); m = std::regex_search(std::string("blk.10"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R29|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\d+"); m = std::regex_search(std::string("blk.x"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R30|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\w+\\.weight"); m = std::regex_search(std::string("foo.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R31|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\w+\\.weight"); m = std::regex_search(std::string("foo.body"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R32|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\.weight$"); m = std::regex_search(std::string("x.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R33|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\.weight$"); m = std::regex_search(std::string("x.weight.bak"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R34|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("^blk\\.0\\."); m = std::regex_search(std::string("blk.0.attn"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R35|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("^blk\\.0\\."); m = std::regex_search(std::string("blk.10.attn"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R36|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("attn_[qv]"); m = std::regex_search(std::string("blk.0.attn_v.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R37|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re(".*ffn_down.*"); m = std::regex_search(std::string("blk.1.ffn_down.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R38|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ssm_[fg]_[ab]\\.weight"); m = std::regex_search(std::string("blk.0.ssm_f_a.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R39|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("ssm_[fg]_[ab]\\.weight"); m = std::regex_search(std::string("blk.0.ssm_x_a.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R40|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[0-9]+\\.attn"); m = std::regex_search(std::string("blk.12.attn_q.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R41|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[0-9]+\\.attn"); m = std::regex_search(std::string("blk.aa.attn_q.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R42|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\s"); m = std::regex_search(std::string("no space"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R43|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("\\s"); m = std::regex_search(std::string("has space"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R44|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a\\.b"); m = std::regex_search(std::string("a.b"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R45|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("a\\.b"); m = std::regex_search(std::string("axb"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R46|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("blk\\.[0-9]+\\."); m = std::regex_search(std::string("blk.7.attn_q.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R47|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("blk\\.[0-9]+\\."); m = std::regex_search(std::string("layer.7.attn_q.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R48|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[a-z]{3}\\.[a-z]"); m = std::regex_search(std::string("abc.d"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R49|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("[a-z]{3}\\.[a-z]"); m = std::regex_search(std::string("ab.c"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R50|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("x*y*z"); m = std::regex_search(std::string("zzz"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R51|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("x*y*z"); m = std::regex_search(std::string("xy"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R52|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("q[0-9]{1,2}_k"); m = std::regex_search(std::string("q4_k"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R53|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("q[0-9]{1,2}_k"); m = std::regex_search(std::string("qq4_k"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R54|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("blk\\.\\d+\\..*"); m = std::regex_search(std::string("blk.3.attn_q.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R55|regex|%d\n", m); }
    { int m = -2;
      try { std::regex re("blk\\.\\d+\\..*"); m = std::regex_search(std::string("blk.attn_q.weight"), re) ? 1 : 0; }
      catch (const std::exception &) { m = -1; }
      printf("R56|regex|%d\n", m); }
    return 0;
}
