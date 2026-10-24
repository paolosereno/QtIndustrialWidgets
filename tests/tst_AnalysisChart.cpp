// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/AnalysisChart.h>
#include <QtIndustrialWidgets/StripChart.h>

#include "internal/StripChartTestAccess.h"
#include "internal/AnalysisChartTestAccess.h"
#include "internal/M4Decimator.h"

#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>

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
    const auto &frame = internal::StripChartTestAccess::lastFrame(chart);
    if (channelId < static_cast<int>(frame.channels.size())) {
        for (const auto &seg : frame.channels[static_cast<size_t>(channelId)].segments) out.push_back(static_cast<int>(seg.size()));
    }
    return out;
}

// Synthetic input, delivered synchronously
void mouse(QWidget &w, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons,
           Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent e(type, pos, w.mapToGlobal(pos), button, buttons, mods);
    QApplication::sendEvent(&w, &e);
}

void drag(QWidget &w, QPointF from, QPointF to, Qt::MouseButton button = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    mouse(w, QEvent::MouseButtonPress, from, button, button, mods);
    mouse(w, QEvent::MouseMove, (from + to) * 0.5, Qt::NoButton, button, mods);
    mouse(w, QEvent::MouseMove, to, Qt::NoButton, button, mods);
    mouse(w, QEvent::MouseButtonRelease, to, button, Qt::NoButton, mods);
}

void wheel(QWidget &w, QPointF pos, int delta, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QWheelEvent e(pos, w.mapToGlobal(pos), QPoint(), QPoint(0, delta), Qt::NoButton, mods, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&w, &e);
}

void key(QWidget &w, int k)
{
    QKeyEvent e(QEvent::KeyPress, k, Qt::NoModifier);
    QApplication::sendEvent(&w, &e);
}

// x of time t in the plot of the chart's current view
double xOf(const AnalysisChart &chart, qint64 t)
{
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    return plot.left() + static_cast<double>(t - chart.xStart().count()) * plot.width()
        / static_cast<double>((chart.xEnd() - chart.xStart()).count());
}

// ns of the "<number> <unit>" at the end of a readout text, -1 if it has none
qint64 durationNs(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) return -1;
    const QString unit = parts.back();
    bool ok = false;
    const double v = parts[parts.size() - 2].toDouble(&ok);
    if (!ok) return -1;
    double f = 0.0;
    if (unit == QStringLiteral("ns")) f = 1.0;
    else if (unit == QString(QChar(0x00B5)) + QLatin1Char('s')) f = 1e3;
    else if (unit == QStringLiteral("ms")) f = 1e6;
    else if (unit == QStringLiteral("s")) f = 1e9;
    else if (unit == QStringLiteral("min")) f = 60e9;
    else return -1;
    return static_cast<qint64>(std::llround(v * f));
}

// Dense test record: 2 s at 100 us, a sine with a spike every 997 samples, NaN dropouts at a few
// indices and a 30 ms hole (no samples) from 1.2 s
Series denseSeries()
{
    Series s;
    for (int i = 0; i < 20000; ++i) {
        if (i >= 12000 && i < 12300) continue;
        s.t.push_back(nanoseconds(qint64(5000000000LL) + qint64(i) * 100000));
        double y = std::sin(i / 300.0);
        if (i % 997 == 0) y = 3.0;
        if (i == 4000 || i == 4001 || i == 9000 || i == 15555) y = kNaN;
        s.v.push_back(y);
    }
    return s;
}

// M4Decimator's buckets of the chart's current view, as AnalysisChart lays them out: widthDev
// buckets of floor((t - xStart) * widthDev / span), vertex times shifted back to absolute time
std::vector<internal::M4Bucket> referenceBuckets(const AnalysisChart &chart, const Series &s, int widthDev)
{
    const qint64 origin = chart.xStart().count();
    const qint64 span = (chart.xEnd() - chart.xStart()).count();
    std::vector<qint64> rel;
    for (const auto &t : s.t) rel.push_back(t.count() - origin);
    return internal::M4Decimator::decimateToBuckets(rel.data(), s.v.data(), rel.size(), internal::BucketGrid(widthDev, span), 0, widthDev);
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
    void cursorsStartAtThirdsHidden();
    void cursorReadoutsAndDeltas();
    void bandShowsReadouts();
    void bandTextsIgnoreWidgetSize();
    void bandRowsAreLimited();
    void bandFitsAtMinimumSize_data();
    void bandFitsAtMinimumSize();
    void cursorLinesArePainted();
    void dragNearACursorMovesIt();
    void dragPans();
    void boxZoom();
    void wheelZoomKeepsTheAnchor();
    void doubleClickAndKeys();
    void decimationFollowsMode();
    void decimatedTraceMatchesM4Decimator_data();
    void decimatedTraceMatchesM4Decimator();
    void oneBucketPerDeviceColumn();
    void decimatedDropoutsAreBlankColumns();
    void pyramidFollowsTheData();
    void tracesStayInsideThePlotColumns_data();
    void tracesStayInsideThePlotColumns();
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
    internal::StripChartTestAccess::enableDiagnostics(strip);
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

// #25 addition (1): cursors are hidden until shown and the first data places them at 1/3 and 2/3 of the view
void tst_AnalysisChart::cursorsStartAtThirdsHidden()
{
    AnalysisChart chart;
    QVERIFY(!chart.isCursorVisible(AnalysisChart::Cursor::A));
    QVERIFY(!chart.isCursorVisible(AnalysisChart::Cursor::B));
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const Series s = uniformSeries(3000, 100, 31); // 3000 .. 6000 ns
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 31));
    QCOMPARE(chart.cursorPosition(AnalysisChart::Cursor::A), nanoseconds(4000));
    QCOMPARE(chart.cursorPosition(AnalysisChart::Cursor::B), nanoseconds(5000));
    QVERIFY(!chart.isCursorVisible(AnalysisChart::Cursor::A));
    QVERIFY(!chart.cursorReadout(a, AnalysisChart::Cursor::A).has_value()); // hidden: no readout
    // Later data does not move them
    const Series later = uniformSeries(0, 1000, 21);
    QVERIFY(chart.setChannelData(a, later.t.data(), later.v.data(), 21));
    QCOMPARE(chart.cursorPosition(AnalysisChart::Cursor::A), nanoseconds(4000));
    QCOMPARE(chart.cursorPosition(AnalysisChart::Cursor::B), nanoseconds(5000));

    // Showing, moving and hiding emit cursorsChanged() only on a change
    QSignalSpy spy(&chart, &AnalysisChart::cursorsChanged);
    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    chart.setCursorPosition(AnalysisChart::Cursor::A, nanoseconds(4500));
    chart.setCursorPosition(AnalysisChart::Cursor::A, nanoseconds(4500));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(chart.cursorPosition(AnalysisChart::Cursor::A), nanoseconds(4500));
}

