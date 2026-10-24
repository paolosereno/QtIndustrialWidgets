/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "M4Decimator.h"
#include <QtCore/qglobal.h>
#include <QtCore/QString>
#include <cstddef>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

struct TimeWindow {
    BucketGrid grid;       // bucket assignment of timestamps (ns)
    qint64 dtPx{1};        // nominal bucket width in ns, >= 1 (informational)
    qint64 tStart{0};      // inclusive, grid.boundary(kStart)
    qint64 tEnd{0};        // exclusive, grid.boundary(kStart + numBuckets)
    qint64 kStart{0};      // first bucket index (absolute)
    int    numBuckets{0};  // buckets in the window
};

/**
 * \brief Length in ns of the Time-mode window: the configured span, or widthDev ns when the span
 * is shorter (1 ns buckets, never more buckets than nanoseconds). Labels and ticks must use it.
 */
qint64 effectiveTimeSpan(qint64 timeSpanNs, int widthDev);

TimeWindow computeTimeWindow(qint64 tLatest, qint64 timeSpanNs, int widthDev);

struct IndexBuckets {
    BucketGrid grid;            // bucket assignment of absolute sample indices
    qint64 samplesPerBucket{1}; // nominal samples per bucket, >= 1 (informational)
    qint64 kStart{0};           // first bucket index (absolute)
    int    numBuckets{0};       // buckets in the window
};

IndexBuckets computeIndexBuckets(quint64 totalSamples, size_t count,
                                 int capacity, int widthDev);

enum class GeometryAxisMode {
    SampleIndex,
    Time
};

struct GeometryKey {
    GeometryAxisMode axisMode{GeometryAxisMode::SampleIndex};
    BucketGrid grid;       // bucket assignment (time or sample index)
    int widthDev{0};
    size_t ringSize{0};

    [[nodiscard]] bool isValid() const noexcept {
        return widthDev > 0 && ringSize > 0;
    }

    bool operator==(const GeometryKey &o) const noexcept {
        return axisMode == o.axisMode &&
               grid == o.grid &&
               widthDev == o.widthDev &&
               ringSize == o.ringSize;
    }

    bool operator!=(const GeometryKey &o) const noexcept {
        return !(*this == o);
    }
};

/**
 * \brief Maps axis positions and buckets of a window to device-space x (device pixels).
 *
 * Single source of truth for the horizontal placement of traces and time ticks. Bucket k of the
 * window occupies device column k - kStart of the plot; leftDev is the device x of the plot's left
 * edge.
 */
struct PlotXMapping {
    BucketGrid grid;
    qint64 kStart{0};
    double leftDev{0.0};

    /** \brief Continuous device x of axis position x (raw samples, ticks). */
    [[nodiscard]] double positionDev(qint64 x) const noexcept {
        qint64 rem = 0;
        const qint64 k = mulDivFloor(x, grid.num(), grid.den(), &rem);
        return leftDev + static_cast<double>(k - kStart)
             + static_cast<double>(rem) / static_cast<double>(grid.den());
    }
    /** \brief Device x of the left edge of bucket k's column. */
    [[nodiscard]] double columnLeftDev(qint64 k) const noexcept {
        return leftDev + static_cast<double>(k - kStart);
    }
    /** \brief Device x where decimated vertices of bucket k are drawn (centre of its column). */
    [[nodiscard]] double vertexDev(qint64 k) const noexcept {
        return columnLeftDev(k) + 0.5;
    }
};

/**
 * \brief Builds the x mapping of a window. \p plotLeftDevExact is the device x of the plot's left
 * edge as given by the painter's device transform.
 */
PlotXMapping makePlotXMapping(const BucketGrid &grid, qint64 kStart, double plotLeftDevExact);

struct TimeTick {
    qint64 tNs{0};
    QString label;
};

struct TimeTicksResult {
    qint64 stepNs{1};
    QString unit;
    std::vector<TimeTick> ticks;
};

TimeTicksResult computeTimeTicks(qint64 tStart, qint64 tEnd, double plotWidthPx, double minLabelSpacingPx = 60.0);

} // namespace internal
} // namespace QtIndustrialWidgets
