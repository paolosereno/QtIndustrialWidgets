// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/StripChart.h>
#include <QtGui/QPixmap>

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

QTEST_MAIN(tst_StripChart)
#include "tst_StripChart.moc"