// #25: readouts are the nearest sample (no interpolation); outside the data or hidden: nullopt;
// deltas are t_B - t_A and y_B - y_A
void tst_AnalysisChart::cursorReadoutsAndDeltas()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
    const std::vector<nanoseconds> t{nanoseconds(0), nanoseconds(100), nanoseconds(200), nanoseconds(300)};
    const std::vector<double> v{1.0, 2.0, kNaN, 8.0};
    QVERIFY(chart.setChannelData(a, t.data(), v.data(), 4));
    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    chart.setCursorVisible(AnalysisChart::Cursor::B, true);
    chart.setCursorPosition(AnalysisChart::Cursor::A, nanoseconds(130));
    chart.setCursorPosition(AnalysisChart::Cursor::B, nanoseconds(280));
    const auto ra = chart.cursorReadout(a, AnalysisChart::Cursor::A);
    const auto rb = chart.cursorReadout(a, AnalysisChart::Cursor::B);
    QVERIFY(ra && rb);
    QCOMPARE(ra->t, nanoseconds(100));
    QCOMPARE(ra->value, 2.0);
    QCOMPARE(rb->t, nanoseconds(300));
    QCOMPARE(rb->value, 8.0);
    QCOMPARE(chart.cursorDeltaTime(), nanoseconds(150));
    QCOMPARE(chart.cursorDeltaValue(a), std::optional<double>(6.0));
    // Nearest sample non-finite: a readout, but no delta
    chart.setCursorPosition(AnalysisChart::Cursor::B, nanoseconds(210));
    QVERIFY(chart.cursorReadout(a, AnalysisChart::Cursor::B).has_value());
    QVERIFY(std::isnan(chart.cursorReadout(a, AnalysisChart::Cursor::B)->value));
    QVERIFY(!chart.cursorDeltaValue(a).has_value());
    // Outside the data
    chart.setCursorPosition(AnalysisChart::Cursor::B, nanoseconds(301));
    QVERIFY(!chart.cursorReadout(a, AnalysisChart::Cursor::B).has_value());
    QVERIFY(!chart.cursorDeltaValue(a).has_value());
    QCOMPARE(chart.cursorDeltaTime(), nanoseconds(171)); // positions, not samples
    // Hidden
    chart.setCursorPosition(AnalysisChart::Cursor::B, nanoseconds(280));
    chart.setCursorVisible(AnalysisChart::Cursor::A, false);
    QVERIFY(!chart.cursorReadout(a, AnalysisChart::Cursor::A).has_value());
    QVERIFY(!chart.cursorDeltaValue(a).has_value());
}

// #25: the band lists the cursor times, Δt and per channel y_A, y_B, Δy ("---" when missing)
void tst_AnalysisChart::bandShowsReadouts()
{
    AnalysisChart chart;
    chart.resize(800, 400);
    const int a = chart.addChannel(QStringLiteral("Pressure"), Qt::red);
    const int b = chart.addChannel(QStringLiteral("Flow"), Qt::green);
    const qint64 ms = 1000000;
    std::vector<nanoseconds> t;
    std::vector<double> va, vb;
    for (int i = 0; i <= 100; ++i) {
        t.push_back(nanoseconds(5000 * ms + i * 10 * ms)); // 5 .. 6 s every 10 ms
        va.push_back(i * 0.25);                // 0 .. 25
        vb.push_back(i < 50 ? kNaN : 3.0);
    }
    QVERIFY(chart.setChannelData(a, t.data(), va.data(), 101));
    QVERIFY(chart.setChannelData(b, t.data(), vb.data(), 101));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    QVERIFY(internal::AnalysisChartTestAccess::lastFrame(chart).band.empty()); // no visible cursor, no band

    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    chart.setCursorVisible(AnalysisChart::Cursor::B, true);
    chart.setCursorPosition(AnalysisChart::Cursor::A, nanoseconds(5200 * ms));
    chart.setCursorPosition(AnalysisChart::Cursor::B, nanoseconds(5700 * ms));
    chart.grab();
    const auto &frame = internal::AnalysisChartTestAccess::lastFrame(chart);
    QCOMPARE(frame.band.size(), std::size_t(3));
    const QStringList header = frame.band[0].cells;
    // Times in the unit of the time axis: 200 ms, 700 ms after the data start, 500 ms apart
    QVERIFY2(header[1].startsWith(QStringLiteral("A ")) && durationNs(header[1]) == 200 * ms, qPrintable(header[1]));
    QVERIFY2(header[2].startsWith(QStringLiteral("B ")) && durationNs(header[2]) == 700 * ms, qPrintable(header[2]));
    QVERIFY2(durationNs(header[3]) == 500 * ms, qPrintable(header[3]));
    QCOMPARE(frame.band[1].cells[0], QStringLiteral("Pressure"));
    QVERIFY2(frame.band[1].cells[1].startsWith(QStringLiteral("5.0")), qPrintable(frame.band[1].cells[1]));
    QVERIFY2(frame.band[1].cells[2].startsWith(QStringLiteral("17.5")), qPrintable(frame.band[1].cells[2]));
    QVERIFY2(frame.band[1].cells[3].endsWith(QStringLiteral(" 12.5")) || frame.band[1].cells[3].contains(QStringLiteral(" 12.50")),
             qPrintable(frame.band[1].cells[3]));
    QCOMPARE(frame.band[2].cells[0], QStringLiteral("Flow"));
    QCOMPARE(frame.band[2].cells[1], QStringLiteral("---"));
    QVERIFY2(frame.band[2].cells[2].startsWith(QStringLiteral("3.0")), qPrintable(frame.band[2].cells[2]));
    QVERIFY2(frame.band[2].cells[3].endsWith(QStringLiteral("---")), qPrintable(frame.band[2].cells[3]));
    // The band lies below the plot
    QVERIFY(frame.bandRect.top() >= internal::AnalysisChartTestAccess::plotArea(chart).bottom());
    // The texts do not depend on the widget size (so the band fits at minimumSizeHint())
    std::vector<QStringList> wide;
    for (const auto &row : frame.band) wide.push_back(row.cells);
    for (const QSize size : {QSize(400, 300), QSize(1200, 500)}) {
        chart.resize(size);
        chart.grab();
        const auto &band = internal::AnalysisChartTestAccess::lastFrame(chart).band;
        QCOMPARE(band.size(), wide.size());
        for (std::size_t r = 0; r < band.size(); ++r) QCOMPARE(band[r].cells, wide[r]);
    }
    chart.resize(800, 400);

    // Only cursor B: no A column, no deltas
    chart.setCursorVisible(AnalysisChart::Cursor::A, false);
    chart.grab();
    const auto &onlyB = internal::AnalysisChartTestAccess::lastFrame(chart);
    QCOMPARE(onlyB.band.size(), std::size_t(3));
    QVERIFY(onlyB.band[1].cells[1].isEmpty());
    QVERIFY(onlyB.band[1].cells[3].isEmpty());
    QVERIFY(!onlyB.band[1].cells[2].isEmpty());

    // readoutVisible off: no band, and the plot takes its room
    const double withBand = internal::AnalysisChartTestAccess::plotArea(chart).bottom();
    chart.setReadoutVisible(false);
    chart.grab();
    QVERIFY(internal::AnalysisChartTestAccess::lastFrame(chart).band.empty());
    QVERIFY(internal::AnalysisChartTestAccess::plotArea(chart).bottom() > withBand);
}

