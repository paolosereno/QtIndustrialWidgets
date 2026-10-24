/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QFont>

#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/// Font of the LevelMeter scale labels: the widget font at 70 %, at least 6 pt.
QFont levelMeterScaleFont(const QFont &widgetFont);

/// Scale tick values: the standard dB ladder for a "dB" unit with a range across 0, otherwise 6
/// linear divisions (7 ticks).
std::vector<double> levelMeterTicks(double minimum, double maximum, const QString &unit);

/// Text of a scale tick: a signed integer for "dB", otherwise 1 decimal below a 10-unit range.
QString levelMeterTickText(double tick, double minimum, double maximum, const QString &unit);

/// Ink-aware widths (logical px, the union of advance and glyph ink) of the scale labels.
struct LevelMeterScaleMetrics {
    double widest{0.0};
    double first{0.0}; ///< label of the minimum
    double last{0.0};  ///< label of the maximum
    bool linear{true}; ///< evenly spaced ticks (not the dB ladder)
    double lineHeight{0.0}; ///< line height of the scale font (also the unit font)
    double minGap{4.0};     ///< scaleLabelMinGap() of the scale font (#57)
    double unitWidth{0.0};  ///< width of the unit text in the scale font (0 without a unit)
    double endsWidth{0.0};  ///< half the sum of the widths of the first and last tick labels
    double endsFraction{1.0}; ///< fraction of the range between the first and last ticks
};

LevelMeterScaleMetrics levelMeterScaleMetrics(double minimum, double maximum, const QString &unit, const QFont &widgetFont);

/// Vertical meter: width reserved right of the bars for tick (7 px), gap (3 px), widest label and
/// 2 px, never less than the 36 px used before #40; with a unit, also room for it (4 px from the
/// bars, its width and 2 px, #57).
double levelMeterVerticalScaleMargin(const LevelMeterScaleMetrics &m);

/// Vertical meter with a unit: height reserved above the bars so that the unit, drawn above the top
/// label, keeps minGap from it (#57); 0 without a unit, so such meters keep their layout.
double levelMeterVerticalUnitBand(const LevelMeterScaleMetrics &m);

/// Horizontal meter: height reserved below the bars for the scale (tick, gap and the label
/// rectangle), never less than the 36 px used before #57.
double levelMeterHorizontalScaleMargin(const LevelMeterScaleMetrics &m);

/// Horizontal meter: height of the rectangle a scale label is centred in, 2 px below the tick: the
/// 14 px used before, or the line height of the scale font when taller, so that the text never
/// rises over the ticks (#57).
double levelMeterHorizontalLabelHeight(const LevelMeterScaleMetrics &m);

/// Horizontal meter with a unit: distance from the end of the bars to the left of the unit text. The
/// unit is vertically centred on the last bar (\a barHeight high); 4 px when its line keeps minGap
/// above the ticks, otherwise right of the last label by minGap (#57).
double levelMeterHorizontalUnitOffset(const LevelMeterScaleMetrics &m, double barHeight);

/// Horizontal meter: room kept at each end of the bars so that the end labels, centred on the end
/// ticks, stay inside the chassis (\a channelLabelMargin is the room already left of the bars).
struct LevelMeterInsets {
    double left{0.0};
    double right{0.0};
};
/// With a unit, the right inset also holds the unit at levelMeterHorizontalUnitOffset() (#57).
LevelMeterInsets levelMeterHorizontalInsets(const LevelMeterScaleMetrics &m, double channelLabelMargin, double barHeight);

/**
 * \brief Chooses which scale labels are drawn so that drawn labels never overlap (#57).
 *
 * \a centres and \a extents give each label's centre and size along the scale (one per tick of
 * levelMeterTicks(), in that order). Two labels keep \a minGap when the free space between them,
 * |c_i - c_j| - (e_i + e_j) / 2, is at least \a minGap. Every pair of drawn labels is checked.
 *
 * The first and last labels (the ends of the scale) are always drawn; if they are closer than
 * \a minGap to each other, only the first one is (the rule of the library: an end wins).
 * - Linear scale: every k-th label, k the smallest divisor of the number of intervals that keeps
 *   the gap (as LinearGauge, #31, and RadialGauge, #50).
 * - dB ladder: labels are taken in priority order, each one only if it keeps the gap from every
 *   label already taken: the ends, then 0 dB, then -20, -6, -40, -12, -30, -3, +3, then any other
 *   tick in increasing order. Ticks outside the range are not in \a ticks.
 *
 * Returns the drawn indices in increasing order. Non-finite centres or extents clash with every
 * other label; a non-finite \a minGap is treated as 0.
 */
std::vector<int> levelMeterSelectLabels(const std::vector<double> &ticks, const std::vector<double> &centres,
                                        const std::vector<double> &extents, bool linear, double minGap);

/// Extent of each scale label along the scale: the line height for a vertical meter, the ink-aware
/// width for a horizontal one (same order as levelMeterTicks()).
std::vector<double> levelMeterLabelExtents(Qt::Orientation orientation, double minimum, double maximum,
                                           const QString &unit, const QFont &widgetFont);

/**
 * \brief Minimum size of a LevelMeter from its scale labels (#40).
 *
 * Vertical: width for the bars (6 px each, 4 px gaps) and the scale margin; height for bars long
 * enough that every linear label and both dB ends are drawn, with the title, the unit band and the
 * channel labels (#51). Horizontal: room for the
 * end-label insets (and the unit) and bars long enough that the two end labels (centred on their
 * ticks) keep minGap between them and, for a linear scale, that neighbouring labels do too, so that
 * every linear label and both dB ends are drawn (#57); height for the margins, the title, the bars and
 * the scale (#57). Never smaller than \a base.
 */
QSize levelMeterMinimumSize(Qt::Orientation orientation, int channels, bool scaleVisible, bool channelLabels,
                            bool title, const LevelMeterScaleMetrics &m, const QSize &base);

} // namespace internal
} // namespace QtIndustrialWidgets
