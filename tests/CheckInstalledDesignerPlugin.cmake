# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Installs the build into an empty temporary prefix and loads the installed Qt Designer plugin with
# designer_plugin_loader, with no library search path set in the environment (#127): the installed
# plugin must find the installed library by itself.
#
# Usage: cmake -DBUILD_DIR=<build> -DPREFIX=<temporary prefix> -DPLUGIN=<plugin path relative to the
#              prefix> -DLOADER=<designer_plugin_loader> -P CheckInstalledDesignerPlugin.cmake
foreach(var BUILD_DIR PREFIX PLUGIN LOADER)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${PREFIX}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${PREFIX}"
                RESULT_VARIABLE install_result OUTPUT_QUIET)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "cmake --install into ${PREFIX} failed (${install_result})")
endif()

set(plugin_file "${PREFIX}/${PLUGIN}")
if(NOT EXISTS "${plugin_file}")
    message(FATAL_ERROR "the installed plugin is missing: ${plugin_file}")
endif()

# Only what the installed files carry may lead to the library
unset(ENV{LD_LIBRARY_PATH})
unset(ENV{DYLD_LIBRARY_PATH})
unset(ENV{DYLD_FALLBACK_LIBRARY_PATH})

execute_process(COMMAND "${LOADER}" "${plugin_file}"
                RESULT_VARIABLE load_result OUTPUT_VARIABLE load_output ERROR_VARIABLE load_output)
message(STATUS "${plugin_file}: ${load_output}")
if(NOT load_result EQUAL 0)
    message(FATAL_ERROR "the installed plugin cannot be used (${load_result})")
endif()
