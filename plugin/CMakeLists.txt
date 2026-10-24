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

# Read by the test of the installed plugin (tests/CMakeLists.txt, #127)
set_property(GLOBAL PROPERTY QTINDUSTRIALWIDGETS_DESIGNER_PLUGIN_INSTALL_DIR "${DESIGNER_PLUGIN_INSTALL_DIR}")

install(TARGETS QtIndustrialWidgetsPlugin
    LIBRARY DESTINATION "${DESIGNER_PLUGIN_INSTALL_DIR}"
)
