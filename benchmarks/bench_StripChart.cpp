// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/StripChart.h>
#include <QtTest/QtTest>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtCore/QElapsedTimer>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

using namespace QtIndustrialWidgets;

enum class SignalType {
    SmoothSine,
    BroadbandNoise,
    SpikesAndNaN
};

static const char *signalTypeName(SignalType s) {
    switch (s) {
        case SignalType::SmoothSine: return "SmoothSine";
        case SignalType::BroadbandNoise: return "BroadbandNoise";
        case SignalType::SpikesAndNaN: return "SpikesAndNaN";
    }
    return "Unknown";
}

class BenchStripChart : public QObject {
    Q_OBJECT

private:
    static std::vector<double> generateSignal(SignalType type, int count, int channelIndex) {
        std::vector<double> data(count);
        double phaseOffset = channelIndex * 1.57079632679;
        for (int i = 0; i < count; ++i) {
            double baseSine = std::sin(i * 0.001 + phaseOffset) * 50.0 + 50.0;
            switch (type) {
                case SignalType::SmoothSine:
                    data[i] = baseSine;
                    break;
                case SignalType::BroadbandNoise: {
                    // Carrier sine + broadband noise ensuring every column has a min/max envelope
                    uint64_t rng = static_cast<uint64_t>(i + channelIndex * 1000003) * 6364136223846793005ULL + 1442695040888963407ULL;
                    double rand01 = static_cast<double>(rng >> 32) / 4294967296.0;
                    double noise = (rand01 - 0.5) * 10.0 + ((i % 2 == 0) ? 3.0 : -3.0);
                    data[i] = std::sin(i * 0.001 + phaseOffset) * 30.0 + 50.0 + noise;
                    break;
                }
                case SignalType::SpikesAndNaN:
                    if (i % 10000 == 0) {
                        data[i] = 500.0; // Single-sample spike
                    } else if (i % 25000 < 5) {
                        data[i] = std::numeric_limits<double>::quiet_NaN(); // Dropout
                    } else {
                        data[i] = baseSine;
                    }
                    break;
            }
        }
        return data;
    }

    struct StatResult {
        double minMs;
        double medianMs;
    };

    static StatResult computeStats(std::vector<double> &samples) {
        if (samples.empty()) return {0.0, 0.0};
        std::sort(samples.begin(), samples.end());
        double minVal = samples.front();
        double medVal = samples[samples.size() / 2];
        return {minVal, medVal};
    }

private Q_SLOTS:
    void initTestCase();
    void paintMatrix_data();
    void paintMatrix();
    void firstPaintAfterResize();
    void insertionThroughput_addSample_autoscaleOn();
    void insertionThroughput_addSample_autoscaleOff();
    void insertionThroughput_addUniformSamples_1M();
    void insertionThroughput_addDataPoint_legacy_100k();
    void cleanupTestCase();
};

void BenchStripChart::initTestCase()
{
    qputenv("QTEST_FUNCTION_TIMEOUT", "0");

    // Verify environment: offscreen, DPR 1
    StripChart testChart;
    testChart.resize(1974, 1080);
    QCOMPARE(testChart.devicePixelRatioF(), 1.0);

    qInfo("================================================================================");
    qInfo(" StripChart Benchmark Suite (O(W) Telemetry Engine)");
    qInfo(" Plot Width: 1920 device px (Widget width: 1974 px) | Target DPR: 1.0");
    qInfo("================================================================================");
}

void BenchStripChart::cleanupTestCase()
{
    qInfo("================================================================================");
    qInfo(" Benchmark Run Complete");
    qInfo("================================================================================");
}

void BenchStripChart::paintMatrix_data()
{
    QTest::addColumn<int>("channels");
    QTest::addColumn<int>("samples");
    QTest::addColumn<int>("signalType");
    QTest::addColumn<int>("axisMode");

    const int channelCounts[] = {1, 4};
    const int sampleCounts[] = {10000, 100000, 1000000, 4000000};
    const SignalType signalTypes[] = {SignalType::SmoothSine, SignalType::BroadbandNoise, SignalType::SpikesAndNaN};
    const StripChart::XAxisMode modes[] = {StripChart::XAxisMode::Time, StripChart::XAxisMode::SampleIndex};

    for (int ch : channelCounts) {
        for (int s : sampleCounts) {
            for (auto sig : signalTypes) {
                for (auto m : modes) {
                    const char *sName = (s == 10000) ? "10k" : (s == 100000) ? "100k" : (s == 1000000) ? "1M" : "4M";
                    const char *mName = (m == StripChart::XAxisMode::Time) ? "Time" : "SampleIndex";
                    QString tag = QStringLiteral("%1ch_%2_%3_%4")
                                      .arg(ch)
                                      .arg(QLatin1String(sName))
                                      .arg(QLatin1String(signalTypeName(sig)))
                                      .arg(QLatin1String(mName));
                    QTest::newRow(tag.toUtf8().constData())
                        << ch << s << static_cast<int>(sig) << static_cast<int>(m);
                }
            }
        }
    }
}

