// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/StripChart.h>

#include "FontTestUtils.h"

#include <memory>
#include <internal/StripChartGeometry.h>
#include <internal/M4Decimator.h>
#include "StripChartTestUtils.h"
#include <QtGui/QPixmap>
#include <QtGui/QImage>

#include <cmath>
#include <limits>
#include <vector>
#include <random>
#include <set>

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
    void plotMarginsFollowWidgetFontSize();
    void fontChangeRefreshesCache();
    void labelsFollowWidgetFont();
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
    void bucketGridProperty();
    void scrollIsShiftInvariantAtAnyDpr();
    void decimatedDropoutVisibleAtAnyPosition_data();
    void decimatedDropoutVisibleAtAnyPosition();
    void spikeLitInExactlyOneColumn_data();
    void spikeLitInExactlyOneColumn();
    void rawDecimatedContinuity_data();
    void rawDecimatedContinuity();
    void segmentEndColumnsAreDrawn_data();
    void segmentEndColumnsAreDrawn();
    void sampleIndexCapacityBelowWidth();
    void timeSpanShorterThanWidth_data();
    void timeSpanShorterThanWidth();
    void streamingDoesNotRebuildDecimationStream_data();
    void streamingDoesNotRebuildDecimationStream();
    void yAxisLabelsHaveNoNegativeZero_data();
    void yAxisLabelsHaveNoNegativeZero();
    void legendHasNoNegativeZero_data();
    void legendHasNoNegativeZero();
    void absoluteSecondsLabelsHaveNoNegativeZero();
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
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

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
    const auto &frameSmall = internal::StripChartTestAccess::lastFrame(*chart);
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
    const auto &frameLarge = internal::StripChartTestAccess::lastFrame(*chart);
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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

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

        // Pixels: no trace pixel in the device columns strictly between the x of samples 49 and 51,
        // and strictly between 69 and 71, excluding on each side the extent of the raw pen's round
        // cap, ceil(penWidth / 2 * dpr) device pixels. The remaining range must not be empty.
        qreal dpr = chart->devicePixelRatioF();
        int plotTopDev = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
        int plotBottomDev = static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr));
        const int capDev = static_cast<int>(std::ceil(chDiag.penWidth / 2.0 * dpr));

        int x49Dev = static_cast<int>(std::floor(mapIndex(*chart, 49) * dpr));
        int x51Dev = static_cast<int>(std::ceil(mapIndex(*chart, 51) * dpr));
        QVERIFY2(x49Dev + 1 + capDev < x51Dev - capDev,
                 qPrintable(QString("no column left to check between samples 49 and 51 (cols %1..%2, cap %3)").arg(x49Dev).arg(x51Dev).arg(capDev)));
        qInfo("Gap 49-51: device columns %d..%d checked (cap exclusion %d per side)", x49Dev + 1 + capDev, x51Dev - capDev - 1, capDev);
        for (int x = x49Dev + 1 + capDev; x < x51Dev - capDev; ++x) {
            auto ys = traceYAtColumn(img, chColor, x, plotTopDev, plotBottomDev, 60);
            QVERIFY2(ys.empty(), qPrintable(QString("Trace pixel found between samples 49 and 51 at col %1").arg(x)));
        }

        int x69Dev = static_cast<int>(std::floor(mapIndex(*chart, 69) * dpr));
        int x71Dev = static_cast<int>(std::ceil(mapIndex(*chart, 71) * dpr));
        QVERIFY2(x69Dev + 1 + capDev < x71Dev - capDev,
                 qPrintable(QString("no column left to check between samples 69 and 71 (cols %1..%2, cap %3)").arg(x69Dev).arg(x71Dev).arg(capDev)));
        qInfo("Gap 69-71: device columns %d..%d checked (cap exclusion %d per side)", x69Dev + 1 + capDev, x71Dev - capDev - 1, capDev);
        for (int x = x69Dev + 1 + capDev; x < x71Dev - capDev; ++x) {
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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

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
        // Device column of the bucket that holds the NaN (#12: one bucket per device column)
        const qint64 kNan = internal::BucketGrid(frame.gridNum, frame.gridDen).bucketOf(50000);
        int xNanDev = static_cast<int>(std::floor(frameXMapping(*chart).columnLeftDev(kNan) + 1e-9));
        auto ysNan = traceYAtColumn(img, chColor, xNanDev, plotTopDev, plotBottomDev, 60);

        // The bucket containing the NaN is not drawn: 2 segments and no trace in its column.
        QCOMPARE(chDiag.segments.size(), 2UL);

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
    const auto &frameStep = internal::StripChartTestAccess::lastFrame(*chart);

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
    const auto &frameLinear = internal::StripChartTestAccess::lastFrame(*chart);
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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
        QCOMPARE(frame.channels[0].segments.size(), 1UL);
        QCOMPARE(frame.channels[0].segments[0].size(), 7);
    }

    // setGapThreshold(10 ms): 2 segments (pause 50 ms > 10 ms)
    {
        auto chart = buildRawChart(std::chrono::milliseconds(10));
        renderToImage(*chart);
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

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
        const auto &f0 = internal::StripChartTestAccess::lastFrame(*chart);
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
            const auto &frame = internal::StripChartTestAccess::lastFrame(*testChart);

            // tEnd is the boundary of the bucket after the window (non-uniform grid, #12)
            QCOMPARE(frame.tEnd, internal::BucketGrid(frame.gridNum, frame.gridDen).boundary(frame.kStart + frame.numBuckets));
            QVERIFY2(frame.tStart <= tLatest, qPrintable(QString("tStart (%1) > tLatest (%2)").arg(frame.tStart).arg(tLatest)));
            QVERIFY2(tLatest < frame.tEnd, qPrintable(QString("tLatest (%1) >= tEnd (%2)").arg(tLatest).arg(frame.tEnd)));
            QCOMPARE(frame.tEnd - frame.tStart, testChart->timeSpan().count());
        }
    }

    // Part 2 (pixel shift invariance) is covered at every device pixel ratio by
    // scrollIsShiftInvariantAtAnyDpr, which runs the same scenario (1 s span, 1000 px plot,
    // DecimationMode::Always, stationary noise at 100 kHz) and asserts the same properties:
    // same window and identical pixels within a bucket, a one-bucket advance shifts the trace by
    // exactly one device column.

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
        QCOMPARE(ib.numBuckets, tc.widthDev);

        quint64 startCounter = (tc.totalSamples >= tc.count) ? (tc.totalSamples - tc.count) : 0;
        for (size_t j = 0; j < tc.count; ++j) {
            qint64 idx = static_cast<qint64>(startCounter + j);
            qint64 bucket = ib.grid.bucketOf(idx);
            // With a wrapped buffer only the oldest, partial bucket may fall out on the left
            QVERIFY2(bucket >= ib.kStart || (bucket == ib.kStart - 1 && bucket == ib.grid.bucketOf(static_cast<qint64>(startCounter))),
                     qPrintable(QString("bucket %1 < kStart %2").arg(bucket).arg(ib.kStart)));
            QVERIFY2(bucket < ib.kStart + ib.numBuckets,
                     qPrintable(QString("bucket %1 >= kStart+numBuckets %2 in case cap=%3, W=%4, total=%5, count=%6, numB=%7")
                               .arg(bucket).arg(ib.kStart + ib.numBuckets).arg(tc.capacity).arg(tc.widthDev).arg(tc.totalSamples).arg(tc.count).arg(ib.numBuckets)));
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

            QVERIFY2(win.tEnd == win.grid.boundary(win.kStart + win.numBuckets),
                     qPrintable(QString("tEnd %1 not the boundary of bucket kStart + numBuckets (tLatest=%2)").arg(win.tEnd).arg(tLatest)));
            QVERIFY2(win.tStart <= tLatest && tLatest < win.tEnd,
                     qPrintable(QString("tLatest %1 not in [tStart %2, tEnd %3) (dtPx=%4)").arg(tLatest).arg(win.tStart).arg(win.tEnd).arg(win.dtPx)));
            QCOMPARE(win.tEnd - win.tStart, cfg.spanNs);

            qint64 kLatest = win.grid.bucketOf(tLatest);
            QVERIFY2(kLatest >= win.kStart && kLatest < win.kStart + win.numBuckets,
                     qPrintable(QString("kLatest %1 not in [kStart %2, %3) (tLatest=%4)").arg(kLatest).arg(win.kStart).arg(win.kStart + win.numBuckets).arg(tLatest)));
            QVERIFY2(win.numBuckets == cfg.widthDev,
                     qPrintable(QString("numBuckets %1 != W_dev %2").arg(win.numBuckets).arg(cfg.widthDev)));
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
    int wDev = std::max(1, static_cast<int>(std::round(internal::StripChartTestAccess::plotArea(dummy).width() * dummy.devicePixelRatioF())));

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

    // Oracle (#12): every decimated column is checked against the data of its own bucket, not
    // against the raw rendering. The raw rendering draws transitions with continuous x and can
    // put the rising edge of a spike in the previous column, while decimated buckets are confined
    // to one device column; the raw comparison is therefore informational only.
    internal::StripChartTestAccess::enableDiagnostics(chart, true);

    // Render with DecimationMode::Off (informational comparison)
    chart.setDecimationMode(StripChart::DecimationMode::Off);
    const QImage imgOff = renderToImage(chart);

    // Render with DecimationMode::Always
    chart.setDecimationMode(StripChart::DecimationMode::Always);
    const QImage imgAlways = renderToImage(chart);
    const FrameDiagnostics frame = internal::StripChartTestAccess::lastFrame(chart);
    QCOMPARE(frame.channels[0].path, RenderPath::Decimated);

    auto isRedPixel = [](QRgb rgb) -> bool {
        return qRed(rgb) > 180 && qGreen(rgb) < 80 && qBlue(rgb) < 80;
    };

    const double dpr = chart.devicePixelRatioF();
    const internal::BucketGrid grid(frame.gridNum, frame.gridDen);
    const int widthDev = frame.widthDev;
    const int leftDev = static_cast<int>(frame.plotLeftDev);
    const int rowTop = static_cast<int>(std::floor(frame.plotRect.top() * dpr));
    const int rowBottom = std::min(imgAlways.height() - 1, static_cast<int>(std::ceil(frame.plotRect.bottom() * dpr)));

    // Bucket data: min, max, first and last sample value of every bucket in the window
    struct BucketData {
        double min{std::numeric_limits<double>::infinity()};
        double max{-std::numeric_limits<double>::infinity()};
        double first{0.0};
        double last{0.0};
        bool any{false};
    };
    std::vector<BucketData> buckets(static_cast<size_t>(widthDev));
    for (int i = 0; i < N; ++i) {
        const qint64 c = grid.bucketOf(timestamps[i].count()) - frame.kStart;
        if (c < 0 || c >= widthDev) continue;
        BucketData &b = buckets[static_cast<size_t>(c)];
        if (!b.any) b.first = values[i];
        b.last = values[i];
        b.min = std::min(b.min, values[i]);
        b.max = std::max(b.max, values[i]);
        b.any = true;
    }

    // Device y of a value. An aliased cosmetic pen puts a coordinate y in pixel row round(y)
    // (Qt draws aliased pixels to the right of and below the mathematical point), and a line end
    // can be left out by one pixel: tolerance of 1 row for the rounding of the y mapping.
    auto rowOf = [&](double v) { return static_cast<int>(std::lround(mapY(chart, v) * dpr)); };
    constexpr int kRowTolerance = 1;
    // The glow dot on the latest point covers the trace in the last columns
    const int glowColumns = static_cast<int>(std::ceil(4.0 * dpr)) + 1;

    int checked = 0;
    for (int c = 0; c < widthDev - glowColumns; ++c) {
        const BucketData &b = buckets[static_cast<size_t>(c)];
        if (!b.any) continue;
        const int x = leftDev + c;
        int top = -1;
        int bottom = -1;
        for (int y = rowTop; y <= rowBottom; ++y) {
            if (isRedPixel(imgAlways.pixel(x, y))) {
                if (top < 0) top = y;
                bottom = y;
            }
        }
        QVERIFY2(top >= 0, qPrintable(QString("column %1 has samples but no trace").arg(x)));

        // 1. No lost peak: the lit rows contain the rows of the bucket's max and min
        QVERIFY2(top <= rowOf(b.max) + kRowTolerance && bottom >= rowOf(b.min) - kRowTolerance,
                 qPrintable(QString("column %1: lit rows [%2, %3] miss the bucket's max/min rows [%4, %5]")
                                .arg(x).arg(top).arg(bottom).arg(rowOf(b.max)).arg(rowOf(b.min))));

        // 2. Nothing invented: lit rows stay inside the bucket's range extended to the connecting
        //    segments (last point of the previous bucket, first point of the next)
        double lo = b.min;
        double hi = b.max;
        if (c > 0 && buckets[static_cast<size_t>(c - 1)].any) {
            lo = std::min(lo, buckets[static_cast<size_t>(c - 1)].last);
            hi = std::max(hi, buckets[static_cast<size_t>(c - 1)].last);
        }
        if (c + 1 < widthDev && buckets[static_cast<size_t>(c + 1)].any) {
            lo = std::min(lo, buckets[static_cast<size_t>(c + 1)].first);
            hi = std::max(hi, buckets[static_cast<size_t>(c + 1)].first);
        }
        QVERIFY2(top >= rowOf(hi) - kRowTolerance && bottom <= rowOf(lo) + kRowTolerance,
                 qPrintable(QString("column %1: lit rows [%2, %3] outside the allowed rows [%4, %5]")
                                .arg(x).arg(top).arg(bottom).arg(rowOf(hi)).arg(rowOf(lo))));
        ++checked;
    }
    QVERIFY(checked > (widthDev - glowColumns) * 9 / 10);

    // Informational: per-column vertical extents of the raw (cosmetic 1 px) and decimated renderings
    int maxDiffPerCol = 0;
    int diffCols = 0;
    for (int c = 0; c < widthDev; ++c) {
        const int x = leftDev + c;
        int a0 = -1, a1 = -1, b0 = -1, b1 = -1;
        for (int y = rowTop; y <= rowBottom; ++y) {
            if (isRedPixel(imgOff.pixel(x, y))) { if (a0 < 0) a0 = y; a1 = y; }
            if (isRedPixel(imgAlways.pixel(x, y))) { if (b0 < 0) b0 = y; b1 = y; }
        }
        if (a0 < 0 || b0 < 0) continue;
        const int d = std::max(std::abs(a0 - b0), std::abs(a1 - b1));
        maxDiffPerCol = std::max(maxDiffPerCol, d);
        if (d > 1) {
            ++diffCols;
            qInfo("raw vs decimated: column %d raw rows [%d, %d], decimated rows [%d, %d]", x, a0, a1, b0, b1);
        }
    }
    qInfo() << "M4 widget equivalence (informational): columns differing by > 1 row:" << diffCols
            << "max difference:" << maxDiffPerCol << "rows; checked columns:" << checked;
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
    const auto &f = internal::StripChartTestAccess::lastFrame(chart);
    const internal::BucketGrid grid(f.gridNum, f.gridDen);
    return grid.bucketOf(chart.xAxisMode() == StripChart::XAxisMode::Time ? index * sampleDtNs : index);
}

