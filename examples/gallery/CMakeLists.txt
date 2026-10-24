# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

add_executable(QtIndustrialWidgetsGallery
    main.cpp
    MainWindow.h
    MainWindow.cpp
    Showcases.h
    Showcases.cpp
    GalleryExport.h
    GalleryExport.cpp
    gallery.qrc
)

target_link_libraries(QtIndustrialWidgetsGallery
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Widgets
)

set_target_properties(QtIndustrialWidgetsGallery PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    OUTPUT_NAME "QtIndustrialWidgetsGallery"
)
