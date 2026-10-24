// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #8: at steady state, the heap allocations of a StripChart render do not depend on the number of visible gaps
// (non-finite samples breaking the decimated trace). Built on Linux only, see tests/CMakeLists.txt.

#include "AllocationCounter.h"
#include "internal/StripChartTestAccess.h"

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

// The glow dot on the latest point is clipped around the blank columns of dropouts next to it (#11): that clip
// allocates (a QPainterPath of the blank columns, the painter state it saves) when a dropout lies within the glow's
// radius of the latest point, and only then. Its cost is therefore bounded by the glow's radius, not by the number
// of gaps. The gaps that test the segment buffers (#8) lie in the first 90 % of the window, away from the glow; a
// separate case puts one gap next to the glow.
constexpr qint64 kGapsEnd = kSamples * 9 / 10;
constexpr qint64 kGapNearGlow = kSamples - kSamples / 200; // about 2 device columns before the latest point

// A sine of kSamples samples in the 10 s window, broken by \p gaps single non-finite samples evenly spaced in the
// first 90 % of the window, and by one more next to the glow if \p gapNearGlow
std::vector<double> signal(int gaps, int channel, bool gapNearGlow)
{
    std::vector<double> v(kSamples);
    for (int i = 0; i < kSamples; ++i) v[static_cast<size_t>(i)] = 50.0 + 40.0 * std::sin(0.002 * i + channel);
    for (int g = 1; g <= gaps; ++g) {
        v[static_cast<size_t>(static_cast<qint64>(g) * kGapsEnd / (gaps + 1))] = std::numeric_limits<double>::quiet_NaN();
    }
    if (gapNearGlow) v[static_cast<size_t>(kGapNearGlow)] = std::numeric_limits<double>::quiet_NaN();
    return v;
}

void fill(StripChart &chart, StripChart::XAxisMode mode, int channels, int gaps, bool gapNearGlow = false)
{
    chart.setXAxisMode(mode);
    chart.setCapacity(kSamples);
    chart.setDecimationMode(StripChart::DecimationMode::Always);
    chart.setTimeSpan(std::chrono::nanoseconds(kSpanNs));
    chart.setYRange(0.0, 100.0);
    for (int c = 0; c < channels; ++c) {
        const int id = chart.addChannel(QStringLiteral("Ch%1").arg(c), kColors[c % 4]);
        const std::vector<double> v = signal(gaps, c, gapNearGlow);
        chart.addUniformSamples(id, std::chrono::nanoseconds(0), std::chrono::nanoseconds(kDtNs), v.data(), kSamples);
    }
    chart.resize(400, 300);
}

// The fewest allocations of a measured render of a chart at steady state (after the warm-up renders).
//
// The minimum over the measured renders, not each render: Qt's raster engine blends a large image (here the cached
// grid) in several threads, and QThreadPool allocates on this thread for each task it has to queue, which depends on
// thread scheduling (seen with Qt 6.4.2: 0 to 5 allocations of 2064 bytes per render, from QThreadPool::start() in
// QRasterPaintEngine::drawImage()). Those allocations only add to a render, so the fewest allocations of a render
// are those of the chart itself; an allocation per gap would raise them.
long long fewestAllocationsPerRender(StripChart::XAxisMode mode, int channels, int gaps, bool gapNearGlow)
{
    StripChart chart;
    fill(chart, mode, channels, gaps, gapNearGlow);
    internal::StripChartTestAccess::enableDiagnostics(chart, true);
    QPixmap pix(chart.size());
    chart.render(&pix);
    // The gaps are visible: every channel is drawn as one decimated segment more than it has gaps
    const size_t expected = static_cast<size_t>(gaps + (gapNearGlow ? 2 : 1));
    for (int c = 0; c < channels; ++c) {
        const size_t drawn = internal::StripChartTestAccess::lastFrame(chart).channels.at(static_cast<size_t>(c)).segments.size();
        if (drawn != expected) {
            qWarning("channel %d: %zu segments drawn, %zu expected", c, drawn, expected);
            return -1;
        }
    }
    internal::StripChartTestAccess::enableDiagnostics(chart, false);
    for (int i = 0; i < kWarmupRenders; ++i) chart.render(&pix);
    long long least = -1;
    long long most = -1;
    for (int i = 0; i < kMeasuredRenders; ++i) {
        AllocationScope scope;
        chart.render(&pix);
        least = least < 0 ? scope.count() : std::min(least, scope.count());
        most = std::max(most, scope.count());
    }
    qInfo("%d gaps per channel%s: %lld to %lld allocations per render", gaps, gapNearGlow ? " + 1 next to the glow" : "",
          least, most);
    return least;
}

} // namespace

class tst_StripChartAllocations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void counterSeesLibraryAllocations();
    void allocationsDoNotGrowWithGaps_data();
    void allocationsDoNotGrowWithGaps();
    void glowGapCostDoesNotGrowWithGaps_data();
    void glowGapCostDoesNotGrowWithGaps();
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

// The fewest allocations of a render are the same with 0, 10 and 100 visible gaps per channel, away from the glow
void tst_StripChartAllocations::allocationsDoNotGrowWithGaps()
{
    QFETCH(int, mode);
    QFETCH(int, channels);
    const auto m = static_cast<StripChart::XAxisMode>(mode);
    const long long reference = fewestAllocationsPerRender(m, channels, 0, false);
    QVERIFY(reference > 0);
    for (int gaps : {10, 100}) {
        const long long least = fewestAllocationsPerRender(m, channels, gaps, false);
        QVERIFY(least > 0);
        QVERIFY2(least == reference,
                 qPrintable(QStringLiteral("%1 gaps per channel: at least %2 allocations per render, %3 without gaps")
                                .arg(gaps).arg(least).arg(reference)));
    }
}

void tst_StripChartAllocations::glowGapCostDoesNotGrowWithGaps_data()
{
    allocationsDoNotGrowWithGaps_data();
}

// With one gap next to the glow, the fewest allocations of a render are the same with 0, 10 and 100 more gaps per
// channel: the glow's clip costs the same whatever the number of gaps elsewhere
void tst_StripChartAllocations::glowGapCostDoesNotGrowWithGaps()
{
    QFETCH(int, mode);
    QFETCH(int, channels);
    const auto m = static_cast<StripChart::XAxisMode>(mode);
    const long long withoutGaps = fewestAllocationsPerRender(m, channels, 0, false);
    const long long reference = fewestAllocationsPerRender(m, channels, 0, true);
    QVERIFY(withoutGaps > 0 && reference > 0);
    // The gap next to the glow does make the glow clip around it (this case tests that clip)
    QVERIFY2(reference > withoutGaps,
             qPrintable(QStringLiteral("a gap next to the glow: %1 allocations per render, %2 without it; the glow "
                                       "is not clipped").arg(reference).arg(withoutGaps)));
    for (int gaps : {10, 100}) {
        const long long least = fewestAllocationsPerRender(m, channels, gaps, true);
        QVERIFY(least > 0);
        QVERIFY2(least == reference,
                 qPrintable(QStringLiteral("%1 gaps per channel and one next to the glow: at least %2 allocations per "
                                           "render, %3 with only the one next to the glow")
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
        internal::StripChartTestAccess::enableDiagnostics(chart, true);
        QImage img(chart.size(), QImage::Format_RGB32);
        img.fill(Qt::black);
        chart.render(&img);
        chart.render(&img); // a second frame: the segment buffers of the first one exist
        const auto &frame = internal::StripChartTestAccess::lastFrame(chart);
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
