# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Test of qiw_designer_plugin_dir() (cmake/DesignerPluginDir.cmake, #135): where `cmake --install` puts the Qt Designer
# plugin. QT_INSTALL_PLUGINS set by hand wins; otherwise Qt's plugin directory (QT6_INSTALL_PLUGINS) when the install
# prefix is Qt's prefix; otherwise <libdir>/qt6/plugins/designer, as before #135.
#
# Usage: cmake -DSOURCE_DIR=<repository> -DWORK_DIR=<empty scratch directory> -P DesignerPluginDirTest.cmake

# The policies of the project (CMakeLists.txt), under which plugin/CMakeLists.txt calls the function; without this a
# script runs with the CMake 2.x policies (if(TRUE) would read a variable named TRUE, CMP0012)
cmake_minimum_required(VERSION 3.16)
foreach(var SOURCE_DIR WORK_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()
include("${SOURCE_DIR}/cmake/DesignerPluginDir.cmake")

set(failures 0)
# check(<case> <expected dir> <expected reason> <arguments of qiw_designer_plugin_dir>...)
function(check name expected_dir expected_reason)
    qiw_designer_plugin_dir(dir reason ${ARGN})
    if(dir STREQUAL expected_dir AND reason STREQUAL expected_reason)
        message(STATUS "ok    ${name}: ${dir} (${reason})")
    else()
        message(STATUS "FAIL  ${name}: got '${dir}' (${reason}), expected '${expected_dir}' (${expected_reason})")
        math(EXPR n "${failures} + 1")
        set(failures ${n} PARENT_SCOPE)
    endif()
endfunction()

set(qt "${WORK_DIR}/Qt/6.2.4/gcc_64")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${qt}")
file(CREATE_LINK "${qt}" "${WORK_DIR}/qt-link" SYMBOLIC)

set(default "lib/qt6/plugins/designer")
set(qtdir "plugins/designer")

# The install prefix is Qt's prefix: Qt's plugin directory, however the same directory is written
check("same prefix" "${qtdir}" "qt"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins)
check("same prefix, trailing slash" "${qtdir}" "qt"
      INSTALL_PREFIX "${qt}/" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins)
check("same prefix, '..' segments" "${qtdir}" "qt"
      INSTALL_PREFIX "${qt}/../gcc_64" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins)
check("same prefix, through a symlink" "${qtdir}" "qt"
      INSTALL_PREFIX "${WORK_DIR}/qt-link" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins)
check("Debian layout" "lib/x86_64-linux-gnu/qt6/plugins/designer" "qt"
      INSTALL_PREFIX "/usr" LIBDIR lib/x86_64-linux-gnu QT_PREFIX "/usr" QT_PLUGINS lib/x86_64-linux-gnu/qt6/plugins)
check("letter case, ignored when asked (Windows)" "${qtdir}" "qt"
      INSTALL_PREFIX "C:/QT/6.6.3/MSVC2019_64" LIBDIR lib QT_PREFIX "C:/Qt/6.6.3/msvc2019_64" QT_PLUGINS plugins
      IGNORE_CASE)
# Qt's plugin directory as Qt's package writes it, normalized (an install() DESTINATION with "." segments raises the
# CMP0177 warning): aqt's Qt 6.2.4 gives "./plugins"
check("Qt plugins './plugins' (aqt)" "${qtdir}" "qt"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS ./plugins)
check("Qt plugins with a trailing slash" "${qtdir}" "qt"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins/)
check("Qt plugins with '..' segments" "${qtdir}" "qt"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS lib/../plugins)

# Another prefix: as before #135
check("other prefix" "${default}" "default"
      INSTALL_PREFIX "/opt/QtIndustrialWidgets" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins)
check("prefix inside Qt's prefix" "${default}" "default"
      INSTALL_PREFIX "${qt}/sub" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins)
check("letter case, significant elsewhere" "${default}" "default"
      INSTALL_PREFIX "/OPT/QT" LIBDIR lib QT_PREFIX "/opt/Qt" QT_PLUGINS plugins)
check("multiarch libdir, other prefix" "lib/x86_64-linux-gnu/qt6/plugins/designer" "default"
      INSTALL_PREFIX "/usr/local" LIBDIR lib/x86_64-linux-gnu QT_PREFIX "/usr" QT_PLUGINS lib/x86_64-linux-gnu/qt6/plugins)

# Qt's package without the variables: as before #135
check("no QT6_INSTALL_PREFIX" "${default}" "default"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PLUGINS plugins)
check("no QT6_INSTALL_PLUGINS" "${default}" "default"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PREFIX "${qt}")

# QT_INSTALL_PLUGINS set by hand wins, also when the prefixes are the same
check("manual, same prefix" "/srv/plugins/designer" "manual"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins HAS_MANUAL MANUAL "/srv/plugins")
check("manual, other prefix" "share/plugins/designer" "manual"
      INSTALL_PREFIX "/opt/x" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins HAS_MANUAL MANUAL "share/plugins")

check("manual, empty: as before #135" "/designer" "manual"
      INSTALL_PREFIX "${qt}" LIBDIR lib QT_PREFIX "${qt}" QT_PLUGINS plugins HAS_MANUAL MANUAL "")

file(REMOVE_RECURSE "${WORK_DIR}")
if(NOT failures EQUAL 0)
    message(FATAL_ERROR "${failures} case(s) failed")
endif()
