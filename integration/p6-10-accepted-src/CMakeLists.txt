cmake_minimum_required(VERSION 3.14)
# Minimal top level for the accepted runtime sources: the original is not
# preserved in the restored tree, and only the llama library is needed to run
# the accepted server binary against our build.
project(llama-accepted CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_CUDA ON CACHE BOOL "" FORCE)
set(GGML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# helpers the original top level provided
function(llama_add_compile_flags)
    # the restored build passed no extra flags through this helper
endfunction()

include(FetchContent)
set(GGML_STANDALONE OFF CACHE BOOL "" FORCE)
add_subdirectory(ggml)
add_subdirectory(src)
