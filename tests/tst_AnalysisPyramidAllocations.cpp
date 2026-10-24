// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #97: AnalysisPyramid::buckets() and segmentsFromBuckets() reuse the buffers they are given, so a frame that fits in
// the buffers of an earlier one allocates nothing, whatever the number of segments. AnalysisPyramid is compiled into
// this executable, so the allocation counter sees its allocations on every platform.

#include "AllocationCounter.h"

#include <QtTest/QtTest>
#include <internal/AnalysisPyramid.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets::internal;
using AllocationCounter::AllocationScope;

namespace {

constexpr int kBuckets = 800;
constexpr qint64 kSamplesPerBucket = 50;
constexpr qint64 kSamples = kBuckets * kSamplesPerBucket;

// kSamples samples, one per time unit, broken by \p gaps non-finite samples evenly spaced: gaps + 1 segments
struct Data {
    std::vector<qint64> t;
    std::vector<double> v;
    AnalysisPyramid pyramid;
};

void makeData(int gaps, Data &d)
{
    d.t.resize(kSamples);
    d.v.resize(kSamples);
    for (qint64 i = 0; i < kSamples; ++i) {
        d.t[static_cast<size_t>(i)] = i;
        d.v[static_cast<size_t>(i)] = std::sin(0.01 * static_cast<double>(i));
    }
    for (int g = 1; g <= gaps; ++g) {
        d.v[static_cast<size_t>(static_cast<qint64>(g) * kSamples / (gaps + 1))] = std::numeric_limits<double>::quiet_NaN();
    }
    d.pyramid.build(d.t.data(), d.v.data(), d.t.size());
}

const BucketGrid kGrid(1, kSamplesPerBucket);

} // namespace

class tst_AnalysisPyramidAllocations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void counterSeesAllocations();
    void bucketBufferIsReused();
    void segmentBuffersAreReused_data();
    void segmentBuffersAreReused();
    void buffersSurviveAChannelWithFewerSegments();
};

// The counter must see an allocation made by code of this executable (building the pyramid)
void tst_AnalysisPyramidAllocations::counterSeesAllocations()
{
    Data d;
    makeData(0, d);
    AnalysisPyramid pyramid;
    AllocationScope scope;
    pyramid.build(d.t.data(), d.v.data(), d.t.size());
    QVERIFY2(scope.count() > 0, "the allocation counter does not see the allocations of AnalysisPyramid");
}

// A second buckets() into the same buffer allocates nothing
void tst_AnalysisPyramidAllocations::bucketBufferIsReused()
{
    Data d;
    makeData(10, d);
    std::vector<M4Bucket> buckets;
    d.pyramid.buckets(d.t.data(), d.v.data(), d.t.size(), 0, kGrid, 0, kBuckets, buckets); // first frame
    QCOMPARE(buckets.size(), static_cast<std::size_t>(kBuckets));
    long long allocations = 0;
    {
        AllocationScope scope;
        d.pyramid.buckets(d.t.data(), d.v.data(), d.t.size(), 0, kGrid, 0, kBuckets, buckets);
        allocations = scope.count();
    }
    QCOMPARE(buckets.size(), static_cast<std::size_t>(kBuckets));
    QVERIFY2(allocations == 0, qPrintable(QStringLiteral("%1 allocations in the second buckets()").arg(allocations)));
}

void tst_AnalysisPyramidAllocations::segmentBuffersAreReused_data()
{
    QTest::addColumn<int>("gaps");
    QTest::newRow("0 gaps") << 0;
    QTest::newRow("10 gaps") << 10;
    QTest::newRow("100 gaps") << 100;
}

// A second segmentsFromBuckets() into the same buffers allocates nothing
void tst_AnalysisPyramidAllocations::segmentBuffersAreReused()
{
    QFETCH(int, gaps);
    Data d;
    makeData(gaps, d);
    const std::vector<M4Bucket> buckets = d.pyramid.buckets(d.t.data(), d.v.data(), d.t.size(), 0, kGrid, 0, kBuckets);
    std::vector<std::vector<M4Point>> segments;
    // First frame: the buffers are allocated; the gaps break the trace into gaps + 1 segments
    QCOMPARE(segmentsFromBuckets(buckets, 0, segments), static_cast<std::size_t>(gaps + 1));
    long long allocations = 0;
    std::size_t count = 0;
    {
        AllocationScope scope;
        count = segmentsFromBuckets(buckets, 0, segments);
        allocations = scope.count();
    }
    QCOMPARE(count, static_cast<std::size_t>(gaps + 1));
    QVERIFY2(allocations == 0, qPrintable(QStringLiteral("%1 gaps: %2 allocations in the second segmentsFromBuckets()")
                                              .arg(gaps).arg(allocations)));
}

// AnalysisChart uses the same buffers for every channel: a channel with fewer segments must not free the buffers that
// the next channel with more segments needs, and the segments must be those of the returning overload
void tst_AnalysisPyramidAllocations::buffersSurviveAChannelWithFewerSegments()
{
    Data many;
    Data few;
    makeData(100, many);
    makeData(0, few);
    const std::vector<M4Bucket> manyBuckets = many.pyramid.buckets(many.t.data(), many.v.data(), many.t.size(), 0, kGrid, 0, kBuckets);
    const std::vector<M4Bucket> fewBuckets = few.pyramid.buckets(few.t.data(), few.v.data(), few.t.size(), 0, kGrid, 0, kBuckets);
    std::vector<std::vector<M4Point>> segments;
    QCOMPARE(segmentsFromBuckets(manyBuckets, 0, segments), std::size_t{101});
    QCOMPARE(segmentsFromBuckets(fewBuckets, 0, segments), std::size_t{1});
    long long allocations = 0;
    std::size_t count = 0;
    {
        AllocationScope scope;
        count = segmentsFromBuckets(manyBuckets, 0, segments);
        allocations = scope.count();
    }
    QCOMPARE(count, std::size_t{101});
    QVERIFY2(allocations == 0, qPrintable(QStringLiteral("%1 allocations after a channel with fewer segments").arg(allocations)));
    const auto expected = segmentsFromBuckets(manyBuckets, 0);
    QCOMPARE(expected.size(), count);
    for (std::size_t s = 0; s < count; ++s) {
        QCOMPARE(segments[s].size(), expected[s].size());
        for (std::size_t i = 0; i < expected[s].size(); ++i) {
            QCOMPARE(segments[s][i].t, expected[s][i].t);
            QCOMPARE(segments[s][i].value, expected[s][i].value);
        }
    }
}

QTEST_GUILESS_MAIN(tst_AnalysisPyramidAllocations)
#include "tst_AnalysisPyramidAllocations.moc"
