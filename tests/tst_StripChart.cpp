// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/StripChart.h>
#include <internal/StripChartGeometry.h>
#include <internal/M4Decimator.h>
#include "StripChartTestUtils.h"
#include <QtGui/QPixmap>
#include <QtGui/QImage>

#include <cmath>
#include <limits>
#include <vector>
#include <random>

using namespace QtIndustrialWidgets;

class TestSignalSource : public QObject
{
    Q_OBJECT
public:
    void sendPoint(int ch, double v) { Q_EMIT pointReady(ch, v); }
Q_SIGNALS:
    void pointReady(int ch, double v);
};

namespace {
// Expected-failure message for pixel checks where decimation buckets do not cover whole device
// columns (approved by the maintainer; tracked in #12).
const char *const kBucketNotPixelAligned = "Bucket boundaries are not aligned to device columns yet; see #12";

} // namespace

class tst_StripChart : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void channelManagement();
    void circularBufferInsertion();
    void autoScaling();
    void clearData();
    void signalEmission();
    void rendersTraceAtExpectedPositions();
    void extremeResizeNoCrash();
    void nonFiniteValuesBreakTheTrace();

    // New tests for high-rate DAQ & M4 decimation
    void plusInfAutoscaleRegression();
    void stepInterpolationDrawsHorizontalThenVertical();
    void addSynchronousSamplesTest();
    void timeTicksFollow125Rule();
    void timeTickLabels();
    void invalidInputsValidation();
    void capacityClamp24();
    void outOfOrderRejection();
    void gapsSplitTheTrace();
    void windowIsQuantisedAndScrollIsShiftInvariant();
    void timeWindowCoverage();
    void bulkApiAndSignalEmission();
    void legacyCompatibilityAndSlotPointer();
    void benchmarkHighRatePaint();
    void computeIndexBucketsGrid();
    void sampleIndexDecimationRecentSamples();
    void computeTimeWindowBoundaries();
    void timeBucketBoundarySpikePreservation();
    void timeWindowCoverageConsistency();
    void autoDecimationThreshold();
    void axisModeSwitchClearing();
    void sampleIndexExplicitTimestampIgnored();
    void axisModeRoundTrip();
    void sameAxisModePreservesData();
    void addUniformSamplesSampleIndexMode();
    void evictionConsistency();
    void widgetM4Equivalence();
    void lazyAutoscale();
    void hysteresis();
    void decimatedSingleDropout_data();
    void decimatedSingleDropout();
    void decimatedDropoutAtWindowEdges_data();
    void decimatedDropoutAtWindowEdges();
    void decimatedConsecutiveDropouts_data();
    void decimatedConsecutiveDropouts();
    void decimatedDropoutStreaming_data();
    void decimatedDropoutStreaming();
    void decimatedDropoutEviction_data();
    void decimatedDropoutEviction();
    void autoscaleUsesFiniteValuesOfBlankedBucket();
    void glowDotNeverDrawnInBlankColumn_data();
    void glowDotNeverDrawnInBlankColumn();
};

void tst_StripChart::defaultValues()
{
    StripChart chart;
    QCOMPARE(chart.channelCount(), 0);
    QCOMPARE(chart.capacity(), 300);
    QCOMPARE(chart.yMinimum(), 0.0);
    QCOMPARE(chart.yMaximum(), 100.0);
    QCOMPARE(chart.isAutoScaleY(), false);
    QCOMPARE(chart.xAxisMode(), StripChart::XAxisMode::SampleIndex);
    QCOMPARE(chart.decimationMode(), StripChart::DecimationMode::Auto);
    QCOMPARE(chart.interpolation(), StripChart::Interpolation::Linear);
    QCOMPARE(chart.timeLabelFormat(), StripChart::TimeLabelFormat::Relative);
    QCOMPARE(chart.timeSpan(), std::chrono::seconds(10));
    QCOMPARE(chart.timeSpanSeconds(), 10.0);
}

void tst_StripChart::channelManagement()
{
    StripChart chart;
    int ch1 = chart.addChannel(QStringLiteral("Voltage"), Qt::red, 2.0);
    int ch2 = chart.addChannel(QStringLiteral("Current"), Qt::blue, 1.5);

    QCOMPARE(ch1, 0);
    QCOMPARE(ch2, 1);
    QCOMPARE(chart.channelCount(), 2);

    QCOMPARE(chart.channelName(0), QStringLiteral("Voltage"));
    QCOMPARE(chart.channelColor(0), QColor(Qt::red));
    QCOMPARE(chart.channelPenWidth(0), 2.0);
    QVERIFY(chart.isChannelVisible(0));
}

void tst_StripChart::circularBufferInsertion()
{
    StripChart chart;
    chart.setCapacity(10);
    int ch = chart.addChannel(QStringLiteral("Signal"), Qt::green);

    // Insert more points than capacity to verify circular buffer wraparound
    for (int i = 1; i <= 20; ++i) {
        chart.addDataPoint(ch, static_cast<double>(i * 10));
    }

    QCOMPARE(chart.channelLatestValue(ch), 200.0);
    QCOMPARE(static_cast<int>(chart.channelSampleCount(ch)), 10);
}

void tst_StripChart::autoScaling()
{
    StripChart chart;
    chart.setAutoScaleY(true);
    int ch = chart.addChannel(QStringLiteral("Wave"), Qt::cyan);

    chart.addDataPoint(ch, -150.0);
    chart.addDataPoint(ch, 350.0);

    QVERIFY(chart.yMinimum() <= -150.0);
    QVERIFY(chart.yMaximum() >= 350.0);
}

void tst_StripChart::clearData()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("Data"), Qt::yellow);
    chart.addDataPoint(ch, 42.0);

    chart.clear();
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(0));
    QCOMPARE(chart.channelLatestValue(ch), 0.0);
}

void tst_StripChart::signalEmission()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("Test"), Qt::magenta);

    QSignalSpy spy(&chart, &StripChart::dataAdded);
    chart.addDataPoint(ch, 12.3);

    QCOMPARE(spy.count(), 1);
}

void tst_StripChart::rendersTraceAtExpectedPositions()
{
    // 1. Setup: SampleIndex mode, 50 samples of a sine, raw path
    auto chart = makeChart(800, 600, -1.5, 1.5);
    chart->setXAxisMode(StripChart::XAxisMode::SampleIndex);
    chart->setCapacity(50);
    chart->setDecimationMode(StripChart::DecimationMode::Off);
    chart->setGridVisible(false);
    chart->setLegendVisible(false);

    const QColor channelColor(255, 0, 0); // Pure Red
    int ch = chart->addChannel(QStringLiteral("Sine"), channelColor, 1.8);

    std::vector<double> sinVals(50);
    for (size_t i = 0; i < 50; ++i) {
        sinVals[i] = std::sin(2.0 * M_PI * static_cast<double>(i) / 49.0);
        chart->addDataPoint(ch, sinVals[i]);
    }

    // 2. Render to raster image and check hook
    QImage img = renderToImage(*chart);
    const auto &frame = StripChartTestAccess::lastFrame(*chart);

    QCOMPARE(frame.channels.size(), 1UL);
    const auto &chDiag = frame.channels[0];
    QCOMPARE(chDiag.path, RenderPath::Raw);
    QCOMPARE(chDiag.segments.size(), 1UL);

    const QPolygonF &poly = chDiag.segments[0];
    QCOMPARE(poly.size(), 50);

    for (size_t i = 0; i < 50; ++i) {
        double expectedX = mapIndex(*chart, i);
        double expectedY = mapY(*chart, sinVals[i]);

        QVERIFY2(std::abs(poly[static_cast<int>(i)].x() - expectedX) <= 0.5,
                 qPrintable(QString("Vertex %1 X mismatch: got %2, expected %3").arg(i).arg(poly[static_cast<int>(i)].x()).arg(expectedX)));
        QVERIFY2(std::abs(poly[static_cast<int>(i)].y() - expectedY) <= 0.5,
                 qPrintable(QString("Vertex %1 Y mismatch: got %2, expected %3").arg(i).arg(poly[static_cast<int>(i)].y()).arg(expectedY)));

        if (i > 0) {
            QVERIFY(poly[static_cast<int>(i)].x() > poly[static_cast<int>(i) - 1].x());
        }
    }

    // 3. Pixels:
    // - the trace colour is present in >= 95 % of the columns between first and last vertex x
    // - no trace-colour pixel lies outside the plot rectangle
    qreal dpr = chart->devicePixelRatioF();
    int xStartDev = static_cast<int>(std::round(poly.first().x() * dpr));
    int xEndDev = static_cast<int>(std::round(poly.last().x() * dpr));
    int plotTopDev = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
    int plotBottomDev = static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr));
    int plotLeftDev = static_cast<int>(std::floor(frame.plotRect.left() * dpr));
    int plotRightDev = static_cast<int>(std::ceil(frame.plotRect.right() * dpr));

    int totalCols = xEndDev - xStartDev + 1;
    int presentCols = 0;
    for (int x = xStartDev; x <= xEndDev; ++x) {
        bool found = false;
        for (int y = plotTopDev; y <= plotBottomDev; ++y) {
            QColor c = img.pixelColor(x, y);
            if (c.red() > 70 && c.green() < 50 && c.blue() < 50) {
                found = true;
                break;
            }
        }
        if (found) {
            presentCols++;
        }
    }
    double coverage = static_cast<double>(presentCols) / static_cast<double>(totalCols);
    QVERIFY2(coverage >= 0.95, qPrintable(QString("Coverage %1 below 95% (present: %2, total: %3)").arg(coverage).arg(presentCols).arg(totalCols)));

    for (int x = 0; x < img.width(); ++x) {
        for (int y = 0; y < img.height(); ++y) {
            if (x < plotLeftDev || x > plotRightDev || y < plotTopDev || y > plotBottomDev) {
                QColor c = img.pixelColor(x, y);
                bool isTrace = (c.red() > 200 && c.green() < 60 && c.blue() < 60);
                QVERIFY2(!isTrace, qPrintable(QString("Trace color found outside plot rectangle at (%1, %2)").arg(x).arg(y)));
            }
        }
    }
}

void tst_StripChart::extremeResizeNoCrash()
{
    auto chart = makeChart(2, 2);
    int ch = chart->addChannel(QStringLiteral("Ch0"), Qt::cyan);
    chart->addDataPoint(ch, 50.0);

    // 1. Tiny size (2x2): plotRect <= 10 px, must draw bezel safely without painter crash or divide-by-zero
    QImage imgSmall = renderToImage(*chart);
    QCOMPARE(imgSmall.size(), (QSizeF(2, 2) * chart->devicePixelRatioF()).toSize());
    const auto &frameSmall = StripChartTestAccess::lastFrame(*chart);
    QVERIFY(frameSmall.plotRect.width() <= 10.0 || frameSmall.plotRect.height() <= 10.0);
    if (!frameSmall.channels.empty()) {
        QVERIFY(frameSmall.channels[0].segments.empty());
    }

    // 2. Large size (3840x2160): HiDPI/4K canvas, plotRect.width() > 3700 px
    chart->resize(3840, 2160);
    chart->setXAxisMode(StripChart::XAxisMode::Time);
    chart->setTimeSpan(std::chrono::seconds(10));
    chart->addSample(ch, std::chrono::seconds(1), 10.0);
    chart->addSample(ch, std::chrono::seconds(9), 90.0);

    QImage imgLarge = renderToImage(*chart);
    QCOMPARE(imgLarge.size(), (QSizeF(3840, 2160) * chart->devicePixelRatioF()).toSize());
    const auto &frameLarge = StripChartTestAccess::lastFrame(*chart);
    QVERIFY(frameLarge.plotRect.width() > 3700.0);
    QVERIFY(frameLarge.plotRect.height() > 2000.0);
    QCOMPARE(frameLarge.channels.size(), 1UL);
    QVERIFY(!frameLarge.channels[0].segments.empty());
    for (const auto &pt : frameLarge.channels[0].segments[0]) {
        QVERIFY(pt.x() >= frameLarge.plotRect.left() - 1.0);
        QVERIFY(pt.x() <= frameLarge.plotRect.right() + 1.0);
        QVERIFY(pt.y() >= frameLarge.plotRect.top() - 1.0);
        QVERIFY(pt.y() <= frameLarge.plotRect.bottom() + 1.0);
    }
}

