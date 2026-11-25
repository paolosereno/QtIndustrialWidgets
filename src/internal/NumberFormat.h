/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <vector>

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
 * \brief Texts of the scale labels of RadialGauge, LinearGauge, IndustrialKnob, TankDisplay and IndustrialSlider
 * (#208), one per value of \a values (the tick values of a scale, in order), all with the same decimals.
 *
 * The decimals are as few as possible:
 * 1. the smallest number d from 0 to \a precision with which every value is written exactly, within 1e-9 of the step
 *    between adjacent values (the tick values carry floating-point error);
 * 2. otherwise \a precision, if no two adjacent labels are equal;
 * 3. otherwise the smallest d above \a precision, at most 6, with which every value is written exactly;
 * 4. otherwise the smallest d above \a precision that makes every two adjacent labels distinct;
 * 5. at most 12 decimals (the bound of readoutValueText()): a range that 12 decimals do not separate keeps its
 *    repeated labels.
 *
 * Through formatFixed(): no negative zero, and ties round as QString::number() does.
 */
QStringList scaleLabelTexts(const std::vector<double> &values, int precision);

} // namespace internal
} // namespace QtIndustrialWidgets
