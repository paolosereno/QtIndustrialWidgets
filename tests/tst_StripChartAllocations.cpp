// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

// #8: at steady state, the heap allocations of a StripChart render do not depend on the number of visible gaps
// (non-finite samples breaking the decimated trace). Built on Linux only, see tests/CMakeLists.txt.

#include "AllocationCounter.h"
#include "StripChartTestAccess.h"

#include <QtIndustrialWidgets/StripChart.h>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtTest/QtTest>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets;
using AllocationCounter::AllocationScope;

namespace {

constexpr int kSamples = 100000;
constexpr qint64 kSpanNs = 10000000000LL; // 10 s window
constexpr qint64 kDtNs = kSpanNs / kSamples;
constexpr int kWarmupRenders = 5;
constexpr int kMeasuredRenders = 30;
const QColor kColors[] = {QColor(0, 229, 255), QColor(255, 170, 0), QColor(0, 230, 118), QColor(235, 59, 90)};

// A sine of kSamples samples in the 10 s window, broken by \p gaps single non-finite samples, evenly spaced
std::vector<double> signal(int gaps, int channel)
{
    std::vector<double> v(kSamples);
    for (int i = 0; i < kSamples; ++i) v[static_cast<size_t>(i)] = 50.0 + 40.0 * std::sin(0.002 * i + channel);
    for (int g = 1; g <= gaps; ++g) {
        v[static_cast<size_t>(static_cast<qint64>(g) * kSamples / (gaps + 1))] = std::numeric_limits<double>::quiet_NaN();
    }
    return v;
}

void fill(StripChart &chart, StripChart::XAxisMode mode, int channels, int gaps)
{
    chart.setXAxisMode(mode);
    chart.setCapacity(kSamples);
    chart.setDecimationMode(StripChart::DecimationMode::Always);
    chart.setTimeSpan(std::chrono::nanoseconds(kSpanNs));
    chart.setYRange(0.0, 100.0);
    for (int c = 0; c < channels; ++c) {
        const int id = chart.addChannel(QStringLiteral("Ch%1").arg(c), kColors[c % 4]);
        const std::vector<double> v = signal(gaps, c);
        chart.addUniformSamples(id, std::chrono::nanoseconds(0), std::chrono::nanoseconds(kDtNs), v.data(), kSamples);
    }
    chart.resize(400, 300);
}

// Allocations of the measured renders of a chart at steady state (after the warm-up renders)
std::vector<long long> allocationsPerRender(StripChart &chart)
{
    QPixmap pix(chart.size());
    for (int i = 0; i < kWarmupRenders; ++i) chart.render(&pix);
    std::vector<long long> counts;
    for (int i = 0; i < kMeasuredRenders; ++i) {
        AllocationScope scope;
        chart.render(&pix);
        counts.push_back(scope.count());
    }
    return counts;
}

} // namespace

class tst_StripChartAllocations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void counterSeesLibraryAllocations();
    void allocationsDoNotGrowWithGaps_data();
    void allocationsDoNotGrowWithGaps();
    void allNaNChannelAfterSegmentedChannel_data();
    void allNaNChannelAfterSegmentedChannel();
};

// The counter must see an allocation made inside the shared library (adding a channel stores it)
void tst_StripChartAllocations::counterSeesLibraryAllocations()
{
    StripChart chart;
    AllocationScope scope;
    chart.addChannel(QStringLiteral("Ch0"), Qt::red);
    QVERIFY2(scope.count() > 0, "the allocation counter does not see the allocations of the QtIndustrialWidgets library");
}

void tst_StripChartAllocations::allocationsDoNotGrowWithGaps_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("channels");
    for (auto mode : {StripChart::XAxisMode::Time, StripChart::XAxisMode::SampleIndex}) {
        for (int channels : {1, 4}) {
            QTest::newRow(qPrintable(QStringLiteral("%1, %2 ch")
                                         .arg(mode == StripChart::XAxisMode::Time ? QStringLiteral("Time")
                                                                                  : QStringLiteral("SampleIndex"))
                                         .arg(channels)))
                << static_cast<int>(mode) << channels;
        }
    }
}

