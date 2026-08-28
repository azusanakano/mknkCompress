#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/llvm-mingw" >&2
  exit 2
fi

root="$(cd "$(dirname "$0")" && pwd)"
tool="$1/bin"
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-1787097600}"

for spec in x64:x86_64 x86:i686 arm64:aarch64; do
  arch="${spec%%:*}"
  triple="${spec#*:}-w64-mingw32"
  out="$root/dist/$arch"
  mkdir -p "$out"
  "$tool/$triple-windres" "$root/resources/mknkCompress.rc" \
    -I "$root/resources" -O coff -o "$out/resources.o"
  "$tool/$triple-clang++" -std=c++20 -O2 -DNDEBUG \
    -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
    -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX \
    -municode -mwindows -static -fuse-ld=lld \
    -ffunction-sections -fdata-sections -mguard=cf \
    -Wl,--gc-sections -Wl,-s \
    "$root/src/main.cpp" "$out/resources.o" \
    -o "$out/mknkCompress.exe" \
    -lwindowscodecs -lole32 -loleaut32 -luuid -lshell32 -lshlwapi \
    -lcomctl32 -lcomdlg32 -luxtheme -ldwmapi
  rm "$out/resources.o"
done

echo "Build complete: $root/dist"
