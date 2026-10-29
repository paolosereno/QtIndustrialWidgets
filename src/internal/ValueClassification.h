/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/ValueState.h>

#include <QtCore/QString>
#include <QtCore/qglobal.h>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \enum InputState
 * \brief Classification of a raw input against a range (mirrors ValueState and
 * CircularBargraph::ValueState, value for value).
 */
enum class InputState : quint8 {
    Valid,      ///< Finite and within [minimum, maximum]
    Underrange, ///< Below minimum (including -Inf)
    Overrange,  ///< Above maximum (including +Inf)
    Invalid     ///< NaN
};

/** \brief Classifies a raw input against the range. */
InputState classifyInput(double raw, double minimum, double maximum);

/** \brief The public state of an input state. */
ValueState publicValueState(InputState state);

/** \brief The input state of a public state. */
InputState inputStateOf(ValueState state);

/**
 * \brief Value shown for a raw input: the input clamped to [minimum, maximum] (±Inf gives the bound), or NaN
 * when the input is NaN.
 */
double shownValue(double raw, double minimum, double maximum);

/** \brief value() comparison that treats NaN == NaN, so Invalid -> Invalid is not a change. */
bool sameShownValue(double a, double b);

/**
 * \brief Text of a gauge readout (without unit), #151.
 *
 * Valid, and a finite input outside the range: the input itself, with \a precision decimals; +Inf: '>' +
 * \a maximum; -Inf: '<' + \a minimum; Invalid: "---". An out-of-range input is also marked outside the text
 * (see the widget), so that it is not told by colour alone.
 */
QString gaugeReadoutText(InputState state, double raw, double minimum, double maximum, int precision);

} // namespace internal
} // namespace QtIndustrialWidgets
