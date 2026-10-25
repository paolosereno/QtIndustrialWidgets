/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * \file qtindustrialwidgets_global.h
 * \brief Library export macros and version definitions for QtIndustrialWidgets.
 */

#pragma once

#include <QtCore/qglobal.h>

/**
 * \def QTINDUSTRIALWIDGETS_EXPORT
 * \brief Export/import macro for shared library symbol visibility across dynamic linking boundaries.
 *
 * Empty in a static build: the CMake target defines QTINDUSTRIALWIDGETS_STATIC for the library and for every
 * consumer when the library is built with BUILD_SHARED_LIBS=OFF.
 */
#if defined(QTINDUSTRIALWIDGETS_LIBRARY)
#  define QTINDUSTRIALWIDGETS_EXPORT Q_DECL_EXPORT
#elif defined(QTINDUSTRIALWIDGETS_STATIC)
#  define QTINDUSTRIALWIDGETS_EXPORT
#else
#  define QTINDUSTRIALWIDGETS_EXPORT Q_DECL_IMPORT
#endif

#define QTINDUSTRIALWIDGETS_VERSION_MAJOR 2
#define QTINDUSTRIALWIDGETS_VERSION_MINOR 5
#define QTINDUSTRIALWIDGETS_VERSION_PATCH 1
#define QTINDUSTRIALWIDGETS_VERSION_STR   "2.5.1"

namespace QtIndustrialWidgets {}
namespace qiw = QtIndustrialWidgets;