void BenchStripChart::paintMatrix()
{
    QFETCH(int, channels);
    QFETCH(int, samples);
    QFETCH(int, signalType);
    QFETCH(int, axisMode);

    auto sig = static_cast<SignalType>(signalType);
    auto mode = static_cast<StripChart::XAxisMode>(axisMode);

    StripChart chart;
    chart.setXAxisMode(mode);
    chart.setCapacity(samples);
    chart.setDecimationMode(StripChart::DecimationMode::Auto);

    if (mode == StripChart::XAxisMode::Time) {
        chart.setTimeSpan(std::chrono::seconds(10));
    }

    const QColor colors[] = {QColor(0, 229, 255), QColor(255, 170, 0), QColor(0, 230, 118), QColor(235, 59, 90)};

    qint64 spanNs = 10000000000LL;
    qint64 dtNs = std::max(1LL, spanNs / samples);

    for (int c = 0; c < channels; ++c) {
        int chId = chart.addChannel(QStringLiteral("Ch%1").arg(c), colors[c % 4]);
        std::vector<double> sigData = generateSignal(sig, samples, c);
        chart.addUniformSamples(chId, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs),
                                sigData.data(), samples);
    }

    chart.resize(1974, 1080);
    QPixmap pix(chart.size());

    // Warm-up render
    QElapsedTimer warmupTimer;
    warmupTimer.start();
    chart.render(&pix);
    double warmupMs = warmupTimer.nsecsElapsed() / 1e6;

    // Adaptive iterations: 1 if very slow (> 250 ms), 2 if large, 3 otherwise
    int iterations = (warmupMs > 250.0) ? 1 : ((samples >= 4000000) ? 2 : 3);
    std::vector<double> timings;
    timings.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        QElapsedTimer timer;
        timer.start();
        chart.render(&pix);
        timings.push_back(timer.nsecsElapsed() / 1e6);
    }

    StatResult stats = computeStats(timings);
    const char *sName = (samples == 10000) ? "10k" : (samples == 100000) ? "100k" : (samples == 1000000) ? "1M" : "4M";
    const char *mName = (mode == StripChart::XAxisMode::Time) ? "Time" : "SampleIndex";

    qInfo("[BENCHMARK_PAINT] %d ch | %4s | %-14s | %-11s | min: %6.2f ms | median: %6.2f ms",
          channels, sName, signalTypeName(sig), mName, stats.minMs, stats.medianMs);

    QBENCHMARK_ONCE {
        chart.render(&pix);
    }
}

void BenchStripChart::firstPaintAfterResize()
{
    // Stream rebuild cost: 4 channels x 1M noisy
    constexpr int channels = 4;
    constexpr int samples = 1000000;
    const QColor colors[] = {QColor(0, 229, 255), QColor(255, 170, 0), QColor(0, 230, 118), QColor(235, 59, 90)};

    std::vector<double> timings;
    timings.reserve(3);

    for (int trial = 0; trial < 3; ++trial) {
        StripChart chart;
        chart.setXAxisMode(StripChart::XAxisMode::Time);
        chart.setCapacity(samples);
        chart.setTimeSpan(std::chrono::seconds(10));
        qint64 dtNs = 10000000000LL / samples;

        for (int c = 0; c < channels; ++c) {
            int chId = chart.addChannel(QStringLiteral("Ch%1").arg(c), colors[c % 4]);
            std::vector<double> sigData = generateSignal(SignalType::BroadbandNoise, samples, c);
            chart.addUniformSamples(chId, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs),
                                    sigData.data(), samples);
        }

        // Render at initial size
        chart.resize(1000, 800);
        QPixmap pixInitial(chart.size());
        chart.render(&pixInitial);

        // Resize to 1974 x 1080 (W_dev = 1920)
        chart.resize(1974, 1080);
        QPixmap pix(chart.size());

        QElapsedTimer timer;
        timer.start();
        chart.render(&pix);
        timings.push_back(timer.nsecsElapsed() / 1e6);
    }

    StatResult stats = computeStats(timings);
    qInfo("[BENCHMARK_RESIZE] First paint after resize 4x1M noisy | min: %6.2f ms | median: %6.2f ms",
          stats.minMs, stats.medianMs);

    QVERIFY(stats.medianMs >= 0.0);
}

