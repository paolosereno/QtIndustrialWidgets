# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

add_library(QtIndustrialWidgetsPlugin MODULE
    QtIndustrialWidgetsPlugin.cpp
)

target_link_libraries(QtIndustrialWidgetsPlugin
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Designer
)

set_target_properties(QtIndustrialWidgetsPlugin PROPERTIES
    AUTOMOC ON
    PREFIX ""
    OUTPUT_NAME "QtIndustrialWidgetsPlugin"
)

# Installation of the designer plugin
if(DEFINED QT_INSTALL_PLUGINS)
    set(DESIGNER_PLUGIN_INSTALL_DIR "${QT_INSTALL_PLUGINS}/designer")
else()
    set(DESIGNER_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins/designer")
endif()

# Run path of the installed plugin, which links libQtIndustrialWidgets (#127): first the library
# directory relative to the plugin, so that a moved prefix still works, then its absolute path, for a
# plugin directory outside the prefix (QT_INSTALL_PLUGINS). Windows has no run path: the DLL must be on
# PATH or next to designer.exe (docs/designer_plugin.md).
if(IS_ABSOLUTE "${DESIGNER_PLUGIN_INSTALL_DIR}")
    set(designer_plugin_full_dir "${DESIGNER_PLUGIN_INSTALL_DIR}")
else()
    set(designer_plugin_full_dir "${CMAKE_INSTALL_PREFIX}/${DESIGNER_PLUGIN_INSTALL_DIR}")
endif()
file(RELATIVE_PATH plugin_to_libdir "${designer_plugin_full_dir}" "${CMAKE_INSTALL_FULL_LIBDIR}")
if(APPLE)
    set(plugin_origin "@loader_path")
else()
    set(plugin_origin "$ORIGIN")
endif()
set_target_properties(QtIndustrialWidgetsPlugin PROPERTIES
    INSTALL_RPATH "${plugin_origin}/${plugin_to_libdir};${CMAKE_INSTALL_FULL_LIBDIR}"
)

# Read by the test of the installed plugin (tests/CMakeLists.txt, #127)
set_property(GLOBAL PROPERTY QTINDUSTRIALWIDGETS_DESIGNER_PLUGIN_INSTALL_DIR "${DESIGNER_PLUGIN_INSTALL_DIR}")

install(TARGETS QtIndustrialWidgetsPlugin
    LIBRARY DESTINATION "${DESIGNER_PLUGIN_INSTALL_DIR}"
)
