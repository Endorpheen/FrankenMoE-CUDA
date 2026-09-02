#!/usr/bin/env bash
set -euo pipefail

# Prepare a reproducible worktree from the pinned upstream commits.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPSTREAM_DIR="${ROOT_DIR}/upstream"
WORK_DIR="${ROOT_DIR}/work/llama.cpp-integration"
PATCH_FILE="${ROOT_DIR}/patches/expert-tier-integration.patch"

LLAMA_SHA="0b5be7e4a25862bc2777d0c47eae18788a8c963a"
BMOE_SHA="f24bd5aeccfc8b6a3c9782ab94ef2ea6d7437f37"
EXPERT_SHA="4aaad5d318a790a42c2197975ec8fadbad42602b"

mkdir -p "${UPSTREAM_DIR}" "${ROOT_DIR}/work" "${ROOT_DIR}/runtime/third_party"

clone_at() {
    local url="$1" dir="$2" sha="$3" branch="$4"
    if [[ ! -d "${dir}/.git" ]]; then
        git clone --recursive --branch "${branch}" "${url}" "${dir}"
    fi
    git -C "${dir}" fetch --tags origin
    git -C "${dir}" checkout --detach "${sha}"
    git -C "${dir}" submodule update --init --recursive
    [[ "$(git -C "${dir}" rev-parse HEAD)" == "${sha}" ]]
}

clone_at https://github.com/ggml-org/llama.cpp "${UPSTREAM_DIR}/llama.cpp" "${LLAMA_SHA}" master
clone_at https://github.com/Helldez/BigMoeOnEdge "${UPSTREAM_DIR}/BigMoeOnEdge" "${BMOE_SHA}" main
clone_at https://github.com/01554/llama.cpp "${UPSTREAM_DIR}/llama.cpp-expert-tier" "${EXPERT_SHA}" expert-tier

if [[ ! -d "${WORK_DIR}/.git" ]]; then
    git clone --no-hardlinks "${UPSTREAM_DIR}/llama.cpp-expert-tier" "${WORK_DIR}"
fi
if [[ "$(git -C "${WORK_DIR}" rev-parse HEAD)" != "${EXPERT_SHA}" ]]; then
    echo "Error: the llama.cpp worktree has an unexpected HEAD; nothing was changed" >&2
    exit 1
fi

integration_present() {
    grep -Fq "ggml_cpu_set_expert_ready_hook" "${WORK_DIR}/ggml/include/ggml-cpu.h" &&
        grep -Fq "ggml_cpu_set_expert_ready_hook" "${WORK_DIR}/ggml/src/ggml-cpu/ggml-cpu.c" &&
        grep -Fq "llama_expert_prepare_callback" "${WORK_DIR}/include/llama.h" &&
        grep -Fq "llama_get_expert_cache_stats" "${WORK_DIR}/src/llama-context.cpp" &&
        grep -Fq "buf_staging" "${WORK_DIR}/src/llama-expert-hotstore.h" &&
        grep -Fq "llama_expert_hotstore::upload" "${WORK_DIR}/src/llama-expert-hotstore.cpp" &&
        grep -Fq "ml.init_mappings(use_mlock" "${WORK_DIR}/src/llama-model.cpp" &&
        grep -Fq "server_swap_watchdog_start" "${WORK_DIR}/tools/server/server.cpp"
}

if git -C "${WORK_DIR}" apply --reverse --check "${PATCH_FILE}" >/dev/null 2>&1; then
    echo "Integration patch is already applied"
elif integration_present; then
    echo "Integration patch is present with preserved local changes"
else
    git -C "${WORK_DIR}" apply --check "${PATCH_FILE}"
    git -C "${WORK_DIR}" apply "${PATCH_FILE}"
fi

LINK="${ROOT_DIR}/runtime/third_party/llama.cpp"
if [[ -e "${LINK}" && ! -L "${LINK}" ]]; then
    echo "Error: ${LINK} exists and is not a symlink" >&2
    exit 1
fi
ln -sfn "../../work/llama.cpp-integration" "${LINK}"
echo "Upstream worktrees prepared from UPSTREAMS.md"