// #25 addition (3): the band texts do not depend on the widget size, so the band computed for
// minimumSizeHint() is the band drawn at that size (spans where the axis step at 400 px and at
// 1200 px falls in different units)
void tst_AnalysisChart::bandTextsIgnoreWidgetSize()
{
    for (const qint64 span : {qint64(4000000000LL), qint64(30000000LL), qint64(10000000000LL)}) {
        AnalysisChart chart;
        const int a = chart.addChannel(QStringLiteral("A"), Qt::red);
        const Series s = uniformSeries(1000000000LL, span / 400, 401);
        QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 401));
        chart.setCursorVisible(AnalysisChart::Cursor::A, true);
        chart.setCursorVisible(AnalysisChart::Cursor::B, true);
        internal::AnalysisChartTestAccess::enableDiagnostics(chart);
        std::vector<QStringList> first;
        for (const QSize size : {QSize(400, 300), QSize(800, 400), QSize(1200, 500)}) {
            chart.resize(size);
            chart.grab();
            const auto &band = internal::AnalysisChartTestAccess::lastFrame(chart).band;
            QCOMPARE(band.size(), std::size_t(2));
            if (first.empty()) {
                for (const auto &row : band) first.push_back(row.cells);
                continue;
            }
            for (std::size_t r = 0; r < band.size(); ++r) {
                QVERIFY2(band[r].cells == first[r], qPrintable(QStringLiteral("span %1 at %2 px: '%3' vs '%4'")
                                                                    .arg(span).arg(size.width())
                                                                    .arg(band[r].cells.join(QLatin1Char('|')), first[r].join(QLatin1Char('|')))));
            }
        }
    }
}

// #25 addition (3): at most 6 channel rows, further visible channels summarised in one last row
void tst_AnalysisChart::bandRowsAreLimited()
{
    AnalysisChart chart;
    chart.resize(800, 600);
    const Series s = uniformSeries(0, 1000, 50);
    QList<int> ids;
    for (int i = 0; i < 9; ++i) {
        ids << chart.addChannel(QStringLiteral("Ch%1").arg(i), QColor::fromHsv(i * 40, 255, 255));
        QVERIFY(chart.setChannelData(ids.back(), s.t.data(), s.v.data(), 50));
    }
    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    {
        const auto &band = internal::AnalysisChartTestAccess::lastFrame(chart).band;
        QCOMPARE(band.size(), std::size_t(1 + 6 + 1));
        for (int r = 1; r <= 6; ++r) {
            QCOMPARE(band[static_cast<std::size_t>(r)].cells[0], QStringLiteral("Ch%1").arg(r - 1));
            QCOMPARE(band[static_cast<std::size_t>(r)].swatch, chart.channelColor(ids[r - 1]));
        }
        QCOMPARE(band.back().cells[0], QStringLiteral("+3 more"));
        QVERIFY(!band.back().swatch.isValid());
    }
    // Readouts of the channels not listed stay available
    QVERIFY(chart.cursorReadout(ids[8], AnalysisChart::Cursor::A).has_value());
    // Hidden channels are not listed nor counted
    chart.setChannelVisible(ids[0], false);
    chart.setChannelVisible(ids[1], false);
    chart.grab();
    {
        const auto &band = internal::AnalysisChartTestAccess::lastFrame(chart).band;
        QCOMPARE(band.size(), std::size_t(1 + 6 + 1));
        QCOMPARE(band[1].cells[0], QStringLiteral("Ch2"));
        QCOMPARE(band[1].swatch, chart.channelColor(ids[2])); // the swatch follows the listed channel
        QCOMPARE(band[6].swatch, chart.channelColor(ids[7]));
        QCOMPARE(band.back().cells[0], QStringLiteral("+1 more"));
    }
    chart.setChannelVisible(ids[2], false);
    chart.grab();
    {
        const auto &band = internal::AnalysisChartTestAccess::lastFrame(chart).band;
        QCOMPARE(band.size(), std::size_t(1 + 6));
        QCOMPARE(band.back().cells[0], QStringLiteral("Ch8"));
    }
}