// Logical x range [x0, x1) of decimation bucket k in the last frame: its device column (#12).
std::pair<double, double> bucketXRange(const StripChart &chart, qint64 k, qint64 /*firstRetained*/ = 0)
{
    const auto &f = internal::StripChartTestAccess::lastFrame(chart);
    const double left = frameXMapping(chart).columnLeftDev(k);
    return {left / f.dpr, (left + 1.0) / f.dpr};
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

// Window grid of the dropout charts below (1000 logical px plot, 1 s span, n samples at
// sampleDtNs). Bucket widths are floor or ceil of den / W_dev samples (e.g. 666 or 667 at DPR 1.5).
internal::BucketGrid dropoutGrid(StripChart::XAxisMode mode, int widthDev, qint64 spanNs, qint64 n)
{
    return (mode == StripChart::XAxisMode::Time) ? internal::BucketGrid(widthDev, spanNs)
                                                 : internal::BucketGrid(widthDev, n);
}

// Absolute index of the sample in the middle of bucket k.
qint64 midSampleOfBucket(const internal::BucketGrid &grid, StripChart::XAxisMode mode, qint64 k, qint64 sampleDtNs)
{
    const qint64 b0 = grid.boundary(k);
    const qint64 mid = b0 + (grid.boundary(k + 1) - b0) / 2;
    return (mode == StripChart::XAxisMode::Time) ? mid / sampleDtNs : mid;
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

bool columnHasColour(const QImage &img, const StripChart &chart, int xDev, const QColor &colour)
{
    const auto &f = internal::StripChartTestAccess::lastFrame(chart);
    const double dpr = chart.devicePixelRatioF();
    const int top = static_cast<int>(std::floor(f.plotRect.top() * dpr));
    const int bottom = static_cast<int>(std::ceil(f.plotRect.bottom() * dpr));
    return !traceYAtColumn(img, colour, xDev, top, bottom, 60).empty();
}

bool columnHasTrace(const QImage &img, const StripChart &chart, int xDev)
{
    return columnHasColour(img, chart, xDev, kDropoutColour);
}

// The trace's end point carries the glow dot (channel colour lighter(150)), which covers the trace
// pixels of the columns around it; either colour shows that the column is drawn.
bool columnHasTraceOrGlow(const QImage &img, const StripChart &chart, int xDev)
{
    return columnHasTrace(img, chart, xDev) || columnHasColour(img, chart, xDev, kDropoutColour.lighter(150));
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
    // Geometries whose buckets were 0.25-0.75 px off the device columns before #12; buckets are
    // now aligned to device columns in every geometry.
    QTest::newRow("Time, misaligned") << time << 1974 << qint64(1000000) << qint64(10) << 100000 << qint64(55362) << true;
    QTest::newRow("SampleIndex, misaligned") << index << 1000 << qint64(0) << qint64(0) << 1000000 << qint64(400268) << true;
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
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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

    // 1000 px plot, 1 s span, 1M samples at 1 us; NaN positions from the real bucket grid
    // (buckets need not hold a whole number of n / W_dev samples, e.g. at DPR 1.5)
    constexpr int n = 1000000;
    constexpr qint64 sampleDtNs = 1000;
    constexpr qint64 spanNs = 1000000000;
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);

    WarningGuard guard;
    auto chart = makeDropoutChart(mode, 1054, spanNs, n);
    const double dpr = chart->devicePixelRatioF();
    const int widthDev = qRound(1000 * dpr);
    const internal::BucketGrid grid = dropoutGrid(mode, widthDev, spanNs, n);
    const qint64 k0 = widthDev / 2;
    std::vector<qint64> nans;
    for (qint64 j = 0; j < 3; ++j) {
        nans.push_back(midSampleOfBucket(grid, mode, k0 + j, sampleDtNs)); // one NaN in each of 3 adjacent buckets
    }
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, n, sampleDtNs, n, nans);

    const QImage img = renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
    const auto &chDiag = frame.channels[0];
    QCOMPARE(chDiag.path, RenderPath::Decimated);
    QCOMPARE(internal::BucketGrid(frame.gridNum, frame.gridDen), grid);

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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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
        // The NaN bucket is now the penultimate one: the glow dot on the last point can cover the
        // trace columns around the gap, but never the gap itself.
        QVERIFY(columnHasTraceOrGlow(img, *chart, col - 1));
        QVERIFY(columnHasTraceOrGlow(img, *chart, col + 1));
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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
        const auto &chDiag = frame.channels[0];
        QCOMPARE(chDiag.path, RenderPath::Decimated);
        const qint64 firstRetained = added - capacity;
        QVERIFY(firstRetained < nanIndex);
        const qint64 k = bucketOfSample(*chart, nanIndex, sampleDtNs);
        QCOMPARE(k, bucketOfSample(*chart, firstRetained, sampleDtNs)); // oldest retained bucket
        if (mode == StripChart::XAxisMode::SampleIndex) {
            // SampleIndex windows hold the W_dev newest buckets: with a wrapped buffer the oldest,
            // partial bucket falls out on the left (#12), so there is nothing to draw for it.
            QCOMPARE(k, frame.kStart - 1);
            QVERIFY(chDiag.segments[0].first().x() >= bucketXRange(*chart, k).second);
            return;
        }
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
        const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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

    // 1000 px plot, 1 s span, 1M samples at 1 us; the latest sample is finite. NaN position from
    // the real bucket grid (buckets need not hold a whole number of n / W_dev samples)
    constexpr int n = 1000000;
    constexpr qint64 sampleDtNs = 1000;
    constexpr qint64 spanNs = 1000000000;
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);

    WarningGuard guard;
    auto chart = makeDropoutChart(mode, 1054, spanNs, n);
    const double dpr = chart->devicePixelRatioF();
    const internal::BucketGrid grid = dropoutGrid(mode, qRound(1000 * dpr), spanNs, n);
    const qint64 latestBucket = grid.bucketOf(mode == StripChart::XAxisMode::Time ? (n - 1) * sampleDtNs : n - 1);
    const qint64 nanBucket = nanInLatestBucket ? latestBucket : latestBucket - 1;
    const qint64 nanIndex = midSampleOfBucket(grid, mode, nanBucket, sampleDtNs);
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, n, sampleDtNs, n, {nanIndex});

    const QImage img = renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
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
        // The glow dot (radius 2.5 logical px) is visible in the plot columns around the last point
        // other than the blank NaN column (the last point can sit on the plot's right edge).
        const int from = std::max(left, static_cast<int>(std::floor((last.x() - 2.5) * dpr)));
        const int to = std::min(right - 1, static_cast<int>(std::ceil((last.x() + 2.5) * dpr)));
        int glowColumns = 0;
        for (int x = from; x <= to; ++x) {
            if (x != col && hasGlow(x)) {
                ++glowColumns;
            }
        }
        QVERIFY2(glowColumns > 0, qPrintable(QStringLiteral("no glow around the last point (x %1)").arg(last.x() * dpr)));
    }
}

