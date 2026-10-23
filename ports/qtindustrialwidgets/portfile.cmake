# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO paolosereno/QtIndustrialWidgets
    REF "v${VERSION}"
    SHA512 f247c48e8e30c14e18d9ddbacd8e196296c7309cc2ee60da3bfde2afdec6e244f7df54609113f798174be598920fa58aa502be1877ac5ea1eb9a498ed1e78895
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
