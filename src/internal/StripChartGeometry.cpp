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
    return win;
}

IndexBuckets computeIndexBuckets(quint64 totalSamples, size_t count,
                                 int capacity, int widthDev)
{
    IndexBuckets ib;
    int w = std::max(1, widthDev);
    int cap = std::max(1, capacity);
    ib.samplesPerBucket = std::max(qint64(1), static_cast<qint64>(cap + w - 1) / w);

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

} // namespace internal
} // namespace QtIndustrialWidgets
