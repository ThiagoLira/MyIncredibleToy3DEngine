#!/usr/bin/env bash
# build.sh — configure (if needed) and build with BOTH compilers, then optionally run.
#
#   ./build.sh              build gcc + clang
#   ./build.sh run [args]   build both, then run the gcc binary with args (e.g. --gpu 1)
#   ./build.sh clean        delete both build directories
#   BUILD_TYPE=Release ./build.sh   (default: Debug → validation layers on)
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="${BUILD_TYPE:-Debug}"

if [[ "${1:-}" == "clean" ]]; then
    rm -rf build-gcc build-clang
    exit 0
fi

configure_and_build() {
    local dir="$1" cxx="$2"
    if [[ ! -f "$dir/build.ninja" ]]; then
        cmake -B "$dir" -G Ninja -DCMAKE_CXX_COMPILER="$cxx" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    fi
    ninja -C "$dir"
}

configure_and_build build-gcc   g++
configure_and_build build-clang clang++

# clangd / editors read compile_commands.json from the project root.
ln -sf build-clang/compile_commands.json compile_commands.json

if [[ "${1:-}" == "run" ]]; then
    shift
    exec ./build-gcc/toy3d "$@"
fi
