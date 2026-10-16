/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
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
};

LevelMeterScaleMetrics levelMeterScaleMetrics(double minimum, double maximum, const QString &unit, const QFont &widgetFont);

/// Vertical meter: width reserved right of the bars for tick (7 px), gap (3 px), widest label and
/// 2 px, never less than the 36 px used before #40.
double levelMeterVerticalScaleMargin(const LevelMeterScaleMetrics &m);

/// Horizontal meter: room kept at each end of the bars so that the end labels, centred on the end
/// ticks, stay inside the chassis (\a channelLabelMargin is the room already left of the bars).
struct LevelMeterInsets {
    double left{0.0};
    double right{0.0};
};
LevelMeterInsets levelMeterHorizontalInsets(const LevelMeterScaleMetrics &m, double channelLabelMargin);

/// Gap kept between neighbouring labels of a horizontal linear scale at minimumSizeHint().
constexpr double kLevelMeterLabelGap = 4.0;

/**
 * \brief Minimum size of a LevelMeter from its scale labels (#40).
 *
 * Vertical: room for the bars (6 px each, 4 px gaps) and the scale margin. Horizontal: room for the
 * end-label insets and, for a linear scale, bars long enough that neighbouring labels keep
 * kLevelMeterLabelGap between them. Never smaller than \a base.
 */
QSize levelMeterMinimumSize(Qt::Orientation orientation, int channels, bool scaleVisible, bool channelLabels,
                            const LevelMeterScaleMetrics &m, const QSize &base);

} // namespace internal
} // namespace QtIndustrialWidgets
