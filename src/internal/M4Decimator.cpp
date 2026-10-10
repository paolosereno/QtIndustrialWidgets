/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal/M4Decimator.h"
#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

std::vector<M4Bucket> M4Decimator::decimateToBuckets(
    const qint64 *timestamps,
    const double *values,
    size_t count,
    qint64 dt_px,
    qint64 kStart,
    int numBuckets)
{
    if (numBuckets <= 0 || dt_px <= 0 || !timestamps || !values) {
        return {};
    }

    std::vector<M4Bucket> buckets(static_cast<size_t>(numBuckets));
    qint64 kEnd = kStart + numBuckets;

    for (size_t i = 0; i < count; ++i) {
        qint64 t = timestamps[i];
        qint64 k = floorDiv(t, dt_px);
        if (k >= kStart && k < kEnd) {
            buckets[static_cast<size_t>(k - kStart)].addSample(t, values[i]);
        }
    }

    return buckets;
}

std::vector<std::vector<M4Point>> M4Decimator::decimateToSegments(
    const qint64 *timestamps,
    const double *values,
    size_t count,
    qint64 dt_px,
    qint64 kStart,
    int numBuckets,
    qint64 gapThreshold)
{
    if (numBuckets <= 0 || dt_px <= 0 || !timestamps || !values || count == 0) {
        return {};
    }

    std::vector<M4Bucket> buckets = decimateToBuckets(timestamps, values, count, dt_px, kStart, numBuckets);

    std::vector<std::vector<M4Point>> segments;
    std::vector<M4Point> currentSegment;
    qint64 prevTimestamp = std::numeric_limits<qint64>::min();

    for (size_t i = 0; i < buckets.size(); ++i) {
        const M4Bucket &b = buckets[i];
        if (!b.isValid()) {
            if (b.hasNonFinite && !currentSegment.empty()) {
                segments.push_back(std::move(currentSegment));
                currentSegment.clear();
                prevTimestamp = std::numeric_limits<qint64>::min();
            }
            continue;
        }

        if (b.hasNonFinite && !currentSegment.empty()) {
            segments.push_back(std::move(currentSegment));
            currentSegment.clear();
            prevTimestamp = std::numeric_limits<qint64>::min();
        }

        M4Point verts[4];
        int vCount = b.emitVertices(verts);
        if (vCount == 0) continue;

        if (prevTimestamp != std::numeric_limits<qint64>::min() && gapThreshold > 0) {
            if (verts[0].t - prevTimestamp > gapThreshold) {
                if (!currentSegment.empty()) {
                    segments.push_back(std::move(currentSegment));
                    currentSegment.clear();
                }
            }
        }

        for (int v = 0; v < vCount; ++v) {
            currentSegment.push_back(verts[v]);
        }
        prevTimestamp = verts[vCount - 1].t;

        if (b.hasNonFinite && !currentSegment.empty()) {
            segments.push_back(std::move(currentSegment));
            currentSegment.clear();
            prevTimestamp = std::numeric_limits<qint64>::min();
        }
    }

    if (!currentSegment.empty()) {
        segments.push_back(std::move(currentSegment));
    }

    return segments;
}

// ============================================================================
// IncrementalStream implementation
// ============================================================================

M4Decimator::IncrementalStream::IncrementalStream(int widthDev, size_t ringSize)
    : m_widthDev(std::max(1, widthDev))
    , m_ringSize((ringSize > 0) ? ringSize : static_cast<size_t>(m_widthDev + 4))
    , m_ring(m_ringSize)
{
}

void M4Decimator::IncrementalStream::setWidth(int widthDev, size_t ringSize)
{
    int w = std::max(1, widthDev);
    size_t rSize = (ringSize > 0) ? ringSize : static_cast<size_t>(w + 4);
    if (m_widthDev == w && m_ringSize == rSize) return;
    m_widthDev = w;
    m_ringSize = rSize;
    m_ring.assign(m_ringSize, BucketEntry{});
    reset();
}

void M4Decimator::IncrementalStream::setRingSize(size_t ringSize)
{
    if (ringSize == 0 || ringSize == m_ringSize) return;
    m_ringSize = ringSize;
    m_ring.assign(m_ringSize, BucketEntry{});
    reset();
}

void M4Decimator::IncrementalStream::reset()
{
    m_activeBucketIndex = std::numeric_limits<qint64>::min();
    m_activeBucket.reset();
    for (auto &slot : m_ring) {
        slot.bucketIndex = std::numeric_limits<qint64>::min();
        slot.bucket.reset();
    }
}

