/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
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
     *
     * A bucket that contains any non-finite sample emits no vertex, so its pixel column stays
     * blank and a sensor dropout is never hidden by the bucket's finite samples. The finite
     * extremals are still kept (yMin / yMax are used by autoscale).
     *
     * \param out Output array of capacity at least 4.
     * \return Number of unique vertices emitted (0 to 4).
     */
    int emitVertices(M4Point out[4]) const noexcept {
        if (!hasFinite || hasNonFinite) return 0;

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
 * \brief Exact floor(a * b / c) for b > 0 and c > 0, without overflow in the intermediate product.
 *
 * The product is formed in 128 bits from 64-bit halves (no compiler extensions). If \p remainder
 * is not null it receives a * b - result * c, in [0, c). A result outside the qint64 range is
 * saturated (the remainder is then unspecified).
 */
qint64 mulDivFloor(qint64 a, qint64 b, qint64 c, qint64 *remainder = nullptr) noexcept;

/** \brief Exact ceil(a * b / c) for b > 0 and c > 0 (see mulDivFloor()). */
qint64 mulDivCeil(qint64 a, qint64 b, qint64 c) noexcept;

/**
 * \struct BucketGrid
 * \brief Assigns axis positions (ns or sample indices) to decimation buckets.
 *
 * Bucket k = floor(x * num / den), so `num` buckets cover `den` axis units; bucket widths are
 * floor or ceil of den / num and need not be uniform. Bucket indices are anchored to the absolute
 * axis position. A uniform grid of width w is {1, w}. Bucket assignment is exact integer
 * arithmetic; |x| * num / den must fit in qint64 (always true when num <= den).
 */
struct BucketGrid {
    constexpr BucketGrid() noexcept = default;
    /** \brief num buckets per den axis units; both must be > 0. */
    constexpr BucketGrid(qint64 num, qint64 den) noexcept
        : m_num(num > 0 ? num : 1)
        , m_den(den > 0 ? den : 1)
        , m_fastLimit(std::numeric_limits<qint64>::max() / (num > 0 ? num : 1))
    {
    }

    [[nodiscard]] constexpr qint64 num() const noexcept { return m_num; }
    [[nodiscard]] constexpr qint64 den() const noexcept { return m_den; }

    /**
     * \brief Largest |x| whose bucket index x * num / den fits in qint64 (the valid range of
     * bucketOf()). It is the full qint64 range when num <= den; with more buckets than axis units
     * (num > den, e.g. a SampleIndex capacity below the plot width in DecimationMode::Always) it
     * is floor(INT64_MAX * den / num), at least about 5.6e14 for num <= 16384.
     */
    [[nodiscard]] qint64 maxPosition() const noexcept {
        return mulDivFloor(std::numeric_limits<qint64>::max(), m_den, m_num);
    }

    /** \brief Bucket that contains axis position x; |x| must not exceed maxPosition(). */
    [[nodiscard]] qint64 bucketOf(qint64 x) const noexcept {
        Q_ASSERT_X(m_num <= m_den || (x <= maxPosition() && x >= -maxPosition()), "BucketGrid::bucketOf",
                   "bucket index out of qint64 range: |x| exceeds maxPosition()");
        if (x <= m_fastLimit && x >= -m_fastLimit) { // x * num cannot overflow
            const qint64 p = x * m_num;
            qint64 q = p / m_den;
            if ((p % m_den) != 0 && p < 0) {
                --q;
            }
            return q;
        }
        return mulDivFloor(x, m_num, m_den);
    }

    /** \brief First axis position of bucket k (smallest x with bucketOf(x) >= k). */
    [[nodiscard]] qint64 boundary(qint64 k) const noexcept {
        return mulDivCeil(k, m_den, m_num);
    }

    bool operator==(const BucketGrid &o) const noexcept { return m_num == o.m_num && m_den == o.m_den; }
    bool operator!=(const BucketGrid &o) const noexcept { return !(*this == o); }

private:
    qint64 m_num{1};
    qint64 m_den{1};
    qint64 m_fastLimit{std::numeric_limits<qint64>::max()};
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
        const BucketGrid &grid,
        qint64 kStart,
        int numBuckets);

    /** \brief Uniform-width overload: buckets of dt_px axis units. */
    static std::vector<M4Bucket> decimateToBuckets(
        const qint64 *timestamps,
        const double *values,
        size_t count,
        qint64 dt_px,
        qint64 kStart,
        int numBuckets)
    {
        if (dt_px <= 0) return {};
        return decimateToBuckets(timestamps, values, count, BucketGrid(1, dt_px), kStart, numBuckets);
    }

    /**
     * \brief Runs batch decimation and returns vertices grouped by contiguous segments.
     */
    static std::vector<std::vector<M4Point>> decimateToSegments(
        const qint64 *timestamps,
        const double *values,
        size_t count,
        const BucketGrid &grid,
        qint64 kStart,
        int numBuckets,
        qint64 gapThreshold);

    /** \brief Uniform-width overload: buckets of dt_px axis units. */
    static std::vector<std::vector<M4Point>> decimateToSegments(
        const qint64 *timestamps,
        const double *values,
        size_t count,
        qint64 dt_px,
        qint64 kStart,
        int numBuckets,
        qint64 gapThreshold)
    {
        if (dt_px <= 0) return {};
        return decimateToSegments(timestamps, values, count, BucketGrid(1, dt_px), kStart, numBuckets, gapThreshold);
    }

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

        explicit IncrementalStream(int widthDev = 800, size_t ringSize = 0);

        void setWidth(int widthDev, size_t ringSize = 0);
        void setRingSize(size_t ringSize);
        void reset();

        void addSample(qint64 t, double y, const BucketGrid &grid);
        /** \brief Uniform-width overload: buckets of dt_px axis units. */
        void addSample(qint64 t, double y, qint64 dt_px) {
            if (dt_px > 0) addSample(t, y, BucketGrid(1, dt_px));
        }
        void setBucket(qint64 k, const M4Bucket &bucket);
        void rebuild(const qint64 *timestamps, const double *values, size_t count, const BucketGrid &grid);
        /** \brief Uniform-width overload: buckets of dt_px axis units. */
        void rebuild(const qint64 *timestamps, const double *values, size_t count, qint64 dt_px) {
            if (dt_px > 0) {
                rebuild(timestamps, values, count, BucketGrid(1, dt_px));
            } else {
                reset();
            }
        }

        [[nodiscard]] const M4Bucket *bucketAt(qint64 k) const;
        /**
         * \brief Fills \p segments with the vertices of the visible buckets, broken into segments at non-finite
         *        buckets and at gaps longer than \p gapThreshold, and returns the number of segments.
         *
         * The buffers of \p segments are reused (#8): segments [0, count) hold this extraction, the ones after them
         * are left empty with their capacity, for a later extraction with more segments (StripChart extracts every
         * channel into the same buffers). An extraction that fits in the buffers of an earlier one allocates nothing.
         */
        [[nodiscard]] std::size_t extractVisibleSegments(
            std::vector<std::vector<M4Point>> &segments,
            qint64 kStart, int numBuckets, qint64 gapThreshold) const;
        [[nodiscard]] std::vector<std::vector<M4Point>> extractVisibleSegments(
            qint64 kStart, int numBuckets, qint64 gapThreshold) const;

        [[nodiscard]] int widthDev() const noexcept { return m_widthDev; }
        [[nodiscard]] size_t ringSize() const noexcept { return m_ringSize; }
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