void tst_StripChart::nonFiniteValuesBreakTheTrace()
{
    // Part 1: Retain data/API level NaN/Inf resilience checks
    {
        StripChart chart;
        int ch = chart.addChannel(QStringLiteral("Sensor"), Qt::green);
        chart.setAutoScaleY(true);

        chart.addDataPoint(ch, 10.0);
        chart.addDataPoint(ch, 20.0);

        chart.addDataPoint(ch, std::numeric_limits<double>::infinity());
        chart.addDataPoint(ch, -std::numeric_limits<double>::infinity());

        QVERIFY(std::isfinite(chart.yMinimum()));
        QVERIFY(std::isfinite(chart.yMaximum()));
        QVERIFY(chart.yMinimum() < chart.yMaximum());

        chart.addDataPoint(ch, std::numeric_limits<double>::quiet_NaN());
        QVERIFY(std::isnan(chart.channelLatestValue(ch)));

        chart.setYRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
        QVERIFY(std::isfinite(chart.yMinimum()));
    }

    // Part 2: Behavioural render checks - Raw path: 100 samples with NaN at index 50 and +Inf at index 70
    {
        WarningGuard guard;
        auto chart = makeChart(1000, 300, 0.0, 100.0);
        if (chart->devicePixelRatioF() != 1.0) {
            QSKIP("Pixel gap check requires DPR 1");
        }
        chart->setXAxisMode(StripChart::XAxisMode::SampleIndex);
        chart->setCapacity(100);
        chart->setDecimationMode(StripChart::DecimationMode::Off);
        chart->setGridVisible(false);
        chart->setLegendVisible(false);

        const QColor chColor(255, 0, 0);
        int ch = chart->addChannel(QStringLiteral("Trace"), chColor, 1.0);

        for (int i = 0; i < 100; ++i) {
            double v = 50.0;
            if (i == 50) {
                v = std::numeric_limits<double>::quiet_NaN();
            } else if (i == 70) {
                v = std::numeric_limits<double>::infinity();
            }
            chart->addDataPoint(ch, v);
        }

        QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);

        // WarningGuard sees no painter warning
        QVERIFY2(!guard.hasWarnings(), qPrintable(QString("Unexpected warnings: %1").arg(guard.warnings().join("; "))));

        // Hook: 3 segments: [0, 49], [51, 69], [71, 99]
        QCOMPARE(frame.channels.size(), 1UL);
        const auto &chDiag = frame.channels[0];
        QCOMPARE(chDiag.path, RenderPath::Raw);
        QCOMPARE(chDiag.segments.size(), 3UL);
        QCOMPARE(chDiag.segments[0].size(), 50); // indices 0..49
        QCOMPARE(chDiag.segments[1].size(), 19); // indices 51..69
        QCOMPARE(chDiag.segments[2].size(), 29); // indices 71..99

        for (const auto &seg : chDiag.segments) {
            for (const auto &pt : seg) {
                QVERIFY(std::isfinite(pt.x()));
                QVERIFY(std::isfinite(pt.y()));
            }
        }

        // Pixels: no trace pixel in the columns strictly between the x of samples 49 and 51,
        // and strictly between 69 and 71
        qreal dpr = chart->devicePixelRatioF();
        int plotTopDev = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
        int plotBottomDev = static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr));

        int x49Dev = static_cast<int>(std::floor(mapIndex(*chart, 49) * dpr));
        int x51Dev = static_cast<int>(std::ceil(mapIndex(*chart, 51) * dpr));
        for (int x = x49Dev + 1; x < x51Dev; ++x) {
            auto ys = traceYAtColumn(img, chColor, x, plotTopDev, plotBottomDev, 60);
            QVERIFY2(ys.empty(), qPrintable(QString("Trace pixel found between samples 49 and 51 at col %1").arg(x)));
        }

        int x69Dev = static_cast<int>(std::floor(mapIndex(*chart, 69) * dpr));
        int x71Dev = static_cast<int>(std::ceil(mapIndex(*chart, 71) * dpr));
        for (int x = x69Dev + 1; x < x71Dev; ++x) {
            auto ys = traceYAtColumn(img, chColor, x, plotTopDev, plotBottomDev, 60);
            QVERIFY2(ys.empty(), qPrintable(QString("Trace pixel found between samples 69 and 71 at col %1").arg(x)));
        }
    }

    // Part 3: Decimated path: 100 k samples with NaN at 50 000
    {
        WarningGuard guard;
        auto chart = makeChart(1000, 300, 0.0, 100.0);
        chart->setXAxisMode(StripChart::XAxisMode::SampleIndex);
        chart->setCapacity(100000);
        chart->setDecimationMode(StripChart::DecimationMode::Always);
        chart->setGridVisible(false);
        chart->setLegendVisible(false);

        const QColor chColor(255, 0, 0);
        int ch = chart->addChannel(QStringLiteral("Trace"), chColor, 1.0);

        for (int i = 0; i < 100000; ++i) {
            double v = (i == 50000) ? std::numeric_limits<double>::quiet_NaN() : 50.0;
            chart->addDataPoint(ch, v);
        }

        QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);

        QVERIFY2(!guard.hasWarnings(), qPrintable(QString("Unexpected warnings: %1").arg(guard.warnings().join("; "))));
        QCOMPARE(frame.channels.size(), 1UL);
        const auto &chDiag = frame.channels[0];
        QCOMPARE(chDiag.path, RenderPath::Decimated);

        for (const auto &seg : chDiag.segments) {
            for (const auto &pt : seg) {
                QVERIFY(std::isfinite(pt.x()));
                QVERIFY(std::isfinite(pt.y()));
            }
        }

        qreal dpr = chart->devicePixelRatioF();
        int plotTopDev = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
        int plotBottomDev = static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr));
        int xNanDev = static_cast<int>(std::round(mapIndex(*chart, 50000) * dpr));
        auto ysNan = traceYAtColumn(img, chColor, xNanDev, plotTopDev, plotBottomDev, 60);

        // The bucket containing the NaN is not drawn: 2 segments and no trace in its column.
        QCOMPARE(chDiag.segments.size(), 2UL);

        // At DPR 1 the NaN bucket (samples 49926-50031) spans x 514.305-515.307 px and the next
        // segment starts at x 515.308 (sample 50032), inside device column 515.
        QEXPECT_FAIL("", kBucketNotPixelAligned, Continue);
        QVERIFY2(ysNan.empty(), qPrintable(QString("Trace pixel found in decimated NaN column %1").arg(xNanDev)));
    }
}

void tst_StripChart::plusInfAutoscaleRegression()
{
    StripChart chart;
    chart.setAutoScaleY(true);
    int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);

    chart.addDataPoint(ch, 5.0);
    chart.addDataPoint(ch, 25.0);

    // Regression check: +Inf must not cause autoscale min or max to become infinite
    chart.addDataPoint(ch, std::numeric_limits<double>::infinity());

    QVERIFY(std::isfinite(chart.yMinimum()));
    QVERIFY(std::isfinite(chart.yMaximum()));
    QVERIFY(chart.yMinimum() < chart.yMaximum());
    QVERIFY(chart.yMaximum() >= 25.0);
}

void tst_StripChart::stepInterpolationDrawsHorizontalThenVertical()
{
    // Setup: 4 samples: 0, 10, 10, 0, raw path.
    auto chart = makeChart(400, 300, 0.0, 20.0);
    chart->setXAxisMode(StripChart::XAxisMode::Time);
    chart->setTimeSpan(std::chrono::seconds(10));
    chart->setGridVisible(false);
    chart->setLegendVisible(false);

    const QColor chColor(255, 0, 0);
    int ch = chart->addChannel(QStringLiteral("Square"), chColor, 1.8);

    chart->addSample(ch, std::chrono::seconds(1), 0.0);
    chart->addSample(ch, std::chrono::seconds(3), 10.0);
    chart->addSample(ch, std::chrono::seconds(5), 10.0);
    chart->addSample(ch, std::chrono::seconds(7), 0.0);

    // 1. Step mode
    chart->setInterpolation(StripChart::Interpolation::Step);
    QCOMPARE(chart->interpolation(), StripChart::Interpolation::Step);

    QImage imgStep = renderToImage(*chart);
    const auto &frameStep = StripChartTestAccess::lastFrame(*chart);

    QCOMPARE(frameStep.channels.size(), 1UL);
    const auto &polyStep = frameStep.channels[0].segments[0];
    // 7 vertices (2n - 1)
    QCOMPARE(polyStep.size(), 7);

    // Every consecutive pair shares either x or y; never a diagonal.
    // The horizontal leg comes first:
    // for pair (poly[2i], poly[2i+1]): y is equal (horizontal).
    // for pair (poly[2i+1], poly[2i+2]): x is equal (vertical).
    for (int i = 0; i < 3; ++i) {
        QPointF p0 = polyStep[2 * i];
        QPointF pMid = polyStep[2 * i + 1];
        QPointF p1 = polyStep[2 * i + 2];

        // Horizontal leg
        QVERIFY2(std::abs(pMid.y() - p0.y()) < 1e-4, "Expected horizontal leg");
        QVERIFY2(pMid.x() > p0.x(), "Expected increasing x in horizontal leg");

        // Vertical leg
        QVERIFY2(std::abs(pMid.x() - p1.x()) < 1e-4, "Expected vertical leg");
    }

    for (int i = 0; i < polyStep.size() - 1; ++i) {
        bool sharesX = std::abs(polyStep[i].x() - polyStep[i + 1].x()) < 1e-4;
        bool sharesY = std::abs(polyStep[i].y() - polyStep[i + 1].y()) < 1e-4;
        QVERIFY2(sharesX || sharesY, "Adjacent vertices in Step interpolation must share either X or Y (never diagonal)");
    }

    // 2. Linear mode
    chart->setInterpolation(StripChart::Interpolation::Linear);
    QCOMPARE(chart->interpolation(), StripChart::Interpolation::Linear);

    QImage imgLinear = renderToImage(*chart);
    const auto &frameLinear = StripChartTestAccess::lastFrame(*chart);
    const auto &polyLinear = frameLinear.channels[0].segments[0];
    QCOMPARE(polyLinear.size(), 4);
    // Segment 0 -> 1 is diagonal
    QVERIFY(std::abs(polyLinear[0].x() - polyLinear[1].x()) > 1.0);
    QVERIFY(std::abs(polyLinear[0].y() - polyLinear[1].y()) > 1.0);

    // 3. Pixel check: at the column halfway between sample 0 and sample 1
    qreal dpr = chart->devicePixelRatioF();
    double xHalf = (polyLinear[0].x() + polyLinear[1].x()) / 2.0;
    int colDev = static_cast<int>(std::round(xHalf * dpr));

    int plotTopDev = static_cast<int>(std::floor(frameStep.plotRect.top() * dpr));
    int plotBottomDev = static_cast<int>(std::ceil(frameStep.plotRect.bottom() * dpr));

    double expectedY0 = mapY(*chart, 0.0) * dpr;
    double expectedY5 = mapY(*chart, 5.0) * dpr;

    // Step mode: trace is at mapY(0) +-1 px (or +-2 px for dpr)
    auto ysStep = traceYAtColumn(imgStep, chColor, colDev, plotTopDev, plotBottomDev, 60);
    QVERIFY2(!ysStep.empty(), "Trace pixel not found in Step mode at halfway column");
    bool foundStepY0 = false;
    for (int y : ysStep) {
        if (std::abs(y - expectedY0) <= std::round(2.0 * dpr)) {
            foundStepY0 = true;
            break;
        }
    }
    QVERIFY2(foundStepY0, qPrintable(QString("Step mode trace at column %1 was not at mapY(0) (%2)").arg(colDev).arg(expectedY0)));

    // Linear mode: trace is at mapY(5) +-1 px
    auto ysLinear = traceYAtColumn(imgLinear, chColor, colDev, plotTopDev, plotBottomDev, 60);
    QVERIFY2(!ysLinear.empty(), "Trace pixel not found in Linear mode at halfway column");
    bool foundLinearY5 = false;
    for (int y : ysLinear) {
        if (std::abs(y - expectedY5) <= std::round(2.0 * dpr)) {
            foundLinearY5 = true;
            break;
        }
    }
    QVERIFY2(foundLinearY5, qPrintable(QString("Linear mode trace at column %1 was not at mapY(5) (%2)").arg(colDev).arg(expectedY5)));
}

void tst_StripChart::addSynchronousSamplesTest()
{
    StripChart chart;
    int ch1 = chart.addChannel(QStringLiteral("Ch1"), Qt::red);
    int ch2 = chart.addChannel(QStringLiteral("Ch2"), Qt::green);
    int ch3 = chart.addChannel(QStringLiteral("Ch3"), Qt::blue);

    QSignalSpy spy(&chart, &StripChart::dataAdded);
    chart.addSynchronousSamples(std::chrono::milliseconds(100), {10.5, 20.5, 30.5});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(chart.channelLatestValue(ch1), 10.5);
    QCOMPARE(chart.channelLatestValue(ch2), 20.5);
    QCOMPARE(chart.channelLatestValue(ch3), 30.5);
}

