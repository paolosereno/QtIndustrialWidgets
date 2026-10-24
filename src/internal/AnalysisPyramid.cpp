/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "AnalysisPyramid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {
namespace internal {

void AnalysisPyramid::clear()
{
    m_levels.clear();
    m_levels.shrink_to_fit();
    m_blockFirstT.clear();
    m_blockFirstT.shrink_to_fit();
}

// next follows acc in time; ties keep the earlier minimum and maximum, as M4Bucket::addSample()
void AnalysisPyramid::mergeInto(Node &acc, const Node &next)
{
    acc.hasNonFinite = acc.hasNonFinite || next.hasNonFinite;
    if (next.finite == 0) return;
    if (acc.finite == 0) {
        const bool nonFinite = acc.hasNonFinite;
        acc = next;
        acc.hasNonFinite = nonFinite;
        return;
    }
    if (next.yMin < acc.yMin) {
        acc.yMin = next.yMin;
        acc.iMin = next.iMin;
    }
    if (next.yMax > acc.yMax) {
        acc.yMax = next.yMax;
        acc.iMax = next.iMax;
    }
    acc.iLast = next.iLast;
    acc.finite += next.finite;
}

void AnalysisPyramid::mergeInto(M4Bucket &acc, const Node &node, const qint64 *t, const double *v)
{
    acc.hasNonFinite = acc.hasNonFinite || node.hasNonFinite;
    if (node.finite == 0) return;
    const auto i = [](qint64 index) { return static_cast<std::size_t>(index); };
    if (!acc.hasFinite) {
        acc.tFirst = t[i(node.iFirst)];
        acc.yFirst = v[i(node.iFirst)];
        acc.tMin = t[i(node.iMin)];
        acc.yMin = node.yMin;
        acc.tMax = t[i(node.iMax)];
        acc.yMax = node.yMax;
        acc.hasFinite = true;
    } else {
        if (node.yMin < acc.yMin) {
            acc.yMin = node.yMin;
            acc.tMin = t[i(node.iMin)];
        }
        if (node.yMax > acc.yMax) {
            acc.yMax = node.yMax;
            acc.tMax = t[i(node.iMax)];
        }
    }
    acc.tLast = t[i(node.iLast)];
    acc.yLast = v[i(node.iLast)];
    acc.count += static_cast<size_t>(node.finite);
}

void AnalysisPyramid::build(const qint64 *t, const double *values, std::size_t count)
{
    clear();
    const std::size_t blocks = count / kBlockSize; // a partial last block is read raw
    if (blocks == 0) return;
    m_blockFirstT.resize(blocks);
    for (std::size_t b = 0; b < blocks; ++b) m_blockFirstT[b] = t[b * kBlockSize];
    std::vector<Node> level(blocks);
    for (std::size_t b = 0; b < blocks; ++b) {
        Node n;
        const std::size_t base = b * kBlockSize;
        for (std::size_t j = base; j < base + kBlockSize; ++j) {
            const double y = values[j];
            if (!std::isfinite(y)) {
                n.hasNonFinite = true;
                continue;
            }
            const auto idx = static_cast<qint64>(j);
            if (n.finite == 0) {
                n.yMin = n.yMax = y;
                n.iFirst = n.iMin = n.iMax = idx;
            } else {
                if (y < n.yMin) {
                    n.yMin = y;
                    n.iMin = idx;
                }
                if (y > n.yMax) {
                    n.yMax = y;
                    n.iMax = idx;
                }
            }
            n.iLast = idx;
            ++n.finite;
        }
        level[b] = n;
    }
    m_levels.push_back(std::move(level));
    while (m_levels.back().size() >= 2) {
        const std::vector<Node> &below = m_levels.back();
        std::vector<Node> above(below.size() / 2);
        for (std::size_t j = 0; j < above.size(); ++j) {
            Node n = below[2 * j];
            mergeInto(n, below[2 * j + 1]);
            above[j] = n;
        }
        m_levels.push_back(std::move(above));
    }
}

M4Bucket AnalysisPyramid::aggregate(const qint64 *t, const double *v, std::size_t first, std::size_t last) const
{
    M4Bucket bucket;
    if (first >= last) return bucket;
    std::size_t blockA = (first + kBlockSize - 1) / kBlockSize;
    std::size_t blockB = last / kBlockSize;
    if (m_levels.empty()) blockB = 0;
    else blockB = std::min(blockB, m_levels.front().size());
    if (blockA >= blockB) {
        for (std::size_t i = first; i < last; ++i) bucket.addSample(t[i], v[i]);
        return bucket;
    }
    for (std::size_t i = first; i < blockA * kBlockSize; ++i) bucket.addSample(t[i], v[i]);
    // Blocks [blockA, blockB) in time order: nodes taken from the left are merged in at once, nodes
    // taken from the right are collected and merged in reverse at the end
    Node right[64];
    int rightCount = 0;
    std::size_t a = blockA;
    std::size_t b = blockB;
    for (std::size_t level = 0; a < b; ++level) {
        const std::vector<Node> &nodes = m_levels[level];
        if (a & 1U) mergeInto(bucket, nodes[a++], t, v);
        if (b & 1U) right[rightCount++] = nodes[--b];
        a >>= 1;
        b >>= 1;
    }
    while (rightCount > 0) mergeInto(bucket, right[--rightCount], t, v);
    for (std::size_t i = blockB * kBlockSize; i < last; ++i) bucket.addSample(t[i], v[i]);
    return bucket;
}

std::size_t AnalysisPyramid::lowerBound(const qint64 *t, std::size_t count, std::size_t from, qint64 time) const
{
    // Blocks: the first block whose first timestamp is >= time; the answer lies in the block before
    // it (or is that block's first sample). Past the whole blocks: the partial last block.
    std::size_t lo = from;
    std::size_t hi = count;
    if (!m_blockFirstT.empty()) {
        const std::size_t firstBlock = from / kBlockSize;
        const auto it = std::lower_bound(m_blockFirstT.begin() + static_cast<std::ptrdiff_t>(std::min(firstBlock, m_blockFirstT.size())),
                                         m_blockFirstT.end(), time);
        const auto b = static_cast<std::size_t>(it - m_blockFirstT.begin());
        if (b < m_blockFirstT.size()) hi = std::min(count, std::max(from, b * kBlockSize));
        if (b > 0) lo = std::min(hi, std::max(from, (b - 1) * kBlockSize));
    }
    return static_cast<std::size_t>(std::lower_bound(t + lo, t + hi, time) - t);
}

std::vector<M4Bucket> AnalysisPyramid::buckets(const qint64 *t, const double *v, std::size_t count, qint64 origin,
                                               const BucketGrid &grid, qint64 kStart, int numBuckets) const
{
    std::vector<M4Bucket> out;
    buckets(t, v, count, origin, grid, kStart, numBuckets, out);
    return out;
}

void AnalysisPyramid::buckets(const qint64 *t, const double *v, std::size_t count, qint64 origin, const BucketGrid &grid,
                              qint64 kStart, int numBuckets, std::vector<M4Bucket> &out) const
{
    if (numBuckets <= 0 || t == nullptr || v == nullptr) {
        out.clear();
        return;
    }
    out.assign(static_cast<std::size_t>(numBuckets), M4Bucket()); // keeps the capacity of an earlier call
    // Sample index of the first timestamp of each bucket: t - origin >= grid.boundary(k)
    auto firstIndexAt = [&](qint64 k, std::size_t from) {
        const qint64 b = grid.boundary(k);
        // origin + b, saturated
        qint64 tb = 0;
        if (b > 0 && origin > std::numeric_limits<qint64>::max() - b) tb = std::numeric_limits<qint64>::max();
        else if (b < 0 && origin < std::numeric_limits<qint64>::min() - b) tb = std::numeric_limits<qint64>::min();
        else tb = origin + b;
        return lowerBound(t, count, from, tb);
    };
    std::size_t i0 = firstIndexAt(kStart, 0);
    for (int k = 0; k < numBuckets && i0 < count; ++k) {
        const std::size_t i1 = firstIndexAt(kStart + k + 1, i0);
        out[static_cast<std::size_t>(k)] = aggregate(t, v, i0, i1);
        i0 = i1;
    }
}

std::vector<std::vector<M4Point>> segmentsFromBuckets(const std::vector<M4Bucket> &buckets, qint64 gapThreshold)
{
    std::vector<std::vector<M4Point>> segments;
    segments.resize(segmentsFromBuckets(buckets, gapThreshold, segments));
    return segments;
}

std::size_t segmentsFromBuckets(const std::vector<M4Bucket> &buckets, qint64 gapThreshold,
                                std::vector<std::vector<M4Point>> &segments)
{
    // Same rules, in the same order, as M4Decimator::decimateToSegments(). The buffers are kept (#97): a segment is
    // opened (its buffer cleared or added) at its first vertex, the buffers after the last segment are cleared at the end
    std::size_t count = 0;
    std::vector<M4Point> *current = nullptr;
    qint64 prevTimestamp = std::numeric_limits<qint64>::min();
    for (const M4Bucket &b : buckets) {
        if (b.hasNonFinite) {
            current = nullptr;
            prevTimestamp = std::numeric_limits<qint64>::min();
            continue; // a bucket with a non-finite sample emits no vertex (M4Bucket::emitVertices())
        }
        M4Point verts[4];
        const int n = b.emitVertices(verts);
        if (n == 0) continue;
        if (prevTimestamp != std::numeric_limits<qint64>::min() && gapThreshold > 0 && verts[0].t - prevTimestamp > gapThreshold) {
            current = nullptr;
        }
        if (!current) {
            if (count < segments.size()) {
                segments[count].clear();
            } else {
                segments.emplace_back();
            }
            current = &segments[count++];
        }
        for (int i = 0; i < n; ++i) current->push_back(verts[i]);
        prevTimestamp = verts[n - 1].t;
    }
    for (std::size_t s = count; s < segments.size(); ++s) {
        segments[s].clear();
    }
    return count;
}

} // namespace internal
} // namespace QtIndustrialWidgets
