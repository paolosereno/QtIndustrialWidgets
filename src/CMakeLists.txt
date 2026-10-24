# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

add_library(NumberFormat OBJECT
    internal/NumberFormat.cpp
    internal/NumberFormat.h
)

target_include_directories(NumberFormat
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(NumberFormat
    PUBLIC
        Qt6::Core
)

set_target_properties(NumberFormat PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(M4Decimator OBJECT
    internal/M4Decimator.cpp
    internal/M4Decimator.h
    internal/StripChartGeometry.cpp
    internal/StripChartGeometry.h
)

target_include_directories(M4Decimator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(M4Decimator
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

set_target_properties(M4Decimator PROPERTIES
    AUTOMOC ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(CircularBargraphSegments OBJECT
    internal/CircularBargraphSegments.cpp
    internal/CircularBargraphSegments.h
)

target_include_directories(CircularBargraphSegments
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(CircularBargraphSegments
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

set_target_properties(CircularBargraphSegments PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(LinearGaugeScale OBJECT
    internal/LinearGaugeScale.cpp
    internal/LinearGaugeScale.h
)

target_include_directories(LinearGaugeScale
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(LinearGaugeScale PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(IndustrialSwitchLayout OBJECT
    internal/IndustrialSwitchLayout.cpp
    internal/IndustrialSwitchLayout.h
)

target_include_directories(IndustrialSwitchLayout
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(IndustrialSwitchLayout
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

set_target_properties(IndustrialSwitchLayout PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(RadialGaugeLayout OBJECT
    internal/RadialGaugeLayout.cpp
    internal/RadialGaugeLayout.h
)

target_include_directories(RadialGaugeLayout
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(RadialGaugeLayout PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(FontScale OBJECT
    internal/FontScale.cpp
    internal/FontScale.h
)

target_include_directories(FontScale
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(FontScale
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

set_target_properties(FontScale PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(LevelMeterLayout OBJECT
    internal/LevelMeterLayout.cpp
    internal/LevelMeterLayout.h
)

target_include_directories(LevelMeterLayout
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(LevelMeterLayout
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

set_target_properties(LevelMeterLayout PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

add_library(AnalysisChartView OBJECT
    internal/AnalysisChartView.cpp
    internal/AnalysisChartView.h
    internal/AnalysisPyramid.cpp
    internal/AnalysisPyramid.h
)

target_include_directories(AnalysisChartView
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(AnalysisChartView
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

set_target_properties(AnalysisChartView PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Shared or static as BUILD_SHARED_LIBS says, shared when it is not set (#107). Decided here rather than with a global
# option(), which would also make shared the untyped libraries of a project that adds this one with FetchContent.
if(NOT DEFINED BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
    set(QTINDUSTRIALWIDGETS_LIBRARY_TYPE SHARED)
else()
    set(QTINDUSTRIALWIDGETS_LIBRARY_TYPE STATIC)
endif()
add_library(QtIndustrialWidgets ${QTINDUSTRIALWIDGETS_LIBRARY_TYPE}
    RadialGauge.cpp
    LinearGauge.cpp
    SevenSegmentDisplay.cpp
    LedIndicator.cpp
    IndustrialKnob.cpp
    StripChart.cpp
    AnalysisChart.cpp
    IndustrialSwitch.cpp
    LevelMeter.cpp
    AnnunciatorPanel.cpp
    Compass.cpp
    CircularBargraph.cpp
    $<TARGET_OBJECTS:NumberFormat>
    $<TARGET_OBJECTS:M4Decimator>
    $<TARGET_OBJECTS:CircularBargraphSegments>
    $<TARGET_OBJECTS:LinearGaugeScale>
    $<TARGET_OBJECTS:RadialGaugeLayout>
    $<TARGET_OBJECTS:IndustrialSwitchLayout>
    $<TARGET_OBJECTS:LevelMeterLayout>
    $<TARGET_OBJECTS:FontScale>
    $<TARGET_OBJECTS:AnalysisChartView>
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/qtindustrialwidgets_global.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/RadialGauge.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/LinearGauge.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/SevenSegmentDisplay.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/LedIndicator.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/IndustrialKnob.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/StripChart.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/AnalysisChart.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/IndustrialSwitch.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/LevelMeter.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/AnnunciatorPanel.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/Compass.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/CircularBargraph.h
)

add_library(QtIndustrialWidgets::QtIndustrialWidgets ALIAS QtIndustrialWidgets)

# QTINDUSTRIALWIDGETS_EXPORT (qtindustrialwidgets_global.h): exported symbols when building the shared library,
# nothing for a static one; the static definition is public, so the exported target passes it to consumers
if(QTINDUSTRIALWIDGETS_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(QtIndustrialWidgets PRIVATE QTINDUSTRIALWIDGETS_LIBRARY)
else()
    target_compile_definitions(QtIndustrialWidgets PUBLIC QTINDUSTRIALWIDGETS_STATIC)
endif()

target_include_directories(QtIndustrialWidgets
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(QtIndustrialWidgets
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
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
