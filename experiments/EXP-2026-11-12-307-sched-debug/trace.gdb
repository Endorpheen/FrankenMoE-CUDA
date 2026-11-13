set breakpoint pending on
break ggml_backend_tensor_copy
commands
silent
printf "COPY src=%s dst=%s\n", ((struct ggml_tensor*)$rdi)->name, ((struct ggml_tensor*)$rsi)->name
printf "  sbuf=%s dbuf=%s nbytes=%ld\n", ggml_backend_buffer_name(((struct ggml_tensor*)$rdi)->buffer), ggml_backend_buffer_name(((struct ggml_tensor*)$rsi)->buffer), ggml_nbytes(((struct ggml_tensor*)$rdi))
continue
end
run