// ---------------------------------------------------------------------------------------------
// Decimation buckets aligned to device pixel columns (issue #12)
// ---------------------------------------------------------------------------------------------
namespace {

internal::PlotXMapping frameMapping(const FrameDiagnostics &f)
{
    return internal::makePlotXMapping(internal::BucketGrid(f.gridNum, f.gridDen), f.kStart, f.plotLeftDev);
}

// Device column of bucket k in the last frame
int deviceColumnOfBucket(const FrameDiagnostics &f, qint64 k)
{
    return static_cast<int>(std::floor(frameMapping(f).columnLeftDev(k) + 1e-9));
}

int plotTopDev(const FrameDiagnostics &f) { return static_cast<int>(std::floor(f.plotRect.top() * f.dpr)); }
int plotBottomDev(const FrameDiagnostics &f) { return static_cast<int>(std::ceil(f.plotRect.bottom() * f.dpr)); }

} // namespace

void tst_StripChart::bucketGridProperty()
{
    std::mt19937_64 rng(20261012);
    const double dprs[] = {1.0, 1.25, 1.5, 2.0};
    std::uniform_int_distribution<int> widthLogicalDist(1, 4000);
    std::uniform_real_distribution<double> leftDist(0.0, 100.0);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    auto checkWindow = [&](const internal::BucketGrid &grid, qint64 kStart, int widthDev, const internal::PlotXMapping &m,
                           qint64 xFirst, qint64 xLast, const QString &where) {
        // Integer device boundary for the plot's left edge
        QVERIFY2(m.leftDev == std::round(m.leftDev), qPrintable("plot left not on a device pixel boundary: " + where));
        // Bucket k occupies device column k - kStart; its first position lands at that column's left edge
        const qint64 ks[] = {kStart, kStart + 1, kStart + widthDev / 2, kStart + widthDev - 1};
        for (qint64 k : ks) {
            QVERIFY2(m.columnLeftDev(k) == m.leftDev + static_cast<double>(k - kStart), qPrintable(where));
            const qint64 b = grid.boundary(k);
            if (grid.bucketOf(b) != k) {
                continue; // empty bucket (fewer positions than buckets, e.g. capacity < W_dev)
            }
            const double pos = m.positionDev(b);
            QVERIFY2(pos >= m.columnLeftDev(k) && pos < m.columnLeftDev(k) + 1.0,
                     qPrintable(QString("bucket %1 boundary at device x %2, column [%3, +1) %4").arg(k).arg(pos, 0, 'f', 6).arg(m.columnLeftDev(k)).arg(where)));
            QVERIFY2(m.positionDev(b - 1) <= m.columnLeftDev(k), // < in exact arithmetic; a double can round it to the edge
                     qPrintable(QString("position before bucket %1 not left of its column %2").arg(k).arg(where)));
        }
        // Every position in the window belongs to exactly one bucket and lies inside its column
        if (xLast < xFirst) return;
        for (int s = 0; s < 8; ++s) {
            const qint64 x = xFirst + static_cast<qint64>(unit(rng) * static_cast<double>(xLast - xFirst));
            const qint64 k = grid.bucketOf(x);
            QVERIFY2(k >= kStart && k < kStart + widthDev, qPrintable(QString("x %1 in bucket %2 outside window %3").arg(x).arg(k).arg(where)));
            QVERIFY2(grid.boundary(k) <= x && x < grid.boundary(k + 1), qPrintable(where));
            const double pos = m.positionDev(x);
            QVERIFY2(pos >= m.columnLeftDev(k) && pos < m.columnLeftDev(k) + 1.0,
                     qPrintable(QString("x %1 at device x %2 outside its column %3").arg(x).arg(pos, 0, 'f', 6).arg(where)));
        }
    };

    for (int i = 0; i < 4000; ++i) {
        const double dpr = dprs[i % 4];
        const int widthDev = std::max(1, static_cast<int>(std::lround(widthLogicalDist(rng) * dpr))); // 1 .. 8000
        const double leftDevExact = leftDist(rng) * dpr;

        // Time mode
        {
            qint64 span = widthDev;
            switch (i % 3) {
                case 0: span += static_cast<qint64>(unit(rng) * 1e6); break;
                case 1: span += static_cast<qint64>(unit(rng) * 1e11); break;
                default: span += static_cast<qint64>(unit(rng) * 1e15); break;
            }
            qint64 tLatest = 0;
            switch (i % 4) {
                case 0: tLatest = 1000000000000000000LL - static_cast<qint64>(unit(rng) * 1e15); break;
                case 1: tLatest = -1000000000000000000LL + static_cast<qint64>(unit(rng) * 1e15) + span; break;
                default: tLatest = static_cast<qint64>((unit(rng) - 0.5) * 2e12); break;
            }
            const QString where = QString("(Time case %1: W_dev %2, dpr %3, left %4, span %5, tLatest %6)")
                                      .arg(i).arg(widthDev).arg(dpr).arg(leftDevExact).arg(span).arg(tLatest);
            const auto win = internal::computeTimeWindow(tLatest, span, widthDev);
            QVERIFY2(win.numBuckets == widthDev, qPrintable(QString("numBuckets %1 ").arg(win.numBuckets) + where));
            QVERIFY2(win.tStart <= tLatest && tLatest < win.tEnd, qPrintable(where));
            QVERIFY2(win.tEnd - win.tStart == span, qPrintable(where));
            QVERIFY2(win.tStart == win.grid.boundary(win.kStart), qPrintable(where));
            QVERIFY2(win.tEnd == win.grid.boundary(win.kStart + widthDev), qPrintable(where));
            const auto m = internal::makePlotXMapping(win.grid, win.kStart, leftDevExact);
            checkWindow(win.grid, win.kStart, widthDev, m, win.tStart, win.tEnd - 1, where);
            if (QTest::currentTestFailed()) return;
        }

        // SampleIndex mode
        {
            int capacity = 1;
            switch (i % 3) {
                case 0: capacity = 1 + static_cast<int>(unit(rng) * 3000); break;
                case 1: capacity = 3000 + static_cast<int>(unit(rng) * 1e6); break;
                default: capacity = 1000000 + static_cast<int>(unit(rng) * 1e7); break;
            }
            quint64 total = static_cast<quint64>(unit(rng) * 5.0 * capacity);
            if (i % 4 == 0 && capacity >= widthDev) {
                total = 1000000000000000000ULL - static_cast<quint64>(unit(rng) * 1e15);
            }
            const size_t count = static_cast<size_t>(std::min<quint64>(total, static_cast<quint64>(capacity)));
            const QString where = QString("(SampleIndex case %1: W_dev %2, dpr %3, left %4, capacity %5, total %6)")
                                      .arg(i).arg(widthDev).arg(dpr).arg(leftDevExact).arg(capacity).arg(total);
            const auto ib = internal::computeIndexBuckets(total, count, capacity, widthDev);
            QVERIFY2(ib.numBuckets == widthDev, qPrintable(QString("numBuckets %1 ").arg(ib.numBuckets) + where));
            const auto m = internal::makePlotXMapping(ib.grid, ib.kStart, leftDevExact);
            if (count == 0) {
                continue;
            }
            const qint64 oldest = static_cast<qint64>(total - count);
            const qint64 newest = static_cast<qint64>(total - 1);
            const qint64 kOldest = ib.grid.bucketOf(oldest);
            const qint64 kNewest = ib.grid.bucketOf(newest);
            QVERIFY2(ib.kStart == std::max(kOldest, kNewest + 1 - widthDev), qPrintable(where));
            if (count == static_cast<size_t>(capacity) && capacity >= widthDev) {
                QVERIFY2(kNewest == ib.kStart + widthDev - 1, qPrintable("buffer full but trace not at the right edge " + where));
            }
            // Retained samples left of the window: only the oldest partial bucket
            if (ib.kStart > kOldest) {
                QVERIFY2(ib.kStart == kOldest + 1, qPrintable("more than the oldest bucket dropped " + where));
            }
            checkWindow(ib.grid, ib.kStart, widthDev, m, std::max(oldest, ib.grid.boundary(ib.kStart)), newest, where);
            if (QTest::currentTestFailed()) return;
        }
    }
}

