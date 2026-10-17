// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/AnalysisChart.h>
#include <QtIndustrialWidgets/StripChart.h>

#include "StripChartTestAccess.h"
#include "internal/AnalysisChartTestAccess.h"

#include <QtGui/QImage>

#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets;
using std::chrono::nanoseconds;

namespace {

const double kNaN = std::numeric_limits<double>::quiet_NaN();

struct Series {
    std::vector<nanoseconds> t;
    std::vector<double> v;
};

// n samples every dt ns from t0, value sin(i / 20)
Series uniformSeries(qint64 t0, qint64 dt, int n)
{
    Series s;
    for (int i = 0; i < n; ++i) {
        s.t.push_back(nanoseconds(t0 + i * dt));
        s.v.push_back(std::sin(i / 20.0));
    }
    return s;
}

// Segment sizes (points per drawn polyline) of a channel in the last AnalysisChart frame
std::vector<int> analysisSegments(const AnalysisChart &chart, int channelId)
{
    std::vector<int> out;
    for (const auto &ch : internal::AnalysisChartTestAccess::lastFrame(chart).channels) {
        if (ch.id == channelId) {
            for (const auto &seg : ch.segments) out.push_back(static_cast<int>(seg.size()));
        }
    }
    return out;
}

// Segment sizes of a channel in the last StripChart frame
std::vector<int> stripSegments(const StripChart &chart, int channelId)
{
    std::vector<int> out;
    const auto &frame = StripChartTestAccess::lastFrame(chart);
    if (channelId < static_cast<int>(frame.channels.size())) {
        for (const auto &seg : frame.channels[static_cast<size_t>(channelId)].segments) out.push_back(static_cast<int>(seg.size()));
    }
    return out;
}

QString describe(const std::vector<int> &v)
{
    QStringList parts;
    for (int x : v) parts << QString::number(x);
    return parts.join(QLatin1Char(','));
}

} // namespace

class tst_AnalysisChart : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void channelIdsAreStable();
    void clearKeepsChannels();
    void dataIsCopied();
    void rejectsDecreasingTimestamps();
    void rejectsInvalidArguments();
    void emptySingleAndAllNaN();
    void firstDataSetsTheView();
    void setXRangeIsClamped();
    void setYRangeValidation();
    void hiddenChannelsLeaveAutoscale();
    void nearestSample();
    void dropoutBreaksTheTrace();
    void dropoutIsAGapInPixels();
    void gapsMatchStripChart_data();
    void gapsMatchStripChart();
    void timeLabels();
    void valueLabelsGetRoom();
};

// #25 D1: ids are stable, never reused, unaffected by removing other channels
void tst_AnalysisChart::channelIdsAreStable()
{
    AnalysisChart chart;
    QSignalSpy spy(&chart, &AnalysisChart::channelsChanged);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const int b = chart.addChannel(QStringLiteral("B"), Qt::green);
    const int c = chart.addChannel(QStringLiteral("C"), Qt::blue);
    QVERIFY(chart.removeChannel(b));
    QVERIFY(!chart.removeChannel(b));
    QCOMPARE(chart.channelIds(), (QList<int>{a, c}));
    QCOMPARE(chart.channelName(c), QStringLiteral("C"));
    QCOMPARE(chart.channelColor(c), QColor(Qt::blue));
    const int d = chart.addChannel(QStringLiteral("D"), Qt::white);
    QVERIFY(d != a && d != b && d != c);
    chart.removeAllChannels();
    QCOMPARE(chart.channelCount(), 0);
    const int e = chart.addChannel(QStringLiteral("E"), Qt::white);
    QVERIFY(e != a && e != b && e != c && e != d);
    QVERIFY(spy.count() >= 7);
    // Unknown ids
    QCOMPARE(chart.channelName(999), QString());
    QVERIFY(!chart.channelColor(999).isValid());
    QVERIFY(!chart.isChannelVisible(999));
}

