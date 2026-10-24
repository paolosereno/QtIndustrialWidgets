// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "AnalysisChartView.h"

#include "M4Decimator.h"
#include "NumberFormat.h"
#include "StripChartGeometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {
namespace internal {

qint64 gapThresholdFor(const qint64 *t, std::size_t n, qint64 userThreshold)
{
    if (userThreshold > 0) {
        return userThreshold;
    }
    if (t == nullptr || n < 2) {
        return 0;
    }
    // StripChartPrivate::appendSample(): the EMA is updated for every strictly positive step
    double ema = 0.0;
    bool hasEma = false;
    for (std::size_t i = 1; i < n; ++i) {
        if (t[i] > t[i - 1]) {
            const double dt = static_cast<double>(t[i] - t[i - 1]);
            ema = hasEma ? 0.9 * ema + 0.1 * dt : dt;
            hasEma = true;
        }
    }
    return (hasEma && ema > 0.0) ? static_cast<qint64>(4.0 * ema) : 0;
}

qint64 nearestSampleIndex(const qint64 *t, std::size_t n, qint64 query)
{
    if (t == nullptr || n == 0 || query < t[0] || query > t[n - 1]) {
        return -1;
    }
    // First sample at or after the query
    const qint64 *after = std::lower_bound(t, t + n, query);
    const std::size_t i = static_cast<std::size_t>(after - t);
    if (i == 0) {
        return 0;
    }
    if (*after == query) {
        return static_cast<qint64>(i);
    }
    // Earliest sample of the run before the query (equal timestamps: the first one)
    const qint64 before = t[i - 1];
    const std::size_t j = static_cast<std::size_t>(std::lower_bound(t, t + i, before) - t);
    // Ties go to the earlier sample
    return (query - before <= *after - query) ? static_cast<qint64>(j) : static_cast<qint64>(i);
}

qint64 minimumXSpan(const std::vector<double> &meanIntervals)
{
    double smallest = std::numeric_limits<double>::infinity();
    for (double m : meanIntervals) {
        if (std::isfinite(m) && m > 0.0) {
            smallest = std::min(smallest, m);
        }
    }
    if (!std::isfinite(smallest)) {
        return 1;
    }
    const double span = std::ceil(4.0 * smallest);
    return span >= static_cast<double>(std::numeric_limits<qint64>::max()) ? std::numeric_limits<qint64>::max()
                                                                            : std::max<qint64>(1, static_cast<qint64>(span));
}

namespace {

// The maximum (the data extent) wins over the minimum span; the minimum applies alone only when the
// data spans a single instant
qint64 effectiveMinimumSpan(qint64 extent, qint64 minSpan)
{
    minSpan = std::max<qint64>(1, minSpan);
    return extent > 0 ? std::min(minSpan, extent) : minSpan;
}

} // namespace

XRange fullXRange(qint64 dataStart, qint64 dataEnd, qint64 minSpan)
{
    if (dataEnd < dataStart) {
        std::swap(dataStart, dataEnd);
    }
    const qint64 extent = dataEnd - dataStart;
    if (extent > 0) {
        return {dataStart, dataEnd};
    }
    const qint64 span = effectiveMinimumSpan(extent, minSpan);
    const qint64 start = dataStart - span / 2;
    return {start, start + span};
}

XRange clampXRange(XRange requested, qint64 dataStart, qint64 dataEnd, qint64 minSpan)
{
    if (requested.end < requested.start) {
        std::swap(requested.start, requested.end);
    }
    if (dataEnd < dataStart) {
        std::swap(dataStart, dataEnd);
    }
    const XRange full = fullXRange(dataStart, dataEnd, minSpan);
    // Asking for the whole extent or more shows the data
    if (requested.span() >= full.span()) {
        return full;
    }
    const qint64 span = std::max(requested.span(), effectiveMinimumSpan(dataEnd - dataStart, minSpan));
    // Keep the centre of the request when the span grows to the minimum
    const qint64 centre = requested.start + requested.span() / 2;
    qint64 start = centre - span / 2;
    // At most one view width outside the data: start >= dataStart - span, end <= dataEnd + span
    const qint64 lowest = dataStart - span;
    const qint64 highest = dataEnd;
    start = std::clamp(start, lowest, std::max(lowest, highest));
    return {start, start + span};
}

YRange autoscaledYRange(double minimum, double maximum, bool hasFiniteValues)
{
    YRange r;
    if (!hasFiniteValues || !std::isfinite(minimum) || !std::isfinite(maximum)) {
        return r;
    }
    r.hasData = true;
    if (qFuzzyCompare(minimum, maximum)) {
        r.minimum = minimum - 1.0;
        r.maximum = maximum + 1.0;
    } else {
        const double margin = (maximum - minimum) * 0.10; // StripChartPrivate::kAutoScaleMargin
        r.minimum = minimum - margin;
        r.maximum = maximum + margin;
    }
    return r;
}

namespace {

// Unit of a time step, as computeTimeTicks() chooses it
void unitOf(qint64 stepNs, QString &unit, double &divisor)
{
    if (stepNs < 1000) {
        unit = QStringLiteral("ns");
        divisor = 1.0;
    } else if (stepNs < 1000000) {
        unit = QStringLiteral("µs");
        divisor = 1e3;
    } else if (stepNs < 1000000000LL) {
        unit = QStringLiteral("ms");
        divisor = 1e6;
    } else if (stepNs < 60000000000LL) {
        unit = QStringLiteral("s");
        divisor = 1e9;
    } else {
        unit = QStringLiteral("min");
        divisor = 60e9;
    }
}

// Decimals that keep consecutive multiples of step / divisor distinct
int decimalsFor(double step)
{
    if (!(step > 0.0) || !std::isfinite(step)) {
        return 0;
    }
    int decimals = 0;
    while (decimals < 9 && std::abs(step * std::pow(10.0, decimals) - std::round(step * std::pow(10.0, decimals))) > 1e-6) {
        ++decimals;
    }
    return decimals;
}

QString elapsedLabel(qint64 ns, qint64 stepNs)
{
    QString unit;
    double divisor = 1.0;
    unitOf(stepNs, unit, divisor);
    return formatFixed(static_cast<double>(ns) / divisor, decimalsFor(static_cast<double>(stepNs) / divisor))
        + QLatin1Char(' ') + unit;
}

QString wallClockLabel(const QDateTime &origin, qint64 t, qint64 stepNs)
{
    const qint64 ms = M4Decimator::floorDiv(t, 1000000);
    const qint64 subMs = t - ms * 1000000; // 0 .. 999999 ns
    QString label = origin.addMSecs(ms).toString(QStringLiteral("HH:mm:ss.zzz"));
    if (stepNs < 1000) {
        label += QStringLiteral("%1").arg(subMs, 6, 10, QLatin1Char('0'));
    } else if (stepNs < 1000000) {
        label += QStringLiteral("%1").arg(subMs / 1000, 3, 10, QLatin1Char('0'));
    }
    return label;
}

} // namespace

std::vector<AxisTick> timeAxisTicks(XRange view, qint64 origin, double plotWidthPx, double minSpacingPx,
                                    bool absolute, const QDateTime &wallClockOrigin)
{
    std::vector<AxisTick> out;
    if (view.span() <= 0 || !(plotWidthPx > 0.0)) {
        return out;
    }
    // Wall-clock labels are aligned on the clock (origin 0), elapsed labels on the data start
    const bool wallClock = absolute && wallClockOrigin.isValid();
    const qint64 base = absolute ? 0 : origin;
    const TimeTicksResult ticks = computeTimeTicks(view.start - base, view.end - base, plotWidthPx, minSpacingPx);
    for (const TimeTick &tick : ticks.ticks) {
        const qint64 t = tick.tNs + base;
        out.push_back({static_cast<double>(t), wallClock ? wallClockLabel(wallClockOrigin, t, ticks.stepNs)
                                                         : elapsedLabel(t - base, ticks.stepNs)});
    }
    return out;
}

std::vector<AxisTick> valueAxisTicks(double minimum, double maximum, double plotHeightPx, double minSpacingPx)
{
    std::vector<AxisTick> out;
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || !(maximum > minimum) || !(plotHeightPx > 0.0)) {
        return out;
    }
    const double spacing = minSpacingPx > 0.0 ? minSpacingPx : 30.0;
    const int maxDivs = std::max(1, static_cast<int>(std::floor(plotHeightPx / spacing)));
    const double raw = (maximum - minimum) / maxDivs;
    const double base = std::pow(10.0, std::floor(std::log10(raw)));
    const double fraction = raw / base;
    double step = (fraction <= 1.0 ? 1.0 : fraction <= 2.0 ? 2.0 : fraction <= 5.0 ? 5.0 : 10.0) * base;
    const int decimals = std::max(0, -static_cast<int>(std::floor(std::log10(step) + 1e-9)));
    const double first = std::ceil(minimum / step - 1e-9) * step;
    for (int i = 0; i < 1000; ++i) {
        const double v = first + i * step;
        if (v > maximum + step * 1e-9) {
            break;
        }
        out.push_back({v, formatFixed(v, decimals)});
    }
    return out;
}