void tst_StripChart::scrollIsShiftInvariantAtAnyDpr()
{
    // As Part 2 of windowIsQuantisedAndScrollIsShiftInvariant, at whatever DPR the test runs:
    // advancing within a bucket changes no pixel, advancing by one bucket shifts the trace by
    // exactly one device column.
    auto chart = makeChart(1054, 400, 0.0, 100.0);
    chart->setXAxisMode(StripChart::XAxisMode::Time);
    chart->setDecimationMode(StripChart::DecimationMode::Always);
    chart->setTimeSpan(std::chrono::seconds(1));
    chart->setGridVisible(false);
    chart->setLegendVisible(false);
    chart->setCapacity(300000);
    const QColor chColor(255, 0, 0);
    const int ch = chart->addChannel(QStringLiteral("Noise"), chColor, 1.0);

    renderToImage(*chart);
    const FrameDiagnostics f0 = internal::StripChartTestAccess::lastFrame(*chart);
    const internal::BucketGrid grid(f0.gridNum, f0.gridDen);
    const double dpr = chart->devicePixelRatioF();

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(10.0, 90.0);
    const qint64 dtSample = 10000; // 10 us
    qint64 t = 0;
    auto appendUntil = [&](qint64 tStop) {
        while (t < tStop) {
            chart->addSample(ch, std::chrono::nanoseconds(t), dist(rng));
            t += dtSample;
        }
    };
    auto inBucket = [&](qint64 k, double frac) {
        const qint64 b0 = grid.boundary(k);
        return b0 + static_cast<qint64>(static_cast<double>(grid.boundary(k + 1) - b0) * frac);
    };
    const qint64 k0 = grid.bucketOf(1050000000LL); // t = 1.05 s: the 1 s window is full of data

    appendUntil(inBucket(k0, 0.2));
    const QImage imgA = renderToImage(*chart);
    const FrameDiagnostics fA = internal::StripChartTestAccess::lastFrame(*chart);
    const int left = static_cast<int>(std::floor(fA.plotLeftDev + 1e-9));
    const int right = left + fA.widthDev;
    const int margin = static_cast<int>(std::ceil(4.0 * dpr)) + 1; // glow dot and latest sample
    const int top = plotTopDev(fA);
    const int bottom = std::min(plotBottomDev(fA), imgA.height() - 1);

    appendUntil(inBucket(k0, 0.6));
    const QImage imgB = renderToImage(*chart);
    const FrameDiagnostics fB = internal::StripChartTestAccess::lastFrame(*chart);
    QCOMPARE(fB.kStart, fA.kStart); // same window
    for (int x = left; x < right - margin; ++x) {
        for (int y = top; y <= bottom; ++y) {
            if (imgB.pixelColor(x, y) != imgA.pixelColor(x, y)) {
                QFAIL(qPrintable(QString("frame changed within a bucket at x %1 y %2").arg(x).arg(y)));
            }
        }
    }

    appendUntil(inBucket(k0 + 1, 0.2));
    const QImage imgC = renderToImage(*chart);
    const FrameDiagnostics fC = internal::StripChartTestAccess::lastFrame(*chart);
    QCOMPARE(fC.kStart, fA.kStart + 1); // exactly one bucket
    for (int x = left + 2; x < right - margin; ++x) {
        for (int y = top; y <= bottom; ++y) {
            if (imgC.pixelColor(x, y) != imgA.pixelColor(x + 1, y)) {
                QFAIL(qPrintable(QString("one-bucket scroll is not a one-column shift at x %1 y %2").arg(x).arg(y)));
            }
        }
    }
}