void tst_AnalysisChart::bandFitsAtMinimumSize_data()
{
    QTest::addColumn<int>("channels");
    QTest::addColumn<QString>("name");
    QTest::addColumn<double>("pointSizeFactor");
    QTest::addRow("1 channel") << 1 << QStringLiteral("T") << 1.0;
    QTest::addRow("6 channels") << 6 << QStringLiteral("Temperature") << 1.0;
    QTest::addRow("9 channels") << 9 << QStringLiteral("Temperature") << 1.0;
    QTest::addRow("long names") << 3 << QStringLiteral("Main steam header pressure, line 2") << 1.0;
    QTest::addRow("large font") << 9 << QStringLiteral("Temperature") << 2.0;
}

// #25 addition (3), #44 rule: at minimumSizeHint() the readout band is whole: every row inside the
// widget, every cell wide enough for its text, and the plot keeps some height
void tst_AnalysisChart::bandFitsAtMinimumSize()
{
    QFETCH(int, channels);
    QFETCH(QString, name);
    QFETCH(double, pointSizeFactor);
    AnalysisChart chart;
    QFont f = chart.font();
    f.setPointSizeF(f.pointSizeF() * pointSizeFactor);
    chart.setFont(f);
    const Series s = uniformSeries(0, 1000, 50);
    for (int i = 0; i < channels; ++i) {
        const int id = chart.addChannel(name + QString::number(i), Qt::red);
        QVERIFY(chart.setChannelData(id, s.t.data(), s.v.data(), 50));
    }
    const QSize withoutBand = chart.minimumSizeHint();
    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    chart.setCursorVisible(AnalysisChart::Cursor::B, true);
    const QSize hint = chart.minimumSizeHint();
    // The band adds to the minimum height (above the 160 px floor)
    QVERIFY2(hint.height() >= withoutBand.height() && (channels < 6 || hint.height() > withoutBand.height()),
             qPrintable(QStringLiteral("%1 -> %2").arg(withoutBand.height()).arg(hint.height())));
    chart.resize(hint);
    chart.show();
    QVERIFY(QTest::qWaitForWindowExposed(&chart));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    const auto &frame = internal::AnalysisChartTestAccess::lastFrame(chart);
    QCOMPARE(frame.band.size(), std::size_t(1 + std::min(channels, 6) + (channels > 6 ? 1 : 0)));
    const QRectF widget(0, 0, chart.width(), chart.height());
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    QVERIFY2(plot.height() >= 20.0, qPrintable(QStringLiteral("plot height %1").arg(plot.height())));
    QVERIFY(frame.bandRect.top() >= plot.bottom());
    const QFontMetricsF fm(frame.bandFont);
    for (const auto &row : frame.band) {
        QCOMPARE(row.cellRects.size(), static_cast<std::size_t>(row.cells.size()));
        for (int c = 0; c < row.cells.size(); ++c) {
            const QRectF r = row.cellRects[static_cast<std::size_t>(c)];
            const QString what = QStringLiteral("'%1' in %2,%3 %4x%5, widget %6x%7")
                                     .arg(row.cells[c]).arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())
                                     .arg(chart.width()).arg(chart.height());
            QVERIFY2(widget.contains(r), qPrintable(what));
            QVERIFY2(r.width() + 0.01 >= fm.horizontalAdvance(row.cells[c]), qPrintable(what));
        }
    }
}

// #25: each visible cursor is a line of the cursor colour across the plot at its time
void tst_AnalysisChart::cursorLinesArePainted()
{
    AnalysisChart chart;
    chart.resize(600, 320);
    chart.setGridVisible(false);
    chart.setCursorColor(QColor(255, 0, 255));
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = uniformSeries(0, 1000, 101); // 0 .. 100 us
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 101));
    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    chart.setCursorPosition(AnalysisChart::Cursor::A, nanoseconds(25000));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    const QImage img = chart.grab().toImage();
    const auto &frame = internal::AnalysisChartTestAccess::lastFrame(chart);
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    QVERIFY(std::abs(frame.cursorX[0] - xOf(chart, 25000)) < 0.01);
    QCOMPARE(frame.cursorX[1], -1.0); // B hidden: not drawn
    const double dpr = img.devicePixelRatio();
    // Share of the plot rows where the line deposits its whole ink around x: the magenta coverage over
    // the background, summed over +-2 device pixels (an antialiased 1 px line at a fractional DPR
    // spreads over two pixels), is at least 0.9
    const int bgX = static_cast<int>(plot.left() * dpr) + 3;
    auto magentaRows = [&](double x) {
        int hits = 0;
        int rows = 0;
        for (int y = static_cast<int>(std::ceil(plot.top() * dpr)) + 1; y < static_cast<int>(plot.bottom() * dpr) - 1; ++y, ++rows) {
            const QColor bg = img.pixelColor(bgX, y);
            double ink = 0.0;
            for (int dx = -2; dx <= 2; ++dx) {
                const QColor c = img.pixelColor(static_cast<int>(x * dpr) + dx, y);
                const bool magenta = c.green() <= bg.green() + 16 && std::abs(c.red() - c.blue()) <= 24;
                if (magenta) ink += std::max(0.0, static_cast<double>(c.red() - bg.red()) / (255.0 - bg.red()));
            }
            hits += ink >= 0.9 ? 1 : 0;
        }
        return rows > 0 ? static_cast<double>(hits) / rows : 0.0;
    };
    QVERIFY2(magentaRows(frame.cursorX[0]) > 0.9, qPrintable(QString::number(magentaRows(frame.cursorX[0]))));
    QVERIFY2(magentaRows(xOf(chart, 75000)) < 0.1, qPrintable(QString::number(magentaRows(xOf(chart, 75000)))));
}

