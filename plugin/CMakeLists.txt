# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

add_library(QtIndustrialWidgetsPlugin MODULE
    QtIndustrialWidgetsPlugin.cpp
)

target_link_libraries(QtIndustrialWidgetsPlugin
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt${QT_VERSION_MAJOR}::Designer
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
    set(DESIGNER_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qt${QT_VERSION_MAJOR}/plugins/designer")
endif()

install(TARGETS QtIndustrialWidgetsPlugin
    LIBRARY DESTINATION "${DESIGNER_PLUGIN_INSTALL_DIR}"
)