void tst_StripChart::timeTicksFollow125Rule()
{
    // Pure function tests across spans and widths
    std::vector<qint64> spans = {
        100000LL,             // 100 µs
        500000000LL,          // 0.5 s
        1000000000LL,         // 1.0 s
        3700000000LL,         // 3.7 s
        10000000000LL,        // 10 s
        60000000000LL,        // 60 s
        3600000000000LL       // 1 h
    };
    std::vector<double> widths = {300.0, 800.0, 1920.0};

    for (qint64 span : spans) {
        for (double w : widths) {
            auto res = internal::computeTimeTicks(0, span, w, 60.0);

            // 1. stepNs is {1, 2, 5} * 10^k
            QVERIFY(res.stepNs > 0);
            double expVal = std::floor(std::log10(static_cast<double>(res.stepNs)));
            double mantissa = std::round(res.stepNs / std::pow(10.0, expVal));
            QVERIFY2(mantissa == 1.0 || mantissa == 2.0 || mantissa == 5.0 || mantissa == 10.0,
                     qPrintable(QString("Step %1 not in 1-2-5 progression (mantissa=%2)").arg(res.stepNs).arg(mantissa)));

            // 2. Every tick is a multiple of stepNs and lies inside [tStart, tEnd]
            for (const auto &tick : res.ticks) {
                QCOMPARE(tick.tNs % res.stepNs, 0LL);
                QVERIFY(tick.tNs >= 0);
                QVERIFY(tick.tNs <= span);
            }

            // 3. Adjacent ticks are >= minLabelSpacingPx apart (or spacing * 0.8)
            double tickSpacingPx = (static_cast<double>(res.stepNs) / static_cast<double>(span)) * w;
            QVERIFY2(tickSpacingPx >= 60.0 * 0.8,
                     qPrintable(QString("Ticks too dense: spacing=%1 px (< 48 px) for span=%2 w=%3").arg(tickSpacingPx).arg(span).arg(w)));

            // 4. At least 2 ticks
            QVERIFY2(res.ticks.size() >= 2,
                     qPrintable(QString("Expected >= 2 ticks, got %1 for span=%2 w=%3").arg(res.ticks.size()).arg(span).arg(w)));

            // 5. Unit chosen from the step
            if (res.stepNs < 1000) {
                QCOMPARE(res.unit, QStringLiteral("ns"));
            } else if (res.stepNs < 1000000) {
                QCOMPARE(res.unit, QStringLiteral("µs"));
            } else if (res.stepNs < 1000000000LL) {
                QCOMPARE(res.unit, QStringLiteral("ms"));
            } else if (res.stepNs < 60000000000LL) {
                QCOMPARE(res.unit, QStringLiteral("s"));
            } else {
                QCOMPARE(res.unit, QStringLiteral("min"));
            }
        }
    }
}

void tst_StripChart::timeTickLabels()
{
    // 1. Label promotion rule: for span = 1 s, width = 800 px, no label contains "1000 ms"
    auto res1s = internal::computeTimeTicks(0, 1000000000LL, 800.0, 60.0);
    for (const auto &t : res1s.ticks) {
        QVERIFY2(!t.label.contains(QStringLiteral("1000 ms")),
                 qPrintable(QString("Label '%1' violated promotion rule (contains '1000 ms')").arg(t.label)));
    }
    // Left edge (t = 0) must be "-1 s" or "-1.0 s"
    QVERIFY2(res1s.ticks.front().label == QStringLiteral("-1 s") || res1s.ticks.front().label == QStringLiteral("-1.0 s"),
             qPrintable(QString("Expected '-1 s' or '-1.0 s', got '%1'").arg(res1s.ticks.front().label)));

    // 2. Absolute format: with setTimeOrigin and known tLatest
    {
        auto chart = makeChart(800, 400);
        chart->setXAxisMode(StripChart::XAxisMode::Time);
        chart->setTimeLabelFormat(StripChart::TimeLabelFormat::Absolute);
        chart->setTimeSpan(std::chrono::seconds(10));
        QDateTime origin(QDate(2026, 1, 1), QTime(12, 0, 0), Qt::UTC);
        chart->setTimeOrigin(origin);

        int ch = chart->addChannel(QStringLiteral("Ch0"), Qt::green);
        chart->addSample(ch, std::chrono::seconds(10), 42.0);

        renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);

        QVERIFY(frame.xTicks.size() >= 2);
        for (const auto &tick : frame.xTicks) {
            // Label must follow HH:mm:ss
            QVERIFY2(tick.label.contains(QStringLiteral("12:00:")),
                     qPrintable(QString("Expected HH:mm:ss label, got '%1'").arg(tick.label)));
        }
    }

    // 3. Widget integration: FrameDiagnostics::xTicks
    {
        auto chart = makeChart(800, 400);
        chart->setXAxisMode(StripChart::XAxisMode::Time);
        chart->setTimeLabelFormat(StripChart::TimeLabelFormat::Relative);
        chart->setTimeSpan(std::chrono::seconds(10));
        int ch = chart->addChannel(QStringLiteral("Ch0"), Qt::green);
        chart->addSample(ch, std::chrono::seconds(10), 42.0);

        renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);

        QVERIFY(frame.xTicks.size() >= 2);
        for (size_t i = 0; i < frame.xTicks.size(); ++i) {
            QVERIFY(frame.xTicks[i].xPos >= frame.plotRect.left() - 0.5);
            QVERIFY(frame.xTicks[i].xPos <= frame.plotRect.right() + 0.5);
            if (i > 0) {
                QVERIFY(frame.xTicks[i].xPos > frame.xTicks[i - 1].xPos);
                QVERIFY(frame.xTicks[i].tNs > frame.xTicks[i - 1].tNs);
                QVERIFY(frame.xTicks[i].label != frame.xTicks[i - 1].label);
            }
        }
    }
}

void tst_StripChart::invalidInputsValidation()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("Test"), Qt::yellow);

    // 1. Bad channel IDs
    chart.addSample(-1, std::chrono::nanoseconds(100), 5.0);
    chart.addSample(99, std::chrono::nanoseconds(100), 5.0);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(0));

    // 2. Null pointers
    chart.addSamples(ch, nullptr, nullptr, 10);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(0));

    // 3. Count <= 0
    std::chrono::nanoseconds tArr[] = {std::chrono::nanoseconds(10)};
    double vArr[] = {1.0};
    chart.addSamples(ch, tArr, vArr, 0);
    chart.addSamples(ch, tArr, vArr, -3);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(0));

    // 4. dt <= 0 in addUniformSamples (must be rejected)
    chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(0), vArr, 1);
    chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(-10), vArr, 1);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(0));
}

void tst_StripChart::capacityClamp24()
{
    StripChart chart;

    // Minimum clamp: 10
    chart.setCapacity(4);
    QCOMPARE(chart.capacity(), 10);

    // Maximum clamp: 2^24 = 16,777,216
    chart.setCapacity(20000000);
    QCOMPARE(chart.capacity(), 16777216);

    // Restore moderate capacity
    chart.setCapacity(500);
    QCOMPARE(chart.capacity(), 500);
}

void tst_StripChart::outOfOrderRejection()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    int ch = chart.addChannel(QStringLiteral("DAQ"), Qt::cyan);

    // 1. Initial sample at t = 100 ns
    chart.addSample(ch, std::chrono::nanoseconds(100), 10.0);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(1));
    QCOMPARE(chart.rejectedSampleCount(ch), 0ULL);

    // 2. Out-of-order sample at t = 50 ns (must be rejected)
    chart.addSample(ch, std::chrono::nanoseconds(50), 99.0);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(1));
    QCOMPARE(chart.rejectedSampleCount(ch), 1ULL);
    QCOMPARE(chart.channelLatestValue(ch), 10.0);

    // 3. Equal timestamp t = 100 ns (allowed by spec)
    chart.addSample(ch, std::chrono::nanoseconds(100), 12.0);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(2));
    QCOMPARE(chart.rejectedSampleCount(ch), 1ULL);
    QCOMPARE(chart.channelLatestValue(ch), 12.0);
}

void tst_StripChart::gapsSplitTheTrace()
{
    // Part 1: Raw path
    // 6 samples at 1 ms spacing (t = 0-5 ms), then one sample at t = 55 ms.
    auto buildRawChart = [](std::chrono::nanoseconds customGap) {
        auto chart = makeChart(500, 300, 0.0, 10.0);
        chart->setXAxisMode(StripChart::XAxisMode::Time);
        chart->setTimeSpan(std::chrono::milliseconds(100));
        chart->setGridVisible(false);
        chart->setLegendVisible(false);
        if (customGap.count() > 0) {
            chart->setGapThreshold(customGap);
        }

        const QColor chColor(255, 0, 0);
        int ch = chart->addChannel(QStringLiteral("Pulses"), chColor, 1.8);

        std::vector<double> vals(6, 5.0);
        chart->addUniformSamples(ch, std::chrono::milliseconds(0), std::chrono::milliseconds(1), vals.data(), 6);
        chart->addSample(ch, std::chrono::milliseconds(55), 5.0);
        return chart;
    };

    // Automatic threshold (EMA = 1 ms -> 4 ms): 2 segments
    {
        auto chart = buildRawChart(std::chrono::nanoseconds(0));
        QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);

        QCOMPARE(frame.channels.size(), 1UL);
        const auto &chDiag = frame.channels[0];
        QCOMPARE(chDiag.path, RenderPath::Raw);
        QCOMPARE(chDiag.segments.size(), 2UL);
        QCOMPARE(chDiag.segments[0].size(), 6);
        QCOMPARE(chDiag.segments[1].size(), 1);

        // Pixels: with automatic threshold, no trace pixel in columns strictly between x(5 ms) and x(55 ms)
        qreal dpr = chart->devicePixelRatioF();
        int x5Dev = static_cast<int>(std::round(mapT(*chart, 5000000LL) * dpr));
        int x55Dev = static_cast<int>(std::round(mapT(*chart, 55000000LL) * dpr));
        int plotTopDev = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
        int plotBottomDev = static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr));

        for (int x = x5Dev + 2; x < x55Dev - 2; ++x) {
            auto ys = traceYAtColumn(img, QColor(255, 0, 0), x, plotTopDev, plotBottomDev, 60);
            QVERIFY2(ys.empty(), qPrintable(QString("Trace pixel found in gap column %1").arg(x)));
        }
    }

    // setGapThreshold(100 ms): 1 segment (pause 50 ms < 100 ms)
    {
        auto chart = buildRawChart(std::chrono::milliseconds(100));
        renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);
        QCOMPARE(frame.channels[0].segments.size(), 1UL);
        QCOMPARE(frame.channels[0].segments[0].size(), 7);
    }

    // setGapThreshold(10 ms): 2 segments (pause 50 ms > 10 ms)
    {
        auto chart = buildRawChart(std::chrono::milliseconds(10));
        renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);
        QCOMPARE(frame.channels[0].segments.size(), 2UL);
    }

    // Part 2: Decimated path
    // DecimationMode::Always and dense data (100 kHz before and after 50 ms pause)
    {
        auto chart = makeChart(800, 300, 0.0, 10.0);
        chart->setXAxisMode(StripChart::XAxisMode::Time);
        chart->setDecimationMode(StripChart::DecimationMode::Always);
        chart->setTimeSpan(std::chrono::milliseconds(200));
        chart->setGridVisible(false);
        chart->setLegendVisible(false);
        chart->setCapacity(5000);

        const QColor chColor(255, 0, 0);
        int ch = chart->addChannel(QStringLiteral("Pulses"), chColor, 1.0);

        // 100 kHz (10 µs spacing) for 20 ms -> 2000 samples (t = 0..20 ms)
        std::vector<double> vals(2000, 5.0);
        chart->addUniformSamples(ch, std::chrono::milliseconds(0), std::chrono::microseconds(10), vals.data(), 2000);

        // Pause of 50 ms -> next burst at t = 70 ms
        chart->addUniformSamples(ch, std::chrono::milliseconds(70), std::chrono::microseconds(10), vals.data(), 2000);

        QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);

        QCOMPARE(frame.channels.size(), 1UL);
        const auto &chDiag = frame.channels[0];
        QCOMPARE(chDiag.path, RenderPath::Decimated);
        QCOMPARE(chDiag.segments.size(), 2UL);

        qreal dpr = chart->devicePixelRatioF();
        int x20Dev = static_cast<int>(std::round(mapT(*chart, 20000000LL) * dpr));
        int x70Dev = static_cast<int>(std::round(mapT(*chart, 70000000LL) * dpr));
        int plotTopDev = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
        int plotBottomDev = static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr));

        for (int x = x20Dev + 3; x < x70Dev - 3; ++x) {
            auto ys = traceYAtColumn(img, chColor, x, plotTopDev, plotBottomDev, 60);
            QVERIFY2(ys.empty(), qPrintable(QString("Trace pixel found in decimated gap column %1").arg(x)));
        }
    }
}