// #25 D3: a left drag that starts within 4 logical px of a visible cursor moves it; farther away it pans
void tst_AnalysisChart::dragNearACursorMovesIt()
{
    AnalysisChart chart;
    chart.resize(600, 320);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = uniformSeries(0, 1000, 1001); // 0 .. 1 ms
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 1001));
    chart.setCursorVisible(AnalysisChart::Cursor::A, true);
    chart.setCursorPosition(AnalysisChart::Cursor::A, nanoseconds(500000));
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    const double y = plot.center().y();
    const double x = xOf(chart, 500000);
    const qint64 start = chart.xStart().count();
    const qint64 end = chart.xEnd().count();

    // 3.5 px away: grabs the cursor, the view stays
    drag(chart, QPointF(x + 3.5, y), QPointF(xOf(chart, 200000), y));
    QCOMPARE(chart.xStart().count(), start);
    QCOMPARE(chart.xEnd().count(), end);
    QVERIFY2(std::abs(chart.cursorPosition(AnalysisChart::Cursor::A).count() - 200000) <= 1,
             qPrintable(QString::number(chart.cursorPosition(AnalysisChart::Cursor::A).count())));

    // 6 px away: pans, the cursor keeps its time
    chart.setCursorPosition(AnalysisChart::Cursor::A, nanoseconds(500000));
    chart.setXRange(nanoseconds(250000), nanoseconds(750000));
    const double x2 = xOf(chart, 500000);
    drag(chart, QPointF(x2 + 6.0, y), QPointF(x2 + 106.0, y));
    QVERIFY(chart.xStart().count() < 250000);
    QCOMPARE(chart.cursorPosition(AnalysisChart::Cursor::A), nanoseconds(500000));

    // A hidden cursor is not grabbed
    chart.setXRange(nanoseconds(250000), nanoseconds(750000));
    chart.setCursorVisible(AnalysisChart::Cursor::A, false);
    drag(chart, QPointF(x2, y), QPointF(x2 + 100.0, y));
    QVERIFY(chart.xStart().count() < 250000);
    QCOMPARE(chart.cursorPosition(AnalysisChart::Cursor::A), nanoseconds(500000));
}

// #25 D3: a left drag pans X and Y by the distance dragged
void tst_AnalysisChart::dragPans()
{
    AnalysisChart chart;
    chart.resize(600, 320);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = uniformSeries(0, 1000, 1001);
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 1001));
    chart.setXRange(nanoseconds(400000), nanoseconds(600000));
    chart.setYRange(0.0, 1.0);
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    const QPointF from = plot.center();
    drag(chart, from, from + QPointF(plot.width() / 4.0, plot.height() / 2.0));
    // Right by 1/4 of the width: 1/4 of the span earlier; down by half the height: half the range higher
    QVERIFY2(std::abs(chart.xStart().count() - 350000) <= 1, qPrintable(QString::number(chart.xStart().count())));
    QVERIFY2(std::abs(chart.xEnd().count() - 550000) <= 1, qPrintable(QString::number(chart.xEnd().count())));
    QVERIFY(std::abs(chart.yMinimum() - 0.5) < 1e-9);
    QVERIFY(std::abs(chart.yMaximum() - 1.5) < 1e-9);
    // A press outside the plot does nothing
    const qint64 before = chart.xStart().count();
    drag(chart, QPointF(2, 2), QPointF(200, 2));
    QCOMPARE(chart.xStart().count(), before);
}

// #25 D3: Shift + left drag or right drag draws a box and zooms to it; a box under 4 px is ignored
void tst_AnalysisChart::boxZoom()
{
    AnalysisChart chart;
    chart.resize(600, 320);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = uniformSeries(0, 1000, 1001);
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 1001));
    chart.setYRange(0.0, 10.0);
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    auto at = [&](double fx, double fy) { return QPointF(plot.left() + fx * plot.width(), plot.top() + fy * plot.height()); };

    // While dragging, the box is drawn
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    mouse(chart, QEvent::MouseButtonPress, at(0.25, 0.2), Qt::RightButton, Qt::RightButton);
    mouse(chart, QEvent::MouseMove, at(0.5, 0.6), Qt::NoButton, Qt::RightButton);
    chart.grab();
    const QRectF box = internal::AnalysisChartTestAccess::lastFrame(chart).boxRect;
    QVERIFY2(std::abs(box.left() - at(0.25, 0).x()) < 0.01 && std::abs(box.bottom() - at(0, 0.6).y()) < 0.01,
             qPrintable(QStringLiteral("%1,%2 %3x%4").arg(box.x()).arg(box.y()).arg(box.width()).arg(box.height())));
    mouse(chart, QEvent::MouseButtonRelease, at(0.5, 0.6), Qt::RightButton, Qt::NoButton);
    QVERIFY2(std::abs(chart.xStart().count() - 250000) <= 1, qPrintable(QString::number(chart.xStart().count())));
    QVERIFY2(std::abs(chart.xEnd().count() - 500000) <= 1, qPrintable(QString::number(chart.xEnd().count())));
    QVERIFY(std::abs(chart.yMinimum() - 4.0) < 1e-9);
    QVERIFY(std::abs(chart.yMaximum() - 8.0) < 1e-9);
    chart.grab();
    QVERIFY(internal::AnalysisChartTestAccess::lastFrame(chart).boxRect.isNull()); // gone after the release

    // Shift + left drag, the other way round
    chart.resetView();
    chart.setYRange(0.0, 10.0);
    drag(chart, at(0.75, 0.5), at(0.5, 0.0), Qt::LeftButton, Qt::ShiftModifier);
    QVERIFY2(std::abs(chart.xStart().count() - 500000) <= 1, qPrintable(QString::number(chart.xStart().count())));
    QVERIFY(std::abs(chart.xEnd().count() - 750000) <= 1);
    QVERIFY(std::abs(chart.yMinimum() - 5.0) < 1e-9);
    QVERIFY(std::abs(chart.yMaximum() - 10.0) < 1e-9);

    // Under 4 px in one direction: a click, not a zoom
    chart.resetView();
    chart.setYRange(0.0, 10.0);
    drag(chart, at(0.25, 0.25), at(0.25, 0.25) + QPointF(100.0, 3.5), Qt::RightButton);
    QCOMPARE(chart.xStart().count(), qint64(0));
    QCOMPARE(chart.xEnd().count(), qint64(1000000));
    QCOMPARE(chart.yMaximum(), 10.0);
}