void BenchStripChart::insertionThroughput_addSample_autoscaleOn()
{
    // 1M samples already in buffer, autoscale ON
    constexpr int baseSamples = 1000000;
    constexpr int targetSamples = 100000;

    StripChart chart;
    chart.setCapacity(baseSamples + targetSamples + 1000);
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setTimeSpan(std::chrono::seconds(10));
    chart.setAutoScaleY(true);

    int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);
    std::vector<double> baseData = generateSignal(SignalType::BroadbandNoise, baseSamples, 0);
    chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(10000), baseData.data(), baseSamples);

    // Quick test to see if autoscale is eager (O(N)) or lazy (O(1))
    QElapsedTimer probeTimer;
    probeTimer.start();
    for (int i = 0; i < 50; ++i) {
        chart.addSample(ch, std::chrono::nanoseconds(baseSamples * 10000LL + i * 10000LL), 50.0);
    }
    qint64 probeNs = probeTimer.nsecsElapsed();
    double probeUsPerSample = (probeNs / 50.0) / 1000.0;

    int testCount = (probeUsPerSample > 100.0) ? 500 : targetSamples;

    std::vector<double> timingsMs;
    timingsMs.reserve(3);

    for (int trial = 0; trial < 3; ++trial) {
        qint64 startT = (baseSamples + 1000 + trial * testCount) * 10000LL;
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < testCount; ++i) {
            chart.addSample(ch, std::chrono::nanoseconds(startT + i * 10000LL), 50.0 + (i % 20));
        }
        qint64 elapsedNs = timer.nsecsElapsed();
        double projected100kMs = (static_cast<double>(elapsedNs) / testCount) * targetSamples / 1e6;
        timingsMs.push_back(projected100kMs);
    }

    StatResult stats = computeStats(timingsMs);
    double minUsPerSample = (stats.minMs * 1e3) / targetSamples;
    double medUsPerSample = (stats.medianMs * 1e3) / targetSamples;

    qInfo("[BENCHMARK_INSERTION] addSample autoscale ON (100k calls, 1M base) | min: %7.2f ms (%.3f us/call) | median: %7.2f ms (%.3f us/call)",
          stats.minMs, minUsPerSample, stats.medianMs, medUsPerSample);
}

void BenchStripChart::insertionThroughput_addSample_autoscaleOff()
{
    constexpr int baseSamples = 1000000;
    constexpr int targetSamples = 100000;

    StripChart chart;
    chart.setCapacity(baseSamples + targetSamples + 1000);
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setTimeSpan(std::chrono::seconds(10));
    chart.setAutoScaleY(false);

    int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);
    std::vector<double> baseData = generateSignal(SignalType::BroadbandNoise, baseSamples, 0);
    chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(10000), baseData.data(), baseSamples);

    std::vector<double> timingsMs;
    timingsMs.reserve(3);

    for (int trial = 0; trial < 3; ++trial) {
        qint64 startT = (baseSamples + 1000 + trial * targetSamples) * 10000LL;
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < targetSamples; ++i) {
            chart.addSample(ch, std::chrono::nanoseconds(startT + i * 10000LL), 50.0 + (i % 20));
        }
        qint64 elapsedNs = timer.nsecsElapsed();
        timingsMs.push_back(elapsedNs / 1e6);
    }

    StatResult stats = computeStats(timingsMs);
    double minUsPerSample = (stats.minMs * 1e3) / targetSamples;
    double medUsPerSample = (stats.medianMs * 1e3) / targetSamples;

    qInfo("[BENCHMARK_INSERTION] addSample autoscale OFF (100k calls, 1M base) | min: %7.2f ms (%.3f us/call) | median: %7.2f ms (%.3f us/call)",
          stats.minMs, minUsPerSample, stats.medianMs, medUsPerSample);
}

void BenchStripChart::insertionThroughput_addUniformSamples_1M()
{
    constexpr int samples = 1000000;
    std::vector<double> data = generateSignal(SignalType::BroadbandNoise, samples, 0);

    std::vector<double> timingsMs;
    timingsMs.reserve(3);

    for (int trial = 0; trial < 3; ++trial) {
        StripChart chart;
        chart.setCapacity(samples);
        chart.setXAxisMode(StripChart::XAxisMode::Time);
        int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);

        QElapsedTimer timer;
        timer.start();
        chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(1000), data.data(), samples);
        timingsMs.push_back(timer.nsecsElapsed() / 1e6);
    }

    StatResult stats = computeStats(timingsMs);
    qInfo("[BENCHMARK_INSERTION] addUniformSamples 1M block | min: %6.2f ms | median: %6.2f ms",
          stats.minMs, stats.medianMs);
}

void BenchStripChart::insertionThroughput_addDataPoint_legacy_100k()
{
    constexpr int targetSamples = 100000;

    std::vector<double> timingsMs;
    timingsMs.reserve(3);

    for (int trial = 0; trial < 3; ++trial) {
        StripChart chart;
        chart.setCapacity(targetSamples + 1000);
        chart.setXAxisMode(StripChart::XAxisMode::SampleIndex);
        chart.setAutoScaleY(false);
        int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < targetSamples; ++i) {
            chart.addDataPoint(ch, static_cast<double>(i % 100));
        }
        timingsMs.push_back(timer.nsecsElapsed() / 1e6);
    }

    StatResult stats = computeStats(timingsMs);
    double minUsPerSample = (stats.minMs * 1e3) / targetSamples;
    double medUsPerSample = (stats.medianMs * 1e3) / targetSamples;

    qInfo("[BENCHMARK_INSERTION] addDataPoint legacy 100k | min: %6.2f ms (%.3f us/call) | median: %6.2f ms (%.3f us/call)",
          stats.minMs, minUsPerSample, stats.medianMs, medUsPerSample);
}

QTEST_MAIN(BenchStripChart)
#include "bench_StripChart.moc"