void tst_StripChart::windowIsQuantisedAndScrollIsShiftInvariant()
{
    // Part 1: Window quantization invariant
    // Hook, for tLatest in {123456789 ns, an exact multiple of dtPx, multiple + 1, multiple - 1}:
    // - tEnd % dtPx == 0
    // - tStart <= tLatest < tEnd
    // - tEnd - tStart == timeSpan
    {
        auto chart = makeChart(800, 300);
        chart->setXAxisMode(StripChart::XAxisMode::Time);
        chart->setTimeSpan(std::chrono::seconds(10));
        chart->addChannel(QStringLiteral("Ch0"), Qt::cyan);

        renderToImage(*chart);
        const auto &f0 = StripChartTestAccess::lastFrame(*chart);
        qint64 dtPx = f0.dtPx;
        QVERIFY(dtPx > 0);

        qint64 mult = 50 * dtPx;
        std::vector<qint64> testTimes = {
            123456789LL,
            mult - 1,
            mult,
            mult + 1
        };

        for (qint64 tLatest : testTimes) {
            auto testChart = makeChart(800, 300);
            testChart->setXAxisMode(StripChart::XAxisMode::Time);
            testChart->setTimeSpan(std::chrono::seconds(10));
            int testCh = testChart->addChannel(QStringLiteral("Ch0"), Qt::cyan);

            testChart->addSample(testCh, std::chrono::nanoseconds(tLatest), 42.0);
            renderToImage(*testChart);
            const auto &frame = StripChartTestAccess::lastFrame(*testChart);

            QCOMPARE(frame.tEnd % frame.dtPx, 0LL);
            QVERIFY2(frame.tStart <= tLatest, qPrintable(QString("tStart (%1) > tLatest (%2)").arg(frame.tStart).arg(tLatest)));
            QVERIFY2(tLatest < frame.tEnd, qPrintable(QString("tLatest (%1) >= tEnd (%2)").arg(tLatest).arg(frame.tEnd)));
            QCOMPARE(frame.tEnd - frame.tStart, testChart->timeSpan().count());
        }
    }

    // Part 2: Shift invariance (pixels)
    // Time mode, DecimationMode::Always, a stationary noisy signal at 100 kHz, W_dev = 1000, timeSpan = 1 s, dtPx = 1 ms
    {
        auto chart = makeChart(1054, 400, 0.0, 100.0);
        chart->setXAxisMode(StripChart::XAxisMode::Time);
        chart->setDecimationMode(StripChart::DecimationMode::Always);
        chart->setTimeSpan(std::chrono::seconds(1));
        chart->setGridVisible(false);
        chart->setLegendVisible(false);
        chart->setCapacity(200000);

        const QColor chColor(255, 0, 0);
        int ch = chart->addChannel(QStringLiteral("Noise"), chColor, 1.0);

        // Generate stationary pseudo-random signal at 100 kHz (10 µs = 10,000 ns spacing)
        // 100 samples per 1 ms bucket
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> dist(10.0, 90.0);

        // Initial render to discover exact dtPx and bucket width
        renderToImage(*chart);
        const auto &fInit = StripChartTestAccess::lastFrame(*chart);
        qint64 dtPx = fInit.dtPx;
        qreal dpr = chart->devicePixelRatioF();
        if (dpr != 1.0) {
            QSKIP("Pixel shift invariance test requires DPR 1");
        }
        int shiftCols = static_cast<int>(std::round(1.0 * dpr));

        // Fill initial samples up to 1 s + 20 % of bucket 1050
        qint64 dtSample = 10000LL; // 10 µs
        int samplesPerBucket = std::max(1, static_cast<int>(dtPx / dtSample));
        int totalInitialSamples = 1050 * samplesPerBucket + samplesPerBucket / 5;

        qint64 currentT = 0;
        for (int i = 0; i < totalInitialSamples; ++i) {
            chart->addSample(ch, std::chrono::nanoseconds(currentT), dist(rng));
            currentT += dtSample;
        }

        // 1. Render frame A
        QImage imgA = renderToImage(*chart);
        FrameDiagnostics diagA = StripChartTestAccess::lastFrame(*chart);
        int plotLeftDev = static_cast<int>(std::round(diagA.plotRect.left() * dpr));
        int plotRightDev = static_cast<int>(std::round(diagA.plotRect.right() * dpr));
        int plotTopDev = static_cast<int>(std::round(diagA.plotRect.top() * dpr));
        int plotBottomDev = static_cast<int>(std::round(diagA.plotRect.bottom() * dpr));

        // 2. Append samples so that tLatest advances within the same bucket (e.g. 20 % of bucket)
        int subBucketSamples = std::max(1, samplesPerBucket / 5);
        for (int i = 0; i < subBucketSamples; ++i) {
            chart->addSample(ch, std::chrono::nanoseconds(currentT), dist(rng));
            currentT += dtSample;
        }
        QImage imgB = renderToImage(*chart);
        FrameDiagnostics diagB = StripChartTestAccess::lastFrame(*chart);
        QCOMPARE(diagB.tEnd, diagA.tEnd); // must be in identical bucket!

        // Assert B is pixel-identical to A, except the rightmost columns affected by latest sample / glow dot
        for (int x = plotLeftDev; x < plotRightDev - 5 * shiftCols; ++x) {
            for (int y = plotTopDev; y <= plotBottomDev; ++y) {
                QCOMPARE(imgB.pixelColor(x, y), imgA.pixelColor(x, y));
            }
        }

        // 3. Append remaining samples to complete exactly one more bucket of data
        int remainingSamples = samplesPerBucket;
        for (int i = 0; i < remainingSamples; ++i) {
            chart->addSample(ch, std::chrono::nanoseconds(currentT), dist(rng));
            currentT += dtSample;
        }
        QImage imgC = renderToImage(*chart);
        FrameDiagnostics diagC = StripChartTestAccess::lastFrame(*chart);
        QCOMPARE(diagC.tEnd - diagA.tEnd, diagA.dtPx); // exactly 1 bucket shift!

        // Assert C shifted left by exactly 1 device column equals A's,
        // except the leftmost and rightmost columns
        for (int x = plotLeftDev + 2 * shiftCols; x < plotRightDev - 5 * shiftCols; ++x) {
            for (int y = plotTopDev; y <= plotBottomDev; ++y) {
                if (imgC.pixelColor(x, y) != imgA.pixelColor(x + shiftCols, y)) {
                    qWarning("Part 3 mismatch at x=%d y=%d", x, y);
                }
                QCOMPARE(imgC.pixelColor(x, y), imgA.pixelColor(x + shiftCols, y));
            }
        }
    }
}

void tst_StripChart::timeWindowCoverage()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setCapacity(10);
    chart.setTimeSpan(std::chrono::seconds(10)); // 10 seconds span
    int ch = chart.addChannel(QStringLiteral("Fast"), Qt::yellow);

    // Insert 10 samples spanning only 100 ms total
    std::vector<double> vals(10, 5.0);
    chart.addUniformSamples(ch, std::chrono::milliseconds(0), std::chrono::milliseconds(10), vals.data(), 10);

    // Buffer is full (10 samples) but covers only 100 ms, not 10 s -> isTimeWindowFullyCovered is false
    QVERIFY(!chart.isTimeWindowFullyCovered());
}

void tst_StripChart::bulkApiAndSignalEmission()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("Bulk"), Qt::cyan);

    QSignalSpy spy(&chart, &StripChart::dataAdded);

    std::vector<std::chrono::nanoseconds> t(100);
    std::vector<double> v(100, 1.0);
    for (size_t i = 0; i < 100; ++i) {
        t[i] = std::chrono::nanoseconds(i * 10);
    }

    // addSamples must emit dataAdded exactly once
    chart.addSamples(ch, t.data(), v.data(), 100);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(100));

    // addUniformSamples must emit dataAdded exactly once
    chart.addUniformSamples(ch, std::chrono::nanoseconds(1000), std::chrono::nanoseconds(10), v.data(), 50);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(150));
}

void tst_StripChart::legacyCompatibilityAndSlotPointer()
{
    StripChart chart;
    QCOMPARE(chart.xAxisMode(), StripChart::XAxisMode::SampleIndex);

    TestSignalSource src;
    int ch = chart.addChannel(QStringLiteral("Legacy"), Qt::white);

    // Pointer-to-member connect without qOverload must compile cleanly
    connect(&src, &TestSignalSource::pointReady, &chart, &StripChart::addDataPoint);

    src.sendPoint(ch, 77.7);
    QCOMPARE(chart.channelLatestValue(ch), 77.7);
    QCOMPARE(chart.channelSampleCount(ch), static_cast<qsizetype>(1));
}

void tst_StripChart::benchmarkHighRatePaint()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setDecimationMode(StripChart::DecimationMode::Always);
    chart.setCapacity(1000000);
    chart.setTimeSpan(std::chrono::seconds(10));

    constexpr int numChannels = 4;
    constexpr int samplesPerChannel = 1000000;
    std::vector<double> samples(samplesPerChannel);
    for (int i = 0; i < samplesPerChannel; ++i) {
        samples[i] = std::sin(i * 0.001) * 50.0;
    }

    for (int c = 0; c < numChannels; ++c) {
        int ch = chart.addChannel(QStringLiteral("Ch%1").arg(c), Qt::cyan);
        chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(10), samples.data(), samplesPerChannel);
    }

    chart.resize(1920, 1080);
    QPixmap pix(chart.size());

    QBENCHMARK {
        chart.render(&pix);
    }
}

void tst_StripChart::computeIndexBucketsGrid()
{
    struct TestCase {
        int capacity;
        int widthDev;
        quint64 totalSamples;
        size_t count;
    };

    std::vector<TestCase> cases = {
        {2500, 1000, 2500, 2500},            // capacity = 2500, W = 1000
        {2000, 1000, 2000, 2000},            // capacity an exact multiple of W
        {2001, 1000, 2001, 2001},            // capacity = 2W + 1
        {2500, 1000, 500, 500},              // totalSamples < capacity
        {2500, 1000, 100003, 2500},          // totalSamples >> capacity with non-aligned start
        {300, 800, 300, 300},                // capacity < W
        {10000, 1000, 50000, 10000}          // large capacity
    };

    for (const auto &tc : cases) {
        auto ib = internal::computeIndexBuckets(tc.totalSamples, tc.count, tc.capacity, tc.widthDev);
        QVERIFY(ib.samplesPerBucket >= 1);
        QVERIFY(ib.numBuckets <= tc.widthDev + 1);

        quint64 startCounter = (tc.totalSamples >= tc.count) ? (tc.totalSamples - tc.count) : 0;
        for (size_t j = 0; j < tc.count; ++j) {
            qint64 idx = static_cast<qint64>(startCounter + j);
            qint64 bucket = internal::M4Decimator::floorDiv(idx, ib.samplesPerBucket);
            QVERIFY2(bucket >= ib.kStart, qPrintable(QString("bucket %1 < kStart %2").arg(bucket).arg(ib.kStart)));
            QVERIFY2(bucket < ib.kStart + ib.numBuckets,
                     qPrintable(QString("bucket %1 >= kStart+numBuckets %2 in case cap=%3, W=%4, total=%5, count=%6, spb=%7, numB=%8")
                               .arg(bucket).arg(ib.kStart + ib.numBuckets).arg(tc.capacity).arg(tc.widthDev).arg(tc.totalSamples).arg(tc.count).arg(ib.samplesPerBucket).arg(ib.numBuckets)));
        }
    }
}

