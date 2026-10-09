# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

add_executable(QtIndustrialWidgetsGallery
    main.cpp
    MainWindow.h
    MainWindow.cpp
)

target_link_libraries(QtIndustrialWidgetsGallery
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(QtIndustrialWidgetsGallery PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME "QtIndustrialWidgetsGallery"
)
