/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Pure functions of AnalysisChart (#25): view limits, nearest sample, gaps, axis ticks and labels.

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/qglobal.h>

#include <cstddef>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/** \brief Visible time range [start, end] in ns. */
struct XRange {
    qint64 start{0};
    qint64 end{0};
    [[nodiscard]] qint64 span() const { return end - start; }
};

/**
 * \brief Time gap that breaks a trace, as StripChart computes it.
 *
 * \p userThreshold when > 0. Otherwise 4 x an exponential moving average of the sampling interval,
 * updated in sample order for every strictly positive step (first step: the step itself; then
 * ema = 0.9 ema + 0.1 dt) and truncated to an integer; 0 (no gap breaks) with fewer than two
 * distinct timestamps.
 */
qint64 gapThresholdFor(const qint64 *t, std::size_t n, qint64 userThreshold);

/**
 * \brief Index of the sample nearest in time to \p query; on a tie, the earlier one. -1 if \p n is 0
 *        or \p query lies outside [t[0], t[n - 1]]. \p t must be non-decreasing; among equal
 *        timestamps the first is returned.
 */
qint64 nearestSampleIndex(const qint64 *t, std::size_t n, qint64 query);

/**
 * \brief Smallest time span the view may show: 4 x the smallest mean sample interval among
 *        \p meanIntervals (entries <= 0 are ignored), at least 1 ns.
 */
qint64 minimumXSpan(const std::vector<double> &meanIntervals);

/**
 * \brief Clamps a requested view to the limits.
 *
 * The span is kept between the minimum span and the data extent (dataEnd - dataStart): a request of
 * the whole extent or more gives exactly the data, and the minimum span is \p minSpan but never more
 * than the extent (\p minSpan alone, centred, when the data spans a single instant). A request below
 * the minimum keeps its centre. The view may leave the data by at most one view width:
 * start >= dataStart - span and end <= dataEnd + span. A reversed request is swapped first.
 */
XRange clampXRange(XRange requested, qint64 dataStart, qint64 dataEnd, qint64 minSpan);

/** \brief The full data extent as a view; \p minSpan centred on the data when it spans a single instant. */
XRange fullXRange(qint64 dataStart, qint64 dataEnd, qint64 minSpan);

/**
 * \brief Y range covering finite values in [minimum, maximum] with a 10 % margin (the StripChart
 *        autoscale margin); +-1 around a constant value. hasData is false without finite values.
 */
struct YRange {
    double minimum{0.0};
    double maximum{1.0};
    bool hasData{false};
};
YRange autoscaledYRange(double minimum, double maximum, bool hasFiniteValues);

/** \brief One axis tick: its position and label. */
struct AxisTick {
    double position{0.0}; ///< ns on the time axis, value on the value axis
    QString label;
};

/**
 * \brief Time axis ticks for \p view: StripChart's 1-2-5 step (computeTimeTicks()), aligned on
 *        \p origin. Labels: elapsed time since \p origin in the unit of the step (Relative), or
 *        \p wallClockOrigin + t as HH:mm:ss.zzz (Absolute; t itself as elapsed time from 0 when
 *        \p wallClockOrigin is invalid).
 */
std::vector<AxisTick> timeAxisTicks(XRange view, qint64 origin, double plotWidthPx, double minSpacingPx,
                                    bool absolute, const QDateTime &wallClockOrigin);

/**
 * \brief Value axis ticks on a 1-2-5 step with at least \p minSpacingPx between them; labels with as
 *        many decimals as the step needs (no negative zero).
 */
std::vector<AxisTick> valueAxisTicks(double minimum, double maximum, double plotHeightPx, double minSpacingPx);

/**
 * \brief Zooms the time range around \p anchor (ns): the span is divided by \p factor (> 1 zooms in)
 *        and \p anchor keeps its relative position. Not clamped (see clampXRange()).
 */
XRange zoomXAround(XRange view, double anchor, double factor);

/** \brief Zooms a value range around \p anchor in the same way; unchanged if the result is degenerate. */
YRange zoomYAround(double minimum, double maximum, double anchor, double factor);

/** \brief Wheel zoom factor of \p angleDelta (1/8 degree units, 120 per notch): 1.25 per notch. */
double wheelZoomFactor(int angleDelta);

/** \brief Maximum number of channel rows of the readout band; further channels share one last row. */
constexpr int kReadoutChannelRows = 6;

/** \brief Time shown in the readout: elapsed since \p origin, or wall clock (see timeAxisTicks()). */
QString readoutTimeText(qint64 t, qint64 origin, bool absolute, const QDateTime &wallClockOrigin, qint64 resolutionNs);

/** \brief Elapsed time text (signed) at \p resolutionNs, e.g. a cursor delta. */
QString durationText(qint64 ns, qint64 resolutionNs);

/** \brief Value shown in the readout: \p decimals digits after the point, "---" if absent or non-finite. */
QString readoutValueText(const double *value, int decimals);

/** \brief Decimals of the value axis labels for the range (valueAxisTicks()); the readout shows one more. */
int valueAxisDecimals(double minimum, double maximum, double plotHeightPx, double minSpacingPx);

} // namespace internal
} // namespace QtIndustrialWidgets
