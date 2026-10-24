# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Registers a test executable at the device pixel ratios the library supports. The DPR is set
# with QT_SCALE_FACTOR (also honoured by the offscreen QPA): <name> runs at 1, <name>_dpr125 at
# 1.25, <name>_dpr150 at 1.5 and <name>_hidpi at 2.
#
# QT_FORCE_STDERR_LOGGING=1: on Windows, without a console (as under ctest in CI), QtTest sends its
# log to OutputDebugString instead of stdout, so ctest showed no output at all (#36).
function(add_dpr_tests test_name)
    set(log_env "QT_FORCE_STDERR_LOGGING=1")
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1;${log_env}")
    add_test(NAME ${test_name}_dpr125 COMMAND ${test_name})
    set_tests_properties(${test_name}_dpr125 PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1.25;${log_env}")
    add_test(NAME ${test_name}_dpr150 COMMAND ${test_name})
    set_tests_properties(${test_name}_dpr150 PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1.5;${log_env}")
    add_test(NAME ${test_name}_hidpi COMMAND ${test_name})
    set_tests_properties(${test_name}_hidpi PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=2;${log_env}")
endfunction()

function(add_industrial_widget_test test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name}
        PRIVATE
            QtIndustrialWidgets::QtIndustrialWidgets
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
    )
    target_include_directories(${test_name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    set_target_properties(${test_name} PROPERTIES
        AUTOMOC ON
    )
    add_dpr_tests(${test_name})
endfunction()

add_executable(tst_RadialGauge
    tst_RadialGauge.cpp
    $<TARGET_OBJECTS:LinearGaugeScale>
    $<TARGET_OBJECTS:RadialGaugeLayout>
)
target_link_libraries(tst_RadialGauge
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_RadialGauge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_RadialGauge PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_RadialGauge)
add_executable(tst_LinearGauge
    tst_LinearGauge.cpp
    $<TARGET_OBJECTS:LinearGaugeScale>
    $<TARGET_OBJECTS:FontScale>
)
target_link_libraries(tst_LinearGauge
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_LinearGauge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_LinearGauge PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_LinearGauge)
add_executable(tst_RadialGaugeLayout
    tst_RadialGaugeLayout.cpp
    $<TARGET_OBJECTS:RadialGaugeLayout>
    $<TARGET_OBJECTS:LinearGaugeScale>
)
target_link_libraries(tst_RadialGaugeLayout
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(tst_RadialGaugeLayout
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_RadialGaugeLayout PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_RadialGaugeLayout)
add_industrial_widget_test(tst_SevenSegmentDisplay)
add_industrial_widget_test(tst_LedIndicator)
add_industrial_widget_test(tst_IndustrialKnob)
add_executable(tst_StripChart
    tst_StripChart.cpp
    $<TARGET_OBJECTS:M4Decimator>
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_StripChart
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_StripChart
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_StripChart PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_StripChart)
add_executable(tst_AnalysisChartView
    tst_AnalysisChartView.cpp
    $<TARGET_OBJECTS:AnalysisChartView>
    $<TARGET_OBJECTS:M4Decimator>
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_AnalysisChartView
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
target_include_directories(tst_AnalysisChartView
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_AnalysisChartView PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_AnalysisChartView)
add_executable(tst_AnalysisChart
    tst_AnalysisChart.cpp
    $<TARGET_OBJECTS:M4Decimator>
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_AnalysisChart
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_AnalysisChart
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_AnalysisChart PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_AnalysisChart)

add_executable(tst_IndustrialSwitch
    tst_IndustrialSwitch.cpp
    $<TARGET_OBJECTS:IndustrialSwitchLayout>
)
target_link_libraries(tst_IndustrialSwitch
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_IndustrialSwitch
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_IndustrialSwitch PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_IndustrialSwitch)
add_executable(tst_LevelMeter
    tst_LevelMeter.cpp
    $<TARGET_OBJECTS:LevelMeterLayout>
    $<TARGET_OBJECTS:NumberFormat>
    $<TARGET_OBJECTS:LinearGaugeScale>
)
target_link_libraries(tst_LevelMeter
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_LevelMeter
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_LevelMeter PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_LevelMeter)
add_executable(tst_LevelMeterLayout
    tst_LevelMeterLayout.cpp
    $<TARGET_OBJECTS:LevelMeterLayout>
    $<TARGET_OBJECTS:NumberFormat>
    $<TARGET_OBJECTS:LinearGaugeScale>
)
target_link_libraries(tst_LevelMeterLayout
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)
target_include_directories(tst_LevelMeterLayout
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_LevelMeterLayout PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_LevelMeterLayout)
add_industrial_widget_test(tst_AnnunciatorPanel)
add_industrial_widget_test(tst_Compass)
add_executable(tst_CircularBargraph
    tst_CircularBargraph.cpp
    $<TARGET_OBJECTS:CircularBargraphSegments>
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_CircularBargraph
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_CircularBargraph
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_CircularBargraph PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_CircularBargraph)

add_executable(tst_M4Decimator
    tst_M4Decimator.cpp
    $<TARGET_OBJECTS:M4Decimator>
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_M4Decimator
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_M4Decimator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_M4Decimator PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_M4Decimator)

# Fixed-point text without negative zero (#46)
add_executable(tst_NumberFormat
    tst_NumberFormat.cpp
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_NumberFormat
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_include_directories(tst_NumberFormat
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_NumberFormat PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_NumberFormat)

# No fixed-point formatting outside internal/NumberFormat (#46)
add_executable(tst_FixedFormatUsage tst_FixedFormatUsage.cpp)
target_link_libraries(tst_FixedFormatUsage
    PRIVATE
        Qt6::Core
        Qt6::Test
)
target_compile_definitions(tst_FixedFormatUsage
    PRIVATE
        QIW_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src"
)
set_target_properties(tst_FixedFormatUsage PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_FixedFormatUsage)

# Every public widget has a showcase in the gallery's image export list (#26)
add_executable(tst_GalleryShowcases
    tst_GalleryShowcases.cpp
    ${PROJECT_SOURCE_DIR}/examples/gallery/Showcases.cpp
)
target_link_libraries(tst_GalleryShowcases
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)
target_include_directories(tst_GalleryShowcases
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/examples/gallery
)
target_compile_definitions(tst_GalleryShowcases
    PRIVATE
        QIW_PUBLIC_HEADER_DIR="${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets"
)
set_target_properties(tst_GalleryShowcases PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_GalleryShowcases)

# #8: heap allocations per frame must not grow with the number of visible gaps. Both executables replace the global
# operator new with a counter (tests/AllocationCounter.h); each test first checks that the counter sees an allocation
# it knows of.
# - tst_M4DecimatorAllocations counts the allocations of M4Decimator, compiled into the executable: they are counted
#   on every platform.
# - tst_StripChartAllocations counts the allocations of a StripChart render, made inside the shared library and Qt.
#   A replacement operator new in the executable sees them only on Linux (ELF symbol binding); with a Windows DLL or a
#   macOS two-level-namespace dylib the library's calls do not reach it, so the test would measure nothing there. It is
#   built and run on Linux only.
add_executable(tst_M4DecimatorAllocations
    tst_M4DecimatorAllocations.cpp
    $<TARGET_OBJECTS:M4Decimator>
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_M4DecimatorAllocations PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(tst_M4DecimatorAllocations PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
set_target_properties(tst_M4DecimatorAllocations PROPERTIES AUTOMOC ON)
add_test(NAME tst_M4DecimatorAllocations COMMAND tst_M4DecimatorAllocations)
set_tests_properties(tst_M4DecimatorAllocations PROPERTIES ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1")

# #97: AnalysisPyramid (in the AnalysisChartView objects) is compiled into the executable: counted on every platform
add_executable(tst_AnalysisPyramidAllocations
    tst_AnalysisPyramidAllocations.cpp
    $<TARGET_OBJECTS:AnalysisChartView>
    $<TARGET_OBJECTS:M4Decimator>
    $<TARGET_OBJECTS:NumberFormat>
)
target_link_libraries(tst_AnalysisPyramidAllocations PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(tst_AnalysisPyramidAllocations PRIVATE ${PROJECT_SOURCE_DIR}/src)
set_target_properties(tst_AnalysisPyramidAllocations PROPERTIES AUTOMOC ON)
add_test(NAME tst_AnalysisPyramidAllocations COMMAND tst_AnalysisPyramidAllocations)
set_tests_properties(tst_AnalysisPyramidAllocations PROPERTIES ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1")

if(UNIX AND NOT APPLE)
    add_executable(tst_StripChartAllocations
        tst_StripChartAllocations.cpp
    )
    target_link_libraries(tst_StripChartAllocations
        PRIVATE
            QtIndustrialWidgets::QtIndustrialWidgets
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
    )
    target_include_directories(tst_StripChartAllocations PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
    set_target_properties(tst_StripChartAllocations PROPERTIES AUTOMOC ON)
    add_test(NAME tst_StripChartAllocations COMMAND tst_StripChartAllocations)

    # #97: the same for AnalysisChart (allocations inside the shared library: Linux only, see above)
    add_executable(tst_AnalysisChartAllocations
        tst_AnalysisChartAllocations.cpp
    )
    target_link_libraries(tst_AnalysisChartAllocations
        PRIVATE
            QtIndustrialWidgets::QtIndustrialWidgets
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Test
    )
    target_include_directories(tst_AnalysisChartAllocations PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
    set_target_properties(tst_AnalysisChartAllocations PROPERTIES AUTOMOC ON)
    add_test(NAME tst_AnalysisChartAllocations COMMAND tst_AnalysisChartAllocations)
endif()

# CITATION.cff must carry the project() version, so that a release does not leave it behind (#130).
add_test(NAME citation_version
    COMMAND ${CMAKE_COMMAND} -DCITATION_FILE=${PROJECT_SOURCE_DIR}/CITATION.cff
            -DEXPECTED_VERSION=${PROJECT_VERSION}
            -P ${PROJECT_SOURCE_DIR}/cmake/CheckCitationVersion.cmake)

# The installed Qt Designer plugin must load from any prefix without a library search path (#127).
# Windows has no run path (the DLL must be on PATH or next to designer.exe), and a plugin directory
# outside the prefix (QT_INSTALL_PLUGINS) would be written outside the temporary prefix.
get_property(designer_plugin_install_dir GLOBAL PROPERTY QTINDUSTRIALWIDGETS_DESIGNER_PLUGIN_INSTALL_DIR)
if(BUILD_DESIGNER_PLUGIN AND NOT WIN32 AND NOT IS_ABSOLUTE "${designer_plugin_install_dir}")
    add_executable(designer_plugin_loader designer_plugin_loader.cpp)
    target_link_libraries(designer_plugin_loader PRIVATE Qt6::Widgets Qt6::Designer)
    add_test(NAME installed_designer_plugin
        COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${PROJECT_BINARY_DIR}
                -DPREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed_designer_plugin
                -DPLUGIN=${designer_plugin_install_dir}/$<TARGET_FILE_NAME:QtIndustrialWidgetsPlugin>
                -DLOADER=$<TARGET_FILE:designer_plugin_loader>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckInstalledDesignerPlugin.cmake)
    set_tests_properties(installed_designer_plugin PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# benchmarks/merge_runs.py merges the runs of one benchmark session; its tests use example runs
# in benchmarks/testdata and only the Python standard library (skipped when Python is not found).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME merge_runs
        COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s ${PROJECT_SOURCE_DIR}/benchmarks -p test_merge_runs.py -v)
    set_tests_properties(merge_runs PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()
