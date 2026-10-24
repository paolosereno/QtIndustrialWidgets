// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// AnalysisChart benchmark (#25): redraw after a zoom or pan step, 4 channels x 10 M samples,
// plot width 1920 device px, against the 16.7 ms target.
//
// Method (as #4 / bench_StripChart):
//  - Every paint case: 5 warm-up renders, then 30 measured redraws; min / median / p95 (nearest
//    rank) are reported. A measured redraw of a zoom or pan case is one input step delivered to the
//    widget (QWheelEvent or QKeyEvent, synchronously) followed by one render, timed together.
//  - PASS when the p95 of the case is <= 16.7 ms, otherwise FAIL.
//  - setChannelData(): wall time of each call and memory: the resident set size growth of the
//    process across the four calls (the caller's buffers freed) and the bytes of the pyramid.
//
// Results are printed on stdout as Markdown; progress lines go to stderr:
//   QT_QPA_PLATFORM=offscreen ./bench_AnalysisChart > bench_analysis.md
//
// Environment variables:
//   BENCH_SAMPLES   samples per channel (default 10000000)

#include <QtIndustrialWidgets/AnalysisChart.h>
#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>

#include "internal/AnalysisChartTestAccess.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QSysInfo>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QPixmap>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <vector>

using namespace QtIndustrialWidgets;
using std::chrono::nanoseconds;

namespace {

constexpr int kChannels = 4;
constexpr int kPlotWidthDev = 1920;
constexpr int kWidgetHeight = 1080;
constexpr int kWarmupRenders = 5;
constexpr int kMeasuredRenders = 30;
constexpr double kTargetMs = 16.7;
constexpr qint64 kDtNs = 10000;                 // 10 us between samples: 10 M samples span 100 s
constexpr qint64 kStartNs = 1700000000000000000LL; // an absolute (epoch-like) start
constexpr qint64 kSpikePeriodNs = 100000000;    // a one-sample spike every 100 ms
constexpr qint64 kDropoutPeriodNs = 250000000;  // a 50 us NaN dropout every 250 ms
constexpr qint64 kDropoutLengthNs = 50000;
const QColor kColors[kChannels] = {QColor(0, 229, 255), QColor(255, 170, 0), QColor(0, 230, 118), QColor(235, 59, 90)}; // bench_StripChart's

// --- Statistics ---

struct Stats {
    double min{0.0};
    double median{0.0};
    double p95{0.0};
};

Stats computeStats(std::vector<double> v)
{
    Stats s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    s.min = v.front();
    s.median = (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
    const auto rank = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(n)));
    s.p95 = v[std::max<std::size_t>(rank, 1) - 1];
    return s;
}

// --- Environment (as bench_StripChart) ---

QString cpuModel()
{
#if defined(Q_OS_LINUX)
    QFile f(QStringLiteral("/proc/cpuinfo"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (const QString &line : QString::fromLatin1(f.readAll()).split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1String("model name"))) return line.section(QLatin1Char(':'), 1).trimmed();
        }
    }
#elif defined(Q_OS_WIN)
    const QString id = qEnvironmentVariable("PROCESSOR_IDENTIFIER");
    if (!id.isEmpty()) return id;
#endif
    return QStringLiteral("unknown");
}

QString compilerDescription()
{
#if defined(__clang__)
    return QStringLiteral("Clang " __clang_version__);
#elif defined(__GNUC__)
    return QStringLiteral("GCC " __VERSION__);
#elif defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_FULL_VER);
#else
    return QStringLiteral("unknown");
#endif
}

QString buildDescription()
{
    QStringList parts;
#if defined(NDEBUG)
    parts << QStringLiteral("NDEBUG defined");
#else
    parts << QStringLiteral("NDEBUG not defined");
#endif
#if defined(__OPTIMIZE__)
    parts << QStringLiteral("__OPTIMIZE__ defined");
#elif defined(__GNUC__)
    parts << QStringLiteral("__OPTIMIZE__ not defined (unoptimised)");
#endif
    return parts.join(QStringLiteral(", "));
}