// The fewest allocations of a measured render are the same with 0, 10 and 100 visible gaps per channel.
//
// The minimum over the measured renders, not each render: Qt's raster engine blends a large image (here the cached
// grid) in several threads, and QThreadPool allocates on this thread for each task it has to queue, which depends on
// thread scheduling (seen with Qt 6.4.2: 0 to 5 allocations of 2064 bytes per render, from QThreadPool::start() in
// QRasterPaintEngine::drawImage()). Those allocations only add to a render, so the fewest allocations of a render
// are those of the chart itself; an allocation per gap would raise them.
void tst_StripChartAllocations::allocationsDoNotGrowWithGaps()
{
    QFETCH(int, mode);
    QFETCH(int, channels);
    long long reference = -1;
    for (int gaps : {0, 10, 100}) {
        StripChart chart;
        fill(chart, static_cast<StripChart::XAxisMode>(mode), channels, gaps);
        StripChartTestAccess::enableDiagnostics(chart, true);
        QPixmap pix(chart.size());
        chart.render(&pix);
        // The gaps are visible: every channel is drawn as gaps + 1 decimated segments
        for (int c = 0; c < channels; ++c) {
            QCOMPARE(StripChartTestAccess::lastFrame(chart).channels.at(static_cast<size_t>(c)).segments.size(),
                     static_cast<size_t>(gaps + 1));
        }
        StripChartTestAccess::enableDiagnostics(chart, false);
        const std::vector<long long> counts = allocationsPerRender(chart);
        const long long most = *std::max_element(counts.begin(), counts.end());
        const long long least = *std::min_element(counts.begin(), counts.end());
        qInfo("%d gaps per channel: %lld to %lld allocations per render", gaps, least, most);
        if (gaps == 0) {
            reference = least;
            continue;
        }
        QVERIFY2(least == reference,
                 qPrintable(QStringLiteral("%1 gaps per channel: at least %2 allocations per render, %3 without gaps")
                                .arg(gaps).arg(least).arg(reference)));
    }
}

void tst_StripChartAllocations::allNaNChannelAfterSegmentedChannel_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("Time") << static_cast<int>(StripChart::XAxisMode::Time);
    QTest::newRow("SampleIndex") << static_cast<int>(StripChart::XAxisMode::SampleIndex);
}

// A channel holding only non-finite samples draws no segment and no glow dot, also when the channel drawn before it
// left more segment buffers (100 gaps): the plot is the same as without that channel
void tst_StripChartAllocations::allNaNChannelAfterSegmentedChannel()
{
    QFETCH(int, mode);
    auto render = [&](bool withNaNChannel) {
        StripChart chart;
        fill(chart, static_cast<StripChart::XAxisMode>(mode), 1, 100);
        if (withNaNChannel) {
            const int id = chart.addChannel(QStringLiteral("NaN"), Qt::magenta);
            const std::vector<double> v(kSamples, std::numeric_limits<double>::quiet_NaN());
            chart.addUniformSamples(id, std::chrono::nanoseconds(0), std::chrono::nanoseconds(kDtNs), v.data(), kSamples);
        }
        StripChartTestAccess::enableDiagnostics(chart, true);
        QImage img(chart.size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        chart.render(&img);
        chart.render(&img); // a second frame: the segment buffers of the first one exist
        const auto &frame = StripChartTestAccess::lastFrame(chart);
        if (withNaNChannel) {
            [&] { QVERIFY(frame.channels.at(1).segments.empty()); }();
        }
        return img.copy(frame.plotRect.toAlignedRect());
    };
    const QImage with = render(true);
    const QImage without = render(false);
    QCOMPARE(with.size(), without.size());
    QVERIFY2(with == without, "the plot differs when an all-NaN channel follows a channel with 100 gaps");
}

QTEST_MAIN(tst_StripChartAllocations)
#include "tst_StripChartAllocations.moc"
