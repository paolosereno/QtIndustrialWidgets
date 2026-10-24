// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>

#include "internal/AnalysisChartView.h"
#include "internal/AnalysisPyramid.h"

#include <QtCore/QRandomGenerator>

#include <cmath>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets::internal;

class tst_AnalysisChartView : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void gapThresholdFollowsStripChart();
    void nearestSampleIndex_data();
    void nearestSampleIndex();
    void minimumSpan();
    void fullRange();
    void clampKeepsSpanWithinLimits();
    void clampKeepsViewNearTheData();
    void autoscale();
    void relativeTimeTicks();
    void absoluteTimeTicks();
    void timeLabelsAreDistinct();
    void valueTicks();
    void zoomXKeepsTheAnchor();
    void zoomYKeepsTheAnchor();
    void wheelFactor();
    void readoutValues();
    void readoutTimes();
    void pyramidMatchesM4Decimator_data();
    void pyramidMatchesM4Decimator();
    void pyramidEdgeCases();
    void segmentsMatchM4Decimator();
    void pyramidLowerBound();
};

// #25 D4: the automatic gap threshold is StripChart's: 4 x an EMA of the strictly positive steps,
// in sample order (first step itself, then 0.9 ema + 0.1 dt), truncated; a positive user threshold wins
void tst_AnalysisChartView::gapThresholdFollowsStripChart()
{
    const std::vector<qint64> uniform{0, 1000, 2000, 3000};
    QCOMPARE(gapThresholdFor(uniform.data(), uniform.size(), 0), qint64(4000));
    QCOMPARE(gapThresholdFor(uniform.data(), uniform.size(), 123), qint64(123));
    // Steps 10, 20: ema = 10, then 0.9 * 10 + 0.1 * 20 = 11 -> 44
    const std::vector<qint64> twoSteps{0, 10, 30};
    QCOMPARE(gapThresholdFor(twoSteps.data(), twoSteps.size(), 0), qint64(44));
    // Equal timestamps do not update the EMA
    const std::vector<qint64> withEqual{0, 10, 10, 10, 30};
    QCOMPARE(gapThresholdFor(withEqual.data(), withEqual.size(), 0), qint64(44));
    // The order matters: steps 20, 10 give 0.9 * 20 + 0.1 * 10 = 19 -> 76
    const std::vector<qint64> reversedSteps{0, 20, 30};
    QCOMPARE(gapThresholdFor(reversedSteps.data(), reversedSteps.size(), 0), qint64(76));
    // No step: no automatic gap
    const std::vector<qint64> single{5};
    QCOMPARE(gapThresholdFor(single.data(), single.size(), 0), qint64(0));
    const std::vector<qint64> allEqual{5, 5, 5};
    QCOMPARE(gapThresholdFor(allEqual.data(), allEqual.size(), 0), qint64(0));
    QCOMPARE(gapThresholdFor(nullptr, 0, 0), qint64(0));
}

void tst_AnalysisChartView::nearestSampleIndex_data()
{
    QTest::addColumn<qint64>("query");
    QTest::addColumn<qint64>("expected");
    // t = 0, 10, 20, 20, 20, 40
    QTest::addRow("exact first") << qint64(0) << qint64(0);
    QTest::addRow("exact middle") << qint64(10) << qint64(1);
    QTest::addRow("exact last") << qint64(40) << qint64(5);
    QTest::addRow("closer to the earlier") << qint64(13) << qint64(1);
    QTest::addRow("closer to the later") << qint64(17) << qint64(2);
    QTest::addRow("tie goes to the earlier") << qint64(15) << qint64(1);
    QTest::addRow("equal timestamps: the first one") << qint64(20) << qint64(2);
    QTest::addRow("after a run of equal timestamps, tie") << qint64(30) << qint64(2);
    QTest::addRow("after a run, closer to the later") << qint64(31) << qint64(5);
    QTest::addRow("before the data") << qint64(-1) << qint64(-1);
    QTest::addRow("after the data") << qint64(41) << qint64(-1);
}

