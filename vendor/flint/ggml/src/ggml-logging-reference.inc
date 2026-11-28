struct ggml_logger_state {
    ggml_log_callback log_callback;
    void * log_callback_user_data;
};
static struct ggml_logger_state g_logger_state = {ggml_log_callback_default, NULL};

static void fg_ggml_log_message(enum ggml_log_level level, uint64_t text) {
    g_logger_state.log_callback(level, (const char *)(uintptr_t)text, g_logger_state.log_callback_user_data);
}

void ggml_log_set(ggml_log_callback log_callback, void * user_data) {
    g_logger_state.log_callback = log_callback ? log_callback : ggml_log_callback_default;
    g_logger_state.log_callback_user_data = user_data;
}