QString runGit(const QStringList &args)
{
    QProcess p;
    p.start(QStringLiteral("git"), QStringList{QStringLiteral("-C"), QFileInfo(QStringLiteral(__FILE__)).absolutePath()} + args);
    if (!p.waitForFinished(10000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) return QString();
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

QString gitDescription()
{
    const QString hash = runGit({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    if (hash.isEmpty()) return QStringLiteral("unknown (source not in a git checkout)");
    const QString dirty = runGit({QStringLiteral("status"), QStringLiteral("--porcelain"), QStringLiteral("--untracked-files=no")});
    return dirty.isEmpty() ? hash : hash + QStringLiteral(" (working tree modified)");
}

// Resident set size in bytes (Linux), -1 elsewhere
qint64 residentBytes()
{
#if defined(Q_OS_LINUX)
    QFile f(QStringLiteral("/proc/self/status"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        for (const QString &line : QString::fromLatin1(f.readAll()).split(QLatin1Char('\n'))) {
            if (line.startsWith(QLatin1String("VmRSS:"))) {
                return line.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char(' '), 0, 0).toLongLong() * 1024;
            }
        }
    }
#endif
    return -1;
}

void out(const QString &s)
{
    std::fputs(s.toUtf8().constData(), stdout);
    std::fputc('\n', stdout);
}

QString msCell(double v) { return QString::number(v, 'f', 2); }
QString mib(double bytes) { return QString::number(bytes / (1024.0 * 1024.0), 'f', 1); }

// --- Data ---

enum class Signal { SmoothSine, BroadbandNoise, SpikesAndNaN };

const char *signalName(Signal s)
{
    switch (s) {
    case Signal::SmoothSine: return "SmoothSine";
    case Signal::BroadbandNoise: return "BroadbandNoise";
    case Signal::SpikesAndNaN: return "SpikesAndNaN";
    }
    return "?";
}

// bench_StripChart's signals, sample for sample (its reference interval is this benchmark's 10 us):
// a sine of period 62.8 ms between 0 and 100; BroadbandNoise adds per-sample noise and an
// alternating +-3 offset to a smaller sine; SpikesAndNaN adds a one-sample spike to 500 every 100 ms
// and a 50 us NaN dropout every 250 ms
void generate(Signal signal, int channel, qint64 samples, std::vector<nanoseconds> &t, std::vector<double> &v)
{
    t.resize(static_cast<std::size_t>(samples));
    v.resize(static_cast<std::size_t>(samples));
    const double phaseOffset = channel * 1.57079632679;
    for (qint64 i = 0; i < samples; ++i) {
        const qint64 rel = i * kDtNs;
        t[static_cast<std::size_t>(i)] = nanoseconds(kStartNs + rel);
        const double phase = 0.001 * static_cast<double>(i);
        const double baseSine = std::sin(phase + phaseOffset) * 50.0 + 50.0;
        double y = baseSine;
        if (signal == Signal::BroadbandNoise) {
            const auto rng = static_cast<std::uint64_t>(i + channel * 1000003) * 6364136223846793005ULL + 1442695040888963407ULL;
            const double rand01 = static_cast<double>(rng >> 32) / 4294967296.0;
            const double noise = (rand01 - 0.5) * 10.0 + ((i % 2 == 0) ? 3.0 : -3.0);
            y = std::sin(phase + phaseOffset) * 30.0 + 50.0 + noise;
        } else if (signal == Signal::SpikesAndNaN) {
            if (rel % kSpikePeriodNs < kDtNs) y = 500.0;
            else if (rel % kDropoutPeriodNs < kDropoutLengthNs) y = std::numeric_limits<double>::quiet_NaN();
        }
        v[static_cast<std::size_t>(i)] = y;
    }
}

struct PaintCase {
    QString name;
    QString visible; // samples per channel in the view at the start / end of the case
    Stats ms;
    int decimated{0}; // channels drawn decimated in the last frame
};

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    bool ok = false;
    const qint64 samples = qEnvironmentVariableIntValue("BENCH_SAMPLES", &ok) > 0 && ok
        ? qEnvironmentVariableIntValue("BENCH_SAMPLES") : 10000000LL;

    AnalysisChart chart;
    chart.setReadoutVisible(false);
    chart.resize(kPlotWidthDev + 80, kWidgetHeight);
    if (chart.devicePixelRatioF() != 1.0) {
        std::fprintf(stderr, "error: the benchmark needs DPR 1 (got %g)\n", chart.devicePixelRatioF());
        return 1;
    }

    // setChannelData(): time and memory
    std::vector<double> loadMs;
    const qint64 rssBefore = residentBytes();
    std::size_t pyramidBytes = 0;
    QList<int> ids;
    for (int c = 0; c < kChannels; ++c) {
        ids << chart.addChannel(QStringLiteral("Ch%1").arg(c), kColors[c]);
        std::vector<nanoseconds> t;
        std::vector<double> v;
        generate(Signal::SpikesAndNaN, c, samples, t, v);
        QElapsedTimer timer;
        timer.start();
        if (!chart.setChannelData(ids.back(), t.data(), v.data(), static_cast<qsizetype>(samples))) {
            std::fprintf(stderr, "error: setChannelData failed\n");
            return 1;
        }
        loadMs.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        pyramidBytes += internal::AnalysisChartTestAccess::pyramidBytes(chart, ids.back());
        std::fprintf(stderr, "[LOAD] channel %d: %.1f ms\n", c, loadMs.back());
    } // the caller's buffers are freed here, before the memory is read
    const qint64 rssAfter = residentBytes();

    // Widget width giving a plot of exactly kPlotWidthDev device px (the left margin follows the labels)
    chart.resetView();
    for (int i = 0; i < 4; ++i) {
        const double w = internal::AnalysisChartTestAccess::plotArea(chart).width();
        if (static_cast<int>(std::lround(w)) == kPlotWidthDev) break;
        chart.resize(chart.width() + kPlotWidthDev - static_cast<int>(std::lround(w)), kWidgetHeight);
    }
    const int plotWidth = static_cast<int>(std::lround(internal::AnalysisChartTestAccess::plotArea(chart).width()));

    QPixmap pix(chart.size());
    const QRectF plot = internal::AnalysisChartTestAccess::plotArea(chart);
    const QPointF centre = plot.center();
    auto visibleSamples = [&]() {
        return QString::number(static_cast<double>((chart.xEnd() - chart.xStart()).count()) / kDtNs, 'g', 4);
    };
    // Read from one extra frame rendered with the test diagnostics on, outside the measurement
    auto decimatedChannels = [&]() {
        internal::AnalysisChartTestAccess::enableDiagnostics(chart, true);
        chart.render(&pix);
        internal::AnalysisChartTestAccess::enableDiagnostics(chart, false);
        int n = 0;
        for (const auto &ch : internal::AnalysisChartTestAccess::lastFrame(chart).channels) n += ch.decimated ? 1 : 0;
        return n;
    };
    auto wheel = [&](int delta) {
        QWheelEvent e(centre, chart.mapToGlobal(centre), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&chart, &e);
    };
    auto key = [&](int k) {
        QKeyEvent e(QEvent::KeyPress, k, Qt::NoModifier);
        QApplication::sendEvent(&chart, &e);
    };

    std::vector<PaintCase> cases;
    // step: the input delivered before each render (none for a static redraw)
    auto runCase = [&](const QString &name, const std::function<void(int)> &step) {
        for (int i = 0; i < kWarmupRenders; ++i) chart.render(&pix);
        PaintCase pc;
        pc.name = name;
        const QString from = visibleSamples();
        std::vector<double> timings;
        for (int i = 0; i < kMeasuredRenders; ++i) {
            QElapsedTimer timer;
            timer.start();
            if (step) step(i);
            chart.render(&pix);
            timings.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }
        pc.visible = from == visibleSamples() ? from : from + QStringLiteral(" → ") + visibleSamples();
        pc.ms = computeStats(timings);
        pc.decimated = decimatedChannels();
        std::fprintf(stderr, "[PAINT] %-40s | %s | min %7.2f | median %7.2f | p95 %7.2f ms\n", qPrintable(name),
                     qPrintable(pc.visible), pc.ms.min, pc.ms.median, pc.ms.p95);
        cases.push_back(pc);
    };

    chart.resetView();
    runCase(QStringLiteral("Full view, redraw only (SpikesAndNaN)"), {});
    chart.resetView();
    runCase(QStringLiteral("Zoom in, wheel x1.25 per step"), [&](int) { wheel(120); });
    runCase(QStringLiteral("Zoom out, wheel x1.25 per step"), [&](int) { wheel(-120); });
    // Pan at 1/10 and 1/100 of the record: alternately 15 steps right, 15 left (10 % each)
    for (const qint64 divisor : {qint64(10), qint64(100)}) {
        const qint64 span = (chart.xEnd() - chart.xStart()).count();
        const qint64 mid = chart.xStart().count() + span / 2;
        const qint64 full = (samples - 1) * kDtNs;
        chart.setXRange(nanoseconds(mid - full / divisor / 2), nanoseconds(mid + full / divisor / 2));
        runCase(QStringLiteral("Pan, arrow keys 10 % per step, 1/%1 of the record").arg(divisor),
                [&](int i) { key(i < kMeasuredRenders / 2 ? Qt::Key_Right : Qt::Key_Left); });
        chart.resetView();
    }
    // The drawing cost depends on the data: the full view with the other two signals
    for (const Signal signal : {Signal::SmoothSine, Signal::BroadbandNoise}) {
        for (int c = 0; c < kChannels; ++c) {
            std::vector<nanoseconds> t;
            std::vector<double> v;
            generate(signal, c, samples, t, v);
            chart.setChannelData(ids[c], t.data(), v.data(), static_cast<qsizetype>(samples));
        }
        chart.resetView();
        runCase(QStringLiteral("Full view, redraw only (%1)").arg(QLatin1String(signalName(signal))), {});
    }

    // --- Markdown ---
    out(QStringLiteral("<!-- bench_AnalysisChart results: begin -->"));
    out(QString());
    out(QStringLiteral("#### Environment"));
    out(QString());
    out(QStringLiteral("| Item | Value |"));
    out(QStringLiteral("|---|---|"));
    auto row = [](const QString &k, const QString &v) { out(QStringLiteral("| %1 | %2 |").arg(k, v)); };
    row(QStringLiteral("CPU"), QStringLiteral("%1 (%2 logical CPUs, %3)").arg(cpuModel()).arg(QThread::idealThreadCount()).arg(QSysInfo::currentCpuArchitecture()));
    row(QStringLiteral("OS"), QSysInfo::prettyProductName());
    row(QStringLiteral("Kernel"), QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion());
    row(QStringLiteral("Compiler"), compilerDescription());
    row(QStringLiteral("Build"), buildDescription());
    row(QStringLiteral("Qt"), QStringLiteral("%1 (compiled against %2)").arg(QLatin1String(qVersion()), QLatin1String(QT_VERSION_STR)));
    row(QStringLiteral("QPA platform"), QGuiApplication::platformName());
    row(QStringLiteral("Render target"), QStringLiteral("QPixmap via QWidget::render() (CPU raster)"));
    row(QStringLiteral("Device pixel ratio"), QString::number(chart.devicePixelRatioF()));
    row(QStringLiteral("Widget size"), QStringLiteral("%1 × %2 px").arg(chart.width()).arg(chart.height()));
    row(QStringLiteral("Plot width W_dev"), QStringLiteral("%1 device px").arg(plotWidth));
    row(QStringLiteral("Data"), QStringLiteral("%1 channels × %2 samples, every %3 µs").arg(kChannels).arg(samples).arg(kDtNs / 1000));
    row(QStringLiteral("Library version"), QStringLiteral(QTINDUSTRIALWIDGETS_VERSION_STR));
    row(QStringLiteral("Benchmark source commit"), gitDescription());
    out(QString());
    out(QStringLiteral("#### Redraw after a zoom or pan step (target ≤ %1 ms at p95)").arg(kTargetMs));
    out(QString());
    out(QStringLiteral("| Case | Samples per channel in view | Decimated channels | min (ms) | median (ms) | p95 (ms) | Target |"));
    out(QStringLiteral("|---|---|---|---|---|---|---|"));
    for (const auto &c : cases) {
        out(QStringLiteral("| %1 | %2 | %3 / %4 | %5 | %6 | %7 | %8 |").arg(c.name, c.visible).arg(c.decimated).arg(kChannels)
                .arg(msCell(c.ms.min), msCell(c.ms.median), msCell(c.ms.p95), c.ms.p95 <= kTargetMs ? QStringLiteral("PASS") : QStringLiteral("FAIL")));
    }
    out(QString());
    out(QStringLiteral("#### setChannelData()"));
    out(QString());
    const Stats load = computeStats(loadMs);
    const double dataBytes = static_cast<double>(kChannels) * static_cast<double>(samples) * 16.0;
    out(QStringLiteral("| Item | Value |"));
    out(QStringLiteral("|---|---|"));
    out(QStringLiteral("| Time per call (%1 calls of %2 samples) | min %3 ms, median %4 ms |").arg(kChannels).arg(samples).arg(msCell(load.min), msCell(load.median)));
    out(QStringLiteral("| Stored samples (8 B timestamp + 8 B value) | %1 MiB |").arg(mib(dataBytes)));
    out(QStringLiteral("| Pyramid | %1 MiB (%2 B/sample, %3 % of the samples) |").arg(mib(static_cast<double>(pyramidBytes)))
            .arg(static_cast<double>(pyramidBytes) / (kChannels * static_cast<double>(samples)), 0, 'f', 2)
            .arg(100.0 * static_cast<double>(pyramidBytes) / dataBytes, 0, 'f', 1));
    out(QStringLiteral("| Resident set growth across the %1 calls | %2 |").arg(kChannels)
            .arg(rssBefore >= 0 && rssAfter >= 0 ? mib(static_cast<double>(rssAfter - rssBefore)) + QStringLiteral(" MiB") : QStringLiteral("n/a")));
    out(QString());
    out(QStringLiteral("<!-- bench_AnalysisChart results: end -->"));
    return 0;
}
