# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO paolosereno/QtIndustrialWidgets
    REF "v${VERSION}"
    SHA512 9dcca635c926399616105e0ad20853cb2737a976760a33dc46055aba0c8c572b715765d98621a915de93ce666dbe945952e62574d1e4c0d99fa8fa20058264d3
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_EXAMPLES=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_DOCS=OFF
        -DBUILD_DESIGNER_PLUGIN=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME QtIndustrialWidgets CONFIG_PATH lib/cmake/QtIndustrialWidgets)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/MIT.txt")
