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

# Installation of the designer plugin (#135): QT_INSTALL_PLUGINS when it is set by hand; otherwise Qt's own plugin
# directory (QT6_INSTALL_PLUGINS, from Qt's CMake package) when the library is installed into Qt's prefix, so that
# that Qt's Designer loads it; otherwise <libdir>/qt6/plugins/designer. See cmake/DesignerPluginDir.cmake
include("${PROJECT_SOURCE_DIR}/cmake/DesignerPluginDir.cmake")
set(designer_plugin_dir_args
    INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}"
    LIBDIR "${CMAKE_INSTALL_LIBDIR}"
    QT_PREFIX "${QT6_INSTALL_PREFIX}"
    QT_PLUGINS "${QT6_INSTALL_PLUGINS}")
if(DEFINED QT_INSTALL_PLUGINS)
    list(APPEND designer_plugin_dir_args HAS_MANUAL MANUAL "${QT_INSTALL_PLUGINS}")
endif()
if(CMAKE_HOST_WIN32)
    list(APPEND designer_plugin_dir_args IGNORE_CASE)
endif()
qiw_designer_plugin_dir(DESIGNER_PLUGIN_INSTALL_DIR designer_plugin_dir_reason ${designer_plugin_dir_args})
if(designer_plugin_dir_reason STREQUAL "manual")
    set(designer_plugin_dir_why "QT_INSTALL_PLUGINS is set")
elseif(designer_plugin_dir_reason STREQUAL "qt")
    set(designer_plugin_dir_why "Qt's plugin directory: CMAKE_INSTALL_PREFIX is Qt's prefix")
else()
    set(designer_plugin_dir_why "CMAKE_INSTALL_PREFIX is not Qt's prefix ${QT6_INSTALL_PREFIX}")
endif()
message(STATUS "QtIndustrialWidgets: Designer plugin destination: ${DESIGNER_PLUGIN_INSTALL_DIR} (${designer_plugin_dir_why})")

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
