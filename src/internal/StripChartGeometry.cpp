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
    win.tEnd = M4Decimator::floorDiv(tLatest + win.dtPx - 1, win.dtPx) * win.dtPx;
    win.tStart = win.tEnd - span;
    win.kStart = M4Decimator::floorDiv(win.tStart, win.dtPx);
    win.numBuckets = w;
    return win;
}

IndexBuckets computeIndexBuckets(quint64 totalSamples, size_t count,
                                 int capacity, int widthDev)
{
    IndexBuckets ib;
    int w = std::max(1, widthDev);
    int cap = std::max(1, capacity);
    ib.samplesPerBucket = std::max(qint64(1), static_cast<qint64>(cap) / w);
    quint64 startCounter = (totalSamples >= count) ? (totalSamples - count) : 0;
    qint64 firstAbsoluteIndex = static_cast<qint64>(startCounter);
    ib.kStart = M4Decimator::floorDiv(firstAbsoluteIndex, ib.samplesPerBucket);
    ib.numBuckets = w;
    return ib;
}

} // namespace internal
} // namespace QtIndustrialWidgets
