/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Min/max pyramid of AnalysisChart (#25): M4 buckets of a stored channel in O(log N) per bucket.

#include "M4Decimator.h"

#include <QtCore/qglobal.h>

#include <cstddef>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief Min/max pyramid over the samples of one channel.
 *
 * Level 0 aggregates blocks of kBlockSize consecutive samples; each level above aggregates pairs of
 * nodes of the level below (a node is kept only when it covers whole blocks). A node stores the
 * indices of the first and last finite samples and of the earliest minimum and maximum, the number of
 * finite samples and whether the block holds a non-finite sample, so aggregate() reproduces exactly
 * what M4Bucket::addSample() gives for the same samples in order.
 *
 * It also keeps the first timestamp of every block, so the sample index of a time is found by a binary
 * search over that compact index and one block, not over the whole timestamp array.
 *
 * The pyramid does not own the samples: aggregate() and buckets() read them from the arrays passed
 * in, which must be the arrays the pyramid was built from.
 */
class AnalysisPyramid
{
public:
    static constexpr std::size_t kBlockSize = 64;

    void build(const qint64 *t, const double *values, std::size_t count);
    void clear();

    /** \brief Bytes held by the nodes (capacity of the level arrays). Inline: read by the benchmark. */
    [[nodiscard]] std::size_t memoryBytes() const
    {
        std::size_t bytes = m_levels.capacity() * sizeof(std::vector<Node>) + m_blockFirstT.capacity() * sizeof(qint64);
        for (const auto &level : m_levels) bytes += level.capacity() * sizeof(Node);
        return bytes;
    }

    /**
     * \brief M4 aggregate of the samples [first, last): the raw samples up to the first and from the
     *        last block boundary, whole blocks through the pyramid.
     */
    [[nodiscard]] M4Bucket aggregate(const qint64 *t, const double *v, std::size_t first, std::size_t last) const;

    /**
     * \brief The buckets M4Decimator::decimateToBuckets() gives for the same samples, with the grid
     *        applied to t - origin: bucket k (0 <= k < numBuckets) holds the samples with
     *        grid.bucketOf(t - origin) == kStart + k. \p t must be non-decreasing and every
     *        t - origin of the queried range within the grid's maxPosition().
     */
    /** \brief First index i in [from, count) with t[i] >= time (count if none), as std::lower_bound(). */
    [[nodiscard]] std::size_t lowerBound(const qint64 *t, std::size_t count, std::size_t from, qint64 time) const;

    [[nodiscard]] std::vector<M4Bucket> buckets(const qint64 *t, const double *v, std::size_t count, qint64 origin,
                                                const BucketGrid &grid, qint64 kStart, int numBuckets) const;
    /** \brief As buckets(), into \p out, whose memory is reused: a call that fits in it allocates nothing (#97). */
    void buckets(const qint64 *t, const double *v, std::size_t count, qint64 origin, const BucketGrid &grid,
                 qint64 kStart, int numBuckets, std::vector<M4Bucket> &out) const;

private:
    struct Node {
        double yMin{0.0};
        double yMax{0.0};
        qint64 iFirst{-1}; // -1: no finite sample
        qint64 iLast{-1};
        qint64 iMin{-1};
        qint64 iMax{-1};
        qint64 finite{0};
        bool hasNonFinite{false};
    };

    static void mergeInto(Node &acc, const Node &next);
    static void mergeInto(M4Bucket &acc, const Node &node, const qint64 *t, const double *v);

    std::vector<std::vector<Node>> m_levels; // m_levels[L][j] covers blocks [j * 2^L, (j + 1) * 2^L)
    std::vector<qint64> m_blockFirstT;       // t[b * kBlockSize] of every whole block b
};

/**
 * \brief Segments of the vertices of \p buckets, by the rules of M4Decimator::decimateToSegments():
 *        a bucket holding a non-finite sample emits nothing and breaks the trace, empty buckets do not,
 *        and a step between consecutive vertices above \p gapThreshold (> 0) breaks it.
 */
std::vector<std::vector<M4Point>> segmentsFromBuckets(const std::vector<M4Bucket> &buckets, qint64 gapThreshold);

/**
 * \brief As segmentsFromBuckets() above, into \p segments, and returns the number of segments.
 *
 * The buffers of \p segments are reused (#97, as M4Decimator::IncrementalStream::extractVisibleSegments() for #8):
 * segments [0, count) hold the result, the ones after them are left empty with their capacity, for a later call with
 * more segments (AnalysisChart uses the same buffers for every channel). A call that fits in the buffers of an
 * earlier one allocates nothing.
 */
[[nodiscard]] std::size_t segmentsFromBuckets(const std::vector<M4Bucket> &buckets, qint64 gapThreshold,
                                              std::vector<std::vector<M4Point>> &segments);

} // namespace internal
} // namespace QtIndustrialWidgets