// #25 D3: the wheel zooms X by 1.25 per notch around the mouse; with Ctrl it zooms Y
void tst_AnalysisChart::wheelZoomKeepsTheAnchor()
{
    AnalysisChart chart;
    chart.resize(600, 320);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = uniformSeries(0, 1000, 1001);
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 1001));
    chart.setYRange(0.0, 10.0);
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    const QPointF p(plot.left() + 0.25 * plot.width(), plot.top() + 0.5 * plot.height()); // t = 250 us, v = 5
    wheel(chart, p, 120);
    QVERIFY2(std::abs((chart.xEnd() - chart.xStart()).count() - 800000) <= 1, qPrintable(QString::number((chart.xEnd() - chart.xStart()).count())));
    QVERIFY(std::abs(chart.xStart().count() - 50000) <= 1); // 250000 - 0.25 * 800000
    QCOMPARE(chart.yMaximum(), 10.0);
    wheel(chart, p, -120);
    QVERIFY(std::abs(chart.xStart().count()) <= 1);
    QVERIFY(std::abs(chart.xEnd().count() - 1000000) <= 1);
    // Ctrl: Y around the value under the mouse
    wheel(chart, p, 120, Qt::ControlModifier);
    QVERIFY(std::abs(chart.yMinimum() - 1.0) < 1e-9);
    QVERIFY(std::abs(chart.yMaximum() - 9.0) < 1e-9);
    QVERIFY(std::abs(chart.xStart().count()) <= 1);
}

// #25 D3: double-click and Home reset the view; + and - zoom X around the centre; arrows pan by 10 %
void tst_AnalysisChart::doubleClickAndKeys()
{
    AnalysisChart chart;
    chart.resize(600, 320);
    QCOMPARE(chart.focusPolicy(), Qt::StrongFocus);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = uniformSeries(0, 1000, 1001);
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 1001));
    const double yMin = chart.yMinimum();
    const double yMax = chart.yMaximum();
    chart.setXRange(nanoseconds(100000), nanoseconds(300000));
    chart.setYRange(0.0, 1.0);
    const QPointF c = internal::AnalysisChartTestAccess::plotArea(chart).center();
    mouse(chart, QEvent::MouseButtonPress, c, Qt::LeftButton, Qt::LeftButton);
    mouse(chart, QEvent::MouseButtonRelease, c, Qt::LeftButton, Qt::NoButton);
    mouse(chart, QEvent::MouseButtonDblClick, c, Qt::LeftButton, Qt::LeftButton);
    mouse(chart, QEvent::MouseButtonRelease, c, Qt::LeftButton, Qt::NoButton);
    QCOMPARE(chart.xStart().count(), qint64(0));
    QCOMPARE(chart.xEnd().count(), qint64(1000000));
    QCOMPARE(chart.yMinimum(), yMin);
    QCOMPARE(chart.yMaximum(), yMax);

    chart.setXRange(nanoseconds(400000), nanoseconds(600000));
    chart.setYRange(0.0, 1.0);
    key(chart, Qt::Key_Right);
    QCOMPARE(chart.xStart().count(), qint64(420000));
    key(chart, Qt::Key_Left);
    key(chart, Qt::Key_Left);
    QCOMPARE(chart.xStart().count(), qint64(380000));
    key(chart, Qt::Key_Up);
    QVERIFY(std::abs(chart.yMinimum() - 0.1) < 1e-9);
    key(chart, Qt::Key_Down);
    key(chart, Qt::Key_Down);
    QVERIFY(std::abs(chart.yMinimum() + 0.1) < 1e-9);
    key(chart, Qt::Key_Plus); // centre 480000, span 200000 / 1.25
    QCOMPARE(chart.xEnd().count() - chart.xStart().count(), qint64(160000));
    QCOMPARE(chart.xStart().count(), qint64(400000));
    key(chart, Qt::Key_Minus);
    QCOMPARE(chart.xEnd().count() - chart.xStart().count(), qint64(200000));
    QCOMPARE(chart.xStart().count(), qint64(380000));
    key(chart, Qt::Key_Home);
    QCOMPARE(chart.xStart().count(), qint64(0));
    QCOMPARE(chart.yMaximum(), yMax);
}

// #25: DecimationMode as StripChart: Auto decimates when the samples in the view exceed the device
// columns, Off never, Always always
void tst_AnalysisChart::decimationFollowsMode()
{
    AnalysisChart chart;
    chart.resize(640, 300);
    QCOMPARE(chart.decimationMode(), AnalysisChart::DecimationMode::Auto);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = uniformSeries(0, 1000, 5000); // 0 .. 5 ms
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), 5000));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    auto decimated = [&]() {
        chart.grab();
        const auto &f = internal::AnalysisChartTestAccess::lastFrame(chart);
        return f.channels.size() == 1 && f.channels[0].decimated;
    };
    const int widthDev = static_cast<int>(std::lround(internal::AnalysisChartTestAccess::plotArea(chart).width() * chart.devicePixelRatioF()));
    QVERIFY(5000 > widthDev);
    QVERIFY(decimated());
    // In the view exactly widthDev samples: raw; one more: decimated
    chart.setXRange(nanoseconds(1000000), nanoseconds(1000000 + qint64(widthDev - 1) * 1000));
    QVERIFY(!decimated());
    chart.setXRange(nanoseconds(1000000), nanoseconds(1000000 + qint64(widthDev) * 1000));
    QVERIFY(decimated());
    QSignalSpy spy(&chart, &AnalysisChart::appearanceChanged);
    chart.setDecimationMode(AnalysisChart::DecimationMode::Off);
    chart.setDecimationMode(AnalysisChart::DecimationMode::Off);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!decimated());
    chart.setDecimationMode(AnalysisChart::DecimationMode::Always);
    chart.setXRange(nanoseconds(1000000), nanoseconds(1100000)); // 101 samples
    QVERIFY(decimated());
    QCOMPARE(chart.property("decimationMode").value<AnalysisChart::DecimationMode>(), AnalysisChart::DecimationMode::Always);
}

void tst_AnalysisChart::decimatedTraceMatchesM4Decimator_data()
{
    QTest::addColumn<qint64>("start");
    QTest::addColumn<qint64>("end");
    QTest::addColumn<qint64>("gapThreshold");
    QTest::addRow("full view") << qint64(0) << qint64(0) << qint64(0);
    QTest::addRow("hole and dropouts") << qint64(5300000000LL) << qint64(6400000000LL) << qint64(0);
    QTest::addRow("odd window") << qint64(5123456789LL) << qint64(5987654321LL) << qint64(0);
    QTest::addRow("user threshold") << qint64(5000000000LL) << qint64(7000000000LL) << qint64(250000);
}

