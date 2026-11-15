# Staged S1.1 build: llama.cpp (CPU only) with one Goose module behind a flag.
#
#   OFF (default): the native json-schema-to-grammar.cpp body is selected, the
#                  Goose object is not compiled and not linked.
#   ON           : json_schema_to_grammar() dispatches to the Goose module
#                  (goose/json/format-grammar.goose -> goose-gen/json-schema-grammar.c)
#                  through common/fg-goose-schema-bridge.cpp. The native body
#                  stays compiled under json_schema_to_grammar_native().
#
# Nothing here touches the production tree: this file, tests/ and goose-gen/
# live only in the staged copy.
cmake_minimum_required(VERSION 3.21)

project(fg-goose-staged LANGUAGES C CXX)

option(FG_GOOSE_JSON_SCHEMA
       "Select the Goose json-schema-to-grammar module instead of the native converter"
       OFF)

option(FG_GOOSE_GGUF
       "Select the Goose GGUF writer and split plan instead of the native ones"
       OFF)

# Mutation runs point this at a mutated copy of the generated unit and compile
# it at -O0; the normal builds keep the canonical file at the project's flags.
set(FG_GGUF_UNIT "goose-gen/gguf-stack.c" CACHE FILEPATH "the composed Goose GGUF unit")
option(FG_GGUF_MUT "compile the Goose unit at -O0 (mutation runs)" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# llama.cpp scope: CPU only, libraries only, no server, no model tools, no
# network, no LLGuidance. These are the same switches the base build uses.
set(LLAMA_BUILD_COMMON   ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_APP      OFF CACHE BOOL "" FORCE)
set(LLAMA_OPENSSL        OFF CACHE BOOL "" FORCE)
set(LLAMA_SUBPROCESS     OFF CACHE BOOL "" FORCE)
set(LLAMA_LLGUIDANCE     OFF CACHE BOOL "" FORCE)
set(GGML_CUDA            OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE          ON  CACHE BOOL "" FORCE)

add_subdirectory(llama.cpp)

# --- the one Goose module ---------------------------------------------------

if(FG_GOOSE_JSON_SCHEMA)
    # Generated C is one translation unit and one object of its own; the Goose
    # runtime is self-contained (no host hook is needed by this module).
    add_library(fg-goose-json-schema OBJECT goose-gen/json-schema-grammar.c)
    set_target_properties(fg-goose-json-schema PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(fg-goose-json-schema PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    # Generated code: keep llama.cpp's warning set out of it.
    target_compile_options(fg-goose-json-schema PRIVATE -w)

    target_sources(llama-common PRIVATE common/fg-goose-schema-bridge.cpp)
    target_compile_definitions(llama-common PRIVATE FG_GOOSE_JSON_SCHEMA=1)
    target_include_directories(llama-common PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_link_libraries(llama-common PRIVATE fg-goose-json-schema)
endif()

# --- the GGUF stack: writer and split plan ---------------------------------
#
# OFF (default): gguf_write_to_buf / gguf_get_meta_size / gguf_get_meta_data
#                call their original bodies, and split_strategy::should_split
#                keeps the native predicate. The Goose object is not compiled.
# ON           : those four calls go into the composed Goose unit
#                (goose-gen/gguf-stack.c) through
#                common/fg-goose-gguf-bridge.cpp. The native bodies stay
#                compiled under *_native() as the gate oracle and the rollback.

if(FG_GOOSE_GGUF)
    add_library(fg-goose-gguf OBJECT ${FG_GGUF_UNIT})
    set_target_properties(fg-goose-gguf PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories(fg-goose-gguf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
    target_compile_options(fg-goose-gguf PRIVATE -w)
    if(FG_GGUF_MUT)
        target_compile_options(fg-goose-gguf PRIVATE -O0)
    endif()

    # gguf.cpp (ggml-base) is the only translation unit that knows about Goose.
    target_sources(ggml-base PRIVATE common/fg-goose-gguf-bridge.cpp)
    target_compile_definitions(ggml-base PRIVATE FG_GOOSE_GGUF=1)
    target_link_libraries(ggml-base PRIVATE fg-goose-gguf)
endif()

# The staged header lives next to gguf.cpp, so the untouched llama.cpp CMake
# build of this tree also compiles the OFF path.
if(FG_GOOSE_GGUF)
    target_include_directories(ggml-base PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/goose-gen)
endif()

# --- gate -------------------------------------------------------------------

add_executable(fg-goose-smoke tests/fg_goose_smoke.cpp)
target_link_libraries(fg-goose-smoke PRIVATE llama-common)
if(FG_GOOSE_JSON_SCHEMA)
    target_compile_definitions(fg-goose-smoke PRIVATE FG_GOOSE_JSON_SCHEMA=1)
endif()

# Prints the selected grammar text per corpus schema; the base and OFF builds
# must print the same text (switch-off parity).
add_executable(fg-goose-dump tests/fg_goose_dump.cpp)
target_link_libraries(fg-goose-dump PRIVATE llama-common)

# --- S1.2 gate: the Goose GGUF writer against the native writer -------------

add_executable(fg-gguf-smoke tests/fg_gguf_smoke.cpp)
target_link_libraries(fg-gguf-smoke PRIVATE ggml)
target_include_directories(fg-gguf-smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/ggml/src)
if(FG_GOOSE_GGUF)
    target_compile_definitions(fg-gguf-smoke PRIVATE FG_GOOSE_GGUF=1)
endif()

# Synthetic GGUF input for the split-plan differential.
add_executable(fg-gguf-make tests/fg_gguf_make.cpp)
target_link_libraries(fg-gguf-make PRIVATE ggml)

# The split tool of this tree, built from the pinned upstream source so the
# split plan can be compared OFF (native predicate) against ON (Goose plan).
add_executable(fg-gguf-split llama.cpp/tools/gguf-split/gguf-split.cpp)
target_link_libraries(fg-gguf-split PRIVATE llama-common llama)
target_include_directories(fg-gguf-split PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
if(FG_GOOSE_GGUF)
    target_compile_definitions(fg-gguf-split PRIVATE FG_GOOSE_GGUF=1)
endif()

message(STATUS "FG_GOOSE_JSON_SCHEMA = ${FG_GOOSE_JSON_SCHEMA}")
message(STATUS "FG_GOOSE_GGUF        = ${FG_GOOSE_GGUF}")