// #25 D2: clear() empties the data only, as StripChart::clear()
void tst_AnalysisChart::clearKeepsChannels()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const Series s = uniformSeries(0, 1000, 10);
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 10));
    QVERIFY(chart.hasData());
    chart.clear();
    QCOMPARE(chart.channelCount(), 1);
    QCOMPARE(chart.channelName(a), QStringLiteral("A"));
    QCOMPARE(chart.channelSampleCount(a), qsizetype(0));
    QVERIFY(!chart.hasData());
    // New data after clear() sets the view again
    const Series later = uniformSeries(5000000, 1000, 10);
    QVERIFY(chart.setChannelData(a, later.t.data(), later.v.data(), 10));
    QCOMPARE(chart.xStart(), nanoseconds(5000000));
}

// #25: the widget copies the data; the caller's buffers may change or be freed afterwards
void tst_AnalysisChart::dataIsCopied()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    auto *s = new Series(uniformSeries(0, 10, 5));
    QVERIFY(chart.setChannelData(a, s->t.data(), s->v.data(), 5));
    const double before = s->v[2];
    s->v[2] = 1234.0;
    s->t[2] = nanoseconds(999);
    delete s;
    const auto sample = chart.nearestSample(a, nanoseconds(20));
    QVERIFY(sample.has_value());
    QCOMPARE(sample->t, nanoseconds(20));
    QCOMPARE(sample->value, before);
}

// #25: decreasing timestamps are rejected; the old data stays and a warning is issued
void tst_AnalysisChart::rejectsDecreasingTimestamps()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const Series good = uniformSeries(0, 10, 4);
    QVERIFY(chart.setChannelData(a, good.t.data(), good.v.data(), 4));
    QSignalSpy spy(&chart, &AnalysisChart::channelsChanged);
    std::vector<nanoseconds> t{nanoseconds(0), nanoseconds(20), nanoseconds(10)};
    std::vector<double> v{1.0, 2.0, 3.0};
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("timestamps decrease at index 2")));
    QVERIFY(!chart.setChannelData(a, t.data(), v.data(), 3));
    QCOMPARE(chart.channelSampleCount(a), qsizetype(4));
    QCOMPARE(chart.nearestSample(a, nanoseconds(30))->value, good.v[3]);
    QCOMPARE(spy.count(), 0);
    // Equal timestamps are allowed (non-decreasing)
    std::vector<nanoseconds> equal{nanoseconds(0), nanoseconds(10), nanoseconds(10)};
    QVERIFY(chart.setChannelData(a, equal.data(), v.data(), 3));
}

void tst_AnalysisChart::rejectsInvalidArguments()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const Series s = uniformSeries(0, 10, 3);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unknown channel 42")));
    QVERIFY(!chart.setChannelData(42, s.t.data(), s.v.data(), 3));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("invalid arguments")));
    QVERIFY(!chart.setChannelData(a, s.t.data(), s.v.data(), -1));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("invalid arguments")));
    QVERIFY(!chart.setChannelData(a, nullptr, s.v.data(), 3));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("invalid arguments")));
    QVERIFY(!chart.setChannelData(a, s.t.data(), nullptr, 3));
    QVERIFY(!chart.hasData());
}