void tst_StripChart::sampleIndexDecimationRecentSamples()
{
    auto testMode = [](StripChart::DecimationMode mode) -> double {
        StripChart chart;
        chart.setGridVisible(false);
        chart.setLegendVisible(false);
        chart.setYRange(0.0, 10.0);
        chart.setCapacity(2500);
        chart.setDecimationMode(mode);

        // Compute widget dimensions for W_dev == 1000 with DPR == 1.0
        // plotArea formula: leftMargin = 42.0, rightMargin = 12.0 -> width = 1000 + 54 = 1054
        // topMargin = 12.0 (legend off), bottomMargin = 16.0 (SampleIndex mode) -> height = 400
        constexpr double pLeft = 42.0;
        constexpr double pTop = 12.0;
        constexpr double pWidth = 1000.0;
        chart.resize(1054, 400);

        int ch = chart.addChannel(QStringLiteral("TestTrace"), Qt::red, 1.0);
        for (int i = 0; i < 1900; ++i) {
            chart.addDataPoint(ch, 0.0);
        }
        for (int i = 0; i < 600; ++i) {
            chart.addDataPoint(ch, 10.0);
        }

        QImage img(chart.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        chart.render(&img);

        // Sample 1900 x position: frac = 1900 / 2499
        double frac1900 = 1900.0 / 2499.0;
        double x1900 = pLeft + frac1900 * pWidth;
        int xStart = static_cast<int>(std::ceil(x1900));
        int xEnd = static_cast<int>(std::floor(pLeft + pWidth));

        int totalColumns = xEnd - xStart + 1;
        int matchedColumns = 0;

        // Top band of the plot: value 10.0 maps to pTop
        int yTopMin = static_cast<int>(pTop);
        int yTopMax = static_cast<int>(pTop + 10.0);

        for (int x = xStart; x <= xEnd; ++x) {
            bool foundTrace = false;
            for (int y = yTopMin; y <= yTopMax; ++y) {
                QRgb px = img.pixel(x, y);
                if (qRed(px) > 60 && qRed(px) > qGreen(px) + 30 && qRed(px) > qBlue(px) + 30) {
                    foundTrace = true;
                    break;
                }
            }
            if (foundTrace) {
                matchedColumns++;
            }
        }

        return static_cast<double>(matchedColumns) / static_cast<double>(totalColumns);
    };

    double ratioOff = testMode(StripChart::DecimationMode::Off);
    QVERIFY2(ratioOff >= 0.95, qPrintable(QString("DecimationMode::Off ratio %1 < 0.95").arg(ratioOff)));

    double ratioAlways = testMode(StripChart::DecimationMode::Always);
    QVERIFY2(ratioAlways >= 0.95, qPrintable(QString("DecimationMode::Always ratio %1 < 0.95").arg(ratioAlways)));
}

void tst_StripChart::computeTimeWindowBoundaries()
{
    struct Config {
        qint64 spanNs;
        int widthDev;
    };

    std::vector<Config> configs = {
        {10000000000LL, 1000}, // 10 s, W=1000 -> dtPx = 10 ms (exact multiple)
        {10000000000LL, 800},  // 10 s, W=800 -> dtPx = 12.5 ms
        {7000000000LL, 1000},  // 7 s, W=1000 -> dtPx = 7 ms
        {1000000007LL, 1000},  // span not a multiple of W_dev: dtPx = 1 ms
        {1000000LL, 500}       // 1 ms, W=500 -> dtPx = 2000 ns
    };

    for (const auto &cfg : configs) {
        qint64 dtPx = std::max(qint64(1), cfg.spanNs / cfg.widthDev);

        std::vector<qint64> testValues = {
            0,
            dtPx - 1,
            dtPx,
            10 * dtPx,
            10 * dtPx + 1,
            1000000000000000000LL, // ~10^18
            -100000000LL           // negative timestamp
        };

        for (qint64 tLatest : testValues) {
            auto win = internal::computeTimeWindow(tLatest, cfg.spanNs, cfg.widthDev);

            QVERIFY2(win.tEnd % win.dtPx == 0,
                     qPrintable(QString("tEnd %1 not multiple of dtPx %2 (tLatest=%3)").arg(win.tEnd).arg(win.dtPx).arg(tLatest)));
            QVERIFY2(win.tStart <= tLatest && tLatest < win.tEnd,
                     qPrintable(QString("tLatest %1 not in [tStart %2, tEnd %3) (dtPx=%4)").arg(tLatest).arg(win.tStart).arg(win.tEnd).arg(win.dtPx)));
            QCOMPARE(win.tEnd - win.tStart, cfg.spanNs);

            qint64 kLatest = internal::M4Decimator::floorDiv(tLatest, win.dtPx);
            QVERIFY2(kLatest >= win.kStart && kLatest < win.kStart + win.numBuckets,
                     qPrintable(QString("kLatest %1 not in [kStart %2, %3) (tLatest=%4)").arg(kLatest).arg(win.kStart).arg(win.kStart + win.numBuckets).arg(tLatest)));
            QVERIFY2(win.numBuckets <= cfg.widthDev + 1,
                     qPrintable(QString("numBuckets %1 > W_dev+1 %2").arg(win.numBuckets).arg(cfg.widthDev + 1)));
        }
    }
}

void tst_StripChart::timeBucketBoundarySpikePreservation()
{
    auto testSpike = [](StripChart::DecimationMode mode, qint64 spikeTimeNs) -> bool {
        StripChart chart;
        chart.setXAxisMode(StripChart::XAxisMode::Time);
        chart.setTimeSpan(std::chrono::seconds(1)); // 1 s span
        chart.setGapThreshold(std::chrono::seconds(10));
        chart.setCapacity(25000);
        chart.setDecimationMode(mode);
        chart.setGridVisible(false);
        chart.setLegendVisible(false);
        chart.setYRange(0.0, 10.0);

        // Widget sizing for W_dev == 1000 (DPR 1.0)
        // Time mode: leftMargin = 42.0, rightMargin = 12.0 -> width = 1054
        // topMargin = 12.0, bottomMargin = 26.0 -> height = 400
        chart.resize(1054, 400);

        int ch = chart.addChannel(QStringLiteral("SpikeChannel"), Qt::red, 1.0);

        // Feed 20 000 samples of 0.0 at 100 us steps from t = 0
        std::vector<std::chrono::nanoseconds> t(20000);
        std::vector<double> v(20000, 0.0);
        for (int i = 0; i < 20000; ++i) {
            t[i] = std::chrono::nanoseconds(static_cast<qint64>(i) * 100000LL); // 100 us
        }
        chart.addSamples(ch, t.data(), v.data(), 20000);

        // Spike of 10.0 at spikeTimeNs
        chart.addSample(ch, std::chrono::nanoseconds(spikeTimeNs), 10.0);

        QImage img(chart.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        chart.render(&img);

        constexpr double pLeft = 42.0;
        constexpr double pTop = 12.0;
        constexpr double pWidth = 1000.0;

        int xEnd = static_cast<int>(std::floor(pLeft + pWidth));
        int xStart = xEnd - 5;
        int yTopMin = static_cast<int>(pTop);
        int yTopMax = static_cast<int>(pTop + 10.0);

        bool foundSpike = false;
        for (int x = xStart; x <= xEnd; ++x) {
            for (int y = yTopMin; y <= yTopMax; ++y) {
                QRgb px = img.pixel(x, y);
                if (qRed(px) > 60 && qRed(px) > qGreen(px) + 30 && qRed(px) > qBlue(px) + 30) {
                    foundSpike = true;
                    break;
                }
            }
            if (foundSpike) break;
        }
        return foundSpike;
    };

    // Control case: spike at t = 2s + 1ns should pass even on older code
    bool controlOff = testSpike(StripChart::DecimationMode::Off, 2000000001LL);
    QVERIFY(controlOff);
    bool controlAlways = testSpike(StripChart::DecimationMode::Always, 2000000001LL);
    QVERIFY(controlAlways);

    // Defect case: spike at exactly t = 2s (exact multiple of dtPx = 1ms)
    bool spikeAlways = testSpike(StripChart::DecimationMode::Always, 2000000000LL);
    QVERIFY2(spikeAlways, "Spike at exact bucket boundary t=2.0s was dropped in DecimationMode::Always");

    bool spikeOff = testSpike(StripChart::DecimationMode::Off, 2000000000LL);
    QVERIFY2(spikeOff, "Spike at exact bucket boundary t=2.0s was dropped in DecimationMode::Off");
}

void tst_StripChart::timeWindowCoverageConsistency()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setTimeSpan(std::chrono::seconds(1)); // 1 s span
    chart.setCapacity(1000);
    int ch = chart.addChannel(QStringLiteral("Trace"), Qt::yellow);

    chart.resize(1054, 400); // W_dev = 1000 -> dtPx = 1 ms

    std::vector<std::chrono::nanoseconds> t(1000);
    std::vector<double> v(1000, 1.0);
    for (int i = 0; i < 1000; ++i) {
        t[i] = std::chrono::nanoseconds(1000500000LL + static_cast<qint64>(i) * 1000500LL);
    }
    t[999] = std::chrono::nanoseconds(2000000000LL);
    chart.addSamples(ch, t.data(), v.data(), 1000);

    QVERIFY(chart.isTimeWindowFullyCovered());
}

void tst_StripChart::axisModeSwitchClearing()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    int ch = chart.addChannel(QStringLiteral("V"), Qt::red);

    for (int i = 0; i < 5; ++i) {
        chart.addDataPoint(ch, 10.0 + i);
    }
    QCOMPARE(chart.channelSampleCount(ch), 5);
    quint64 rejectedBefore = chart.rejectedSampleCount(ch);

    // Switch to SampleIndex
    chart.setXAxisMode(StripChart::XAxisMode::SampleIndex);
    // After switch, data must be cleared
    QCOMPARE(chart.channelSampleCount(ch), 0);

    for (int i = 0; i < 5; ++i) {
        chart.addDataPoint(ch, 100.0 + i);
    }
    // New batch accepted, not rejected!
    QCOMPARE(chart.channelSampleCount(ch), 5);
    QCOMPARE(chart.rejectedSampleCount(ch), rejectedBefore);
    QCOMPARE(chart.channelLatestValue(ch), 104.0);
}

void tst_StripChart::sampleIndexExplicitTimestampIgnored()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::SampleIndex);
    int ch = chart.addChannel(QStringLiteral("Sig"), Qt::blue);

    // addSample with huge timestamp 100 s
    chart.addSample(ch, std::chrono::seconds(100), 1.0);
    // addDataPoint with sample index (totalSamples = 1)
    chart.addDataPoint(ch, 2.0);

    // Both must be accepted, count 2, 0 rejections
    QCOMPARE(chart.channelSampleCount(ch), 2);
    QCOMPARE(chart.rejectedSampleCount(ch), 0ULL);
    QCOMPARE(chart.channelLatestValue(ch), 2.0);
}

void tst_StripChart::axisModeRoundTrip()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("PreservedName"), Qt::magenta, 2.5);

    QSignalSpy spy(&chart, &StripChart::xAxisModeChanged);

    chart.addDataPoint(ch, 50.0);
    QCOMPARE(chart.channelSampleCount(ch), 1);

    // SampleIndex -> Time
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    QCOMPARE(chart.channelSampleCount(ch), 0);
    QCOMPARE(chart.channelName(ch), QStringLiteral("PreservedName"));
    QCOMPARE(chart.channelColor(ch), QColor(Qt::magenta));
    QCOMPARE(chart.channelPenWidth(ch), 2.5);
    QVERIFY(chart.isChannelVisible(ch));

    chart.addSample(ch, std::chrono::milliseconds(500), 75.0);
    QCOMPARE(chart.channelSampleCount(ch), 1);

    // Time -> SampleIndex
    chart.setXAxisMode(StripChart::XAxisMode::SampleIndex);
    QCOMPARE(chart.channelSampleCount(ch), 0);
    QCOMPARE(chart.channelName(ch), QStringLiteral("PreservedName"));
    QCOMPARE(chart.channelColor(ch), QColor(Qt::magenta));

    QCOMPARE(spy.count(), 2);
}

void tst_StripChart::sameAxisModePreservesData()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("Data"), Qt::cyan);
    chart.addDataPoint(ch, 10.0);
    chart.addDataPoint(ch, 20.0);
    QCOMPARE(chart.channelSampleCount(ch), 2);

    QSignalSpy spy(&chart, &StripChart::xAxisModeChanged);

    // Setting same mode again
    chart.setXAxisMode(StripChart::XAxisMode::SampleIndex);
    QCOMPARE(chart.channelSampleCount(ch), 2);
    QCOMPARE(chart.channelLatestValue(ch), 20.0);
    QCOMPARE(spy.count(), 0);
}

void tst_StripChart::addUniformSamplesSampleIndexMode()
{
    StripChart chart;
    chart.setCapacity(2000);
    chart.setXAxisMode(StripChart::XAxisMode::SampleIndex);
    int ch = chart.addChannel(QStringLiteral("Uniform"), Qt::yellow);

    std::vector<double> vals(1000);
    for (int i = 0; i < 1000; ++i) {
        vals[i] = static_cast<double>(i * 2);
    }

    chart.addUniformSamples(ch, std::chrono::nanoseconds(1000000), std::chrono::nanoseconds(1000), vals.data(), 1000);

    QCOMPARE(chart.channelSampleCount(ch), 1000);
    QCOMPARE(chart.rejectedSampleCount(ch), 0ULL);
    QCOMPARE(chart.channelLatestValue(ch), 1998.0);
}

