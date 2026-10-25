#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Builds the Linux release packages, the .deb and the .tar.gz (#133). The only place that configures and packages for
# Linux: release.yml publishes what it produces, and the deb job of linux-packages.yml checks the same packages.
#
# Usage: tools/package_linux.sh [--install-deps] <output-dir>
#   --install-deps  install the build dependencies with apt-get (Ubuntu 24.04, as on the CI runners)
# Run from the root of the repository. The build tree is build/package/ (ignored by git).
set -euo pipefail

install_deps=false
if [[ "${1:-}" == "--install-deps" ]]; then
    install_deps=true
    shift
fi
if [[ $# -ne 1 ]]; then
    echo "usage: $0 [--install-deps] <output-dir>" >&2
    exit 2
fi
output_dir=$1

if $install_deps; then
    sudo apt-get update
    sudo apt-get install -y cmake ninja-build qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools \
        libgl1-mesa-dev libxkbcommon-dev
fi

# Prefix /usr (#133): on Ubuntu GNUInstallDirs then gives the multiarch CMAKE_INSTALL_LIBDIR (lib/x86_64-linux-gnu),
# so the plugin lands in Qt's own plugin directory (qtpaths6 --plugin-dir) and the absolute entry of its run path is
# the directory where the .deb puts the library. The .tar.gz has the same layout. Both are built and checked on
# Ubuntu 24.04.
build_dir=build/package
rm -rf "$build_dir"
cmake -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_DOCS=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_DESIGNER_PLUGIN=ON
cmake --build "$build_dir"

# The packages ship the Designer plugin: fail instead of packaging without it
plugin="$build_dir/lib/QtIndustrialWidgetsPlugin.so"
if [[ ! -f "$plugin" ]]; then
    echo "error: $plugin was not built (Qt Designer not found?): not packaging without the Designer plugin" >&2
    exit 1
fi

mkdir -p "$output_dir"
cpack --config "$build_dir/CPackConfig.cmake" -G "DEB;TGZ" -B "$output_dir"
# CPack's own work directory; only the packages stay in the output directory
rm -rf "$output_dir/_CPack_Packages"
ls -l "$output_dir"/*.deb "$output_dir"/*.tar.gz