// #25: the decimated trace is M4Decimator's: the same vertices (first, min, max, last of each bucket)
// and the same breaks at dropouts and gaps, with buckets on the device columns of the view
void tst_AnalysisChart::decimatedTraceMatchesM4Decimator()
{
    QFETCH(qint64, start);
    QFETCH(qint64, end);
    QFETCH(qint64, gapThreshold);
    AnalysisChart chart;
    chart.resize(700, 320);
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    // Replaced data: the pyramid must follow setChannelData()
    const Series first = uniformSeries(5000000000LL, 1000, 30000);
    QVERIFY(chart.setChannelData(a, first.t.data(), first.v.data(), 30000));
    const Series s = denseSeries();
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), static_cast<qsizetype>(s.t.size())));
    chart.setGapThreshold(nanoseconds(gapThreshold));
    if (end > start) chart.setXRange(nanoseconds(start), nanoseconds(end));
    else chart.resetView();
    chart.setDecimationMode(AnalysisChart::DecimationMode::Always);
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    const auto &frame = internal::AnalysisChartTestAccess::lastFrame(chart);
    QCOMPARE(frame.channels.size(), std::size_t(1));
    QVERIFY(frame.channels[0].decimated);

    const qint64 origin = chart.xStart().count();
    const qint64 span = (chart.xEnd() - chart.xStart()).count();
    std::vector<qint64> rel;
    for (const auto &t : s.t) rel.push_back(t.count() - origin);
    // The documented automatic threshold: 4 x the EMA of the positive steps (0.9 / 0.1), truncated
    double ema = 0.0;
    bool hasEma = false;
    for (std::size_t i = 1; i < s.t.size(); ++i) {
        const double dt = static_cast<double>((s.t[i] - s.t[i - 1]).count());
        if (dt <= 0.0) continue;
        ema = hasEma ? 0.9 * ema + 0.1 * dt : dt;
        hasEma = true;
    }
    const qint64 gap = gapThreshold > 0 ? gapThreshold : static_cast<qint64>(4.0 * ema);
    const auto expected = internal::M4Decimator::decimateToSegments(rel.data(), s.v.data(), rel.size(),
                                                                    internal::BucketGrid(frame.widthDev, span), 0, frame.widthDev, gap);
    const auto &segs = frame.channels[0].segments;
    const auto &polys = frame.channels[0].polylines;
    QVERIFY(expected.size() >= 2); // the dropouts break the trace
    QCOMPARE(segs.size(), expected.size());
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        QCOMPARE(segs[i].size(), expected[i].size());
        for (std::size_t j = 0; j < expected[i].size(); ++j) {
            QCOMPARE(segs[i][j], expected[i][j].t + origin);
            const double y = plot.bottom() - (expected[i][j].value - chart.yMinimum()) / (chart.yMaximum() - chart.yMinimum()) * plot.height();
            QVERIFY2(std::abs(polys[i][static_cast<int>(j)].y() - y) < 1e-9, qPrintable(QStringLiteral("segment %1 vertex %2").arg(i).arg(j)));
        }
    }
}

// #25 (#12): one bucket per device column: the plot spans widthDev whole device columns and every
// vertex lies at the centre of the column of its bucket, at most 4 per column
void tst_AnalysisChart::oneBucketPerDeviceColumn()
{
    AnalysisChart chart;
    chart.resize(733, 320); // an odd size
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    const Series s = denseSeries();
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), static_cast<qsizetype>(s.t.size())));
    chart.setXRange(nanoseconds(5111111111LL), nanoseconds(6333333333LL));
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.grab();
    const auto &frame = internal::AnalysisChartTestAccess::lastFrame(chart);
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    const double dpr = chart.devicePixelRatioF();
    QCOMPARE(frame.devScale, dpr);
    QCOMPARE(frame.widthDev, static_cast<int>(std::lround(plot.width() * dpr)));
    QVERIFY2(std::abs(plot.width() * dpr - frame.widthDev) < 1e-9, qPrintable(QString::number(plot.width() * dpr))); // whole columns
    QCOMPARE(frame.leftDev, plot.left() * dpr);                                                                          // on a column boundary
    QVERIFY(frame.channels[0].decimated);
    const qint64 origin = chart.xStart().count();
    const qint64 span = (chart.xEnd() - chart.xStart()).count();
    std::vector<int> perColumn(static_cast<std::size_t>(frame.widthDev), 0);
    int vertices = 0;
    for (std::size_t i = 0; i < frame.channels[0].segments.size(); ++i) {
        const auto &ts = frame.channels[0].segments[i];
        const QPolygonF &poly = frame.channels[0].polylines[i];
        for (std::size_t j = 0; j < ts.size(); ++j) {
            const qint64 k = internal::BucketGrid(frame.widthDev, span).bucketOf(ts[j] - origin);
            QVERIFY(k >= 0 && k < frame.widthDev);
            QVERIFY2(std::abs(poly[static_cast<int>(j)].x() * dpr - (frame.leftDev + static_cast<double>(k) + 0.5)) < 1e-9,
                     qPrintable(QStringLiteral("vertex at %1 dev, bucket %2").arg(poly[static_cast<int>(j)].x() * dpr).arg(k)));
            ++perColumn[static_cast<std::size_t>(k)];
            ++vertices;
        }
    }
    QVERIFY(vertices > frame.widthDev); // dense data: more than one vertex per column on average
    for (int c : perColumn) QVERIFY(c <= 4);
}