void tst_StripChart::autoDecimationThreshold()
{
    auto measureLineThickness = [](StripChart::XAxisMode axisMode, int sampleCount) -> int {
        StripChart chart;
        chart.setXAxisMode(axisMode);
        chart.setDecimationMode(StripChart::DecimationMode::Auto);
        chart.setGridVisible(false);
        chart.setLegendVisible(false);
        chart.setYRange(0.0, 10.0);
        chart.setCapacity(sampleCount + 100);
        if (axisMode == StripChart::XAxisMode::Time) {
            chart.setTimeSpan(std::chrono::seconds(10));
        }

        // Plot width = 1000 px, DPR = 1.0 (margins: left 42, right 12 -> 1054)
        chart.resize(1054, 400);

        int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::red, 7.0);

        if (axisMode == StripChart::XAxisMode::Time) {
            qint64 dtNs = 10000000000LL / sampleCount;
            std::vector<double> vals(sampleCount, 5.0);
            chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs), vals.data(), sampleCount);
        } else {
            for (int i = 0; i < sampleCount; ++i) {
                chart.addDataPoint(ch, 5.0);
            }
        }

        QImage img(chart.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        chart.render(&img);

        int x = 500;
        int coloredPixels = 0;
        for (int y = 0; y < img.height(); ++y) {
            QRgb px = img.pixel(x, y);
            if (qRed(px) > 100) {
                coloredPixels++;
            }
        }
        return coloredPixels;
    };

    StripChart dummy;
    dummy.resize(1054, 400);
    int wDev = std::max(1, static_cast<int>(std::round(StripChartTestAccess::plotArea(dummy).width() * dummy.devicePixelRatioF())));

    int thickTimeOff = measureLineThickness(StripChart::XAxisMode::Time, wDev);
    int thickTimeOn = measureLineThickness(StripChart::XAxisMode::Time, wDev + 1);
    QVERIFY2(thickTimeOff >= 5, qPrintable(QString("Time mode with %1 samples should be raw (thick >= 5), got %2").arg(wDev).arg(thickTimeOff)));
    QVERIFY2(thickTimeOn <= 2, qPrintable(QString("Time mode with %1 samples should be decimated (cosmetic 1px <= 2), got %2").arg(wDev + 1).arg(thickTimeOn)));

    int thickIndexOff = measureLineThickness(StripChart::XAxisMode::SampleIndex, wDev);
    int thickIndexOn = measureLineThickness(StripChart::XAxisMode::SampleIndex, wDev + 1);
    QVERIFY2(thickIndexOff >= 5, qPrintable(QString("SampleIndex mode with %1 samples should be raw (thick >= 5), got %2").arg(wDev).arg(thickIndexOff)));
    QVERIFY2(thickIndexOn <= 2, qPrintable(QString("SampleIndex mode with %1 samples should be decimated (cosmetic 1px <= 2), got %2").arg(wDev + 1).arg(thickIndexOn)));
}

void tst_StripChart::evictionConsistency()
{
    auto testLeftmostColumn = [](StripChart::XAxisMode mode) {
        StripChart chart;
        chart.setXAxisMode(mode);
        chart.setGridVisible(false);
        chart.setLegendVisible(false);
        chart.setYRange(0.0, 100.0);
        chart.setCapacity(400); // capacity < window
        if (mode == StripChart::XAxisMode::Time) {
            chart.setTimeSpan(std::chrono::seconds(10));
        }

        // Plot width = 1000 px, DPR = 1.0 (margins: left 42, right 12 -> 1054)
        chart.resize(1054, 400);
        int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::red, 1.0);

        // Add 600 samples (> capacity 400)
        if (mode == StripChart::XAxisMode::Time) {
            for (int i = 0; i < 600; ++i) {
                chart.addSample(ch, std::chrono::nanoseconds(i * 10000000LL), 50.0 + 30.0 * std::sin(i * 0.1));
            }
        } else {
            for (int i = 0; i < 600; ++i) {
                chart.addDataPoint(ch, 50.0 + 30.0 * std::sin(i * 0.1));
            }
        }

        auto isRedPixel = [](QRgb rgb) -> bool {
            return qRed(rgb) > 180 && qGreen(rgb) < 80 && qBlue(rgb) < 80;
        };

        auto findLeftmostX = [&](StripChart::DecimationMode decMode) -> int {
            chart.setDecimationMode(decMode);
            QImage img(chart.size(), QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::black);
            chart.render(&img);

            for (int x = 0; x < img.width(); ++x) {
                for (int y = 0; y < img.height(); ++y) {
                    if (isRedPixel(img.pixel(x, y))) {
                        return x;
                    }
                }
            }
            return -1;
        };

        int xOff = findLeftmostX(StripChart::DecimationMode::Off);
        int xAlways = findLeftmostX(StripChart::DecimationMode::Always);

        QVERIFY(xOff > 0);
        QVERIFY(xAlways > 0);
        QVERIFY2(std::abs(xOff - xAlways) <= 1,
                 qPrintable(QString("Leftmost column mismatch for mode %1: Off=%2, Always=%3")
                                .arg(static_cast<int>(mode))
                                .arg(xOff)
                                .arg(xAlways)));
    };

    testLeftmostColumn(StripChart::XAxisMode::Time);
    testLeftmostColumn(StripChart::XAxisMode::SampleIndex);
}

void tst_StripChart::widgetM4Equivalence()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setTimeSpan(std::chrono::seconds(10));
    chart.setGridVisible(false);
    chart.setLegendVisible(false);
    chart.setYRange(0.0, 100.0);
    chart.setCapacity(60000);

    // 1054 x 400 -> plot width = 1000 px
    chart.resize(1054, 400);
    int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::red, 1.0);

    // Enable cosmetic 1px unaliased pen for raw path in this test
    internal::StripChartTestAccess::setRawPenCosmetic1px(chart, true);

    // 200,000 samples over 1000 pixels (200 samples/pixel) with broadband noise
    const int N = 200000;
    std::vector<std::chrono::nanoseconds> timestamps(N);
    std::vector<double> values(N);
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> dist(10.0, 90.0);

    for (int i = 0; i < N; ++i) {
        double tSec = i * 10.0 / N;
        timestamps[i] = std::chrono::nanoseconds(static_cast<qint64>(tSec * 1e9));
        values[i] = dist(rng);
    }
    // Specific spikes to verify extreme preservation
    values[80000] = 98.0;
    values[140000] = 2.0;

    chart.setCapacity(250000);
    chart.addSamples(ch, timestamps.data(), values.data(), N);

    // Render with DecimationMode::Off
    chart.setDecimationMode(StripChart::DecimationMode::Off);
    QImage imgOff(chart.size(), QImage::Format_ARGB32_Premultiplied);
    imgOff.fill(Qt::black);
    chart.render(&imgOff);

    // Render with DecimationMode::Always
    chart.setDecimationMode(StripChart::DecimationMode::Always);
    QImage imgAlways(chart.size(), QImage::Format_ARGB32_Premultiplied);
    imgAlways.fill(Qt::black);
    chart.render(&imgAlways);

    auto isRedPixel = [](QRgb rgb) -> bool {
        return qRed(rgb) > 180 && qGreen(rgb) < 80 && qBlue(rgb) < 80;
    };

    // Compare first exact equality
    if (imgOff == imgAlways) {
        QCOMPARE(imgOff, imgAlways);
    } else {
        // Antialiasing difference on raw vs 1px cosmetic pen:
        // Compare vertical pixel extents per column in the plot area (x in [42, 1041])
        int maxDiffPerCol = 0;
        int diffCols = 0;
        int activeCols = 0;

        for (int x = 42; x < 1042; ++x) {
            int minYOff = -1, maxYOff = -1;
            int minYAlways = -1, maxYAlways = -1;

            for (int y = 0; y < chart.height(); ++y) {
                if (isRedPixel(imgOff.pixel(x, y))) {
                    if (minYOff < 0) minYOff = y;
                    maxYOff = y;
                }
                if (isRedPixel(imgAlways.pixel(x, y))) {
                    if (minYAlways < 0) minYAlways = y;
                    maxYAlways = y;
                }
            }

            if (minYOff >= 0 && minYAlways >= 0) {
                activeCols++;
                int dMin = std::abs(minYOff - minYAlways);
                int dMax = std::abs(maxYOff - maxYAlways);
                int colDiff = std::max(dMin, dMax);
                if (colDiff > maxDiffPerCol) {
                    maxDiffPerCol = colDiff;
                }
                if (colDiff > 0) {
                    diffCols++;
                }
            }
        }

        qInfo() << "M4 widget equivalence: activeCols:" << activeCols
                << "diffCols:" << diffCols << "maxDiffPerCol:" << maxDiffPerCol;
        QVERIFY(activeCols > 900);
        QVERIFY2(maxDiffPerCol <= 1,
                 qPrintable(QString("M4 widget equivalence: max diff per col %1 > 1 px").arg(maxDiffPerCol)));
        QVERIFY2(diffCols <= (activeCols / 100),
                 qPrintable(QString("M4 widget equivalence: differing cols %1 exceeds 1% of %2").arg(diffCols).arg(activeCols)));
    }
}

void tst_StripChart::lazyAutoscale()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setAutoScaleY(true);
    int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);

    QSignalSpy spy(&chart, &StripChart::yRangeChanged);

    // After 1000 addSample calls with autoscale on, spy shows 0 emissions
    for (int i = 0; i < 1000; ++i) {
        chart.addSample(ch, std::chrono::nanoseconds((i + 1) * 1000000LL), 10.0 + (i % 20));
    }
    QCOMPARE(spy.count(), 0);

    // When yMinimum() is called, range is recomputed and yRangeChanged is emitted at most 1 time
    double yMin = chart.yMinimum();
    QVERIFY(yMin < 10.0);
    QCOMPARE(spy.count(), 1);

    // Subsequent call without new data does not emit again
    QCOMPARE(chart.yMaximum(), chart.yMaximum());
    QCOMPARE(spy.count(), 1);
}

void tst_StripChart::hysteresis()
{
    StripChart chart;
    chart.resize(1000, 500);
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setTimeSpan(std::chrono::seconds(1)); // 1 s visible window
    chart.setAutoScaleY(true);
    int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::yellow);

    // Initial base sample and trigger autoscale
    chart.addSample(ch, std::chrono::nanoseconds(1000000LL), 10.0);
    (void)chart.yMaximum(); // initializes autoscale

    // A spike grows the range immediately
    chart.addSample(ch, std::chrono::nanoseconds(2000000LL), 200.0);
    // Add sample immediately after spike so spike is not the boundary sample later
    chart.addSample(ch, std::chrono::nanoseconds(3000000LL), 10.0);
    double maxAfterSpike = chart.yMaximum();
    QVERIFY(maxAfterSpike >= 200.0);

    // Move the spike out of the visible window
    // visible window will be [1.5 s, 2.5 s], spike was at 2 ms (0.002 s)
    qint64 tBase = 2500000000LL; // 2.5 seconds
    chart.addSample(ch, std::chrono::nanoseconds(tBase - 500000000LL), 10.0); // 2.0 s

    // 29 consecutive recomputations: range must not shrink
    for (int i = 1; i <= 29; ++i) {
        chart.addSample(ch, std::chrono::nanoseconds(tBase + i * 1000000LL), 10.0);
        QCOMPARE(chart.yMaximum(), maxAfterSpike);
    }

    // 30th recomputation: range must shrink
    chart.addSample(ch, std::chrono::nanoseconds(tBase + 30 * 1000000LL), 10.0);
    double maxAfterHysteresis = chart.yMaximum();
    QVERIFY2(maxAfterHysteresis < maxAfterSpike,
             qPrintable(QString("Expected shrink after 30 frames: got %1 vs %2").arg(maxAfterHysteresis).arg(maxAfterSpike)));
    QVERIFY(maxAfterHysteresis < 50.0);
}

