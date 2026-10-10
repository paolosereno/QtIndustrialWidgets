/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/qglobal.h>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \struct M4Point
 * \brief A single timestamped waveform sample point.
 */
struct M4Point {
    qint64 t{0};       ///< Nanosecond timestamp or sample index
    double value{0.0}; ///< Waveform sample value
};

/**
 * \struct M4Bucket
 * \brief Pixel-column bucket aggregate storing First, Min, Max, and Last extremals.
 */
struct M4Bucket {
    qint64 tFirst{0};
    double yFirst{0.0};
    qint64 tLast{0};
    double yLast{0.0};
    qint64 tMin{0};
    double yMin{std::numeric_limits<double>::infinity()};
    qint64 tMax{0};
    double yMax{-std::numeric_limits<double>::infinity()};
    size_t count{0};
    bool hasFinite{false};
    bool hasNonFinite{false};

    void reset() noexcept {
        tFirst = 0;
        yFirst = 0.0;
        tLast = 0;
        yLast = 0.0;
        tMin = 0;
        yMin = std::numeric_limits<double>::infinity();
        tMax = 0;
        yMax = -std::numeric_limits<double>::infinity();
        count = 0;
        hasFinite = false;
        hasNonFinite = false;
    }

    void addSample(qint64 t, double y) noexcept {
        if (!std::isfinite(y)) {
            hasNonFinite = true;
            return;
        }
        if (!hasFinite) {
            tFirst = t;
            yFirst = y;
            tLast = t;
            yLast = y;
            tMin = t;
            yMin = y;
            tMax = t;
            yMax = y;
            hasFinite = true;
        } else {
            if (y < yMin) {
                yMin = y;
                tMin = t;
            }
            if (y > yMax) {
                yMax = y;
                tMax = t;
            }
            tLast = t;
            yLast = y;
        }
        count++;
    }

    /**
     * \brief Emits up to 4 vertices in chronological order, deduplicating coincident points.
     * \param out Output array of capacity at least 4.
     * \return Number of unique vertices emitted (0 to 4).
     */
    int emitVertices(M4Point out[4]) const noexcept {
        if (!hasFinite) return 0;

        M4Point raw[4];
        if (tMin <= tMax) {
            raw[0] = {tFirst, yFirst};
            raw[1] = {tMin, yMin};
            raw[2] = {tMax, yMax};
            raw[3] = {tLast, yLast};
        } else {
            raw[0] = {tFirst, yFirst};
            raw[1] = {tMax, yMax};
            raw[2] = {tMin, yMin};
            raw[3] = {tLast, yLast};
        }

        int n = 0;
        for (int i = 0; i < 4; ++i) {
            if (n == 0 || raw[i].t != out[n - 1].t || raw[i].value != out[n - 1].value) {
                out[n++] = raw[i];
            }
        }
        return n;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return hasFinite;
    }
};

/**
 * \class M4Decimator
 * \brief Min/max pixel-column decimator providing batch and incremental O(1) decimation.
 */
class M4Decimator {
public:
    /** \brief Mathematical integer floor division a / b. */
    static constexpr qint64 floorDiv(qint64 a, qint64 b) noexcept {
        if (b <= 0) return 0;
        qint64 res = a / b;
        qint64 rem = a % b;
        if (rem != 0 && (a < 0)) {
            res--;
        }
        return res;
    }

    /**
     * \brief Runs batch decimation over a sample slice and returns bucket aggregates.
     */
    static std::vector<M4Bucket> decimateToBuckets(
        const qint64 *timestamps,
        const double *values,
        size_t count,
        qint64 dt_px,
        qint64 kStart,
        int numBuckets);

    /**
     * \brief Runs batch decimation and returns vertices grouped by contiguous segments.
     */
    static std::vector<std::vector<M4Point>> decimateToSegments(
        const qint64 *timestamps,
        const double *values,
        size_t count,
        qint64 dt_px,
        qint64 kStart,
        int numBuckets,
        qint64 gapThreshold);

    /**
     * \class IncrementalStream
     * \brief Ring buffer of size (W_dev + 2) maintaining current and historical bucket aggregates in O(1) per sample.
     */
    class IncrementalStream {
    public:
        struct BucketEntry {
            qint64 bucketIndex{std::numeric_limits<qint64>::min()};
            M4Bucket bucket;
        };

        explicit IncrementalStream(int widthDev = 800);

        void setWidth(int widthDev);
        void reset();

        void addSample(qint64 t, double y, qint64 dt_px);
        void rebuild(const qint64 *timestamps, const double *values, size_t count, qint64 dt_px);

        [[nodiscard]] const M4Bucket *bucketAt(qint64 k) const;
        [[nodiscard]] std::vector<std::vector<M4Point>> extractVisibleSegments(
            qint64 kStart, int numBuckets, qint64 gapThreshold) const;

        [[nodiscard]] int widthDev() const noexcept { return m_widthDev; }
        [[nodiscard]] qint64 activeBucketIndex() const noexcept { return m_activeBucketIndex; }
        [[nodiscard]] const M4Bucket &activeBucket() const noexcept { return m_activeBucket; }

    private:
        [[nodiscard]] size_t slotFor(qint64 k) const noexcept;

        int m_widthDev{800};
        size_t m_ringSize{802};
        std::vector<BucketEntry> m_ring;
        qint64 m_activeBucketIndex{std::numeric_limits<qint64>::min()};
        M4Bucket m_activeBucket;
    };
};

} // namespace internal
} // namespace QtIndustrialWidgets