// #25: nearest sample in time, ties to the earlier one, -1 outside the data
void tst_AnalysisChartView::nearestSampleIndex()
{
    QFETCH(qint64, query);
    QFETCH(qint64, expected);
    const std::vector<qint64> t{0, 10, 20, 20, 20, 40};
    QCOMPARE(QtIndustrialWidgets::internal::nearestSampleIndex(t.data(), t.size(), query), expected);
}

void tst_AnalysisChartView::minimumSpan()
{
    QCOMPARE(minimumXSpan({}), qint64(1));
    QCOMPARE(minimumXSpan({0.0, -5.0, std::numeric_limits<double>::quiet_NaN()}), qint64(1));
    QCOMPARE(minimumXSpan({1000.0, 250.5}), qint64(1002)); // ceil(4 x 250.5)
    QCOMPARE(minimumXSpan({0.1}), qint64(1));               // never below 1 ns
}

void tst_AnalysisChartView::fullRange()
{
    const XRange a = fullXRange(100, 1100, 10);
    QCOMPARE(a.start, qint64(100));
    QCOMPARE(a.end, qint64(1100));
    // Extent below the minimum span: the minimum span centred on the data
    const XRange b = fullXRange(500, 500, 100);
    QCOMPARE(b.span(), qint64(100));
    QCOMPARE(b.start, qint64(450));
}

// #25: the span stays between the minimum span and the data extent
void tst_AnalysisChartView::clampKeepsSpanWithinLimits()
{
    // Too small: the minimum span around the request's centre
    const XRange small = clampXRange({500, 502}, 0, 1000, 40);
    QCOMPARE(small.span(), qint64(40));
    QCOMPARE(small.start, qint64(481));
    // Too large: the full extent
    const XRange large = clampXRange({-5000, 5000}, 0, 1000, 40);
    QCOMPARE(large.start, qint64(0));
    QCOMPARE(large.end, qint64(1000));
    // Reversed request
    const XRange reversed = clampXRange({800, 200}, 0, 1000, 40);
    QCOMPARE(reversed.start, qint64(200));
    QCOMPARE(reversed.end, qint64(800));
}

// #25: the view may leave the data by at most one view width
void tst_AnalysisChartView::clampKeepsViewNearTheData()
{
    const XRange left = clampXRange({-900, -700}, 0, 1000, 40);
    QCOMPARE(left.span(), qint64(200));
    QCOMPARE(left.start, qint64(-200)); // start >= dataStart - span
    const XRange right = clampXRange({1700, 1900}, 0, 1000, 40);
    QCOMPARE(right.end, qint64(1200));  // end <= dataEnd + span
    const XRange inside = clampXRange({100, 300}, 0, 1000, 40);
    QCOMPARE(inside.start, qint64(100));
    QCOMPARE(inside.end, qint64(300));
}

void tst_AnalysisChartView::autoscale()
{
    const YRange r = autoscaledYRange(0.0, 10.0, true);
    QVERIFY(r.hasData);
    QCOMPARE(r.minimum, -1.0);
    QCOMPARE(r.maximum, 11.0);
    const YRange constant = autoscaledYRange(5.0, 5.0, true);
    QCOMPARE(constant.minimum, 4.0);
    QCOMPARE(constant.maximum, 6.0);
    QVERIFY(!autoscaledYRange(0.0, 0.0, false).hasData);
    QVERIFY(!autoscaledYRange(std::numeric_limits<double>::quiet_NaN(), 1.0, true).hasData);
}

// #25 D6: Relative labels are the time elapsed since the data start, with ticks aligned on it
void tst_AnalysisChartView::relativeTimeTicks()
{
    const qint64 origin = 1234567890123LL; // an arbitrary absolute start
    const auto ticks = timeAxisTicks({origin, origin + 10000000000LL}, origin, 600.0, 60.0, false, QDateTime());
    QVERIFY(ticks.size() >= 3);
    QCOMPARE(static_cast<qint64>(ticks.front().position), origin);
    QCOMPARE(ticks.front().label, QStringLiteral("0 s"));
    const qint64 step = static_cast<qint64>(ticks[1].position - ticks[0].position);
    QCOMPARE(ticks[1].label, QStringLiteral("%1 s").arg(step / 1000000000LL));
}

