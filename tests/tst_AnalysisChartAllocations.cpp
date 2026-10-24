// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #97: at steady state, the heap allocations of an AnalysisChart render do not depend on the number of decimated
// segments, whether the trace is broken by non-finite samples or by time gaps. Built on Linux only, see
// tests/CMakeLists.txt.

#include "AllocationCounter.h"

#include <QtIndustrialWidgets/AnalysisChart.h>
#include "internal/AnalysisChartTestAccess.h"

#include <QtGui/QPixmap>
#include <QtTest/QtTest>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets;
using AllocationCounter::AllocationScope;
using std::chrono::nanoseconds;

namespace {

constexpr int kSamples = 100000;
constexpr qint64 kDtNs = 10000;        // 10 us between samples
constexpr qint64 kJumpNs = 5000000;    // a time gap: 5 ms of samples left out
constexpr qint64 kGapThresholdNs = 1000000; // 1 ms: a jump of 5 ms breaks the trace, 10 us does not
constexpr int kWarmupRenders = 5;
constexpr int kMeasuredRenders = 30;
const QColor kColors[] = {QColor(0, 229, 255), QColor(255, 170, 0), QColor(0, 230, 118), QColor(235, 59, 90)};

enum class Break { NonFinite, TimeGap };

// A sine of samples every kDtNs over kSamples * kDtNs, broken \p breaks times, evenly spaced: by a non-finite sample, or
// by a time gap (the samples of kJumpNs left out). The first and the last sample, and so the axis and its labels,
// are the same whatever the breaks.
void signal(Break kind, int breaks, int channel, std::vector<nanoseconds> &t, std::vector<double> &v)
{
    t.clear();
    v.clear();
    int next = 1;
    qint64 skipUntil = -1;
    for (int i = 0; i < kSamples; ++i) {
        const qint64 now = static_cast<qint64>(i) * kDtNs;
        const bool here = next <= breaks && i == static_cast<int>(static_cast<qint64>(next) * kSamples / (breaks + 1));
        if (here) {
            ++next;
            if (kind == Break::TimeGap) skipUntil = now + kJumpNs;
        }
        if (now < skipUntil) continue;
        t.push_back(nanoseconds(now));
        v.push_back(here && kind == Break::NonFinite ? std::numeric_limits<double>::quiet_NaN()
                                                     : 50.0 + 40.0 * std::sin(0.002 * i + channel));
    }
}

// The fewest allocations of a measured render of a chart at steady state (after the warm-up renders), or -1 if the
// breaks do not give the expected decimated segments.
//
// The minimum over the measured renders, not each render: Qt's raster engine blends a large image in several threads,
// and QThreadPool allocates on this thread for each task it has to queue, which depends on thread scheduling (#8).
// Those allocations only add to a render, so the fewest allocations of a render are those of the chart itself; an
// allocation per segment would raise them.
long long fewestAllocationsPerRender(Break kind, int channels, int breaks)
{
    AnalysisChart chart;
    chart.setReadoutVisible(false);
    chart.setDecimationMode(AnalysisChart::DecimationMode::Always);
    chart.setGapThreshold(nanoseconds(kGapThresholdNs));
    for (int c = 0; c < channels; ++c) {
        const int id = chart.addChannel(QStringLiteral("Ch%1").arg(c), kColors[c % 4]);
        std::vector<nanoseconds> t;
        std::vector<double> v;
        signal(kind, breaks, c, t, v);
        chart.setChannelData(id, t.data(), v.data(), static_cast<qsizetype>(t.size()));
    }
    chart.resize(400, 300);
    chart.resetView();
    chart.setYRange(0.0, 100.0);
    internal::AnalysisChartTestAccess::enableDiagnostics(chart, true);
    QPixmap pix(chart.size());
    chart.render(&pix);
    // The breaks are visible: every channel is drawn decimated, as one segment more than it has breaks
    for (const auto &ch : internal::AnalysisChartTestAccess::lastFrame(chart).channels) {
        if (!ch.decimated || ch.segments.size() != static_cast<size_t>(breaks + 1)) {
            qWarning("channel %d: decimated %d, %zu segments drawn, %d expected", ch.id, ch.decimated ? 1 : 0,
                     ch.segments.size(), breaks + 1);
            return -1;
        }
    }
    internal::AnalysisChartTestAccess::enableDiagnostics(chart, false);
    for (int i = 0; i < kWarmupRenders; ++i) chart.render(&pix);
    long long least = -1;
    long long most = -1;
    for (int i = 0; i < kMeasuredRenders; ++i) {
        AllocationScope scope;
        chart.render(&pix);
        least = least < 0 ? scope.count() : std::min(least, scope.count());
        most = std::max(most, scope.count());
    }
    qInfo("%d breaks per channel: %lld to %lld allocations per render", breaks, least, most);
    return least;
}

} // namespace

class tst_AnalysisChartAllocations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void counterSeesLibraryAllocations();
    void allocationsDoNotGrowWithSegments_data();
    void allocationsDoNotGrowWithSegments();
};

// The counter must see an allocation made inside the shared library (setChannelData() copies the samples)
void tst_AnalysisChartAllocations::counterSeesLibraryAllocations()
{
    AnalysisChart chart;
    const int id = chart.addChannel(QStringLiteral("Ch0"), Qt::red);
    std::vector<nanoseconds> t;
    std::vector<double> v;
    signal(Break::NonFinite, 0, 0, t, v);
    AllocationScope scope;
    QVERIFY(chart.setChannelData(id, t.data(), v.data(), static_cast<qsizetype>(t.size())));
    QVERIFY2(scope.count() > 0, "the allocation counter does not see the allocations of the QtIndustrialWidgets library");
}

void tst_AnalysisChartAllocations::allocationsDoNotGrowWithSegments_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("channels");
    for (auto kind : {Break::NonFinite, Break::TimeGap}) {
        for (int channels : {1, 4}) {
            QTest::newRow(qPrintable(QStringLiteral("%1, %2 ch")
                                         .arg(kind == Break::NonFinite ? QStringLiteral("non-finite samples")
                                                                       : QStringLiteral("time gaps"))
                                         .arg(channels)))
                << static_cast<int>(kind) << channels;
        }
    }
}

// The fewest allocations of a render are the same with 0, 10 and 100 breaks per channel
void tst_AnalysisChartAllocations::allocationsDoNotGrowWithSegments()
{
    QFETCH(int, kind);
    QFETCH(int, channels);
    const auto k = static_cast<Break>(kind);
    const long long reference = fewestAllocationsPerRender(k, channels, 0);
    QVERIFY(reference > 0);
    for (int breaks : {10, 100}) {
        const long long least = fewestAllocationsPerRender(k, channels, breaks);
        QVERIFY(least > 0);
        QVERIFY2(least == reference,
                 qPrintable(QStringLiteral("%1 breaks per channel: at least %2 allocations per render, %3 without breaks")
                                .arg(breaks).arg(least).arg(reference)));
    }
}

QTEST_MAIN(tst_AnalysisChartAllocations)
#include "tst_AnalysisChartAllocations.moc"
