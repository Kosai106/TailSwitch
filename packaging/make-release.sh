#!/usr/bin/env bash
# Build a Release binary, run the test suite, and assemble the distributable
# archive. Run inside the build container (or any environment with the
# documented Qt 6 / KF6 toolchain); the produced archive is used on SteamOS.
#
#   distrobox enter tailswitch-kde-dev -- ./packaging/make-release.sh
#
# Output: dist/tailswitch-<version>-linux-x86_64.tar.gz
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$root/build/release}"
dist_dir="$root/dist"
version="$(sed -n 's/^project(TailSwitch VERSION \([0-9][0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
arch="$(uname -m)"
name="tailswitch-$version-linux-$arch"
stage="$dist_dir/$name"

cmake -S "$root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir"
ctest --test-dir "$build_dir" --output-on-failure

rm -rf "$stage"
mkdir -p "$stage"
cp "$build_dir/tailswitch" "$stage/tailswitch"
strip --strip-unneeded "$stage/tailswitch" 2>/dev/null || true
cp "$root/packaging/tailswitch.desktop" "$stage/"
cp "$root/assets/tailscale.svg" "$stage/tailswitch.svg"
cp "$root/packaging/install.sh" "$root/packaging/uninstall.sh" "$stage/"
cp "$root/LICENSE" "$root/README.md" "$root/CHANGELOG.md" "$stage/"
cp "$root/assets/README.md" "$stage/TRADEMARKS.md"
chmod 755 "$stage/tailswitch" "$stage/install.sh" "$stage/uninstall.sh"

tar -C "$dist_dir" -czf "$dist_dir/$name.tar.gz" "$name"
(cd "$dist_dir" && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")
echo "Built $dist_dir/$name.tar.gz"
cat "$dist_dir/$name.tar.gz.sha256"