// #25: Absolute labels are timeOrigin + t (HH:mm:ss.zzz); t itself without an origin
void tst_AnalysisChartView::absoluteTimeTicks()
{
    const QDateTime origin(QDate(2026, 10, 17), QTime(10, 0, 0), Qt::UTC);
    const auto ticks = timeAxisTicks({0, 2000000000LL}, 0, 400.0, 60.0, true, origin);
    QVERIFY(!ticks.empty());
    QCOMPARE(ticks.front().label, origin.toString(QStringLiteral("HH:mm:ss.zzz")));
    QCOMPARE(ticks.back().label, origin.addMSecs(static_cast<qint64>(ticks.back().position) / 1000000).toString(QStringLiteral("HH:mm:ss.zzz")));
    // Without an origin: t as elapsed time from 0, aligned on 0
    const auto raw = timeAxisTicks({1500000000LL, 3500000000LL}, 1500000000LL, 400.0, 60.0, true, QDateTime());
    QVERIFY(!raw.empty());
    QVERIFY(raw.front().label.endsWith(QStringLiteral(" s")) || raw.front().label.endsWith(QStringLiteral(" ms")));
    QCOMPARE(std::fmod(raw.front().position, 1e8), 0.0);
}

// Consecutive time labels never repeat (decimals follow the step)
void tst_AnalysisChartView::timeLabelsAreDistinct()
{
    for (qint64 span : {qint64(37), qint64(4500), qint64(2500000), qint64(1700000000LL), qint64(250000000000LL)}) {
        const auto ticks = timeAxisTicks({0, span}, 0, 1000.0, 40.0, false, QDateTime());
        for (std::size_t i = 1; i < ticks.size(); ++i) {
            QVERIFY2(ticks[i].label != ticks[i - 1].label, qPrintable(QStringLiteral("span %1: '%2' twice").arg(span).arg(ticks[i].label)));
        }
    }
}

void tst_AnalysisChartView::valueTicks()
{
    const auto ticks = valueAxisTicks(-0.37, 1.2, 300.0, 30.0);
    QVERIFY(ticks.size() >= 3);
    for (std::size_t i = 1; i < ticks.size(); ++i) {
        QVERIFY(ticks[i].position > ticks[i - 1].position);
        QVERIFY(ticks[i].label != ticks[i - 1].label);
        QVERIFY2(!ticks[i].label.startsWith(QStringLiteral("-0")) || ticks[i].position < 0.0, qPrintable(ticks[i].label));
    }
    // A 1-2-5 step and at least the requested spacing
    const double step = ticks[1].position - ticks[0].position;
    const double mantissa = step / std::pow(10.0, std::floor(std::log10(step)));
    QVERIFY(std::abs(mantissa - 1.0) < 1e-9 || std::abs(mantissa - 2.0) < 1e-9 || std::abs(mantissa - 5.0) < 1e-9);
    QVERIFY(step / (1.2 + 0.37) * 300.0 >= 30.0 - 1e-9);
    // No ticks for an empty or invalid range
    QVERIFY(valueAxisTicks(1.0, 1.0, 300.0, 30.0).empty());
    QVERIFY(valueAxisTicks(std::numeric_limits<double>::quiet_NaN(), 1.0, 300.0, 30.0).empty());
}

// #25 D3: the span is divided by the factor and the anchor keeps its relative position
void tst_AnalysisChartView::zoomXKeepsTheAnchor()
{
    const XRange in = zoomXAround({0, 1000}, 250.0, 2.0);
    QCOMPARE(in.start, qint64(125));
    QCOMPARE(in.end, qint64(625));
    const XRange out = zoomXAround({0, 1000}, 250.0, 0.5);
    QCOMPARE(out.start, qint64(-250));
    QCOMPARE(out.end, qint64(1750));
    // Anchor at the right edge stays there
    const XRange right = zoomXAround({1000, 2000}, 2000.0, 1.25);
    QCOMPARE(right.end, qint64(2000));
    QCOMPARE(right.span(), qint64(800));
    // Invalid factor or anchor: unchanged
    for (double f : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        const XRange same = zoomXAround({0, 1000}, 500.0, f);
        QCOMPARE(same.start, qint64(0));
        QCOMPARE(same.end, qint64(1000));
    }
    QCOMPARE(zoomXAround({0, 1000}, std::numeric_limits<double>::quiet_NaN(), 2.0).end, qint64(1000));
}