// ---------------------------------------------------------------------------------------------
// Sensor dropouts in decimated traces: a decimation bucket that contains a non-finite sample is
// not drawn, so its pixel column stays blank and the trace is broken on both sides.
// ---------------------------------------------------------------------------------------------
namespace {

const QColor kDropoutColour(255, 0, 0);

double smoothValue(qint64 i, qint64 n)
{
    return 50.0 + 30.0 * std::sin(6.283185307179586 * static_cast<double>(i) / static_cast<double>(n));
}

std::unique_ptr<StripChart> makeDropoutChart(StripChart::XAxisMode mode, int widgetWidth, qint64 spanNs, int capacity)
{
    auto chart = makeChart(widgetWidth, 300, 0.0, 100.0);
    chart->setXAxisMode(mode);
    if (mode == StripChart::XAxisMode::Time) {
        chart->setTimeSpan(std::chrono::nanoseconds(spanNs));
    }
    chart->setCapacity(capacity);
    chart->setDecimationMode(StripChart::DecimationMode::Always);
    chart->setGridVisible(false);
    chart->setLegendVisible(false);
    return chart;
}

// Appends samples with absolute indices [first, first + count): a smooth signal (period `shapeN`
// samples), with NaN at the indices listed in `nanIndices`. Time mode uses t = index * sampleDtNs.
void appendDropoutSamples(StripChart &chart, int ch, qint64 first, qint64 count, qint64 sampleDtNs,
                          qint64 shapeN, const std::vector<qint64> &nanIndices)
{
    std::vector<double> values(static_cast<size_t>(count));
    for (qint64 j = 0; j < count; ++j) {
        values[static_cast<size_t>(j)] = smoothValue(first + j, shapeN);
    }
    for (qint64 n : nanIndices) {
        if (n >= first && n < first + count) {
            values[static_cast<size_t>(n - first)] = std::numeric_limits<double>::quiet_NaN();
        }
    }
    if (chart.xAxisMode() == StripChart::XAxisMode::Time) {
        chart.addUniformSamples(ch, std::chrono::nanoseconds(first * sampleDtNs),
                                std::chrono::nanoseconds(sampleDtNs), values.data(), count);
    } else {
        for (double v : values) {
            chart.addDataPoint(ch, v);
        }
    }
}

// Decimation bucket that holds the sample with absolute index `index` in the last frame.
qint64 bucketOfSample(const StripChart &chart, qint64 index, qint64 sampleDtNs)
{
    const auto &f = StripChartTestAccess::lastFrame(chart);
    if (chart.xAxisMode() == StripChart::XAxisMode::Time) {
        return internal::M4Decimator::floorDiv(index * sampleDtNs, f.dtPx);
    }
    return internal::M4Decimator::floorDiv(index, f.samplesPerBucket);
}

// Logical x range [x0, x1) of decimation bucket k in the last frame, computed with the same
// formula as the paint code. `firstRetained` is the absolute index of the oldest retained sample
// (SampleIndex mode positions samples relative to it).
std::pair<double, double> bucketXRange(const StripChart &chart, qint64 k, qint64 firstRetained = 0)
{
    const auto &f = StripChartTestAccess::lastFrame(chart);
    if (chart.xAxisMode() == StripChart::XAxisMode::Time) {
        return {mapT(chart, k * f.dtPx), mapT(chart, (k + 1) * f.dtPx)};
    }
    auto xOf = [&](qint64 i) {
        double frac = static_cast<double>(i - firstRetained) / (chart.capacity() - 1);
        return f.plotRect.left() + frac * f.plotRect.width();
    };
    return {xOf(k * f.samplesPerBucket), xOf((k + 1) * f.samplesPerBucket)};
}

// True when both bucket edges fall on device pixel boundaries, so the bucket covers exactly one
// device column.
bool bucketAlignedToDeviceColumn(double x0, double x1, double dpr)
{
    const double a = x0 * dpr;
    const double b = x1 * dpr;
    return std::abs(a - std::round(a)) < 0.01 && std::abs(b - std::round(b)) < 0.01
        && std::round(b) - std::round(a) == 1.0;
}

// Device column of a bucket: the column that contains the bucket's centre.
int bucketColumn(double x0, double x1, double dpr)
{
    return static_cast<int>(std::floor((x0 + x1) * 0.5 * dpr));
}

int verticesInRange(const ChannelFrameDiagnostics &d, double x0, double x1)
{
    int n = 0;
    for (const auto &seg : d.segments) {
        for (const auto &pt : seg) {
            if (pt.x() >= x0 && pt.x() < x1) {
                ++n;
            }
        }
    }
    return n;
}

bool columnHasTrace(const QImage &img, const StripChart &chart, int xDev)
{
    const auto &f = StripChartTestAccess::lastFrame(chart);
    const double dpr = chart.devicePixelRatioF();
    const int top = static_cast<int>(std::floor(f.plotRect.top() * dpr));
    const int bottom = static_cast<int>(std::ceil(f.plotRect.bottom() * dpr));
    return !traceYAtColumn(img, kDropoutColour, xDev, top, bottom, 60).empty();
}

} // namespace

void tst_StripChart::decimatedSingleDropout_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::addColumn<int>("widgetWidth");
    QTest::addColumn<qint64>("spanNs");
    QTest::addColumn<qint64>("sampleDtNs");
    QTest::addColumn<int>("sampleCount");
    QTest::addColumn<qint64>("nanIndex");
    QTest::addColumn<bool>("aligned");

    const int time = static_cast<int>(StripChart::XAxisMode::Time);
    const int index = static_cast<int>(StripChart::XAxisMode::SampleIndex);

    // 1000 px plot: 1 s span and 1M samples give buckets on device pixel boundaries (DPR 1 and 2).
    QTest::newRow("Time, aligned") << time << 1054 << qint64(1000000000) << qint64(1000) << 1000000 << qint64(500300) << true;
    QTest::newRow("SampleIndex, aligned") << index << 1054 << qint64(0) << qint64(0) << 1000000 << qint64(500300) << true;
    // Geometries where bucket edges fall 0.25-0.75 px inside a device column (DPR 1 and 2).
    QTest::newRow("Time, misaligned") << time << 1974 << qint64(1000000) << qint64(10) << 100000 << qint64(55362) << false;
    QTest::newRow("SampleIndex, misaligned") << index << 1000 << qint64(0) << qint64(0) << 1000000 << qint64(400268) << false;
}

void tst_StripChart::decimatedSingleDropout()
{
    QFETCH(int, axisMode);
    QFETCH(int, widgetWidth);
    QFETCH(qint64, spanNs);
    QFETCH(qint64, sampleDtNs);
    QFETCH(int, sampleCount);
    QFETCH(qint64, nanIndex);
    QFETCH(bool, aligned);

    WarningGuard guard;
    auto chart = makeDropoutChart(static_cast<StripChart::XAxisMode>(axisMode), widgetWidth, spanNs, sampleCount);
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, sampleCount, sampleDtNs, sampleCount, {nanIndex});

    const QImage img = renderToImage(*chart);
    const auto &frame = StripChartTestAccess::lastFrame(*chart);
    const double dpr = chart->devicePixelRatioF();
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
    QCOMPARE(frame.channels.size(), 1UL);
    const auto &chDiag = frame.channels[0];
    QCOMPARE(chDiag.path, RenderPath::Decimated);

    const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
    const auto [x0, x1] = bucketXRange(*chart, k);
    // The row really has the geometry it claims
    QCOMPARE(bucketAlignedToDeviceColumn(x0, x1, dpr), aligned);

    // Hook: no vertex in the NaN bucket, trace split into exactly two segments around it
    QCOMPARE(verticesInRange(chDiag, x0, x1), 0);
    QCOMPARE(chDiag.segments.size(), 2UL);
    QVERIFY(chDiag.segments[0].last().x() < x0);
    QVERIFY(chDiag.segments[1].first().x() >= x1);

    // Pixels: the bucket's column is blank, its neighbours are drawn
    const int col = bucketColumn(x0, x1, dpr);
    QVERIFY2(columnHasTrace(img, *chart, col - 1), qPrintable(QStringLiteral("no trace left of col %1").arg(col)));
    QVERIFY2(columnHasTrace(img, *chart, col + 1), qPrintable(QStringLiteral("no trace right of col %1").arg(col)));
    QVERIFY2(!columnHasTrace(img, *chart, col),
             qPrintable(QStringLiteral("trace pixel in NaN column %1 (bucket x %2-%3 px)").arg(col).arg(x0 * dpr).arg(x1 * dpr)));
}

void tst_StripChart::decimatedDropoutAtWindowEdges_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::addColumn<bool>("lastBucket");

    const int time = static_cast<int>(StripChart::XAxisMode::Time);
    const int index = static_cast<int>(StripChart::XAxisMode::SampleIndex);
    QTest::newRow("Time, first visible bucket") << time << false;
    QTest::newRow("Time, last (active) bucket") << time << true;
    QTest::newRow("SampleIndex, first visible bucket") << index << false;
    QTest::newRow("SampleIndex, last (active) bucket") << index << true;
}

void tst_StripChart::decimatedDropoutAtWindowEdges()
{
    QFETCH(int, axisMode);
    QFETCH(bool, lastBucket);

    // Aligned geometry: 1000 px plot, 1 s span, 1M samples at 1 us
    constexpr int n = 1000000;
    constexpr qint64 sampleDtNs = 1000;
    const qint64 nanIndex = lastBucket ? 999700 : 300;

    WarningGuard guard;
    auto chart = makeDropoutChart(static_cast<StripChart::XAxisMode>(axisMode), 1054, 1000000000, n);
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, n, sampleDtNs, n, {nanIndex});

    const QImage img = renderToImage(*chart);
    const auto &frame = StripChartTestAccess::lastFrame(*chart);
    const double dpr = chart->devicePixelRatioF();
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
    const auto &chDiag = frame.channels[0];
    QCOMPARE(chDiag.path, RenderPath::Decimated);

    const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
    QCOMPARE(k, lastBucket ? frame.kStart + frame.numBuckets - 1 : frame.kStart);
    const auto [x0, x1] = bucketXRange(*chart, k);
    QVERIFY(bucketAlignedToDeviceColumn(x0, x1, dpr));

    QCOMPARE(verticesInRange(chDiag, x0, x1), 0);
    QCOMPARE(chDiag.segments.size(), 1UL);
    if (lastBucket) {
        QVERIFY(chDiag.segments[0].last().x() < x0);
    } else {
        QVERIFY(chDiag.segments[0].first().x() >= x1);
    }

    const int col = bucketColumn(x0, x1, dpr);
    QVERIFY(columnHasTrace(img, *chart, lastBucket ? col - 1 : col + 1));
    QVERIFY2(!columnHasTrace(img, *chart, col), qPrintable(QStringLiteral("trace pixel in NaN column %1").arg(col)));
}

void tst_StripChart::decimatedConsecutiveDropouts_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::newRow("Time") << static_cast<int>(StripChart::XAxisMode::Time);
    QTest::newRow("SampleIndex") << static_cast<int>(StripChart::XAxisMode::SampleIndex);
}

void tst_StripChart::decimatedConsecutiveDropouts()
{
    QFETCH(int, axisMode);

    // Aligned geometry: 1000 px plot, 1 s span, 1M samples at 1 us; 1M / W_dev samples per bucket
    constexpr int n = 1000000;
    constexpr qint64 sampleDtNs = 1000;

    WarningGuard guard;
    auto chart = makeDropoutChart(static_cast<StripChart::XAxisMode>(axisMode), 1054, 1000000000, n);
    const double dpr = chart->devicePixelRatioF();
    const int widthDev = qRound(1000 * dpr);
    const qint64 perBucket = n / widthDev;
    const qint64 k0 = widthDev / 2;
    std::vector<qint64> nans;
    for (qint64 j = 0; j < 3; ++j) {
        nans.push_back((k0 + j) * perBucket + perBucket / 2); // one NaN in each of 3 adjacent buckets
    }
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, n, sampleDtNs, n, nans);

    const QImage img = renderToImage(*chart);
    const auto &frame = StripChartTestAccess::lastFrame(*chart);
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
    const auto &chDiag = frame.channels[0];
    QCOMPARE(chDiag.path, RenderPath::Decimated);

    std::vector<int> cols;
    for (qint64 j = 0; j < 3; ++j) {
        const qint64 k = bucketOfSample(*chart, nans[static_cast<size_t>(j)], sampleDtNs);
        QCOMPARE(k, k0 + j);
        const auto [x0, x1] = bucketXRange(*chart, k);
        QVERIFY(bucketAlignedToDeviceColumn(x0, x1, dpr));
        QCOMPARE(verticesInRange(chDiag, x0, x1), 0);
        cols.push_back(bucketColumn(x0, x1, dpr));
    }
    const auto first = bucketXRange(*chart, k0);
    const auto last = bucketXRange(*chart, k0 + 2);
    QCOMPARE(chDiag.segments.size(), 2UL);
    QVERIFY(chDiag.segments[0].last().x() < first.first);
    QVERIFY(chDiag.segments[1].first().x() >= last.second);

    // One blank run of exactly 3 device columns
    QCOMPARE(cols[1], cols[0] + 1);
    QCOMPARE(cols[2], cols[0] + 2);
    QVERIFY(columnHasTrace(img, *chart, cols[0] - 1));
    QVERIFY(columnHasTrace(img, *chart, cols[2] + 1));
    for (int c : cols) {
        QVERIFY2(!columnHasTrace(img, *chart, c), qPrintable(QStringLiteral("trace pixel in NaN column %1").arg(c)));
    }
}

