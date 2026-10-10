/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/qglobal.h>
#include <cstddef>

namespace QtIndustrialWidgets {
namespace internal {

struct TimeWindow {
    qint64 dtPx{1};        // bucket width in ns, >= 1
    qint64 tStart{0};      // inclusive
    qint64 tEnd{0};        // exclusive
    qint64 kStart{0};      // first bucket index (absolute, floor(tStart / dtPx))
    int    numBuckets{0};  // buckets needed to cover [tStart, tEnd)
};

TimeWindow computeTimeWindow(qint64 tLatest, qint64 timeSpanNs, int widthDev);

struct IndexBuckets {
    qint64 samplesPerBucket{1}; // >= 1
    qint64 kStart{0};           // floor(firstAbsoluteIndex / samplesPerBucket)
    int    numBuckets{0};       // buckets needed to cover every retained sample
};

IndexBuckets computeIndexBuckets(quint64 totalSamples, size_t count,
                                 int capacity, int widthDev);

} // namespace internal
} // namespace QtIndustrialWidgets
