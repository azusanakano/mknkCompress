#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")" && pwd)"
node_binary="${NODE_BINARY:-$(command -v node)}"
node_modules_dir="${NODE_MODULES_DIR:-${project_dir}/node_modules}"
release_dir="${project_dir}/release"
work_dir="$(mktemp -d)"
trap 'rm -rf -- "$work_dir"' EXIT
stage="${work_dir}/mknkCompress-3.0.0-Linux-x64"

"${project_dir}/build-linux.sh"
mkdir -p "$stage/app/node_modules/@img" "$stage/runtime" "$stage/licenses" "$release_dir"
cp "${project_dir}/build/mknkCompress" "$stage/mknkCompress"
cp "${project_dir}/app/compress.mjs" "$stage/app/compress.mjs"
cp "${project_dir}/README.md" "${project_dir}/LICENSE" "${project_dir}/THIRD-PARTY-NOTICES.md" \
   "${project_dir}/BUILD-REPORT.txt" "${project_dir}/mknkCompress.desktop" "${project_dir}/mknkCompress.svg" "$stage/"
cp "$node_binary" "$stage/runtime/node"
cp -r "$node_modules_dir/sharp" "$stage/app/node_modules/"
cp -r "$node_modules_dir/detect-libc" "$stage/app/node_modules/"
cp -r "$node_modules_dir/semver" "$stage/app/node_modules/"
cp -r "$node_modules_dir/@img/colour" "$stage/app/node_modules/@img/"
cp -r "$node_modules_dir/@img/sharp-linux-x64" "$stage/app/node_modules/@img/"
cp -r "$node_modules_dir/@img/sharp-libvips-linux-x64" "$stage/app/node_modules/@img/"
cp "$node_modules_dir/sharp/LICENSE" "$stage/licenses/SHARP-APACHE-2.0.txt"
cp "$node_modules_dir/detect-libc/LICENSE" "$stage/licenses/DETECT-LIBC-APACHE-2.0.txt"
cp "$node_modules_dir/semver/LICENSE" "$stage/licenses/SEMVER-ISC.txt"
cp "$node_modules_dir/@img/colour/LICENSE.md" "$stage/licenses/COLOUR-APACHE-2.0.md"
strip --strip-unneeded "$stage/runtime/node" 2>/dev/null || true
chmod +x "$stage/mknkCompress" "$stage/runtime/node"
"$stage/mknkCompress" --self-test
tar -C "$work_dir" -cJf "$release_dir/mknkCompress-3.0.0-Linux-x64.tar.xz" "mknkCompress-3.0.0-Linux-x64"
sha256sum "$release_dir/mknkCompress-3.0.0-Linux-x64.tar.xz"
