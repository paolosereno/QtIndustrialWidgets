/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal/M4Decimator.h"
#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

namespace {

// Unsigned 64 x 64 -> 128-bit product from 32-bit halves.
void mul64x64(quint64 a, quint64 b, quint64 &hi, quint64 &lo) noexcept
{
    const quint64 aLo = a & 0xffffffffULL;
    const quint64 aHi = a >> 32;
    const quint64 bLo = b & 0xffffffffULL;
    const quint64 bHi = b >> 32;
    const quint64 p0 = aLo * bLo;
    const quint64 p1 = aLo * bHi;
    const quint64 p2 = aHi * bLo;
    const quint64 p3 = aHi * bHi;
    const quint64 mid = (p0 >> 32) + (p1 & 0xffffffffULL) + (p2 & 0xffffffffULL);
    lo = (p0 & 0xffffffffULL) | (mid << 32);
    hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
}

// (hi:lo) / d for hi < d (the quotient fits in 64 bits), by binary long division.
void div128by64(quint64 hi, quint64 lo, quint64 d, quint64 &quotient, quint64 &remainder) noexcept
{
    quint64 rem = hi;
    quint64 quot = 0;
    for (int i = 63; i >= 0; --i) {
        const bool carry = (rem >> 63) != 0;
        rem = (rem << 1) | ((lo >> i) & 1ULL);
        quot <<= 1;
        if (carry || rem >= d) {
            rem -= d;
            quot |= 1ULL;
        }
    }
    quotient = quot;
    remainder = rem;
}

} // namespace

qint64 mulDivFloor(qint64 a, qint64 b, qint64 c, qint64 *remainder) noexcept
{
    constexpr qint64 kMax = std::numeric_limits<qint64>::max();
    constexpr qint64 kMin = std::numeric_limits<qint64>::min();
    if (b <= 0 || c <= 0) {
        if (remainder) *remainder = 0;
        return 0;
    }
    // a = q * c + r with 0 <= r < c, then a * b / c = q * b + (r * b) / c
    qint64 q = a / c;
    qint64 r = a % c;
    if (r < 0) {
        r += c;
        --q;
    }
    quint64 hi = 0;
    quint64 lo = 0;
    mul64x64(static_cast<quint64>(r), static_cast<quint64>(b), hi, lo);
    quint64 frac = 0;
    quint64 rem = 0;
    div128by64(hi, lo, static_cast<quint64>(c), frac, rem); // r * b / c < b, fits
    if (remainder) *remainder = static_cast<qint64>(rem);

    // q * b + frac, saturated. Overflow is decided on the final sum: the intermediate product
    // q * b can leave the qint64 range while q * b + frac still fits (0 <= frac < b).
    const qint64 f = static_cast<qint64>(frac);
    if (q >= 0) {
        if (q > (kMax - f) / b) return kMax;
        return q * b + f;
    }
    // q < 0: q * b + frac == (q + 1) * b - (b - frac), with 0 < b - frac <= b. The sum is >= kMin
    // iff (q + 1) * b >= kMin + (b - frac), i.e. q + 1 >= ceil((kMin + (b - frac)) / b); the
    // numerator is negative, so C++ truncating division rounds it up as required.
    const qint64 d = b - f;
    if (q + 1 < (kMin + d) / b) return kMin;
    return (q + 1) * b - d;
}

qint64 mulDivCeil(qint64 a, qint64 b, qint64 c) noexcept
{
    qint64 rem = 0;
    const qint64 f = mulDivFloor(a, b, c, &rem);
    if (rem != 0 && f != std::numeric_limits<qint64>::max()) {
        return f + 1;
    }
    return f;
}

std::vector<M4Bucket> M4Decimator::decimateToBuckets(
    const qint64 *timestamps,
    const double *values,
    size_t count,
    const BucketGrid &grid,
    qint64 kStart,
    int numBuckets)
{
    if (numBuckets <= 0 || !timestamps || !values) {
        return {};
    }

    std::vector<M4Bucket> buckets(static_cast<size_t>(numBuckets));
    qint64 kEnd = kStart + numBuckets;

    for (size_t i = 0; i < count; ++i) {
        qint64 t = timestamps[i];
        qint64 k = grid.bucketOf(t);
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
    const BucketGrid &grid,
    qint64 kStart,
    int numBuckets,
    qint64 gapThreshold)
{
    if (numBuckets <= 0 || !timestamps || !values || count == 0) {
        return {};
    }

    std::vector<M4Bucket> buckets = decimateToBuckets(timestamps, values, count, grid, kStart, numBuckets);

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

void M4Decimator::IncrementalStream::addSample(qint64 t, double y, const BucketGrid &grid)
{
    qint64 k = grid.bucketOf(t);

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
    const qint64 *timestamps, const double *values, size_t count, const BucketGrid &grid)
{
    reset();
    if (!timestamps || !values || count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        addSample(timestamps[i], values[i], grid);
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

std::size_t M4Decimator::IncrementalStream::extractVisibleSegments(
    std::vector<std::vector<M4Point>> &segments,
    qint64 kStart, int numBuckets, qint64 gapThreshold) const
{
    // The buffers are kept (#8): used segments are cleared when they are reached, the rest at the end
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

    for (size_t s = segIdx; s < segments.size(); ++s) {
        segments[s].clear();
    }
    return segIdx;
}

std::vector<std::vector<M4Point>> M4Decimator::IncrementalStream::extractVisibleSegments(
    qint64 kStart, int numBuckets, qint64 gapThreshold) const
{
    std::vector<std::vector<M4Point>> segments;
    segments.resize(extractVisibleSegments(segments, kStart, numBuckets, gapThreshold));
    return segments;
}

} // namespace internal
} // namespace QtIndustrialWidgets