void tst_StripChart::decimatedDropoutVisibleAtAnyPosition_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::addColumn<int>("widgetWidth");
    QTest::addColumn<qint64>("spanNs");
    QTest::addColumn<qint64>("sampleDtNs");
    QTest::addColumn<int>("sampleCount");
    QTest::addColumn<qint64>("nanIndex");

    const int time = static_cast<int>(StripChart::XAxisMode::Time);
    const int index = static_cast<int>(StripChart::XAxisMode::SampleIndex);
    // Time: 10 s window on a 1920 px plot, 1M samples at 10 us
    for (qint64 nan : {5003LL, 123457LL, 300011LL, 500000LL, 777777LL, 998765LL}) {
        QTest::newRow(qPrintable(QString("Time 10 s / 1920 px, NaN at %1").arg(nan)))
            << time << 1974 << qint64(10000000000LL) << qint64(10000) << 1000000 << nan;
    }
    // SampleIndex: 946 px plot, capacity 100k (the #11 case at 50 000, and other positions)
    for (qint64 nan : {50000LL, 1234LL, 33333LL, 66667LL, 98765LL}) {
        QTest::newRow(qPrintable(QString("SampleIndex 946 px / 100k, NaN at %1").arg(nan)))
            << index << 1000 << qint64(0) << qint64(0) << 100000 << nan;
    }
}

void tst_StripChart::decimatedDropoutVisibleAtAnyPosition()
{
    QFETCH(int, axisMode);
    QFETCH(int, widgetWidth);
    QFETCH(qint64, spanNs);
    QFETCH(qint64, sampleDtNs);
    QFETCH(int, sampleCount);
    QFETCH(qint64, nanIndex);

    WarningGuard guard;
    auto chart = makeDropoutChart(static_cast<StripChart::XAxisMode>(axisMode), widgetWidth, spanNs, sampleCount);
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    appendDropoutSamples(*chart, ch, 0, sampleCount, sampleDtNs, sampleCount, {nanIndex});

    const QImage img = renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    QVERIFY2(!guard.hasWarnings(), qPrintable(guard.warnings().join(QStringLiteral("; "))));
    const auto &chDiag = frame.channels[0];
    QCOMPARE(chDiag.path, RenderPath::Decimated);
    QCOMPARE(chDiag.segments.size(), 2UL);

    const internal::BucketGrid grid(frame.gridNum, frame.gridDen);
    const qint64 k = grid.bucketOf(axisMode == static_cast<int>(StripChart::XAxisMode::Time) ? nanIndex * sampleDtNs : nanIndex);
    const int col = deviceColumnOfBucket(frame, k);
    QVERIFY2(!columnHasTrace(img, *chart, col), qPrintable(QStringLiteral("trace pixel in NaN column %1").arg(col)));
    QVERIFY(columnHasTraceOrGlow(img, *chart, col - 1));
    QVERIFY(columnHasTraceOrGlow(img, *chart, col + 1));
}

void tst_StripChart::spikeLitInExactlyOneColumn_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::addColumn<int>("widgetWidth");
    QTest::addColumn<qint64>("spanNs");
    QTest::addColumn<qint64>("sampleDtNs");
    QTest::addColumn<int>("sampleCount");
    QTest::addColumn<qint64>("spikeIndex");

    const int time = static_cast<int>(StripChart::XAxisMode::Time);
    const int index = static_cast<int>(StripChart::XAxisMode::SampleIndex);
    QTest::newRow("Time 1 ms / 1920 px") << time << 1974 << qint64(1000000) << qint64(10) << 100000 << qint64(55362);
    QTest::newRow("Time 10 s / 1920 px") << time << 1974 << qint64(10000000000LL) << qint64(10000) << 1000000 << qint64(123457);
    QTest::newRow("SampleIndex 946 px / 100k") << index << 1000 << qint64(0) << qint64(0) << 100000 << qint64(50003);
    QTest::newRow("SampleIndex 946 px / 1M") << index << 1000 << qint64(0) << qint64(0) << 1000000 << qint64(400268);
}

void tst_StripChart::spikeLitInExactlyOneColumn()
{
    QFETCH(int, axisMode);
    QFETCH(int, widgetWidth);
    QFETCH(qint64, spanNs);
    QFETCH(qint64, sampleDtNs);
    QFETCH(int, sampleCount);
    QFETCH(qint64, spikeIndex);

    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);
    auto chart = makeDropoutChart(mode, widgetWidth, spanNs, sampleCount);
    const int ch = chart->addChannel(QStringLiteral("Trace"), kDropoutColour, 1.0);
    std::vector<double> values(static_cast<size_t>(sampleCount), 50.0);
    values[static_cast<size_t>(spikeIndex)] = 90.0; // single-sample spike
    if (mode == StripChart::XAxisMode::Time) {
        chart->addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(sampleDtNs), values.data(), sampleCount);
    } else {
        for (double v : values) {
            chart->addDataPoint(ch, v);
        }
    }

    const QImage img = renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    QCOMPARE(frame.channels[0].path, RenderPath::Decimated);
    const internal::BucketGrid grid(frame.gridNum, frame.gridDen);
    const qint64 k = grid.bucketOf(mode == StripChart::XAxisMode::Time ? spikeIndex * sampleDtNs : spikeIndex);
    const int col = deviceColumnOfBucket(frame, k);

    // Columns with trace pixels clearly above the 50.0 baseline
    const double dpr = chart->devicePixelRatioF();
    const int baselineDev = static_cast<int>(std::floor(mapY(*chart, 50.0) * dpr));
    const int top = plotTopDev(frame);
    const int left = static_cast<int>(std::floor(frame.plotLeftDev + 1e-9));
    std::vector<int> spikeCols;
    for (int x = left; x < left + frame.widthDev; ++x) {
        if (!traceYAtColumn(img, kDropoutColour, x, top, baselineDev - static_cast<int>(std::ceil(3 * dpr)), 60).empty()) {
            spikeCols.push_back(x);
        }
    }
    QString cols;
    for (int c : spikeCols) cols += QString::number(c) + QLatin1Char(' ');
    QVERIFY2(spikeCols.size() == 1 && spikeCols.front() == col,
             qPrintable(QString("spike columns [%1], expected only %2").arg(cols.trimmed()).arg(col)));
}

void tst_StripChart::rawDecimatedContinuity_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::newRow("Time") << static_cast<int>(StripChart::XAxisMode::Time);
    QTest::newRow("SampleIndex") << static_cast<int>(StripChart::XAxisMode::SampleIndex);
}

