// Native oracle, part 2: remap_layer and remap_imatrix from src/llama-quant.cpp.
// A separate translation unit: llama-quant.cpp and quantize.cpp collide.
#include "native_api.h"

#include "llama-quant.cpp"

std::string n_remap_layer(const std::string & name, const std::vector<int> & prune,
                          std::map<int, std::string> & mapped, int & next_id) {
    return remap_layer(name, prune, mapped, next_id);
}

std::string n_remap_imatrix(const std::string & name, const std::map<int, std::string> & mapped) {
    return remap_imatrix(name, mapped);
}
