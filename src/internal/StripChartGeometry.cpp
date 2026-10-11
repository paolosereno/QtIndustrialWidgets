/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "StripChartGeometry.h"
#include "M4Decimator.h"
#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

TimeWindow computeTimeWindow(qint64 tLatest, qint64 timeSpanNs, int widthDev)
{
    TimeWindow win;
    int w = std::max(1, widthDev);
    qint64 span = std::max(qint64(1), timeSpanNs);
    win.dtPx = std::max(qint64(1), span / w);
    win.tEnd = (M4Decimator::floorDiv(tLatest, win.dtPx) + 1) * win.dtPx;
    win.tStart = win.tEnd - span;
    win.kStart = M4Decimator::floorDiv(win.tStart, win.dtPx);
    qint64 kEnd = win.tEnd / win.dtPx;
    win.numBuckets = static_cast<int>(kEnd - win.kStart);
    win.grid = BucketGrid(1, win.dtPx);
    return win;
}

IndexBuckets computeIndexBuckets(quint64 totalSamples, size_t count,
                                 int capacity, int widthDev)
{
    IndexBuckets ib;
    int w = std::max(1, widthDev);
    int cap = std::max(1, capacity);
    ib.samplesPerBucket = std::max(qint64(1), static_cast<qint64>(cap + w - 1) / w);

    ib.grid = BucketGrid(1, ib.samplesPerBucket);

    if (count == 0) {
        quint64 startCounter = totalSamples;
        ib.kStart = M4Decimator::floorDiv(static_cast<qint64>(startCounter), ib.samplesPerBucket);
        ib.numBuckets = 1;
        return ib;
    }

    quint64 startCounter = (totalSamples >= count) ? (totalSamples - count) : 0;
    qint64 firstAbsoluteIndex = static_cast<qint64>(startCounter);
    qint64 lastAbsoluteIndex = static_cast<qint64>(startCounter + count - 1);
    ib.kStart = M4Decimator::floorDiv(firstAbsoluteIndex, ib.samplesPerBucket);
    qint64 kLast = M4Decimator::floorDiv(lastAbsoluteIndex, ib.samplesPerBucket);
    ib.numBuckets = static_cast<int>(kLast - ib.kStart + 1);
    return ib;
}

PlotXMapping makePlotXMapping(const BucketGrid &grid, qint64 kStart, double plotLeftDevExact)
{
    PlotXMapping m;
    m.grid = grid;
    m.kStart = kStart;
    m.leftDev = plotLeftDevExact;
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

        QString labelStr = QString::number(labelVal, 'f',
            (std::abs(labelVal) >= 10.0 || qFuzzyIsNull(std::remainder(labelVal, 1.0))) ? 0 : 1)
            + QLatin1Char(' ') + labelUnit;

        result.ticks.push_back({t, labelStr});
    }

    return result;
}

} // namespace internal
} // namespace QtIndustrialWidgets
