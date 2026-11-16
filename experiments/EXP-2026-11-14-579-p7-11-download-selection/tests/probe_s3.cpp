// Native oracle for the download selection slice. Includes the translation
// unit so the file-local selection statics are callable directly.
#include "download.cpp"

#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

// common.cpp's formatting helper, supplied here so the translation unit links
// without pulling the whole common object. It is a vsnprintf wrapper, not a
// decision: every compared string still comes from the native call.
std::string string_format(const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int size = vsnprintf(nullptr, 0, fmt, ap) + 1;
    va_end(ap);
    std::string result(size - 1, 0);
    va_start(ap, fmt);
    vsnprintf(result.data(), size, fmt, ap);
    va_end(ap);
    return result;
}

// common_pull_file has no callable decision helper; this is a transcription of
// its response branch, kept next to the probe so the port can be compared.
static int resume_action(long downloaded, int status) {
    if (downloaded > 0 && status != 206) {
        return (status == 416 || status == 200) ? 1 : 2;
    }
    if (downloaded == 0 && status != 200) {
        return 2;
    }
    return 0;
}

static int resume_header(long downloaded, int supports_ranges) {
    return (supports_ranges && downloaded > 0) ? 1 : 0;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    { const std::string name = "model-Q4_K_M.gguf";
      auto s = get_gguf_split_info(name);
      printf("C0|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-Q8_0.gguf";
      auto s = get_gguf_split_info(name);
      printf("C1|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-F16.gguf";
      auto s = get_gguf_split_info(name);
      printf("C2|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-NVFP4.gguf";
      auto s = get_gguf_split_info(name);
      printf("C3|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-Q4_K_M-00003-of-00004.gguf";
      auto s = get_gguf_split_info(name);
      printf("C4|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-00001-of-00002.gguf";
      auto s = get_gguf_split_info(name);
      printf("C5|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "dir/sub/model-Q4_K_M.gguf";
      auto s = get_gguf_split_info(name);
      printf("C6|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model.bin";
      auto s = get_gguf_split_info(name);
      printf("C7|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-Q4_K_M-001-of-00003.gguf";
      auto s = get_gguf_split_info(name);
      printf("C8|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-q4_k_m.gguf";
      auto s = get_gguf_split_info(name);
      printf("C9|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model.IQ2_XS.gguf";
      auto s = get_gguf_split_info(name);
      printf("C10|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { const std::string name = "model-Q4_K_M-00001-of-00003.gguf";
      auto s = get_gguf_split_info(name);
      printf("C11|split|%d|%s|%s|%d|%d|%d\n",
             (int) !string_ends_with(name, ".gguf"), s.prefix.c_str(), s.tag.c_str(),
             s.index, s.count, (int) extract_quant_bits(name)); }
    { auto parts = common_download_get_all_parts("model-Q4_K_M-00002-of-00004.gguf");
      printf("C12|parts|%zu", parts.size());
      for (auto & p : parts) printf("|%s", p.c_str());
      printf("\n"); }
    { auto parts = common_download_get_all_parts("model-Q4_K_M.gguf");
      printf("C13|parts|%zu", parts.size());
      for (auto & p : parts) printf("|%s", p.c_str());
      printf("\n"); }
    { auto parts = common_download_get_all_parts("https://host/m-Q8_0-00001-of-00002.gguf");
      printf("C14|parts|%zu", parts.size());
      for (auto & p : parts) printf("|%s", p.c_str());
      printf("\n"); }
    { auto fs = hf_cache::hf_files{{ "a-Q4_K_M-00001-of-00003.gguf" }, { "a-Q4_K_M-00002-of-00003.gguf" }, { "a-Q8_0.gguf" }, { "b-00001-of-00002.gguf" }}; auto g = get_split_files(fs, fs[0]);
      printf("C15|group|%zu", g.size());
      for (auto & f : g) { int at = -1; for (size_t k = 0; k < fs.size(); k++) if (fs[k].path == f.path) at = (int) k; printf("|%d", at); }
      printf("\n"); }
    { auto fs = hf_cache::hf_files{{ "a-Q4_K_M-00001-of-00003.gguf" }, { "a-Q4_K_M-00002-of-00003.gguf" }, { "a-Q8_0.gguf" }, { "b-00001-of-00002.gguf" }}; auto g = get_split_files(fs, fs[2]);
      printf("C16|group|%zu", g.size());
      for (auto & f : g) { int at = -1; for (size_t k = 0; k < fs.size(); k++) if (fs[k].path == f.path) at = (int) k; printf("|%d", at); }
      printf("\n"); }
    { auto fs = hf_cache::hf_files{{ "m-F16.gguf" }, { "m-Q4_K_M.gguf" }, { "m-Q8_0.gguf" }}; auto m = find_best_model(fs, "");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C17|model|%d\n", at); }
    { auto fs = hf_cache::hf_files{{ "m-F16.gguf" }, { "m-Q4_K_M.gguf" }, { "m-Q8_0.gguf" }}; auto m = find_best_model(fs, "Q8_0");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C18|model|%d\n", at); }
    { auto fs = hf_cache::hf_files{{ "m-Q4_K_M-00001-of-00003.gguf" }, { "m-Q4_K_M-00002-of-00003.gguf" }}; auto m = find_best_model(fs, "");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C19|model|%d\n", at); }
    { auto fs = hf_cache::hf_files{{ "mmproj-x.gguf" }, { "imatrix.gguf" }, { "m-Q4_K_M.gguf" }}; auto m = find_best_model(fs, "");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C20|model|%d\n", at); }
    { auto fs = hf_cache::hf_files{{ "repo/m-Q4_K_M.gguf" }, { "repo/mmproj-Q4_K_M.gguf" }, { "repo/mmproj-F16.gguf" }}; auto m = find_best_sibling(fs, fs[0].path, "mmproj", "Q4_K_M");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C21|sibling|%d\n", at); }
    { auto fs = hf_cache::hf_files{{ "repo/m.gguf" }, { "repo/sub/mmproj.gguf" }, { "repo/mmproj.gguf" }}; auto m = find_best_sibling(fs, fs[0].path, "mmproj", "");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C22|sibling|%d\n", at); }
    { auto fs = hf_cache::hf_files{{ "repo/m-Q4_K_M.gguf" }, { "repo/mmproj-F16.gguf" }, { "repo/mmproj-Q8_0.gguf" }}; auto m = find_best_sibling(fs, fs[0].path, "mmproj", "");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C23|sibling|%d\n", at); }
    { auto fs = hf_cache::hf_files{{ "repo/m.gguf" }, { "repo/mtp-1.gguf" }}; auto m = find_best_sibling(fs, fs[0].path, "mmproj", "");
      int at = -1; for (size_t k = 0; k < fs.size(); k++) if (!m.path.empty() && fs[k].path == m.path) at = (int) k;
      printf("C24|sibling|%d\n", at); }
    { int at = -1; std::string kw;
      auto & sc = common_download_sidecars();
      for (size_t k = 0; k < sc.size(); k++) if ("dflash" == sc[k].name) { at = (int) k; kw = sc[k].keyword; }
      printf("C25|sidecar|%d|%s|%d|%d\n", at, kw.c_str(),
             (int) name_is_sidecar(std::string("repo/dflash-x.gguf")),
             (int) gguf_filename_is_model(std::string("repo/dflash.gguf"))); }
    { int at = -1; std::string kw;
      auto & sc = common_download_sidecars();
      for (size_t k = 0; k < sc.size(); k++) if ("unknown" == sc[k].name) { at = (int) k; kw = sc[k].keyword; }
      printf("C26|sidecar|%d|%s|%d|%d\n", at, kw.c_str(),
             (int) name_is_sidecar(std::string("repo/dflash-x.gguf")),
             (int) gguf_filename_is_model(std::string("repo/unknown.gguf"))); }
    { printf("C27|ismodel");
      printf("|%d", (int) gguf_filename_is_model(std::string("repo/m-Q4_K_M.gguf")));
      printf("|%d", (int) gguf_filename_is_model(std::string("repo/imatrix.gguf")));
      printf("|%d", (int) gguf_filename_is_model(std::string("repo/mtp-x.gguf")));
      printf("|%d", (int) gguf_filename_is_model(std::string("repo/mmproj.gguf")));
      printf("|%d", (int) gguf_filename_is_model(std::string("repo/M-Q4_K_M.GGUF")));
      printf("\n"); }
    { common_download_hf_plan plan;
      if (1) plan.mmproj.path = "a";
      if (0) plan.mtp.path = "b";
      if (1) plan.dflash.path = "c";
      if (0) plan.eagle3.path = "d";
      if (1) plan.dspark.path = "e";
      auto names = common_download_plan_sidecars(plan);
      printf("C28|plan|%zu", names.size());
      for (auto & nm : names) { int at = -1; auto & sc = common_download_sidecars();
        for (size_t k = 0; k < sc.size(); k++) if (nm == sc[k].name) at = (int) k;
        printf("|%d", at); }
      printf("\n"); }
    { printf("C29|repo");
      try { auto r = common_download_split_repo_tag(std::string("user/model:Q4_K_M"));
        printf("|0|%s|%s", r.first.c_str(), r.second.c_str()); }
      catch (const std::exception &) { printf("|1||"); }
      try { auto r = common_download_split_repo_tag(std::string("bad"));
        printf("|0|%s|%s", r.first.c_str(), r.second.c_str()); }
      catch (const std::exception &) { printf("|1||"); }
      try { auto r = common_download_split_repo_tag(std::string("a/b:c:d"));
        printf("|0|%s|%s", r.first.c_str(), r.second.c_str()); }
      catch (const std::exception &) { printf("|1||"); }
      try { auto r = common_download_split_repo_tag(std::string("a/b:"));
        printf("|0|%s|%s", r.first.c_str(), r.second.c_str()); }
      catch (const std::exception &) { printf("|1||"); }
      printf("\n"); }
    { printf("C30|resume");
      printf("|%d", resume_action(100, 206));
      printf("|%d", resume_action(100, 416));
      printf("|%d", resume_action(100, 200));
      printf("|%d", resume_action(100, 500));
      printf("|%d", resume_action(0, 500));
      printf("|%d", resume_action(0, 200));
      printf("|%d|%d|%d", resume_header(100, 1), resume_header(0, 1), resume_header(100, 0));
      printf("\n"); }
    return 0;
}
