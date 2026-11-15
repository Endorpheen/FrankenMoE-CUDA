// Staged S1.2 adapter, included by ggml/src/gguf.cpp when FG_GOOSE_GGUF=ON.
//
// It reads the private struct gguf_context (kv vector, tensor infos, alignment,
// version) and hands plain views to common/fg-goose-gguf-bridge.cpp. This file
// is the whole host-side seam: nothing else in gguf.cpp knows about Goose.

static bool fg_gguf_goose_write(const struct gguf_context * ctx,
                                std::vector<int8_t> & buf, bool only_meta,
                                fg_goose::entry site) {
    const size_t n_kv = ctx->kv.size();
    const size_t n_t  = ctx->info.size();

    std::vector<fg_goose::kv_view> kvs(n_kv);
    std::vector<std::vector<const char *>> str_lists(n_kv);

    for (size_t i = 0; i < n_kv; i++) {
        const gguf_kv & kv = ctx->kv[i];
        fg_goose::kv_view & v = kvs[i];
        v.key      = kv.key.c_str();
        v.type     = static_cast<int32_t>(kv.type);
        v.is_array = kv.is_array;
        if (kv.is_array) {
            if (kv.type == GGUF_TYPE_STRING) {
                str_lists[i].reserve(kv.data_string.size());
                for (const std::string & s : kv.data_string) {
                    str_lists[i].push_back(s.c_str());
                }
                v.strs   = str_lists[i].data();
                v.n_strs = str_lists[i].size();
            } else {
                // Numeric and bool arrays keep one byte image per element type;
                // the native writer emits that same image.
                v.data    = kv.data.data();
                v.n_data  = kv.data.size();
                v.n_elems = kv.data.size() / gguf_type_size(kv.type);
            }
        } else if (kv.type == GGUF_TYPE_STRING) {
            v.data   = kv.data_string.front().data();
            v.n_data = kv.data_string.front().size();
        } else {
            v.data   = kv.data.data();
            v.n_data = kv.data.size();
        }
    }

    std::vector<fg_goose::tensor_view> ts(n_t);
    std::vector<std::vector<int8_t>> staged(n_t);  // backend tensors only

    for (size_t i = 0; i < n_t; i++) {
        const gguf_tensor_info & info = ctx->info[i];
        fg_goose::tensor_view & v = ts[i];
        v.name  = info.t.name;
        v.ne    = info.t.ne;
        v.ndims = static_cast<int32_t>(ggml_n_dims(&info.t));
        v.type  = static_cast<int32_t>(info.t.type);
        if (only_meta) {
            continue;  // descriptors only, the module must not need a data image
        }
        const size_t nbytes = ggml_nbytes(&info.t);
        if (info.t.buffer != nullptr) {
            staged[i].resize(nbytes);
            ggml_backend_tensor_get(&info.t, staged[i].data(), 0, nbytes);
            v.data   = staged[i].data();
            v.n_data = nbytes;
        } else if (info.t.data != nullptr) {
            v.data   = info.t.data;
            v.n_data = nbytes;
        } else {
            // Native asserts in this case (GGML_ASSERT(info.t.data)); refuse it
            // through the module's own -1 contract instead.
            return false;
        }
    }

    return fg_goose::gguf_write(site, ctx->version, ctx->alignment, kvs.data(), n_kv,
                                ts.data(), n_t, buf, only_meta);
}
