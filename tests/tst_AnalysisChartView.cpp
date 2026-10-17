// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>

#include "internal/AnalysisChartView.h"

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

QTEST_MAIN(tst_AnalysisChartView)
#include "tst_AnalysisChartView.moc"
