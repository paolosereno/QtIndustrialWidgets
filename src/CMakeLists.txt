# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

add_library(M4Decimator OBJECT
    internal/M4Decimator.cpp
    internal/M4Decimator.h
)

target_include_directories(M4Decimator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(M4Decimator
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
)

set_target_properties(M4Decimator PROPERTIES
    AUTOMOC ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(QtIndustrialWidgets SHARED
    RadialGauge.cpp
    LinearGauge.cpp
    SevenSegmentDisplay.cpp
    LedIndicator.cpp
    IndustrialKnob.cpp
    StripChart.cpp
    IndustrialSwitch.cpp
    LevelMeter.cpp
    AnnunciatorPanel.cpp
    Compass.cpp
    $<TARGET_OBJECTS:M4Decimator>
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/qtindustrialwidgets_global.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/RadialGauge.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/LinearGauge.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/SevenSegmentDisplay.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/LedIndicator.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/IndustrialKnob.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/StripChart.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/IndustrialSwitch.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/LevelMeter.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/AnnunciatorPanel.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/Compass.h
)

add_library(QtIndustrialWidgets::QtIndustrialWidgets ALIAS QtIndustrialWidgets)

target_compile_definitions(QtIndustrialWidgets
    PRIVATE
        QTINDUSTRIALWIDGETS_LIBRARY
)

target_include_directories(QtIndustrialWidgets
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(QtIndustrialWidgets
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(QtIndustrialWidgets PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME QtIndustrialWidgets
    AUTOMOC ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

include(GNUInstallDirs)

install(TARGETS QtIndustrialWidgets
    EXPORT QtIndustrialWidgetsTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
