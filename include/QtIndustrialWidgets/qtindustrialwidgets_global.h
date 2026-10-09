/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
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
 */
#if defined(QTINDUSTRIALWIDGETS_LIBRARY)
#  define QTINDUSTRIALWIDGETS_EXPORT Q_DECL_EXPORT
#elif defined(QTINDUSTRIALWIDGETS_STATIC)
#  define QTINDUSTRIALWIDGETS_EXPORT
#else
#  define QTINDUSTRIALWIDGETS_EXPORT Q_DECL_IMPORT
#endif

#define QTINDUSTRIALWIDGETS_VERSION_MAJOR 1
#define QTINDUSTRIALWIDGETS_VERSION_MINOR 0
#define QTINDUSTRIALWIDGETS_VERSION_PATCH 0
#define QTINDUSTRIALWIDGETS_VERSION_STR   "1.0.0"

namespace QtIndustrialWidgets {}
namespace qiw = QtIndustrialWidgets;