XRange zoomXAround(XRange view, double anchor, double factor)
{
    if (!(factor > 0.0) || !std::isfinite(factor) || !std::isfinite(anchor) || view.span() <= 0) {
        return view;
    }
    const double start = static_cast<double>(view.start);
    const double span = static_cast<double>(view.span());
    const double newSpan = std::max(1.0, span / factor);
    const double rel = (anchor - start) / span;
    const double newStart = anchor - rel * newSpan;
    const qint64 s0 = static_cast<qint64>(std::llround(newStart));
    return {s0, s0 + static_cast<qint64>(std::llround(newSpan))};
}

YRange zoomYAround(double minimum, double maximum, double anchor, double factor)
{
    YRange r{minimum, maximum, true};
    if (!(factor > 0.0) || !std::isfinite(factor) || !std::isfinite(anchor) || !(maximum > minimum)) {
        return r;
    }
    const double span = (maximum - minimum) / factor;
    const double rel = (anchor - minimum) / (maximum - minimum);
    const double lo = anchor - rel * span;
    const double hi = lo + span;
    // Keep a span that the axis can still label
    if (!std::isfinite(lo) || !std::isfinite(hi) || !(hi > lo)
        || span < 1e-12 * std::max(1.0, std::max(std::abs(lo), std::abs(hi)))) {
        return r;
    }
    r.minimum = lo;
    r.maximum = hi;
    return r;
}