size_t M4Decimator::IncrementalStream::slotFor(qint64 k) const noexcept
{
    qint64 s = static_cast<qint64>(m_ringSize);
    qint64 rem = k % s;
    if (rem < 0) rem += s;
    return static_cast<size_t>(rem);
}

void M4Decimator::IncrementalStream::addSample(qint64 t, double y, qint64 dt_px)
{
    if (dt_px <= 0) return;
    qint64 k = floorDiv(t, dt_px);

    if (m_activeBucketIndex == std::numeric_limits<qint64>::min()) {
        m_activeBucketIndex = k;
        m_activeBucket.reset();
        m_activeBucket.addSample(t, y);
    } else if (k == m_activeBucketIndex) {
        m_activeBucket.addSample(t, y);
    } else if (k > m_activeBucketIndex) {
        // Commit completed active bucket into ring buffer
        size_t slot = slotFor(m_activeBucketIndex);
        m_ring[slot] = {m_activeBucketIndex, m_activeBucket};

        m_activeBucketIndex = k;
        m_activeBucket.reset();
        m_activeBucket.addSample(t, y);
    }
}

void M4Decimator::IncrementalStream::setBucket(qint64 k, const M4Bucket &bucket)
{
    if (k == m_activeBucketIndex) {
        m_activeBucket = bucket;
        return;
    }
    if (m_ring.empty()) return;
    size_t slot = slotFor(k);
    m_ring[slot] = {k, bucket};
}

void M4Decimator::IncrementalStream::rebuild(
    const qint64 *timestamps, const double *values, size_t count, qint64 dt_px)
{
    reset();
    if (!timestamps || !values || count == 0 || dt_px <= 0) return;

    for (size_t i = 0; i < count; ++i) {
        addSample(timestamps[i], values[i], dt_px);
    }
}

const M4Bucket *M4Decimator::IncrementalStream::bucketAt(qint64 k) const
{
    if (k == m_activeBucketIndex) {
        return &m_activeBucket;
    }
    if (m_ring.empty()) return nullptr;
    size_t slot = slotFor(k);
    if (m_ring[slot].bucketIndex == k) {
        return &m_ring[slot].bucket;
    }
    return nullptr;
}

void M4Decimator::IncrementalStream::extractVisibleSegments(
    std::vector<std::vector<M4Point>> &segments,
    qint64 kStart, int numBuckets, qint64 gapThreshold) const
{
    segments.clear();
    if (numBuckets <= 0) return;

    size_t segIdx = 0;
    auto getNextSegment = [&]() -> std::vector<M4Point>& {
        if (segIdx < segments.size()) {
            segments[segIdx].clear();
        } else {
            segments.emplace_back();
            segments.back().reserve(1024);
        }
        return segments[segIdx++];
    };

    std::vector<M4Point> *currentSegment = nullptr;
    qint64 prevTimestamp = std::numeric_limits<qint64>::min();

    for (int i = 0; i < numBuckets; ++i) {
        qint64 k = kStart + i;
        const M4Bucket *b = bucketAt(k);
        if (!b || !b->isValid()) {
            if (b && b->hasNonFinite && currentSegment && !currentSegment->empty()) {
                currentSegment = nullptr;
                prevTimestamp = std::numeric_limits<qint64>::min();
            }
            continue;
        }

        if (b->hasNonFinite && currentSegment && !currentSegment->empty()) {
            currentSegment = nullptr;
            prevTimestamp = std::numeric_limits<qint64>::min();
        }

        M4Point verts[4];
        int vCount = b->emitVertices(verts);
        if (vCount == 0) continue;

        if (prevTimestamp != std::numeric_limits<qint64>::min() && gapThreshold > 0) {
            if (verts[0].t - prevTimestamp > gapThreshold) {
                if (currentSegment && !currentSegment->empty()) {
                    currentSegment = nullptr;
                }
            }
        }

        if (!currentSegment) {
            currentSegment = &getNextSegment();
        }

        for (int v = 0; v < vCount; ++v) {
            currentSegment->push_back(verts[v]);
        }
        prevTimestamp = verts[vCount - 1].t;

        if (b->hasNonFinite && currentSegment && !currentSegment->empty()) {
            currentSegment = nullptr;
            prevTimestamp = std::numeric_limits<qint64>::min();
        }
    }

    segments.resize(segIdx);
}

std::vector<std::vector<M4Point>> M4Decimator::IncrementalStream::extractVisibleSegments(
    qint64 kStart, int numBuckets, qint64 gapThreshold) const
{
    std::vector<std::vector<M4Point>> segments;
    extractVisibleSegments(segments, kStart, numBuckets, gapThreshold);
    return segments;
}

} // namespace internal
} // namespace QtIndustrialWidgets