void tst_StripChart::rawDecimatedContinuity()
{
    QFETCH(int, axisMode);
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);

    // A ramp: every sample has a unique value, so a vertex's y identifies its sample.
    auto build = [&](StripChart::DecimationMode dm, int n, int capacity) {
        auto chart = makeChart(1054, 300, 0.0, 100.0);
        chart->setXAxisMode(mode);
        chart->setTimeSpan(std::chrono::seconds(1));
        chart->setCapacity(capacity);
        chart->setDecimationMode(dm);
        chart->setGridVisible(false);
        chart->setLegendVisible(false);
        const int ch = chart->addChannel(QStringLiteral("Ramp"), kDropoutColour, 1.0);
        const qint64 dt = 1000000000LL / n; // the samples fill the window
        for (int i = 0; i < n; ++i) {
            const double v = 10.0 + 80.0 * i / capacity;
            if (mode == StripChart::XAxisMode::Time) {
                chart->addSample(ch, std::chrono::nanoseconds(i * dt), v);
            } else {
                chart->addDataPoint(ch, v);
            }
        }
        renderToImage(*chart);
        return chart;
    };
    // Points (x, y) of the trace, keyed by y
    auto points = [](const StripChart &chart) {
        std::vector<QPointF> pts;
        for (const auto &seg : internal::StripChartTestAccess::lastFrame(chart).channels[0].segments) {
            pts.insert(pts.end(), seg.begin(), seg.end());
        }
        return pts;
    };
    auto compare = [&](const StripChart &rawChart, const StripChart &decChart) {
        const auto raw = points(rawChart);
        auto dec = points(decChart);
        const double dpr = rawChart.devicePixelRatioF();
        QVERIFY(!dec.empty());
        // Ramp: samples drawn only in the decimated chart (part 2) have a value above the raw maximum
        double rawTop = raw.front().y();
        for (const QPointF &r : raw) rawTop = std::min(rawTop, r.y());
        dec.erase(std::remove_if(dec.begin(), dec.end(), [&](const QPointF &d) { return d.y() < rawTop - 1e-9; }), dec.end());
        int matched = 0;
        double worst = 0.0;
        for (const QPointF &d : dec) {
            for (const QPointF &r : raw) {
                if (std::abs(r.y() - d.y()) < 1e-9) {
                    worst = std::max(worst, std::abs(r.x() - d.x()) * dpr);
                    ++matched;
                    break;
                }
            }
        }
        QVERIFY2(matched == static_cast<int>(dec.size()), qPrintable(QString("matched %1 of %2 vertices").arg(matched).arg(dec.size())));
        QVERIFY2(worst <= 1.0, qPrintable(QString("raw and decimated positions of the same sample differ by %1 device px").arg(worst)));
    };

    auto probe = makeChart(1054, 300);
    const int widthDev = qRound(1000 * probe->devicePixelRatioF());

    // 1. The same data drawn raw (Off) and decimated (Always)
    {
        const int n = widthDev + 50;
        auto rawChart = build(StripChart::DecimationMode::Off, n, n);
        auto decChart = build(StripChart::DecimationMode::Always, n, n);
        QCOMPARE(internal::StripChartTestAccess::lastFrame(*rawChart).channels[0].path, RenderPath::Raw);
        QCOMPARE(internal::StripChartTestAccess::lastFrame(*decChart).channels[0].path, RenderPath::Decimated);
        compare(*rawChart, *decChart);
    }
    // 2. Auto just below and just above W_dev visible samples (SampleIndex: the window does not move)
    if (mode == StripChart::XAxisMode::SampleIndex) {
        const int capacity = 4 * widthDev;
        auto below = build(StripChart::DecimationMode::Auto, widthDev, capacity);
        auto above = build(StripChart::DecimationMode::Auto, widthDev + 1, capacity);
        QCOMPARE(internal::StripChartTestAccess::lastFrame(*below).channels[0].path, RenderPath::Raw);
        QCOMPARE(internal::StripChartTestAccess::lastFrame(*above).channels[0].path, RenderPath::Decimated);
        compare(*below, *above);
    }
}

void tst_StripChart::segmentEndColumnsAreDrawn_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::newRow("Time") << static_cast<int>(StripChart::XAxisMode::Time);
    QTest::newRow("SampleIndex") << static_cast<int>(StripChart::XAxisMode::SampleIndex);
}

void tst_StripChart::segmentEndColumnsAreDrawn()
{
    // A flat signal gives every bucket zero vertical extent, the case where an aliased cosmetic
    // polyline leaves out its first or last pixel. The columns at both ends of every segment, and
    // a segment made of a single bucket, must still be drawn.
    QFETCH(int, axisMode);
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);
    constexpr int n = 1000000;
    constexpr qint64 sampleDtNs = 1000;
    constexpr qint64 spanNs = 1000000000;

    auto chart = makeDropoutChart(mode, 1054, spanNs, n);
    const double dpr = chart->devicePixelRatioF();
    const int widthDev = qRound(1000 * dpr);
    // Buckets of the window (the data fill exactly one window in both modes)
    const internal::BucketGrid grid = (mode == StripChart::XAxisMode::Time)
        ? internal::BucketGrid(widthDev, spanNs)
        : internal::BucketGrid(widthDev, n);
    auto midSampleOf = [&](qint64 k) {
        const qint64 b0 = grid.boundary(k);
        const qint64 b1 = grid.boundary(k + 1);
        const qint64 mid = b0 + (b1 - b0) / 2;
        return (mode == StripChart::XAxisMode::Time) ? mid / sampleDtNs : mid;
    };
    const qint64 kGap = widthDev / 4;       // one blank bucket: a segment ends before it, one starts after
    const qint64 kIsolated = widthDev / 2;  // blank buckets on both sides: a one-bucket segment
    const std::vector<qint64> nans = {midSampleOf(kGap), midSampleOf(kIsolated - 1), midSampleOf(kIsolated + 1)};

    const int ch = chart->addChannel(QStringLiteral("Flat"), kDropoutColour, 1.0);
    std::vector<double> values(static_cast<size_t>(n), 50.0);
    for (qint64 i : nans) values[static_cast<size_t>(i)] = std::numeric_limits<double>::quiet_NaN();
    if (mode == StripChart::XAxisMode::Time) {
        chart->addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(sampleDtNs), values.data(), n);
    } else {
        for (double v : values) chart->addDataPoint(ch, v);
    }

    const QImage img = renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    QCOMPARE(frame.channels[0].path, RenderPath::Decimated);
    QCOMPARE(internal::BucketGrid(frame.gridNum, frame.gridDen), grid);
    QCOMPARE(frame.kStart, qint64(0));
    QCOMPARE(frame.channels[0].segments.size(), 4UL); // [0, kGap), (kGap, kIso-1), {kIso}, (kIso+1, end]

    auto lit = [&](qint64 k) { return columnHasTrace(img, *chart, deviceColumnOfBucket(frame, k)); };
    QVERIFY2(lit(kGap - 1), "last column before the gap not drawn");
    QVERIFY2(!lit(kGap), "gap column drawn");
    QVERIFY2(lit(kGap + 1), "first column after the gap not drawn");
    QVERIFY2(!lit(kIsolated - 1), "gap column drawn");
    QVERIFY2(lit(kIsolated), "one-bucket segment not drawn");
    QVERIFY2(!lit(kIsolated + 1), "gap column drawn");
}

void tst_StripChart::sampleIndexCapacityBelowWidth()
{
    // DecimationMode::Always with fewer samples than device columns: the grid has more buckets than
    // samples (num > den). Every retained sample must still be drawn in the column of its bucket,
    // after the buffer has wrapped several times.
    constexpr int capacity = 300;
    auto chart = makeDropoutChart(StripChart::XAxisMode::SampleIndex, 1054, 0, capacity);
    const int ch = chart->addChannel(QStringLiteral("Ramp"), kDropoutColour, 1.0);
    constexpr int total = 1234; // four wraps
    for (int i = 0; i < total; ++i) {
        chart->addDataPoint(ch, 10.0 + 80.0 * (i % capacity) / capacity + 0.01 * (i / capacity));
    }
    renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    const double dpr = chart->devicePixelRatioF();
    QCOMPARE(frame.channels[0].path, RenderPath::Decimated);
    const internal::BucketGrid grid(frame.gridNum, frame.gridDen);
    QCOMPARE(grid, internal::BucketGrid(frame.widthDev, capacity));
    QVERIFY(grid.num() > grid.den());
    const auto m = frameXMapping(*chart);

    // One vertex per retained sample (at most one sample per bucket), at its bucket's column centre
    std::vector<QPointF> pts;
    for (const auto &seg : frame.channels[0].segments) pts.insert(pts.end(), seg.begin(), seg.end());
    QCOMPARE(static_cast<int>(pts.size()), capacity);
    for (int j = 0; j < capacity; ++j) {
        const qint64 i = total - capacity + j;
        const qint64 k = grid.bucketOf(i);
        QVERIFY(k >= frame.kStart && k < frame.kStart + frame.numBuckets);
        QVERIFY2(std::abs(pts[static_cast<size_t>(j)].x() * dpr - m.vertexDev(k)) < 1e-6,
                 qPrintable(QString("sample %1: x %2, expected %3").arg(i).arg(pts[static_cast<size_t>(j)].x() * dpr).arg(m.vertexDev(k))));
    }
    // The newest sample is within capacity / W_dev columns of the right edge
    const qint64 kNewest = grid.bucketOf(total - 1);
    QVERIFY(kNewest - frame.kStart >= frame.numBuckets - 1 - (frame.widthDev + capacity - 1) / capacity);
}