double wheelZoomFactor(int angleDelta)
{
    return std::pow(1.25, static_cast<double>(angleDelta) / 120.0);
}

namespace {

// Elapsed time in the unit of 10 x the resolution (the axis step), with the decimals of the resolution
QString readoutElapsed(qint64 ns, qint64 resolutionNs)
{
    QString unit;
    double divisor = 1.0;
    unitOf(resolutionNs >= std::numeric_limits<qint64>::max() / 10 ? resolutionNs : resolutionNs * 10, unit, divisor);
    return formatFixed(static_cast<double>(ns) / divisor, decimalsFor(static_cast<double>(resolutionNs) / divisor))
        + QLatin1Char(' ') + unit;
}

} // namespace

QString readoutTimeText(qint64 t, qint64 origin, bool absolute, const QDateTime &wallClockOrigin, qint64 resolutionNs)
{
    resolutionNs = std::max<qint64>(1, resolutionNs);
    if (absolute && wallClockOrigin.isValid()) {
        return wallClockLabel(wallClockOrigin, t, resolutionNs);
    }
    return readoutElapsed(t - (absolute ? 0 : origin), resolutionNs);
}

QString durationText(qint64 ns, qint64 resolutionNs)
{
    return readoutElapsed(ns, std::max<qint64>(1, resolutionNs));
}

QString readoutValueText(const double *value, int decimals)
{
    if (value == nullptr || !std::isfinite(*value)) {
        return QStringLiteral("---");
    }
    return formatFixed(*value, std::clamp(decimals, 0, 12));
}

int valueAxisDecimals(double minimum, double maximum, double plotHeightPx, double minSpacingPx)
{
    const auto ticks = valueAxisTicks(minimum, maximum, plotHeightPx, minSpacingPx);
    if (ticks.size() < 2) {
        return 2;
    }
    const double step = ticks[1].position - ticks[0].position;
    return std::max(0, -static_cast<int>(std::floor(std::log10(step) + 1e-9)));
}

} // namespace internal
} // namespace QtIndustrialWidgets