// #25: count 0 clears; a single sample and an all-NaN channel are valid
void tst_AnalysisChart::emptySingleAndAllNaN()
{
    AnalysisChart chart;
    chart.resize(400, 240);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const Series s = uniformSeries(0, 10, 3);
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 3));
    QVERIFY(chart.setChannelData(a, nullptr, nullptr, 0));
    QCOMPARE(chart.channelSampleCount(a), qsizetype(0));
    QVERIFY(!chart.hasData());

    // One sample: the view is the minimum span (1 ns) around it, and nothing breaks
    const nanoseconds t1(1000);
    const double v1 = 2.5;
    QVERIFY(chart.setChannelData(a, &t1, &v1, 1));
    QCOMPARE(chart.dataStart(), t1);
    QCOMPARE(chart.dataEnd(), t1);
    QVERIFY(chart.xEnd() > chart.xStart());
    QVERIFY(chart.yMinimum() < v1 && chart.yMaximum() > v1);
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();

    // All NaN: data, but no finite value: Y range kept, no polyline drawn
    const double yMin = chart.yMinimum();
    const double yMax = chart.yMaximum();
    std::vector<nanoseconds> t{nanoseconds(0), nanoseconds(10), nanoseconds(20)};
    std::vector<double> v{kNaN, kNaN, kNaN};
    QVERIFY(chart.setChannelData(a, t.data(), v.data(), 3));
    chart.resetView();
    QVERIFY(chart.hasData());
    QCOMPARE(chart.yMinimum(), yMin);
    QCOMPARE(chart.yMaximum(), yMax);
    chart.grab();
    QVERIFY(analysisSegments(chart, a).empty());
}

// #25: the first data sets the view: full extent, Y autoscaled with a 10 % margin
void tst_AnalysisChart::firstDataSetsTheView()
{
    AnalysisChart chart;
    QSignalSpy spy(&chart, &AnalysisChart::viewChanged);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    // 21 samples, 1000 .. 3000 ns every 100 ns: finite values 0 .. 10, the last one NaN
    std::vector<nanoseconds> t;
    std::vector<double> v;
    for (int i = 0; i <= 20; ++i) {
        t.push_back(nanoseconds(1000 + 100 * i));
        v.push_back(i == 20 ? kNaN : (i == 10 ? 10.0 : 0.0));
    }
    QVERIFY(chart.setChannelData(a, t.data(), v.data(), 21));
    QCOMPARE(chart.xStart(), nanoseconds(1000));
    QCOMPARE(chart.xEnd(), nanoseconds(3000));
    QCOMPARE(chart.yMinimum(), -1.0);
    QCOMPARE(chart.yMaximum(), 11.0);
    QVERIFY(spy.count() >= 1);
    // Later data keeps the view (clamped to the new limits)
    chart.setXRange(nanoseconds(1500), nanoseconds(2500));
    const int b = chart.addChannel(QStringLiteral("B"), Qt::green);
    std::vector<nanoseconds> t2{nanoseconds(0), nanoseconds(5000)};
    std::vector<double> v2{100.0, 200.0};
    QVERIFY(chart.setChannelData(b, t2.data(), v2.data(), 2));
    QCOMPARE(chart.xStart(), nanoseconds(1500));
    QCOMPARE(chart.xEnd(), nanoseconds(2500));
    QCOMPARE(chart.yMaximum(), 11.0);
    // resetView() covers both channels
    chart.resetView();
    QCOMPARE(chart.xStart(), nanoseconds(0));
    QCOMPARE(chart.xEnd(), nanoseconds(5000));
    QCOMPARE(chart.yMinimum(), -20.0);
    QCOMPARE(chart.yMaximum(), 220.0);
}

// #25: setXRange() is clamped to the limits and emits viewChanged() only on a change
void tst_AnalysisChart::setXRangeIsClamped()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const Series s = uniformSeries(0, 100, 101); // 0 .. 10000 ns, mean interval 100 ns
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 101));
    QSignalSpy spy(&chart, &AnalysisChart::viewChanged);
    // Below the minimum span (4 x 100 ns)
    chart.setXRange(nanoseconds(5000), nanoseconds(5010));
    QCOMPARE((chart.xEnd() - chart.xStart()).count(), 400);
    // Beyond the extent
    chart.setXRange(nanoseconds(-100000), nanoseconds(100000));
    QCOMPARE(chart.xStart(), nanoseconds(0));
    QCOMPARE(chart.xEnd(), nanoseconds(10000));
    // More than one view width outside the data
    chart.setXRange(nanoseconds(50000), nanoseconds(51000));
    QCOMPARE(chart.xEnd(), nanoseconds(11000));
    const int before = spy.count();
    chart.setXRange(chart.xStart(), chart.xEnd());
    QCOMPARE(spy.count(), before);
}

