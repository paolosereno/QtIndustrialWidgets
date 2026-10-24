/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QString>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief Fixed-point text of \a value with \a decimals digits after the point, never a negative zero.
 *
 * Same text as QString::number(value, 'f', decimals), except that a result showing zero has no
 * sign: a negative value that rounds to zero (e.g. -0.04 with 1 decimal, -0.3 with 0) and -0.0
 * give "0.0" / "0", not "-0.0" / "-0" (#46). Values that do not round to zero are unchanged,
 * including the rounding boundary (-0.05 with 1 decimal is "-0.1"). Non-finite values give the
 * same text as QString::number().
 *
 * Every fixed-point number shown by the widgets goes through this function; tst_FixedFormatUsage
 * fails on QString::number(..., 'f', ...) or .arg(..., 'f', ...) anywhere else in src/.
 */
QString formatFixed(double value, int decimals);

/**
 * \brief Text of a scale label of RadialGauge, LinearGauge and IndustrialKnob.
 *
 * With \a precision 0 the value rounded to the nearest integer; otherwise no decimals for a whole
 * value and one decimal for any other value, through formatFixed().
 */
QString scaleLabelText(double value, int precision);

} // namespace internal
} // namespace QtIndustrialWidgets
