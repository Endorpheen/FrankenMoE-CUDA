# The Goose tooling lives in the FrankenMoE repository, not in this tree. The previous form
# hardcoded ${CMAKE_CURRENT_SOURCE_DIR}/../../.., which resolves to a different directory when this
# source tree is reached through a symlink (measured building via runtime/third_party/llama.cpp:
# "No rule to make target .../runtime/tools/build-sampler-chain.sh"). The first candidate that
# actually contains the script wins, and the choice is not silent.
set(FG_REPO_ROOT "" CACHE PATH "FrankenMoE repository root (holds tools/build-sampler-chain.sh)")
if(NOT FG_REPO_ROOT)
    foreach(cand "${CMAKE_CURRENT_SOURCE_DIR}/../../.." "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.."
                 "/home/end0/AI/FrankenMoE-CUDA")
        get_filename_component(cand_abs "${cand}" ABSOLUTE)
        if(EXISTS "${cand_abs}/tools/build-sampler-chain.sh")
            set(FG_REPO_ROOT "${cand_abs}")
            break()
        endif()
    endforeach()
endif()
if(NOT FG_REPO_ROOT)
    message(FATAL_ERROR "FrankenMoE repository root not found; pass -DFG_REPO_ROOT=<path>")
endif()
message(STATUS "Goose tooling root: ${FG_REPO_ROOT}")

llama_add_compile_flags()

#
# libraries
#

# llama

file(GLOB LLAMA_MODELS_SOURCES "models/*.cpp")

set(LLAMA_CORE_SOURCES
    llama.cpp
    llama-adapter.cpp
    llama-arch.cpp
    llama-batch.cpp
    llama-chat.cpp
    llama-context.cpp
    llama-cparams.cpp
    llama-grammar.cpp
    llama-graph.cpp
    llama-hparams.cpp
    llama-impl.cpp
    llama-io.cpp
    llama-kv-cache.cpp
    llama-kv-cache-iswa.cpp
    llama-kv-cache-dsa.cpp
    llama-kv-cache-dsa-iswa.cpp
    llama-kv-cache-msa.cpp
    llama-kv-cache-dsv4.cpp
    llama-memory.cpp
    llama-moe-cache.cpp
    llama-memory-hybrid.cpp
    llama-memory-hybrid-iswa.cpp
    llama-memory-hybrid-idx.cpp
    llama-memory-recurrent.cpp
    llama-mmap.cpp
    llama-model-loader.cpp
    llama-model-saver.cpp
    llama-model.cpp
    llama-quant.cpp
    llama-sampler.cpp
    llama-vocab.cpp
    unicode-data.cpp
    unicode.cpp
)


# Goose sampler chain (P6-10). The module is compiled with a build root that calls
# every entry point, and the generated C plus its shim join this target. The shim
# exports fg_sampler_*_call, which llama-sampler.cpp uses through weak references.
set(FG_SAMPLER_CHAIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/goose-sampler-chain)
add_custom_command(
    OUTPUT ${FG_SAMPLER_CHAIN_DIR}/sampler_chain.c ${FG_SAMPLER_CHAIN_DIR}/sampler_chain.h
           ${FG_SAMPLER_CHAIN_DIR}/sampler_chain_shim.c
    COMMAND bash ${FG_REPO_ROOT}/tools/build-sampler-chain.sh ${FG_SAMPLER_CHAIN_DIR}
    DEPENDS ${FG_REPO_ROOT}/goose/sampling/sampler-chain-full.goose
            ${FG_REPO_ROOT}/goose/speculative/spec-decisions.goose
            ${FG_REPO_ROOT}/goose/speculative/spec-doors.goose
            ${FG_REPO_ROOT}/tools/gen-grammar-shim.py
            ${FG_REPO_ROOT}/tools/build-sampler-chain.sh
    COMMENT "Building the Goose sampler chain")
# The shim is its own translation unit and includes only the generated header, so the
# generated C is compiled separately and keeps its exported names. `main` is renamed
# because the chain's root still defines one.
set_source_files_properties(${FG_SAMPLER_CHAIN_DIR}/sampler_chain.c
                            PROPERTIES COMPILE_DEFINITIONS "main=fg_chain_main")
set_source_files_properties(${FG_SAMPLER_CHAIN_DIR}/sampler_chain.h
                            PROPERTIES HEADER_FILE_ONLY TRUE)
list(APPEND LLAMA_CORE_SOURCES ${FG_SAMPLER_CHAIN_DIR}/sampler_chain.c
                               ${FG_SAMPLER_CHAIN_DIR}/sampler_chain_shim.c)

add_library(llama
            ../include/llama.h
            ${LLAMA_CORE_SOURCES}
            unicode.h
            ${LLAMA_MODELS_SOURCES}
            )

set_target_properties(llama PROPERTIES
    VERSION ${LLAMA_VERSION_BASE}
    SOVERSION ${LLAMA_VERSION_MAJOR}
    MACHO_CURRENT_VERSION 0 # keep macOS linker from seeing oversized version number
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)

# exclude non-model sources from unity build
set_source_files_properties(${LLAMA_CORE_SOURCES} ../include/llama.h unicode.h
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

configure_file(llama-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/llama-version.h @ONLY)

target_include_directories(llama PRIVATE . ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(llama PUBLIC ../include)
target_compile_features   (llama PRIVATE cxx_std_17) # don't bump

target_link_libraries(llama PUBLIC ggml)

if (BUILD_SHARED_LIBS)
    set_target_properties(llama PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(llama PRIVATE LLAMA_BUILD)
    target_compile_definitions(llama PUBLIC  LLAMA_SHARED)
endif()