void tst_AnalysisChartView::zoomYKeepsTheAnchor()
{
    const YRange in = zoomYAround(0.0, 10.0, 5.0, 2.0);
    QCOMPARE(in.minimum, 2.5);
    QCOMPARE(in.maximum, 7.5);
    const YRange bottom = zoomYAround(0.0, 10.0, 0.0, 2.0);
    QCOMPARE(bottom.minimum, 0.0);
    QCOMPARE(bottom.maximum, 5.0);
    const YRange out = zoomYAround(-1.0, 1.0, 0.0, 0.8);
    QCOMPARE(out.minimum, -1.25);
    QCOMPARE(out.maximum, 1.25);
    // Degenerate input or result: unchanged
    const YRange flat = zoomYAround(1.0, 1.0, 1.0, 2.0);
    QCOMPARE(flat.minimum, 1.0);
    QCOMPARE(flat.maximum, 1.0);
    const YRange tiny = zoomYAround(0.0, 1e-300, 0.0, 1e300);
    QCOMPARE(tiny.maximum, 1e-300);
}

// #25 D3: 1.25 per wheel notch (120 units), fractional for high-resolution wheels
void tst_AnalysisChartView::wheelFactor()
{
    QCOMPARE(wheelZoomFactor(120), 1.25);
    QCOMPARE(wheelZoomFactor(-120), 0.8);
    QCOMPARE(wheelZoomFactor(240), 1.5625);
    QCOMPARE(wheelZoomFactor(60), std::sqrt(1.25));
    QCOMPARE(wheelZoomFactor(0), 1.0);
}

// #25: values with the given decimals, "---" for a missing or non-finite sample, no negative zero
void tst_AnalysisChartView::readoutValues()
{
    const double v = 1.5;
    QCOMPARE(readoutValueText(&v, 2), QStringLiteral("1.50"));
    const double small = -0.0001;
    QCOMPARE(readoutValueText(&small, 2), QStringLiteral("0.00"));
    const double neg = -12.345;
    QCOMPARE(readoutValueText(&neg, 1), QStringLiteral("-12.3"));
    QCOMPARE(readoutValueText(nullptr, 2), QStringLiteral("---"));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(readoutValueText(&nan, 2), QStringLiteral("---"));
    const double inf = -std::numeric_limits<double>::infinity();
    QCOMPARE(readoutValueText(&inf, 2), QStringLiteral("---"));
    // The decimals of the value axis labels
    QCOMPARE(valueAxisDecimals(0.0, 1.0, 300.0, 30.0), 1);   // step 0.1
    QCOMPARE(valueAxisDecimals(0.0, 1000.0, 300.0, 30.0), 0); // step 100
    QCOMPARE(valueAxisDecimals(0.0, 0.01, 300.0, 30.0), 3);  // step 0.001
}

// #25 D6: readout times count from the data start (Relative) or are timeOrigin + t (Absolute), in the
// unit of the axis step (10 x the resolution) with the decimals of the resolution
void tst_AnalysisChartView::readoutTimes()
{
    const qint64 s = 1000000000LL;
    QCOMPARE(readoutTimeText(7 * s + s / 2, 7 * s, false, QDateTime(), 100000000LL), QStringLiteral("0.5 s"));
    QCOMPARE(readoutTimeText(7 * s + 123456789LL, 7 * s, false, QDateTime(), 1000000LL), QStringLiteral("123 ms"));
    const QDateTime origin(QDate(2026, 10, 17), QTime(12, 0, 0), Qt::UTC);
    QCOMPARE(readoutTimeText(1500000000LL, 7 * s, true, origin, 100000000LL), QStringLiteral("12:00:01.500"));
    // Absolute without an origin: t itself, as elapsed time from 0
    QCOMPARE(readoutTimeText(7 * s + s / 2, 7 * s, true, QDateTime(), 100000000LL), QStringLiteral("7.5 s"));
    // Signed durations
    QCOMPARE(durationText(1500000000LL, 100000000LL), QStringLiteral("1.5 s"));
    QCOMPARE(durationText(-250000000LL, 10000000LL), QStringLiteral("-250 ms"));
    QCOMPARE(durationText(0, 10000000LL), QStringLiteral("0 ms"));
}

