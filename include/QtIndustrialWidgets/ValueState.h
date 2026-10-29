/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * \file ValueState.h
 * \brief Classification of the value shown by an indicator (#151).
 *
 * This header holds the meta-object of the QtIndustrialWidgets namespace (Q_NAMESPACE_EXPORT). A namespace can
 * have only one, so future namespace-level enums of the library go in this header.
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>

#include <QtCore/qobjectdefs.h>

namespace QtIndustrialWidgets {

Q_NAMESPACE_EXPORT(QTINDUSTRIALWIDGETS_EXPORT)

/**
 * \brief Classification of the last input of an indicator against its range.
 *
 * The enumerators and their values are those of CircularBargraph::ValueState, so a static_cast between the
 * two is exact.
 */
enum class ValueState {
    Valid,      ///< Finite and within [minimum, maximum] (bounds included)
    Underrange, ///< Below the minimum, or -Inf: shown at the minimum
    Overrange,  ///< Above the maximum, or +Inf: shown at the maximum
    Invalid     ///< NaN: shown as invalid, never as the last value; value() is NaN
};
Q_ENUM_NS(ValueState)

} // namespace QtIndustrialWidgets