void tst_AnalysisChart::setYRangeValidation()
{
    AnalysisChart chart;
    chart.setYRange(-5.0, 5.0);
    QCOMPARE(chart.yMinimum(), -5.0);
    QCOMPARE(chart.yMaximum(), 5.0);
    chart.setYRange(3.0, 3.0);
    chart.setYRange(4.0, 1.0);
    chart.setYRange(kNaN, 1.0);
    chart.setYRange(0.0, std::numeric_limits<double>::infinity());
    QCOMPARE(chart.yMinimum(), -5.0);
    QCOMPARE(chart.yMaximum(), 5.0);
}

// Hidden channels are not drawn and are left out of resetView()'s autoscale
void tst_AnalysisChart::hiddenChannelsLeaveAutoscale()
{
    AnalysisChart chart;
    chart.resize(400, 240);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const int b = chart.addChannel(QStringLiteral("B"), Qt::green);
    std::vector<nanoseconds> t{nanoseconds(0), nanoseconds(100)};
    std::vector<double> va{0.0, 1.0};
    std::vector<double> vb{0.0, 100.0};
    QVERIFY(chart.setChannelData(a, t.data(), va.data(), 2));
    QVERIFY(chart.setChannelData(b, t.data(), vb.data(), 2));
    chart.setChannelVisible(b, false);
    chart.resetView();
    QVERIFY(chart.yMaximum() < 2.0);
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    QCOMPARE(analysisSegments(chart, a).size(), size_t(1));
    QVERIFY(analysisSegments(chart, b).empty());
}

void tst_AnalysisChart::nearestSample()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    std::vector<nanoseconds> t{nanoseconds(0), nanoseconds(10), nanoseconds(20)};
    std::vector<double> v{1.0, kNaN, 3.0};
    QVERIFY(chart.setChannelData(a, t.data(), v.data(), 3));
    QCOMPARE(chart.nearestSample(a, nanoseconds(4))->t, nanoseconds(0));
    QCOMPARE(chart.nearestSample(a, nanoseconds(5))->t, nanoseconds(0)); // tie: the earlier
    QVERIFY(std::isnan(chart.nearestSample(a, nanoseconds(9))->value));  // non-finite nearest sample
    QVERIFY(!chart.nearestSample(a, nanoseconds(-1)).has_value());
    QVERIFY(!chart.nearestSample(a, nanoseconds(21)).has_value());
    QVERIFY(!chart.nearestSample(99, nanoseconds(10)).has_value());
}

// #25: a non-finite sample breaks the trace (raw drawing, StripChart's rule)
void tst_AnalysisChart::dropoutBreaksTheTrace()
{
    AnalysisChart chart;
    chart.resize(400, 240);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    Series s = uniformSeries(0, 1000, 100);
    s.v[40] = kNaN;
    s.v[41] = std::numeric_limits<double>::infinity();
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 100));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    QCOMPARE(analysisSegments(chart, a), (std::vector<int>{40, 58}));
}

