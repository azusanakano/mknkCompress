#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")" && pwd)"
build_dir="${project_dir}/build"
mkdir -p "$build_dir"

g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
  -static-libstdc++ -static-libgcc -Wl,--as-needed \
  "$project_dir/src/main.cpp" \
  -o "$build_dir/mknkCompress" \
  -Wl,-l:libgtk-3.so.0 \
  -Wl,-l:libgdk-3.so.0 \
  -Wl,-l:libgobject-2.0.so.0 \
  -Wl,-l:libglib-2.0.so.0 \
  -pthread

echo "Built: $build_dir/mknkCompress"
