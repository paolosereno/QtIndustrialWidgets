# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO paolosereno/QtIndustrialWidgets
    REF "v${VERSION}"
    SHA512 00a38f4f6e9a9c3286bb7f2e53e2855df6142b97346457c2c73f05abe82f9bd186b422fc83883542ccb89a37ce1dbe45573fa3fc72cb604b7b1a62cf9679a8be
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
vcpkg_cmake_config_fixup(PACKAGE_NAME QtIndustrialWidgets)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSES/MIT.txt")