// #25: the dropout is visible as a gap: no trace pixel between the samples around it
void tst_AnalysisChart::dropoutIsAGapInPixels()
{
    AnalysisChart chart;
    chart.resize(500, 260);
    chart.setGridVisible(false);
    chart.setLegendVisible(false);
    const QColor trace(255, 0, 255);
    const int a = chart.addChannel(QStringLiteral("A"), trace, 2.0);
    Series s = uniformSeries(0, 1000, 200);
    for (int i = 0; i < 200; ++i) s.v[static_cast<size_t>(i)] = 0.0; // flat line in the middle of the plot
    s.v[0] = -1.0;
    s.v[199] = 1.0;
    for (int i = 90; i <= 110; ++i) s.v[static_cast<size_t>(i)] = kNaN;
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 200));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    const QImage img = chart.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const qreal dpr = img.devicePixelRatio();
    const QRectF plot = internal::AnalysisChartTestAccess::lastFrame(chart).plotRect;
    auto xOf = [&](double t) { return (plot.left() + (t - chart.xStart().count()) * plot.width() / (chart.xEnd() - chart.xStart()).count()) * dpr; };
    auto magentaInColumn = [&](int x) {
        for (int y = 0; y < img.height(); ++y) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() > 150 && c.blue() > 150 && c.green() < 100) return true;
        }
        return false;
    };
    // Inside the dropout (samples 91..109, away from the pen caps of 89 and 111)
    for (int x = static_cast<int>(std::ceil(xOf(91000.0))); x <= static_cast<int>(std::floor(xOf(109000.0))); ++x) {
        QVERIFY2(!magentaInColumn(x), qPrintable(QStringLiteral("trace pixel in the dropout at device column %1 (DPR %2)").arg(x).arg(dpr)));
    }
    // Around it the trace is drawn
    QVERIFY(magentaInColumn(static_cast<int>(xOf(60000.0))));
    QVERIFY(magentaInColumn(static_cast<int>(xOf(140000.0))));
}

void tst_AnalysisChart::gapsMatchStripChart_data()
{
    QTest::addColumn<std::vector<qint64>>("t");
    QTest::addColumn<std::vector<double>>("v");
    QTest::addColumn<qint64>("userThreshold");

    // 1 kHz with a 50 ms hole in the timestamps
    std::vector<qint64> t1;
    std::vector<double> v1;
    for (int i = 0; i < 300; ++i) {
        t1.push_back(static_cast<qint64>(i) * 1000000 + (i >= 150 ? 50000000 : 0));
        v1.push_back(std::sin(i / 10.0));
    }
    QTest::addRow("hole") << t1 << v1 << qint64(0);

    // Irregular steps (1 .. 9 ms), some long pauses, NaN runs, duplicate timestamps
    std::vector<qint64> t2;
    std::vector<double> v2;
    qint64 now = 0;
    for (int i = 0; i < 400; ++i) {
        now += static_cast<qint64>((i * 7919) % 9 + 1) * 1000000;
        if (i % 97 == 0) now += 60000000;
        t2.push_back(now);
        if (i % 50 == 25) t2.push_back(now); // duplicate timestamp
        v2.push_back(i % 33 < 3 ? kNaN : std::cos(i / 7.0));
        if (t2.size() > v2.size()) v2.push_back(1.0);
    }
    QTest::addRow("irregular") << t2 << v2 << qint64(0);

    // The same irregular data with a user threshold
    QTest::addRow("user threshold") << t2 << v2 << qint64(5000000);

    // Jittered ~1 ms steps (0.8 .. 1.2 ms), a 25 ms hole, then the rate drops to 10 ms for the last
    // 5 steps. The automatic threshold depends on the EMA weights: 0.9 / 0.1 ends near 4.7 ms
    // (threshold ~19 ms, the hole breaks the trace), 0.5 / 0.5 would end near 9.7 ms (threshold
    // ~39 ms, no break), so the row fails if the EMA differs from StripChart's
    std::vector<qint64> t3;
    std::vector<double> v3;
    qint64 at = 0;
    for (int i = 0; i < 300; ++i) {
        at += 800000 + static_cast<qint64>((i * 7919) % 5) * 100000;
        if (i == 150) at += 25000000;
        t3.push_back(at);
        v3.push_back(std::sin(i / 15.0));
    }
    for (int i = 0; i < 5; ++i) {
        at += 10000000;
        t3.push_back(at);
        v3.push_back(0.5);
    }
    QTest::addRow("jitter and rate change") << t3 << v3 << qint64(0);
}

