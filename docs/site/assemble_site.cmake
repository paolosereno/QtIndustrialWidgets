# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Assembles the GitHub Pages site (target "site", see docs/CMakeLists.txt):
#   SITE_DIR/            landing page (index.html, site.css from SITE_SOURCE_DIR)
#   SITE_DIR/api/        the Doxygen HTML output (HTML_DIR), with the gallery images
#   SITE_DIR/<page>.html a redirect stub for every Doxygen page, at the address it had before the site moved under
#                        api/ (the documentation used to be published at the root)
#   SITE_DIR/api/class<Name>.html and SITE_DIR/class<Name>.html
#                        redirect stubs for the pages of the classes listed in MOVED_CLASSES, which Doxygen made
#                        outside the namespace until #144; they lead to api/classQtIndustrialWidgets_1_1<Name>.html
#   SITE_DIR/google*.html the Google Search Console verification file(s) from SITE_SOURCE_DIR, unchanged
#   SITE_DIR/sitemap.xml the landing page and every page under api/ (not the redirect stubs)
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
# Classes whose description came before the namespace until #144: Doxygen made a second page for each, outside the
# namespace (class<Name>.html), which was published under api/ and, as a redirect stub, at the root
set(MOVED_CLASSES AnnunciatorPanel IndustrialSwitch LedIndicator LevelMeter LinearGauge RadialGauge SevenSegmentDisplay)

file(REMOVE_RECURSE "${SITE_DIR}")
file(MAKE_DIRECTORY "${SITE_DIR}")
file(COPY "${HTML_DIR}/" DESTINATION "${SITE_DIR}/api")
file(COPY "${SITE_SOURCE_DIR}/index.html" "${SITE_SOURCE_DIR}/site.css" DESTINATION "${SITE_DIR}")
file(GLOB verification_files "${SITE_SOURCE_DIR}/google*.html")
file(COPY ${verification_files} DESTINATION "${SITE_DIR}")

# A redirect stub at SITE_DIR/<stub_path> to the Doxygen page api/<target>; href is the link from the stub's directory
function(write_redirect stub_path href target)
    file(WRITE "${SITE_DIR}/${stub_path}"
"<!DOCTYPE html>
<html lang=\"en\">
<head>
<meta charset=\"utf-8\">
<meta name=\"robots\" content=\"noindex\">
<meta http-equiv=\"refresh\" content=\"0; url=${href}\">
<link rel=\"canonical\" href=\"${SITE_URL}/api/${target}\">
<title>Moved to api/${target}</title>
</head>
<body>
<p>This page has moved to <a href=\"${href}\">api/${target}</a>.</p>
</body>
</html>
")
endfunction()

# A redirect stub at the root to api/<target>
function(write_stub stub_name target)
    write_redirect("${stub_name}" "api/${target}" "${target}")
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
foreach(name IN LISTS MOVED_CLASSES)
    set(old "class${name}.html")
    set(new "classQtIndustrialWidgets_1_1${name}.html")
    if(EXISTS "${HTML_DIR}/${old}")
        message(FATAL_ERROR "assemble_site.cmake: Doxygen made ${old} again (a class description outside the namespace?)")
    endif()
    if(NOT EXISTS "${HTML_DIR}/${new}")
        message(FATAL_ERROR "assemble_site.cmake: no ${new} for the redirect stubs of ${old}")
    endif()
    write_redirect("api/${old}" "${new}" "${new}")
    write_stub("${old}" "${new}")
    math(EXPR stubs "${stubs} + 2")
endforeach()
# Sitemap: the landing page and the Doxygen pages, at their api/ addresses (not the redirect stubs)
set(sitemap "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n")
string(APPEND sitemap "  <url><loc>${SITE_URL}/</loc></url>\n")
list(SORT pages)
foreach(page IN LISTS pages)
    string(APPEND sitemap "  <url><loc>${SITE_URL}/api/${page}</loc></url>\n")
endforeach()
string(APPEND sitemap "</urlset>\n")
file(WRITE "${SITE_DIR}/sitemap.xml" "${sitemap}")
list(LENGTH pages api_pages)
math(EXPR sitemap_urls "${api_pages} + 1")

message(STATUS "Site assembled in ${SITE_DIR}: landing page, api/, ${stubs} redirect stubs, sitemap with ${sitemap_urls} URLs")