void tst_StripChart::timeSpanShorterThanWidth_data()
{
    QTest::addColumn<int>("labelFormat");
    QTest::newRow("Relative labels") << static_cast<int>(StripChart::TimeLabelFormat::Relative);
    QTest::newRow("Absolute labels") << static_cast<int>(StripChart::TimeLabelFormat::Absolute);
}

void tst_StripChart::timeSpanShorterThanWidth()
{
    // A time span shorter than W_dev ns uses 1 ns buckets (no more buckets than nanoseconds), so
    // the window is W_dev ns wide. Samples must land in the column of their nanosecond, and the
    // X-axis labels must describe that effective window, not the configured span.
    QFETCH(int, labelFormat);
    constexpr qint64 spanNs = 400;
    auto chart = makeDropoutChart(StripChart::XAxisMode::Time, 1054, spanNs, 100000);
    chart->setGridVisible(true);
    chart->setTimeLabelFormat(static_cast<StripChart::TimeLabelFormat>(labelFormat));
    const int ch = chart->addChannel(QStringLiteral("Ramp"), kDropoutColour, 1.0);
    const double dpr = chart->devicePixelRatioF();
    const int widthDev = qRound(1000 * dpr);
    const qint64 tFirst = 1000000;
    const int n = 3 * widthDev; // 1 sample per ns, three windows' worth
    for (int i = 0; i < n; ++i) {
        chart->addSample(ch, std::chrono::nanoseconds(tFirst + i), 10.0 + 80.0 * i / n);
    }
    renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    QCOMPARE(frame.channels[0].path, RenderPath::Decimated);
    const internal::BucketGrid grid(frame.gridNum, frame.gridDen);
    QCOMPARE(grid, internal::BucketGrid(widthDev, widthDev)); // 1 ns buckets
    QCOMPARE(frame.tEnd - frame.tStart, qint64(widthDev));    // effective window: W_dev ns
    QCOMPARE(frame.tEnd, tFirst + n);
    const auto m = frameXMapping(*chart);

    // Every visible sample is a bucket of its own, drawn at the centre of its column
    std::vector<QPointF> pts;
    for (const auto &seg : frame.channels[0].segments) pts.insert(pts.end(), seg.begin(), seg.end());
    QCOMPARE(static_cast<int>(pts.size()), widthDev);
    for (int c = 0; c < widthDev; ++c) {
        const qint64 t = frame.tStart + c;
        QCOMPARE(grid.bucketOf(t), frame.kStart + c);
        QVERIFY(std::abs(pts[static_cast<size_t>(c)].x() * dpr - m.vertexDev(grid.bucketOf(t))) < 1e-6);
    }

    // X-axis labels describe the effective window [tStart, tEnd): relative ticks are those of the
    // window's length (offset tNs from the window start, labelled relative to the right edge), and
    // every tick sits where the data of its time is drawn (within half a device pixel).
    QVERIFY(!frame.xTicks.empty());
    const bool relative = (labelFormat == static_cast<int>(StripChart::TimeLabelFormat::Relative));
    if (relative) {
        const auto expected = internal::computeTimeTicks(0, frame.tEnd - frame.tStart, frame.plotRect.width(), 60.0);
        QCOMPARE(frame.xTicks.size(), expected.ticks.size());
        for (size_t i = 0; i < expected.ticks.size(); ++i) {
            QCOMPARE(frame.xTicks[i].tNs, expected.ticks[i].tNs);
            QCOMPARE(frame.xTicks[i].label, expected.ticks[i].label);
        }
    }
    for (const auto &tick : frame.xTicks) {
        const qint64 t = relative ? frame.tStart + tick.tNs : tick.tNs;
        const double expectedDev = m.positionDev(t);
        QVERIFY2(std::abs(tick.xPos * dpr - expectedDev) <= 0.5,
                 qPrintable(QString("tick '%1' (t = %2 ns) at device x %3, data of that time at %4; window [%5, %6) ns")
                                .arg(tick.label).arg(t).arg(tick.xPos * dpr, 0, 'f', 2).arg(expectedDev, 0, 'f', 2)
                                .arg(frame.tStart).arg(frame.tEnd)));
    }
}

void tst_StripChart::streamingDoesNotRebuildDecimationStream_data()
{
    QTest::addColumn<int>("axisMode");
    QTest::addColumn<int>("capacity");
    QTest::addColumn<int>("widgetWidth");
    const int time = static_cast<int>(StripChart::XAxisMode::Time);
    const int index = static_cast<int>(StripChart::XAxisMode::SampleIndex);
    // Capacities just below a multiple of the plot width (99950 = 1000 * 100 - 50; 4M on 1920 px
    // = 1919.4 buckets of 2084 samples): before #12 the SampleIndex window alternated between
    // W_dev and W_dev + 1 buckets while streaming, which changed the stream's ring size (issue #7)
    QTest::newRow("SampleIndex, 99950 samples, 1000 px") << index << 99950 << 1054;
    QTest::newRow("SampleIndex, 4M samples, 1920 px") << index << 4000000 << 1974;
    QTest::newRow("Time, 99950 samples, 1000 px") << time << 99950 << 1054;
    QTest::newRow("Time, 4M samples, 1920 px") << time << 4000000 << 1974;
}

void tst_StripChart::streamingDoesNotRebuildDecimationStream()
{
    // While data streams in, the incremental decimation stream is updated sample by sample; a full
    // O(N) rebuild is needed only when the geometry changes (first paint, resize, span, DPR).
    // Periodic rebuilds while streaming caused 50-190 ms frames (issue #7).
    QFETCH(int, axisMode);
    QFETCH(int, capacity);
    QFETCH(int, widgetWidth);
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);
    constexpr qint64 spanNs = 10000000000LL; // 10 s window
    const qint64 dtNs = spanNs / capacity;
    const int chunk = std::max(1, static_cast<int>(std::lround(capacity / 10.0 / 60.0))); // 1/60 s of data

    auto chart = makeDropoutChart(mode, widgetWidth, spanNs, capacity);
    const int ch = chart->addChannel(QStringLiteral("Stream"), kDropoutColour, 1.0);
    qint64 next = 0;
    auto append = [&](qint64 count) {
        appendDropoutSamples(*chart, ch, next, count, dtNs, capacity, {});
        next += count;
    };
    append(capacity); // full buffer

    renderToImage(*chart);
    QCOMPARE(internal::StripChartTestAccess::lastFrame(*chart).channels[0].path, RenderPath::Decimated);
    const quint64 afterFirstPaint = internal::StripChartTestAccess::streamRebuildCount(*chart, ch);
    QCOMPARE(afterFirstPaint, quint64(1)); // the first decimated paint builds the stream

    std::set<qint64> windowStarts;
    for (int frame = 0; frame < 60; ++frame) {
        append(chunk);
        renderToImage(*chart);
        windowStarts.insert(internal::StripChartTestAccess::lastFrame(*chart).kStart);
        QVERIFY2(internal::StripChartTestAccess::streamRebuildCount(*chart, ch) == afterFirstPaint,
                 qPrintable(QString("decimation stream rebuilt while streaming (frame %1, %2 rebuilds)")
                                .arg(frame).arg(internal::StripChartTestAccess::streamRebuildCount(*chart, ch))));
    }
    QVERIFY(windowStarts.size() > 10); // the window really scrolled

    // The incremental stream still matches the data: the last frame's buckets equal a batch
    // decimation of the retained samples
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    const internal::BucketGrid grid(frame.gridNum, frame.gridDen);
    std::vector<qint64> keys;
    std::vector<double> vals;
    for (qint64 i = next - capacity; i < next; ++i) {
        keys.push_back(mode == StripChart::XAxisMode::Time ? i * dtNs : i);
        vals.push_back(smoothValue(i, capacity));
    }
    const auto batch = internal::M4Decimator::decimateToSegments(keys.data(), vals.data(), keys.size(), grid,
                                                                 frame.kStart, frame.numBuckets, 0);
    std::size_t batchVertices = 0;
    for (const auto &seg : batch) batchVertices += seg.size();
    std::size_t drawnVertices = 0;
    for (const auto &seg : frame.channels[0].segments) drawnVertices += static_cast<std::size_t>(seg.size());
    QCOMPARE(drawnVertices, batchVertices);
}