// #25 D4: the same data gives the same trace breaks in StripChart and in AnalysisChart (raw drawing:
// StripChart in Time mode with a window covering all the data, decimation off)
void tst_AnalysisChart::gapsMatchStripChart()
{
    QFETCH(std::vector<qint64>, t);
    QFETCH(std::vector<double>, v);
    QFETCH(qint64, userThreshold);
    std::vector<nanoseconds> tn;
    for (qint64 x : t) tn.push_back(nanoseconds(x));
    const qint64 extent = t.back() - t.front();

    StripChart strip;
    strip.resize(640, 300);
    strip.setXAxisMode(StripChart::XAxisMode::Time);
    strip.setDecimationMode(StripChart::DecimationMode::Off);
    strip.setCapacity(static_cast<int>(t.size()) + 10);
    strip.setTimeSpan(nanoseconds(extent * 2 + 1000000));
    strip.setGapThreshold(nanoseconds(userThreshold));
    const int sId = strip.addChannel(QStringLiteral("S"), Qt::red);
    strip.addSamples(sId, tn.data(), v.data(), static_cast<qsizetype>(t.size()));
    StripChartTestAccess::enableDiagnostics(strip);
    strip.grab();

    AnalysisChart chart;
    chart.resize(640, 300);
    chart.setGapThreshold(nanoseconds(userThreshold));
    const int aId = chart.addChannel(QStringLiteral("A"), Qt::red);
    QVERIFY(chart.setChannelData(aId, tn.data(), v.data(), static_cast<qsizetype>(t.size())));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();

    const std::vector<int> s = stripSegments(strip, sId);
    const std::vector<int> a = analysisSegments(chart, aId);
    QVERIFY2(s.size() > 1, "the data must break the StripChart trace");
    QVERIFY2(s == a, qPrintable(QStringLiteral("StripChart segments [%1], AnalysisChart segments [%2]").arg(describe(s), describe(a))));
}

// #25 D6: Relative labels count from the data start; Absolute labels are timeOrigin + t
void tst_AnalysisChart::timeLabels()
{
    AnalysisChart chart;
    chart.resize(640, 300);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const qint64 start = 7000000000LL; // 7 s
    const Series s = uniformSeries(start, 10000000, 201); // 2 s of data
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 201));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    const auto &rel = internal::AnalysisChartTestAccess::lastFrame(chart).xTicks;
    QVERIFY(rel.size() >= 2);
    QCOMPARE(static_cast<qint64>(rel.front().position), start);
    QVERIFY2(rel.front().label.startsWith(QStringLiteral("0 ")), qPrintable(rel.front().label));

    const QDateTime origin(QDate(2026, 10, 17), QTime(12, 0, 0), Qt::UTC);
    chart.setTimeOrigin(origin);
    chart.setTimeLabelFormat(AnalysisChart::TimeLabelFormat::Absolute);
    chart.grab();
    const auto &abs = internal::AnalysisChartTestAccess::lastFrame(chart).xTicks;
    QVERIFY(!abs.empty());
    const qint64 t0 = static_cast<qint64>(abs.front().position);
    QCOMPARE(abs.front().label, origin.addMSecs(t0 / 1000000).toString(QStringLiteral("HH:mm:ss.zzz")));
    QVERIFY(abs.front().label.startsWith(QStringLiteral("12:00:0")));
}

// The plot leaves room for the widest value label
void tst_AnalysisChart::valueLabelsGetRoom()
{
    AnalysisChart chart;
    chart.resize(640, 300);
    chart.setYRange(0.0, 1.0);
    const double narrow = internal::AnalysisChartTestAccess::plotArea(chart).left();
    chart.setYRange(123456.25, 123456.75);
    const double wide = internal::AnalysisChartTestAccess::plotArea(chart).left();
    QVERIFY2(wide > narrow, qPrintable(QStringLiteral("left margin %1 -> %2").arg(narrow).arg(wide)));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    for (const auto &tick : internal::AnalysisChartTestAccess::lastFrame(chart).yTicks) {
        QVERIFY(!tick.label.isEmpty());
    }
}

QTEST_MAIN(tst_AnalysisChart)
#include "tst_AnalysisChart.moc"