namespace {

// Random channel: irregular steps (some zero: equal timestamps), values with spikes, NaN and +-Inf
struct RandomChannel {
    std::vector<qint64> t;
    std::vector<double> v;
};

RandomChannel randomChannel(quint32 seed, std::size_t n, double nonFiniteShare)
{
    QRandomGenerator rng(seed);
    RandomChannel c;
    qint64 t = 1000000000LL + static_cast<qint64>(rng.bounded(1000));
    for (std::size_t i = 0; i < n; ++i) {
        t += static_cast<qint64>(rng.bounded(4)) == 0 ? 0 : 1 + static_cast<qint64>(rng.bounded(200));
        c.t.push_back(t);
        const double r = rng.generateDouble();
        double y = std::sin(static_cast<double>(i) / 50.0) + 0.1 * rng.generateDouble();
        if (r < nonFiniteShare) {
            y = (rng.bounded(3) == 0) ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
        } else if (r < nonFiniteShare + 0.002) {
            y = rng.bounded(2) == 0 ? 50.0 : -50.0; // spike
        } else if (rng.bounded(10) == 0) {
            y = std::round(y * 4.0) / 4.0; // equal values: ties for min and max
        }
        c.v.push_back(y);
    }
    return c;
}

QString bucketDiff(const M4Bucket &a, const M4Bucket &b)
{
    auto same = [](double x, double y) { return x == y || (std::isnan(x) && std::isnan(y)); };
    QStringList d;
    if (a.tFirst != b.tFirst || !same(a.yFirst, b.yFirst)) d << QStringLiteral("first %1/%2 vs %3/%4").arg(a.tFirst).arg(a.yFirst).arg(b.tFirst).arg(b.yFirst);
    if (a.tLast != b.tLast || !same(a.yLast, b.yLast)) d << QStringLiteral("last %1/%2 vs %3/%4").arg(a.tLast).arg(a.yLast).arg(b.tLast).arg(b.yLast);
    if (a.tMin != b.tMin || !same(a.yMin, b.yMin)) d << QStringLiteral("min %1/%2 vs %3/%4").arg(a.tMin).arg(a.yMin).arg(b.tMin).arg(b.yMin);
    if (a.tMax != b.tMax || !same(a.yMax, b.yMax)) d << QStringLiteral("max %1/%2 vs %3/%4").arg(a.tMax).arg(a.yMax).arg(b.tMax).arg(b.yMax);
    if (a.count != b.count) d << QStringLiteral("count %1 vs %2").arg(a.count).arg(b.count);
    if (a.hasFinite != b.hasFinite || a.hasNonFinite != b.hasNonFinite) d << QStringLiteral("flags");
    return d.join(QStringLiteral(", "));
}

// The reference: M4Decimator on t - origin, timestamps shifted back
std::vector<M4Bucket> referenceBuckets(const RandomChannel &c, qint64 origin, const BucketGrid &grid, qint64 kStart, int numBuckets)
{
    std::vector<qint64> rel(c.t.size());
    for (std::size_t i = 0; i < c.t.size(); ++i) rel[i] = c.t[i] - origin;
    std::vector<M4Bucket> out = M4Decimator::decimateToBuckets(rel.data(), c.v.data(), rel.size(), grid, kStart, numBuckets);
    for (M4Bucket &b : out) {
        if (!b.hasFinite) continue;
        b.tFirst += origin;
        b.tLast += origin;
        b.tMin += origin;
        b.tMax += origin;
    }
    return out;
}

} // namespace

void tst_AnalysisChartView::pyramidMatchesM4Decimator_data()
{
    QTest::addColumn<quint32>("seed");
    QTest::addColumn<int>("count");
    QTest::addColumn<double>("nonFiniteShare");
    QTest::addRow("100k, 0.1 %% non-finite") << 1u << 100000 << 0.001;
    QTest::addRow("100k, no non-finite") << 2u << 100000 << 0.0;
    QTest::addRow("50k, 2 %% non-finite") << 3u << 50000 << 0.02;
    QTest::addRow("3001 (partial last block)") << 4u << 3001 << 0.005;
    QTest::addRow("63 (no whole block)") << 5u << 63 << 0.05;
    QTest::addRow("all non-finite") << 6u << 4096 << 1.0;
}

