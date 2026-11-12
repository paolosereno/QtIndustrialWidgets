# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO paolosereno/QtIndustrialWidgets
    REF "v${VERSION}"
    SHA512 d9aac23016c869403b601d53ae9a41efd7f73dff4561e82be530cbf519f7a6abd6e16bdc59653c72c2c1e63cdc255cb7fad9b6072f7f4028dfb68dbc763bc5f2
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