void tst_StripChart::decimatedDropoutStreaming_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::newRow("Time") << static_cast<int>(StripChart::XAxisMode::Time);
    QTest::newRow("SampleIndex") << static_cast<int>(StripChart::XAxisMode::SampleIndex);
}

void tst_StripChart::decimatedDropoutStreaming()
{
    QFETCH(int, axisMode);

    // Aligned geometry (1000 px, 1 s span, capacity 1M, 1 us samples). The buffer never wraps,
    // so SampleIndex positions stay aligned too.
    constexpr int capacity = 1000000;
    constexpr qint64 sampleDtNs = 1000;
    constexpr qint64 n0 = 998000;
    const qint64 nanIndex = n0 + 4;

    WarningGuard guard;
    auto chart = makeDropoutChart(static_cast<StripChart::XAxisMode>(axisMode), 1054, 1000000000, capacity);
    const double dpr = chart->devicePixelRatioF();
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, n0, sampleDtNs, capacity, {});
    renderToImage(*chart);

    // 1. A NaN arrives in the active (still open) bucket: its column is blank on the next frame
    appendDropoutSamples(*chart, ch, n0, 10, sampleDtNs, capacity, {nanIndex});
    {
        const QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);
        const auto &chDiag = frame.channels[0];
        QCOMPARE(chDiag.path, RenderPath::Decimated);
        const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
        QCOMPARE(k, bucketOfSample(*chart, n0 + 9, sampleDtNs)); // the NaN is in the active bucket
        const auto [x0, x1] = bucketXRange(*chart, k);
        QVERIFY(bucketAlignedToDeviceColumn(x0, x1, dpr));
        QCOMPARE(verticesInRange(chDiag, x0, x1), 0);
        QCOMPARE(chDiag.segments.size(), 1UL);
        QVERIFY(chDiag.segments[0].last().x() < x0);
        const int col = bucketColumn(x0, x1, dpr);
        QVERIFY(columnHasTrace(img, *chart, col - 1));
        QVERIFY2(!columnHasTrace(img, *chart, col), qPrintable(QStringLiteral("active NaN column %1 drawn").arg(col)));
    }

    // 2. Further finite samples close the bucket: the column stays blank
    appendDropoutSamples(*chart, ch, n0 + 10, 1500, sampleDtNs, capacity, {});
    {
        const QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);
        const auto &chDiag = frame.channels[0];
        const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
        QVERIFY(k < bucketOfSample(*chart, n0 + 1509, sampleDtNs)); // the NaN bucket is closed
        const auto [x0, x1] = bucketXRange(*chart, k);
        QVERIFY(bucketAlignedToDeviceColumn(x0, x1, dpr));
        QCOMPARE(verticesInRange(chDiag, x0, x1), 0);
        QCOMPARE(chDiag.segments.size(), 2UL);
        QVERIFY(chDiag.segments[0].last().x() < x0);
        QVERIFY(chDiag.segments[1].first().x() >= x1);
        const int col = bucketColumn(x0, x1, dpr);
        QVERIFY(columnHasTrace(img, *chart, col - 1));
        QVERIFY(columnHasTrace(img, *chart, col + 1));
        QVERIFY2(!columnHasTrace(img, *chart, col), qPrintable(QStringLiteral("closed NaN column %1 drawn").arg(col)));
    }
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
}

void tst_StripChart::decimatedDropoutEviction_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::addColumn<int>("capacity");
    // Time: 5000 samples (50 ms) inside a 1 s window, so the oldest retained bucket is visible.
    // SampleIndex: capacity = W_dev * 100 / (W_dev / 1000) keeps 1e5 / W_dev samples per bucket.
    QTest::newRow("Time") << static_cast<int>(StripChart::XAxisMode::Time) << 5000;
    QTest::newRow("SampleIndex") << static_cast<int>(StripChart::XAxisMode::SampleIndex) << 100000;
}

void tst_StripChart::decimatedDropoutEviction()
{
    QFETCH(int, axisMode);
    QFETCH(int, capacity);

    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);
    constexpr qint64 sampleDtNs = 10000; // 10 us

    WarningGuard guard;
    auto chart = makeDropoutChart(mode, 1054, 1000000000, capacity);
    const double dpr = chart->devicePixelRatioF();
    const qint64 perBucket = 100000 / qRound(1000 * dpr); // samples per bucket in both modes
    const qint64 nanIndex = perBucket + perBucket / 2;    // middle of bucket 1
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, capacity, sampleDtNs, capacity, {nanIndex});

    // 1. Evict part of bucket 1, keeping the NaN: bucket 1 is the oldest retained bucket and stays blank
    qint64 added = capacity;
    const qint64 firstEviction = perBucket + perBucket / 4;
    appendDropoutSamples(*chart, ch, added, firstEviction, sampleDtNs, capacity, {});
    added += firstEviction;
    {
        const QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);
        const auto &chDiag = frame.channels[0];
        QCOMPARE(chDiag.path, RenderPath::Decimated);
        const qint64 firstRetained = added - capacity;
        QVERIFY(firstRetained < nanIndex);
        const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
        QCOMPARE(k, bucketOfSample(*chart, firstRetained, sampleDtNs)); // oldest retained bucket
        const auto [x0, x1] = bucketXRange(*chart, k, firstRetained);
        QCOMPARE(verticesInRange(chDiag, x0, x1), 0);
        QCOMPARE(chDiag.segments.size(), 1UL);
        QVERIFY(chDiag.segments[0].first().x() >= x1);
        const int col = bucketColumn(x0, x1, dpr);
        QVERIFY(columnHasTrace(img, *chart, col + 1));
        QVERIFY2(!columnHasTrace(img, *chart, col), qPrintable(QStringLiteral("NaN column %1 drawn before eviction").arg(col)));
    }

    // 2. Evict the NaN itself: the recomputed oldest bucket holds only finite samples and is drawn again
    const qint64 secondEviction = perBucket / 2;
    appendDropoutSamples(*chart, ch, added, secondEviction, sampleDtNs, capacity, {});
    added += secondEviction;
    {
        const QImage img = renderToImage(*chart);
        const auto &frame = StripChartTestAccess::lastFrame(*chart);
        const auto &chDiag = frame.channels[0];
        const qint64 firstRetained = added - capacity;
        QVERIFY(firstRetained > nanIndex);
        const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
        QCOMPARE(k, bucketOfSample(*chart, firstRetained, sampleDtNs));
        const auto [x0, x1] = bucketXRange(*chart, k, firstRetained);
        QVERIFY(verticesInRange(chDiag, x0, x1) > 0);
        QCOMPARE(chDiag.segments.size(), 1UL);
        QVERIFY(chDiag.segments[0].first().x() >= x0);
        QVERIFY(chDiag.segments[0].first().x() < x1);
        const int firstCol = static_cast<int>(std::floor(chDiag.segments[0].first().x() * dpr));
        QVERIFY2(columnHasTrace(img, *chart, firstCol),
                 qPrintable(QStringLiteral("evicted-NaN bucket not drawn at col %1").arg(firstCol)));
    }
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
}

void tst_StripChart::autoscaleUsesFiniteValuesOfBlankedBucket()
{
    // A bucket with a NaN and a finite spike is not drawn, but autoscale keeps using the spike.
    constexpr int n = 100000;
    auto chart = makeDropoutChart(StripChart::XAxisMode::SampleIndex, 1054, 0, n);
    chart->setAutoScaleY(true);
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    const qint64 nanIndex = 50000;
    for (qint64 i = 0; i < n; ++i) {
        double v = 50.0;
        if (i == nanIndex) {
            v = std::numeric_limits<double>::quiet_NaN();
        } else if (i == nanIndex + 1) {
            v = 90.0; // finite spike in the same bucket as the NaN
        }
        chart->addDataPoint(ch, v);
    }
    renderToImage(*chart);
    const auto &frame = StripChartTestAccess::lastFrame(*chart);
    QCOMPARE(frame.channels[0].path, RenderPath::Decimated);
    QCOMPARE(bucketOfSample(*chart, nanIndex, 0), bucketOfSample(*chart, nanIndex + 1, 0));

    QVERIFY2(chart->yMaximum() >= 90.0, qPrintable(QStringLiteral("yMaximum %1").arg(chart->yMaximum())));
    QVERIFY(chart->yMinimum() <= 50.0);
}

void tst_StripChart::glowDotNeverDrawnInBlankColumn_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::addColumn<bool>("nanInLatestBucket");

    const int time = static_cast<int>(StripChart::XAxisMode::Time);
    const int index = static_cast<int>(StripChart::XAxisMode::SampleIndex);
    QTest::newRow("Time, NaN in latest bucket") << time << true;
    QTest::newRow("Time, NaN in penultimate bucket") << time << false;
    QTest::newRow("SampleIndex, NaN in latest bucket") << index << true;
    QTest::newRow("SampleIndex, NaN in penultimate bucket") << index << false;
}

void tst_StripChart::glowDotNeverDrawnInBlankColumn()
{
    QFETCH(int, axisMode);
    QFETCH(bool, nanInLatestBucket);

    // Aligned geometry: 1000 px plot, 1 s span, 1M samples at 1 us; the latest sample is finite.
    constexpr int n = 1000000;
    constexpr qint64 sampleDtNs = 1000;

    WarningGuard guard;
    auto chart = makeDropoutChart(static_cast<StripChart::XAxisMode>(axisMode), 1054, 1000000000, n);
    const double dpr = chart->devicePixelRatioF();
    const qint64 perBucket = n / qRound(1000 * dpr);
    const qint64 latestBucket = (n - 1) / perBucket;
    const qint64 nanBucket = nanInLatestBucket ? latestBucket : latestBucket - 1;
    const qint64 nanIndex = nanBucket * perBucket + perBucket / 2;
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, n, sampleDtNs, n, {nanIndex});

    const QImage img = renderToImage(*chart);
    const auto &frame = StripChartTestAccess::lastFrame(*chart);
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
    const auto &chDiag = frame.channels[0];
    QCOMPARE(chDiag.path, RenderPath::Decimated);
    QVERIFY(std::isfinite(chart->channelLatestValue(ch)));

    const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
    QCOMPARE(k, nanBucket);
    QCOMPARE(bucketOfSample(*chart, n - 1, sampleDtNs), latestBucket);
    const auto [x0, x1] = bucketXRange(*chart, k);
    QVERIFY(bucketAlignedToDeviceColumn(x0, x1, dpr));
    QCOMPARE(verticesInRange(chDiag, x0, x1), 0);

    const int top = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
    const int bottom = static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr));
    // Any reddish pixel: the trace colour, the glow colour or an antialiased blend of either.
    auto reddishRows = [&](int x) {
        std::vector<int> rows;
        for (int y = top; y <= bottom && y < img.height(); ++y) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() >= 100 && c.red() - c.green() >= 40 && c.red() - c.blue() >= 40) {
                rows.push_back(y);
            }
        }
        return rows;
    };
    const QColor glow = kDropoutColour.lighter(150);
    auto hasGlow = [&](int x) { return !traceYAtColumn(img, glow, x, top, bottom, 60).empty(); };

    // The dropout column contains neither the trace nor the glow dot
    const int col = bucketColumn(x0, x1, dpr);
    QVERIFY2(reddishRows(col).empty(),
             qPrintable(QStringLiteral("trace or glow pixel in NaN column %1 (first at y %2)").arg(col).arg(reddishRows(col).empty() ? -1 : reddishRows(col).front())));

    const int left = static_cast<int>(std::floor(frame.plotRect.left() * dpr));
    const int right = static_cast<int>(std::ceil(frame.plotRect.right() * dpr));
    if (nanInLatestBucket) {
        // The latest bucket is blank: no glow dot at all (the legend keeps the latest value)
        for (int x = left; x < right; ++x) {
            QVERIFY2(!hasGlow(x), qPrintable(QStringLiteral("glow pixel at col %1").arg(x)));
        }
    } else {
        // The glow dot is drawn on the last point, which lies in the latest bucket
        const QPointF last = chDiag.segments.back().last();
        const auto latest = bucketXRange(*chart, latestBucket);
        QVERIFY(last.x() >= latest.first && last.x() < latest.second);
        const int lastCol = static_cast<int>(std::floor(last.x() * dpr));
        QVERIFY2(hasGlow(lastCol), qPrintable(QStringLiteral("no glow at the last point, col %1").arg(lastCol)));
    }
}

QTEST_MAIN(tst_StripChart)
#include "tst_StripChart.moc"
