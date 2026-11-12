# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Where `cmake --install` puts the Qt Designer plugin (#135).
#
# qiw_designer_plugin_dir(<out_dir> <out_reason>
#     INSTALL_PREFIX <CMAKE_INSTALL_PREFIX> LIBDIR <CMAKE_INSTALL_LIBDIR>
#     [QT_PREFIX <QT6_INSTALL_PREFIX>] [QT_PLUGINS <QT6_INSTALL_PLUGINS>]
#     [HAS_MANUAL MANUAL <QT_INSTALL_PLUGINS>] [IGNORE_CASE])
#
# <out_dir> is, in this order:
# - "manual": <MANUAL>/designer, with HAS_MANUAL (QT_INSTALL_PLUGINS defined by hand, even empty, as before #135: it
#             takes precedence; a separate flag, since an empty one-value argument depends on policy CMP0174);
# - "qt":     <QT_PLUGINS>/designer, when INSTALL_PREFIX is the same directory as QT_PREFIX: Qt's own plugin directory,
#             where that Qt's Designer looks (Qt from the online installer or aqt, a distribution's Qt in /usr);
# - "default": <LIBDIR>/qt6/plugins/designer otherwise, also when Qt does not give QT_PREFIX or QT_PLUGINS.
# "The same directory": compared after resolving symlinks, "." and ".." and a trailing slash; IGNORE_CASE (Windows)
# compares without letter case.
function(qiw_designer_plugin_dir out_dir out_reason)
    cmake_parse_arguments(ARG "HAS_MANUAL;IGNORE_CASE" "INSTALL_PREFIX;LIBDIR;QT_PREFIX;QT_PLUGINS;MANUAL" "" ${ARGN})
    if(ARG_HAS_MANUAL)
        set(${out_dir} "${ARG_MANUAL}/designer" PARENT_SCOPE)
        set(${out_reason} "manual" PARENT_SCOPE)
        return()
    endif()
    if(ARG_QT_PREFIX AND ARG_QT_PLUGINS)
        get_filename_component(install_prefix "${ARG_INSTALL_PREFIX}" REALPATH)
        get_filename_component(qt_prefix "${ARG_QT_PREFIX}" REALPATH)
        if(ARG_IGNORE_CASE)
            string(TOLOWER "${install_prefix}" install_prefix)
            string(TOLOWER "${qt_prefix}" qt_prefix)
        endif()
        if(install_prefix STREQUAL qt_prefix)
            set(${out_dir} "${ARG_QT_PLUGINS}/designer" PARENT_SCOPE)
            set(${out_reason} "qt" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(${out_dir} "${ARG_LIBDIR}/qt6/plugins/designer" PARENT_SCOPE)
    set(${out_reason} "default" PARENT_SCOPE)
endfunction()
