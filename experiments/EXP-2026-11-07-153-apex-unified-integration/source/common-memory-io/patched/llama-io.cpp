#include "llama-io.h"
#include "fg153-common-hooks.h"

#include <vector>

void llama_io_write_i::write_string(const std::string & str) {
    if (fg153_common_hooks_on()) { fg153_common_hook_write_string(this, str); return; }
    uint32_t str_size = str.size();

    write(&str_size,  sizeof(str_size));
    write(str.data(), str_size);
}

void llama_io_read_i::read_string(std::string & str) {
    if (fg153_common_hooks_on()) { fg153_common_hook_read_string(this, str); return; }
    uint32_t str_size;
    read(&str_size, sizeof(str_size));

    std::vector<char> buf(str_size);
    read(buf.data(), str_size);

    str.assign(buf.data(), str_size);
}
