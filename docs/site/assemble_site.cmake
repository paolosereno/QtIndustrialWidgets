# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

# Assembles the GitHub Pages site (target "site", see docs/CMakeLists.txt):
#   SITE_DIR/            landing page (index.html, site.css from SITE_SOURCE_DIR)
#   SITE_DIR/api/        the Doxygen HTML output (HTML_DIR), with the gallery images
#   SITE_DIR/<page>.html a redirect stub for every Doxygen page, at the address it had before the site moved under
#                        api/ (the documentation used to be published at the root)
# Usage: cmake -DHTML_DIR=... -DSITE_SOURCE_DIR=... -DSITE_DIR=... -P assemble_site.cmake

foreach(var HTML_DIR SITE_SOURCE_DIR SITE_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "assemble_site.cmake: ${var} is not set")
    endif()
endforeach()
if(NOT EXISTS "${HTML_DIR}/index.html")
    message(FATAL_ERROR "assemble_site.cmake: no Doxygen output in ${HTML_DIR} (build the 'docs' target first)")
endif()

set(SITE_URL "https://paolosereno.github.io/QtIndustrialWidgets")
# Guide pages were published under names that contained the CI runner's source path
set(OLD_GUIDE_PREFIX "md__2home_2runner_2work_2QtIndustrialWidgets_2QtIndustrialWidgets_2docs_2")

file(REMOVE_RECURSE "${SITE_DIR}")
file(MAKE_DIRECTORY "${SITE_DIR}")
file(COPY "${HTML_DIR}/" DESTINATION "${SITE_DIR}/api")
file(COPY "${SITE_SOURCE_DIR}/index.html" "${SITE_SOURCE_DIR}/site.css" DESTINATION "${SITE_DIR}")

function(write_stub stub_name target)
    file(WRITE "${SITE_DIR}/${stub_name}"
"<!DOCTYPE html>
<html lang=\"en\">
<head>
<meta charset=\"utf-8\">
<meta name=\"robots\" content=\"noindex\">
<meta http-equiv=\"refresh\" content=\"0; url=api/${target}\">
<link rel=\"canonical\" href=\"${SITE_URL}/api/${target}\">
<title>Moved to api/${target}</title>
</head>
<body>
<p>This page has moved to <a href=\"api/${target}\">api/${target}</a>.</p>
</body>
</html>
")
endfunction()

file(GLOB pages RELATIVE "${HTML_DIR}" "${HTML_DIR}/*.html")
set(stubs 0)
foreach(page IN LISTS pages)
    if(page STREQUAL "index.html")
        continue() # the root index.html is the landing page
    endif()
    write_stub("${page}" "${page}")
    math(EXPR stubs "${stubs} + 1")
    if(page MATCHES "^md_docs_2(.+)$")
        write_stub("${OLD_GUIDE_PREFIX}${CMAKE_MATCH_1}" "${page}")
        math(EXPR stubs "${stubs} + 1")
    endif()
endforeach()
message(STATUS "Site assembled in ${SITE_DIR}: landing page, api/, ${stubs} redirect stubs")
