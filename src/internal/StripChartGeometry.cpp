/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "StripChartGeometry.h"
#include "NumberFormat.h"
#include "M4Decimator.h"
#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

qint64 effectiveTimeSpan(qint64 timeSpanNs, int widthDev)
{
    return std::max<qint64>(std::max(1, widthDev), timeSpanNs);
}

TimeWindow computeTimeWindow(qint64 tLatest, qint64 timeSpanNs, int widthDev)
{
    // widthDev buckets cover the span: bucket k = floor(t * W / span), anchored to absolute time.
    // The window is the widthDev buckets ending with the bucket of tLatest. Spans shorter than
    // widthDev ns use 1 ns buckets.
    TimeWindow win;
    const int w = std::max(1, widthDev);
    const qint64 span = effectiveTimeSpan(timeSpanNs, w);
    win.grid = BucketGrid(w, span);
    win.dtPx = std::max<qint64>(1, span / w);
    const qint64 kEnd = win.grid.bucketOf(tLatest) + 1;
    win.kStart = kEnd - w;
    win.numBuckets = w;
    win.tStart = win.grid.boundary(win.kStart);
    win.tEnd = win.grid.boundary(kEnd);
    return win;
}

IndexBuckets computeIndexBuckets(quint64 totalSamples, size_t count,
                                 int capacity, int widthDev)
{
    // widthDev buckets cover `capacity` samples: bucket k = floor(i * W / capacity), anchored to
    // the absolute sample index. The window is the widthDev buckets ending with the newest sample's
    // bucket, but never starts before the oldest retained sample's bucket; with a wrapped buffer the
    // oldest, partial bucket can fall out on the left.
    IndexBuckets ib;
    const int w = std::max(1, widthDev);
    const int cap = std::max(1, capacity);
    ib.grid = BucketGrid(w, cap);
    ib.samplesPerBucket = std::max<qint64>(1, (static_cast<qint64>(cap) + w - 1) / w);
    ib.numBuckets = w;

    if (count == 0) {
        ib.kStart = ib.grid.bucketOf(static_cast<qint64>(totalSamples));
        return ib;
    }

    const quint64 startCounter = (totalSamples >= count) ? (totalSamples - count) : 0;
    const qint64 kOldest = ib.grid.bucketOf(static_cast<qint64>(startCounter));
    const qint64 kNewest = ib.grid.bucketOf(static_cast<qint64>(startCounter + count - 1));
    ib.kStart = std::max(kOldest, kNewest + 1 - w);
    return ib;
}

PlotXMapping makePlotXMapping(const BucketGrid &grid, qint64 kStart, double plotLeftDevExact)
{
    // The plot's left edge is snapped to a device pixel boundary so that every bucket covers
    // exactly one device column.
    PlotXMapping m;
    m.grid = grid;
    m.kStart = kStart;
    m.leftDev = std::round(plotLeftDevExact);
    return m;
}

static qint64 next125(qint64 s)
{
    if (s <= 0) return 1;
    double expVal = std::floor(std::log10(static_cast<double>(s)));
    double base = std::pow(10.0, expVal);
    double fraction = static_cast<double>(s) / base;
    double mult = 1.0;
    if (fraction < 1.5) {
        mult = 2.0;
    } else if (fraction < 3.5) {
        mult = 5.0;
    } else {
        mult = 10.0;
    }
    return static_cast<qint64>(std::round(mult * base));
}

TimeTicksResult computeTimeTicks(qint64 tStart, qint64 tEnd, double plotWidthPx, double minLabelSpacingPx)
{
    TimeTicksResult result;
    qint64 spanNs = tEnd - tStart;
    if (spanNs <= 0 || plotWidthPx <= 0.0) {
        return result;
    }

    double spacing = (minLabelSpacingPx > 0.0) ? minLabelSpacingPx : 60.0;
    int maxDivs = std::max(2, static_cast<int>(std::floor(plotWidthPx / spacing)));
    double rawStep = static_cast<double>(spanNs) / maxDivs;
    double expVal = std::floor(std::log10(rawStep));
    double base = std::pow(10.0, expVal);
    double fraction = rawStep / base;

    double mult = 1.0;
    if (fraction <= 1.0) {
        mult = 1.0;
    } else if (fraction <= 2.0) {
        mult = 2.0;
    } else if (fraction <= 5.0) {
        mult = 5.0;
    } else {
        mult = 10.0;
    }

    qint64 step = static_cast<qint64>(std::round(mult * base));

    // Ensure tick spacing in pixels >= minLabelSpacingPx
    while ((static_cast<double>(step) / static_cast<double>(spanNs)) * plotWidthPx < spacing && step < spanNs) {
        step = next125(step);
    }

    result.stepNs = std::max(qint64(1), step);

    // Unit chosen from the step
    double divisor = 1.0;
    if (result.stepNs < 1000) {
        result.unit = QStringLiteral("ns");
        divisor = 1.0;
    } else if (result.stepNs < 1000000) {
        result.unit = QStringLiteral("µs");
        divisor = 1e3;
    } else if (result.stepNs < 1000000000LL) {
        result.unit = QStringLiteral("ms");
        divisor = 1e6;
    } else if (result.stepNs < 60000000000LL) {
        result.unit = QStringLiteral("s");
        divisor = 1e9;
    } else {
        result.unit = QStringLiteral("min");
        divisor = 60e9;
    }

    qint64 firstTick = M4Decimator::floorDiv(tStart + result.stepNs - 1, result.stepNs) * result.stepNs;
    for (qint64 t = firstTick; t <= tEnd; t += result.stepNs) {
        qint64 offset = tEnd - t;
        double relVal = -static_cast<double>(offset) / divisor;
        double absVal = std::abs(relVal);

        QString labelUnit = result.unit;
        double labelVal = relVal;

        // Label rule: A label whose magnitude is >= 1000 in its unit must be promoted to the next unit
        if (absVal >= 1000.0) {
            if (result.unit == QLatin1String("ns")) {
                labelUnit = QStringLiteral("µs");
                labelVal /= 1000.0;
            } else if (result.unit == QLatin1String("µs")) {
                labelUnit = QStringLiteral("ms");
                labelVal /= 1000.0;
            } else if (result.unit == QLatin1String("ms")) {
                labelUnit = QStringLiteral("s");
                labelVal /= 1000.0;
            }
        } else if (result.unit == QLatin1String("s") && absVal >= 60.0) {
            labelUnit = QStringLiteral("min");
            labelVal /= 60.0;
        }

        QString labelStr = formatFixed(labelVal,
            (std::abs(labelVal) >= 10.0 || qFuzzyIsNull(std::remainder(labelVal, 1.0))) ? 0 : 1)
            + QLatin1Char(' ') + labelUnit;

        result.ticks.push_back({t, labelStr});
    }

    return result;
}

} // namespace internal
} // namespace QtIndustrialWidgets
