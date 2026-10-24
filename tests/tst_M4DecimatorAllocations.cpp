// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #8: IncrementalStream::extractVisibleSegments() reuses the segment buffers it is given, so a frame whose segments
// fit in the buffers of an earlier frame allocates nothing, whatever the number of segments (gaps). M4Decimator is
// compiled into this executable, so the allocation counter sees its allocations on every platform.

#include "AllocationCounter.h"

#include <QtTest/QtTest>
#include <internal/M4Decimator.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets::internal;
using AllocationCounter::AllocationScope;

namespace {

constexpr int kBuckets = 800;
constexpr qint64 kSamplesPerBucket = 10;

// A stream over kBuckets buckets whose sine is broken by \p gaps non-finite samples, evenly spaced: gaps + 1 segments
M4Decimator::IncrementalStream makeStream(int gaps)
{
    M4Decimator::IncrementalStream stream(kBuckets);
    const qint64 samples = kBuckets * kSamplesPerBucket;
    const qint64 spacing = gaps > 0 ? samples / (gaps + 1) : samples + 1;
    for (qint64 i = 0; i < samples; ++i) {
        const bool gap = gaps > 0 && i > 0 && i % spacing == 0 && i / spacing <= gaps;
        stream.addSample(i, gap ? std::numeric_limits<double>::quiet_NaN() : std::sin(0.01 * static_cast<double>(i)),
                         kSamplesPerBucket);
    }
    return stream;
}

} // namespace

class tst_M4DecimatorAllocations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void counterSeesAllocations();
    void segmentBuffersAreReused_data();
    void segmentBuffersAreReused();
    void buffersSurviveAStreamWithFewerSegments();
};

// The counter must see an allocation made by code of this executable (the stream's ring buffer)
void tst_M4DecimatorAllocations::counterSeesAllocations()
{
    AllocationScope scope;
    M4Decimator::IncrementalStream stream(kBuckets);
    QVERIFY2(scope.count() > 0, "the allocation counter does not see the allocations of IncrementalStream");
}

void tst_M4DecimatorAllocations::segmentBuffersAreReused_data()
{
    QTest::addColumn<int>("gaps");
    QTest::newRow("0 gaps") << 0;
    QTest::newRow("10 gaps") << 10;
    QTest::newRow("100 gaps") << 100;
}

// The second extraction into the same buffers allocates nothing
void tst_M4DecimatorAllocations::segmentBuffersAreReused()
{
    QFETCH(int, gaps);
    const M4Decimator::IncrementalStream stream = makeStream(gaps);
    std::vector<std::vector<M4Point>> segments;
    // First frame: the buffers are allocated; the gaps break the trace into gaps + 1 segments
    QCOMPARE(stream.extractVisibleSegments(segments, 0, kBuckets, 0), static_cast<std::size_t>(gaps + 1));
    long long allocations = 0;
    std::size_t count = 0;
    {
        AllocationScope scope;
        count = stream.extractVisibleSegments(segments, 0, kBuckets, 0);
        allocations = scope.count();
    }
    QCOMPARE(count, static_cast<std::size_t>(gaps + 1));
    QVERIFY2(allocations == 0, qPrintable(QStringLiteral("%1 gaps: %2 allocations in the second extraction")
                                              .arg(gaps).arg(allocations)));
}

// StripChart extracts every channel into the same buffers: a channel with fewer segments must not free the buffers
// that the next channel with more segments needs
void tst_M4DecimatorAllocations::buffersSurviveAStreamWithFewerSegments()
{
    const M4Decimator::IncrementalStream many = makeStream(100);
    const M4Decimator::IncrementalStream few = makeStream(0);
    std::vector<std::vector<M4Point>> segments;
    QCOMPARE(many.extractVisibleSegments(segments, 0, kBuckets, 0), std::size_t{101});
    QCOMPARE(few.extractVisibleSegments(segments, 0, kBuckets, 0), std::size_t{1});
    long long allocations = 0;
    std::size_t count = 0;
    {
        AllocationScope scope;
        count = many.extractVisibleSegments(segments, 0, kBuckets, 0);
        allocations = scope.count();
    }
    QCOMPARE(count, std::size_t{101});
    QVERIFY2(allocations == 0, qPrintable(QStringLiteral("%1 allocations after a stream with fewer segments")
                                              .arg(allocations)));
}

QTEST_GUILESS_MAIN(tst_M4DecimatorAllocations)
#include "tst_M4DecimatorAllocations.moc"