// #25: the pyramid gives exactly M4Decimator's buckets (first, last, earliest min and max, finite
// count, dropout flag), whatever the zoom level and the position of the window
void tst_AnalysisChartView::pyramidMatchesM4Decimator()
{
    QFETCH(quint32, seed);
    QFETCH(int, count);
    QFETCH(double, nonFiniteShare);
    const RandomChannel c = randomChannel(seed, static_cast<std::size_t>(count), nonFiniteShare);
    AnalysisPyramid pyramid;
    pyramid.build(c.t.data(), c.v.data(), c.v.size());
    const qint64 extent = c.t.back() - c.t.front();
    QRandomGenerator rng(seed * 7919u);
    int compared = 0;
    for (int trial = 0; trial < 60; ++trial) {
        // Spans from a few samples to more than the whole extent; windows anywhere around the data
        const double zoom = std::pow(10.0, -4.0 + 4.3 * rng.generateDouble());
        const qint64 span = std::max<qint64>(1, static_cast<qint64>(static_cast<double>(extent) * zoom));
        const qint64 origin = c.t.front() - span / 4 + static_cast<qint64>(rng.generateDouble() * static_cast<double>(extent));
        const int width = 1 + static_cast<int>(rng.bounded(2000));
        const BucketGrid grid(width, span);
        const auto expected = referenceBuckets(c, origin, grid, 0, width);
        const auto actual = pyramid.buckets(c.t.data(), c.v.data(), c.t.size(), origin, grid, 0, width);
        QCOMPARE(actual.size(), expected.size());
        for (std::size_t k = 0; k < expected.size(); ++k) {
            const QString diff = bucketDiff(actual[k], expected[k]);
            QVERIFY2(diff.isEmpty(), qPrintable(QStringLiteral("trial %1, span %2, width %3, bucket %4: %5")
                                                     .arg(trial).arg(span).arg(width).arg(k).arg(diff)));
            compared += expected[k].count > 0 ? 1 : 0;
        }
    }
    // The trials did hit data: buckets with finite samples, 10 per trial on average (fewer samples: one per sample)
    if (nonFiniteShare < 1.0) QVERIFY2(compared >= std::min(600, count), qPrintable(QString::number(compared)));
}

void tst_AnalysisChartView::pyramidEdgeCases()
{
    AnalysisPyramid empty;
    empty.build(nullptr, nullptr, 0);
    QCOMPARE(empty.aggregate(nullptr, nullptr, 0, 0).count, std::size_t(0));
    QVERIFY(empty.buckets(nullptr, nullptr, 0, 0, BucketGrid(10, 100), 0, 10).empty());

    // Ties: the earliest minimum and maximum across block boundaries
    std::vector<qint64> t(256);
    std::vector<double> v(256, 1.0);
    for (std::size_t i = 0; i < t.size(); ++i) t[i] = static_cast<qint64>(i);
    v[10] = -2.0;
    v[200] = -2.0;
    v[70] = 3.0;
    v[130] = 3.0;
    AnalysisPyramid p;
    p.build(t.data(), v.data(), v.size());
    const M4Bucket all = p.aggregate(t.data(), v.data(), 0, 256);
    QCOMPARE(all.tMin, qint64(10));
    QCOMPARE(all.tMax, qint64(70));
    QCOMPARE(all.count, std::size_t(256));
    // Whole blocks only, starting after the first minimum
    const M4Bucket tail = p.aggregate(t.data(), v.data(), 64, 256);
    QCOMPARE(tail.tMin, qint64(200));
    QCOMPARE(tail.tFirst, qint64(64));
    QCOMPARE(tail.tLast, qint64(255));
    // A NaN inside a whole block marks the bucket
    v[150] = std::numeric_limits<double>::quiet_NaN();
    p.build(t.data(), v.data(), v.size());
    QVERIFY(p.aggregate(t.data(), v.data(), 128, 192).hasNonFinite);
    QVERIFY(!p.aggregate(t.data(), v.data(), 0, 128).hasNonFinite);
    QCOMPARE(p.aggregate(t.data(), v.data(), 128, 192).count, std::size_t(63));
    // About 2 bytes per sample (two nodes of 64 B per block)
    std::vector<double> big(64 * 1024, 0.5);
    std::vector<qint64> bigT(big.size());
    for (std::size_t i = 0; i < bigT.size(); ++i) bigT[i] = static_cast<qint64>(i);
    p.build(bigT.data(), big.data(), big.size());
    // About 2.125 bytes per sample: two nodes of 64 B and one 8 B timestamp per block of 64
    QVERIFY2(p.memoryBytes() <= big.size() * 17 / 8 + 4096, qPrintable(QString::number(p.memoryBytes())));
}

