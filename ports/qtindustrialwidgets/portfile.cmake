# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO paolosereno/QtIndustrialWidgets
    REF "v${VERSION}"
    SHA512 3fe615892fc7b1c2f112780d73c58ea85442994ae18aa05b693855aa154a02914e1a160bcff11f808ed10aa251ad594462e9268ebc8436075cfa1bd19706d480
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
