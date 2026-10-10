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

enum class GeometryAxisMode {
    SampleIndex,
    Time
};

struct GeometryKey {
    GeometryAxisMode axisMode{GeometryAxisMode::SampleIndex};
    qint64 bucketWidth{0}; // dtPx in Time mode, samplesPerBucket in SampleIndex mode
    int widthDev{0};
    size_t ringSize{0};

    [[nodiscard]] bool isValid() const noexcept {
        return bucketWidth > 0 && widthDev > 0 && ringSize > 0;
    }

    bool operator==(const GeometryKey &o) const noexcept {
        return axisMode == o.axisMode &&
               bucketWidth == o.bucketWidth &&
               widthDev == o.widthDev &&
               ringSize == o.ringSize;
    }

    bool operator!=(const GeometryKey &o) const noexcept {
        return !(*this == o);
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