void tst_StripChart::yAxisLabelsHaveNoNegativeZero_data()
{
    QTest::addColumn<int>("divisions");
    for (int d : {5, 6, 8, 10}) {
        QTest::addRow("%d divisions", d) << d;
    }
}

// #46: with the range -0.1..0.1, the ticks closest to zero (e.g. -0.0333 with 6 divisions) are
// labelled "0.0", never "-0.0"
void tst_StripChart::yAxisLabelsHaveNoNegativeZero()
{
    QFETCH(int, divisions);
    auto chart = makeChart(800, 400, -0.1, 0.1);
    chart->setHorizontalDivisions(divisions);
    renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

    QCOMPARE(static_cast<int>(frame.yTicks.size()), divisions + 1);
    const YTickDiagnostic *closest = nullptr;
    for (const auto &tick : frame.yTicks) {
        QVERIFY2(tick.label != QStringLiteral("-0.0") && tick.label != QStringLiteral("-0"),
                 qPrintable(QStringLiteral("tick %1 labelled '%2'").arg(tick.value).arg(tick.label)));
        if (!closest || std::abs(tick.value) < std::abs(closest->value)) {
            closest = &tick;
        }
    }
    QCOMPARE(closest->label, QStringLiteral("0.0"));
}

void tst_StripChart::legendHasNoNegativeZero_data()
{
    QTest::addColumn<double>("value");
    QTest::addColumn<QString>("expected");
    QTest::addRow("-0.0157") << -0.0157 << QStringLiteral("Sine: 0.0");
    // Protection only: with Qt 6 QString::number(-0.0, 'f', 1) is already "0.0", so this row does not fail on main
    QTest::addRow("-0.0") << -0.0 << QStringLiteral("Sine: 0.0");
    QTest::addRow("-0.05 (rounding boundary)") << -0.05 << QStringLiteral("Sine: -0.1");
    QTest::addRow("0.04") << 0.04 << QStringLiteral("Sine: 0.0");
    QTest::addRow("-1.24") << -1.24 << QStringLiteral("Sine: -1.2");
    QTest::addRow("NaN") << std::numeric_limits<double>::quiet_NaN() << QStringLiteral("Sine: ---");
}

// #46: the legend readout of a negative value that rounds to zero has no sign
void tst_StripChart::legendHasNoNegativeZero()
{
    QFETCH(double, value);
    QFETCH(QString, expected);
    auto chart = makeChart(800, 400, -1.5, 1.5);
    const int ch = chart->addChannel(QStringLiteral("Sine"), Qt::cyan);
    chart->addDataPoint(ch, 0.5);
    chart->addDataPoint(ch, value);
    renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);
    QCOMPARE(static_cast<int>(frame.legendLabels.size()), 1);
    QCOMPARE(frame.legendLabels.front(), expected);
}

// #46: absolute time labels without a time origin ("<t> s") of ticks just below zero (explicit
// negative timestamps) have no sign when they round to zero
void tst_StripChart::absoluteSecondsLabelsHaveNoNegativeZero()
{
    auto chart = makeChart(800, 400, 0.0, 1.0);
    chart->setXAxisMode(StripChart::XAxisMode::Time);
    chart->setTimeLabelFormat(StripChart::TimeLabelFormat::Absolute);
    chart->setTimeSpan(std::chrono::milliseconds(200));
    const int ch = chart->addChannel(QStringLiteral("Ch0"), Qt::green);
    for (int ms = -300; ms <= 0; ms += 5) {
        chart->addSample(ch, std::chrono::milliseconds(ms), 0.5);
    }
    renderToImage(*chart);
    const auto &frame = internal::StripChartTestAccess::lastFrame(*chart);

    QVERIFY(frame.xTicks.size() >= 2);
    bool belowZero = false;
    for (const auto &tick : frame.xTicks) {
        belowZero = belowZero || (tick.tNs < 0 && tick.tNs > -50000000);
        QVERIFY2(!tick.label.startsWith(QStringLiteral("-0.0 ")) && !tick.label.startsWith(QStringLiteral("-0 ")),
                 qPrintable(QStringLiteral("tick at %1 ns labelled '%2'").arg(tick.tNs).arg(tick.label)));
    }
    QVERIFY2(belowZero, "no tick between -50 ms and 0: the case is not exercised");
}

// #51: the axis labels (empty chart, legend hidden) are drawn into the static cache with the widget font (family, style, spacing),
// not with the application font: a widget created with another font of the same size draws other
// text pixels. Before #51 the cached text used painter.font() (the application font on a pixmap)
void tst_StripChart::labelsFollowWidgetFont()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<StripChart>();
        if (font) g->setFont(*font);
        g->setLegendVisible(false);
        g->resize(640, 300);
        return g;
    };
    auto standard = make(nullptr);
    const QFont other = FontTest::otherFont(standard->font());
    auto changed = make(&other);
    const int pixels = FontTest::differingPixelCount(FontTest::render(*standard), FontTest::render(*changed));
    QVERIFY2(pixels > 100, qPrintable(QStringLiteral("only %1 pixels change with the widget font").arg(pixels)));
}

// #51: setFont() on a widget already painted refreshes its cached background: the next render is
// identical to that of a widget created with the new font. Before #51 the cache was invalidated only
// by the widget's own setters, resizeEvent() and (some widgets) PaletteChange, not by FontChange
void tst_StripChart::fontChangeRefreshesCache()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<StripChart>();
        if (font) g->setFont(*font);
        g->resize(640, 300);
        return g;
    };
    // Shown widgets: grab() on a hidden one delivers a pending resize event, which refreshes the
    // cache by itself and hides the defect
    auto painted = make(nullptr);
    painted->show();
    QVERIFY(QTest::qWaitForWindowExposed(painted.get()));
    const QFont other = FontTest::otherFont(painted->font());
    const QImage before = FontTest::render(*painted);
    painted->setFont(other);
    QCoreApplication::processEvents();
    const QImage after = FontTest::render(*painted);
    // One window at a time: the active one has the focus (IndustrialKnob draws a focus ring)
    painted->hide();
    auto fresh = make(&other);
    fresh->show();
    QVERIFY(QTest::qWaitForWindowExposed(fresh.get()));
    const QImage expected = FontTest::render(*fresh);
    QVERIFY2(FontTest::differingPixelCount(before, expected) > 100, "the font change is not visible: the test checks nothing");
    const int stale = FontTest::differingPixelCount(after, expected);
    QVERIFY2(stale == 0, qPrintable(QStringLiteral("%1 pixels differ from a widget created with the new font").arg(stale)));
}

// #51 (size rule (b)): the axis label font is scaled by the ratio between the effective pixel sizes
// of the widget font and of the application font, and so is the room kept for the labels: with a
// widget font twice as large the left, top and bottom plot margins are twice as large
void tst_StripChart::plotMarginsFollowWidgetFontSize()
{
    StripChart standard;
    standard.resize(640, 300);
    StripChart large;
    large.setFont(FontTest::doubledFont());
    large.resize(640, 300);
    const QRectF a = internal::StripChartTestAccess::plotArea(standard);
    const QRectF b = internal::StripChartTestAccess::plotArea(large);
    QCOMPARE(b.left(), 2.0 * a.left());
    QCOMPARE(b.top(), 2.0 * a.top());
    QCOMPARE(b.right(), a.right()); // the right margin holds no labels
    QCOMPARE(300.0 - b.bottom(), 2.0 * (300.0 - a.bottom()));
}

QTEST_MAIN(tst_StripChart)
#include "tst_StripChart.moc"