// #25: the trace segments follow M4Decimator::decimateToSegments() (dropout buckets break the
// trace, empty buckets do not, steps above the gap threshold do)
void tst_AnalysisChartView::segmentsMatchM4Decimator()
{
    for (quint32 seed : {11u, 12u, 13u}) {
        const RandomChannel c = randomChannel(seed, 20000, 0.002);
        AnalysisPyramid pyramid;
        pyramid.build(c.t.data(), c.v.data(), c.v.size());
        const qint64 extent = c.t.back() - c.t.front();
        for (qint64 gap : {qint64(0), qint64(150), qint64(400), extent}) {
            for (int width : {37, 640, 1920, 5000}) {
                const BucketGrid grid(width, extent);
                const qint64 origin = c.t.front();
                std::vector<qint64> rel(c.t.size());
                for (std::size_t i = 0; i < c.t.size(); ++i) rel[i] = c.t[i] - origin;
                auto expected = M4Decimator::decimateToSegments(rel.data(), c.v.data(), rel.size(), grid, 0, width, gap);
                const auto actual = segmentsFromBuckets(pyramid.buckets(c.t.data(), c.v.data(), c.t.size(), origin, grid, 0, width), gap);
                QCOMPARE(actual.size(), expected.size());
                for (std::size_t s = 0; s < expected.size(); ++s) {
                    QCOMPARE(actual[s].size(), expected[s].size());
                    for (std::size_t i = 0; i < expected[s].size(); ++i) {
                        QCOMPARE(actual[s][i].t, expected[s][i].t + origin);
                        QCOMPARE(actual[s][i].value, expected[s][i].value);
                    }
                }
            }
        }
    }
}

// The block index search equals std::lower_bound() from any start index, for any time
void tst_AnalysisChartView::pyramidLowerBound()
{
    for (std::size_t n : {std::size_t(0), std::size_t(5), std::size_t(64), std::size_t(65), std::size_t(1000), std::size_t(6400)}) {
        const RandomChannel c = randomChannel(static_cast<quint32>(n) + 100u, n, 0.0);
        AnalysisPyramid pyramid;
        pyramid.build(c.t.data(), c.v.data(), n);
        QRandomGenerator rng(static_cast<quint32>(n));
        const qint64 t0 = n ? c.t.front() - 10 : 0;
        const qint64 t1 = n ? c.t.back() + 10 : 10;
        for (int trial = 0; trial < 2000; ++trial) {
            const std::size_t from = n ? static_cast<std::size_t>(rng.bounded(static_cast<quint32>(n + 1))) : 0;
            // Times on samples (equal timestamps included) and between them
            const qint64 time = (n && trial % 2) ? c.t[static_cast<std::size_t>(rng.bounded(static_cast<quint32>(n)))]
                                                 : t0 + static_cast<qint64>(rng.generateDouble() * static_cast<double>(t1 - t0));
            const auto expected = static_cast<std::size_t>(std::lower_bound(c.t.begin() + static_cast<std::ptrdiff_t>(from), c.t.end(), time) - c.t.begin());
            QCOMPARE(pyramid.lowerBound(c.t.data(), n, from, time), expected);
        }
    }
}

QTEST_MAIN(tst_AnalysisChartView)
#include "tst_AnalysisChartView.moc"
