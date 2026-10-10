// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/StripChart.h>
#include <internal/StripChartGeometry.h>
#include <internal/M4Decimator.h>
#include <internal/StripChartTestAccess.h>
#include <QtGui/QPixmap>
#include <QtGui/QImage>

#include <cmath>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets;

class TestSignalSource : public QObject
{
    Q_OBJECT
public:
    void sendPoint(int ch, double v) { Q_EMIT pointReady(ch, v); }
Q_SIGNALS:
    void pointReady(int ch, double v);
};

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
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();

    // New tests for high-rate DAQ & M4 decimation
    void plusInfAutoscaleRegression();
    void stepInterpolation();
    void addSynchronousSamplesTest();
    void tickSelection125();
    void invalidInputsValidation();
    void capacityClamp24();
    void outOfOrderRejection();
    void gapDetection();
    void windowQuantization();
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

void tst_StripChart::renderOffscreen()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("Wave"), Qt::green);
    for (int i = 0; i < 50; ++i) {
        chart.addDataPoint(ch, std::sin(i * 0.1) * 50.0 + 50.0);
    }
    chart.resize(400, 250);

    QPixmap pix(chart.size());
    chart.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_StripChart::extremeResizeNoCrash()
{
    StripChart chart;
    chart.resize(2, 2);
    QPixmap pixSmall(chart.size());
    chart.render(&pixSmall);

    chart.resize(3840, 2160);
    QPixmap pixLarge(chart.size());
    chart.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

void tst_StripChart::nanAndInfinityResilience()
{
    StripChart chart;
    int ch = chart.addChannel(QStringLiteral("Sensor"), Qt::green);
    chart.setAutoScaleY(true);

    // Initial valid points
    chart.addDataPoint(ch, 10.0);
    chart.addDataPoint(ch, 20.0);

    // Influx of Infinity should NOT blow Y-range to [-inf, +inf]
    chart.addDataPoint(ch, std::numeric_limits<double>::infinity());
    chart.addDataPoint(ch, -std::numeric_limits<double>::infinity());

    QVERIFY(std::isfinite(chart.yMinimum()));
    QVERIFY(std::isfinite(chart.yMaximum()));
    QVERIFY(chart.yMinimum() < chart.yMaximum());

    // Influx of NaN is stored as information (sensor fault) but ignored downstream
    chart.addDataPoint(ch, std::numeric_limits<double>::quiet_NaN());
    QVERIFY(std::isnan(chart.channelLatestValue(ch)));

    // Range setters with NaN/Inf must be rejected
    chart.setYRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
    QVERIFY(std::isfinite(chart.yMinimum()));

    // Chart must render cleanly with no painter errors
    chart.resize(400, 250);
    QPixmap pix(chart.size());
    chart.render(&pix);
    QVERIFY(!pix.isNull());
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

void tst_StripChart::stepInterpolation()
{
    StripChart chart;
    chart.setInterpolation(StripChart::Interpolation::Step);
    QCOMPARE(chart.interpolation(), StripChart::Interpolation::Step);

    int ch = chart.addChannel(QStringLiteral("Square"), Qt::red);
    chart.addDataPoint(ch, 0.0);
    chart.addDataPoint(ch, 10.0);
    chart.addDataPoint(ch, 10.0);
    chart.addDataPoint(ch, 0.0);

    chart.resize(300, 200);
    QPixmap pix(chart.size());
    chart.render(&pix);
    QVERIFY(!pix.isNull());
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

void tst_StripChart::tickSelection125()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);

    chart.setTimeSpan(std::chrono::seconds(10));
    QCOMPARE(chart.timeSpanSeconds(), 10.0);

    chart.setTimeSpan(std::chrono::milliseconds(500));
    QCOMPARE(chart.timeSpanSeconds(), 0.5);

    chart.setTimeSpan(std::chrono::microseconds(100));
    QCOMPARE(chart.timeSpanSeconds(), 0.0001);

    chart.resize(400, 200);
    QPixmap pix(chart.size());
    chart.render(&pix);
    QVERIFY(!pix.isNull());
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

void tst_StripChart::gapDetection()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    int ch = chart.addChannel(QStringLiteral("Pulses"), Qt::green);

    // Insert 1 ms sampling with a 50 ms pause
    std::vector<double> vals(6, 1.0);
    chart.addUniformSamples(ch, std::chrono::milliseconds(0), std::chrono::milliseconds(1), vals.data(), 6);

    // Pause of 50 ms
    chart.addSample(ch, std::chrono::milliseconds(55), 2.0);

    chart.resize(400, 200);
    QPixmap pix(chart.size());
    chart.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_StripChart::windowQuantization()
{
    StripChart chart;
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setTimeSpan(std::chrono::seconds(10));
    int ch = chart.addChannel(QStringLiteral("Sine"), Qt::magenta);

    chart.addSample(ch, std::chrono::nanoseconds(123456789), 42.0);

    chart.resize(800, 300);
    QPixmap pix(chart.size());
    chart.render(&pix);
    QVERIFY(!pix.isNull());
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

    // W_dev is 1000 device pixels.
    // At 1000 samples (<= W_dev): Decimation is OFF, penWidth 7.0 is preserved (thickness >= 5 px).
    // At 1001 samples (> W_dev): Decimation is ON, cosmetic 1px pen is used (thickness <= 2 px).

    int thickTimeOff = measureLineThickness(StripChart::XAxisMode::Time, 1000);
    int thickTimeOn = measureLineThickness(StripChart::XAxisMode::Time, 1001);
    QVERIFY2(thickTimeOff >= 5, qPrintable(QString("Time mode with 1000 samples should be raw (thick >= 5), got %1").arg(thickTimeOff)));
    QVERIFY2(thickTimeOn <= 2, qPrintable(QString("Time mode with 1001 samples should be decimated (cosmetic 1px <= 2), got %1").arg(thickTimeOn)));

    int thickIndexOff = measureLineThickness(StripChart::XAxisMode::SampleIndex, 1000);
    int thickIndexOn = measureLineThickness(StripChart::XAxisMode::SampleIndex, 1001);
    QVERIFY2(thickIndexOff >= 5, qPrintable(QString("SampleIndex mode with 1000 samples should be raw (thick >= 5), got %1").arg(thickIndexOff)));
    QVERIFY2(thickIndexOn <= 2, qPrintable(QString("SampleIndex mode with 1001 samples should be decimated (cosmetic 1px <= 2), got %1").arg(thickIndexOn)));
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

QTEST_MAIN(tst_StripChart)
#include "tst_StripChart.moc"