// #25 (#11): in a decimated trace the device column of a bucket that holds a non-finite sample is
// blank, and every column of a bucket with only finite samples is drawn, at any zoom level
void tst_AnalysisChart::decimatedDropoutsAreBlankColumns()
{
    AnalysisChart chart;
    chart.resize(700, 320);
    chart.setGridVisible(false);
    chart.setLegendVisible(false);
    const QColor trace(0, 255, 0);
    const int a = chart.addChannel(QStringLiteral("A"), trace);
    const Series s = denseSeries();
    QVERIFY(chart.setChannelData(a, s.t.data(), s.v.data(), static_cast<qsizetype>(s.t.size())));
    chart.setGapThreshold(nanoseconds(std::numeric_limits<qint64>::max())); // only the dropouts break the trace
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    chart.setDecimationMode(AnalysisChart::DecimationMode::Always);
    // Full view and 200 ms views around each dropout (more samples than device columns at every DPR)
    const std::vector<std::pair<qint64, qint64>> views{{0, 0}, {5300000000LL, 5500000000LL}, {5800000000LL, 6000000000LL}, {6450000000LL, 6650000000LL}};
    for (const auto &view : views) {
        if (view.second > view.first) chart.setXRange(nanoseconds(view.first), nanoseconds(view.second));
        else chart.resetView();
        const QImage img = chart.grab().toImage();
        const auto &frame = internal::AnalysisChartTestAccess::lastFrame(chart);
        QVERIFY(frame.channels[0].decimated);
        const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
        const double dpr = img.devicePixelRatio();
        const auto buckets = referenceBuckets(chart, s, frame.widthDev);
        int blank = 0;
        int drawn = 0;
        for (int k = 0; k < frame.widthDev; ++k) {
            const auto &b = buckets[static_cast<std::size_t>(k)];
            if (b.count == 0 && !b.hasNonFinite) continue; // empty bucket: a connecting line may cross it
            const int x = static_cast<int>(frame.leftDev) + k;
            int lit = 0;
            for (int y = static_cast<int>(std::ceil(plot.top() * dpr)); y < static_cast<int>(plot.bottom() * dpr); ++y) {
                lit += img.pixelColor(x, y) == trace ? 1 : 0;
            }
            const QString where = QStringLiteral("view %1..%2, column %3: %4 trace px").arg(view.first).arg(view.second).arg(k).arg(lit);
            if (b.hasNonFinite) {
                QVERIFY2(lit == 0, qPrintable(where));
                ++blank;
            } else {
                QVERIFY2(lit > 0, qPrintable(where));
                ++drawn;
            }
        }
        QVERIFY2(blank >= 1 && drawn > frame.widthDev / 2, qPrintable(QStringLiteral("blank %1, drawn %2").arg(blank).arg(drawn)));
    }
}

// #25: setChannelData() builds the pyramid of the new data (replaced or cleared data included)
void tst_AnalysisChart::pyramidFollowsTheData()
{
    AnalysisChart chart;
    const int a = chart.addChannel(QStringLiteral("A"), Qt::green);
    QCOMPARE(internal::AnalysisChartTestAccess::pyramidBytes(chart, a), std::size_t(0));
    const Series big = uniformSeries(0, 1000, 64 * 100);
    QVERIFY(chart.setChannelData(a, big.t.data(), big.v.data(), static_cast<qsizetype>(big.t.size())));
    const std::size_t bigBytes = internal::AnalysisChartTestAccess::pyramidBytes(chart, a);
    QVERIFY(bigBytes > 0);
    const Series small = uniformSeries(0, 1000, 64 * 3);
    QVERIFY(chart.setChannelData(a, small.t.data(), small.v.data(), static_cast<qsizetype>(small.t.size())));
    const std::size_t smallBytes = internal::AnalysisChartTestAccess::pyramidBytes(chart, a);
    QVERIFY2(smallBytes > 0 && smallBytes < bigBytes, qPrintable(QStringLiteral("%1 -> %2").arg(bigBytes).arg(smallBytes)));
    QVERIFY(chart.setChannelData(a, nullptr, nullptr, 0));
    QCOMPARE(internal::AnalysisChartTestAccess::pyramidBytes(chart, a), std::size_t(0));
    QVERIFY(chart.setChannelData(a, big.t.data(), big.v.data(), static_cast<qsizetype>(big.t.size())));
    chart.clear();
    QCOMPARE(internal::AnalysisChartTestAccess::pyramidBytes(chart, a), std::size_t(0));
}

void tst_AnalysisChart::tracesStayInsideThePlotColumns_data()
{
    QTest::addColumn<int>("mode");
    QTest::addRow("raw") << static_cast<int>(AnalysisChart::DecimationMode::Off);
    QTest::addRow("decimated") << static_cast<int>(AnalysisChart::DecimationMode::Always);
}

// #25: traces are clipped to the whole device columns of the plot, also with a sample beyond each
// edge of the view (the raw trace enters and leaves through the edges)
void tst_AnalysisChart::tracesStayInsideThePlotColumns()
{
    QFETCH(int, mode);
    AnalysisChart chart;
    chart.resize(640, 300);
    chart.setGridVisible(false);
    chart.setLegendVisible(false);
    chart.setDecimationMode(static_cast<AnalysisChart::DecimationMode>(mode));
    const QColor trace(0, 255, 0);
    const int a = chart.addChannel(QStringLiteral("A"), trace, 4.0);
    // A steep square wave: the trace crosses both edges of the view at full height
    std::vector<nanoseconds> t;
    std::vector<double> v;
    for (int i = 0; i < 4000; ++i) {
        t.push_back(nanoseconds(qint64(i) * 1000));
        v.push_back((i / 7) % 2 == 0 ? 0.0 : 1.0);
    }
    QVERIFY(chart.setChannelData(a, t.data(), v.data(), 4000));
    chart.setXRange(nanoseconds(1000500), nanoseconds(1300500)); // edges between samples
    internal::AnalysisChartTestAccess::enableDiagnostics(chart);
    const QImage img = chart.grab().toImage();
    const auto &frame = internal::AnalysisChartTestAccess::lastFrame(chart);
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    const double dpr = img.devicePixelRatio();
    const int left = static_cast<int>(frame.leftDev);
    const int right = left + frame.widthDev; // exclusive
    int inside = 0;
    for (int y = static_cast<int>(std::ceil(plot.top() * dpr)); y < static_cast<int>(plot.bottom() * dpr); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            const bool green = c.green() > 120 && c.green() > c.red() + 60 && c.green() > c.blue() + 60;
            if (!green) continue;
            QVERIFY2(x >= left && x < right, qPrintable(QStringLiteral("trace pixel at %1,%2, plot columns %3..%4").arg(x).arg(y).arg(left).arg(right - 1)));
            ++inside;
        }
    }
    QVERIFY(inside > frame.widthDev); // the trace was drawn
}

QTEST_MAIN(tst_AnalysisChart)
#include "tst_AnalysisChart.moc"
