# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Checks that the top-level `version:` of CITATION.cff equals the project() version (#130).
# Usage: cmake -DCITATION_FILE=<path> -DEXPECTED_VERSION=<version> -P CheckCitationVersion.cmake
if(NOT DEFINED CITATION_FILE OR NOT DEFINED EXPECTED_VERSION)
    message(FATAL_ERROR "CITATION_FILE and EXPECTED_VERSION are required")
endif()

file(STRINGS "${CITATION_FILE}" version_lines REGEX "^version:")
list(LENGTH version_lines count)
if(NOT count EQUAL 1)
    message(FATAL_ERROR "${CITATION_FILE}: expected one top-level 'version:' line, found ${count}")
endif()

string(REGEX REPLACE "^version:[ \t]*[\"']?([^\"' \t#]*)[\"']?.*$" "\\1" citation_version "${version_lines}")
if(NOT citation_version STREQUAL EXPECTED_VERSION)
    message(FATAL_ERROR "${CITATION_FILE}: version is '${citation_version}', project() version is "
                        "'${EXPECTED_VERSION}'; update CITATION.cff (version and date-released)")
endif()
message(STATUS "CITATION.cff version ${citation_version} matches project() version ${EXPECTED_VERSION}")
