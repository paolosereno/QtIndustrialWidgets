// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// StripChart benchmark.
//
// Method:
//  - Paint cases: 5 warm-up renders, then 30 measured renders; min / median / p95 are reported.
//  - Streaming cases: before every render, 1/60 s of acquisition (at the case's sample rate) is
//    appended to every channel with addUniformSamples(). Render time, append time and heap
//    allocations per render / per append are reported.
//  - Streaming, SampleIndex and Time interleaved: both charts receive the same data and are
//    rendered alternately in the same loop, so the ratio of their timings is not affected by the
//    machine's state changing between runs.
//  - Steady state, 100k and 4M interleaved (#6, #18): a 1 ch x 100k and a 1 ch x 4M chart are
//    rendered alternately in the same loop; the growth from 100k to 4M is the median of the
//    per-pair ratios, so the machine's state cancels out of it.
//  - First paint after resize: 10 independent trials (cold path).
//  - Insertion cases: real call counts, never extrapolated.
//  - Signals are functions of time, so every sample count draws the same waveform in the window;
//    the number of trace pixels in the rendered frame is reported next to each paint time.
//
// Results are printed on stdout as Markdown tables at the end of the run. To keep stdout clean,
// send the QtTest log elsewhere, e.g.:
//   QT_QPA_PLATFORM=offscreen ./bench_StripChart -o bench_log.txt,txt > bench_results.md
//
// Environment variables:
//   BENCH_ADDSAMPLE_CALLS  addSample() calls per trial in the addSample cases (default 100000)
//   BENCH_INSERT_TRIALS    trials per insertion case (default 10)
//
// This file uses only public API that already existed in 2.0.1, so the same source can be
// compiled against an older release to measure a baseline in the same session.

#include <QtIndustrialWidgets/StripChart.h>
#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtTest/QtTest>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QSysInfo>
#include <QtCore/QThread>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <utility>

using namespace QtIndustrialWidgets;

// ---------------------------------------------------------------------------------------------
// Heap allocation counter (glibc only).
//
// malloc / calloc / realloc are interposed in the benchmark executable and forwarded to glibc.
// This counts every heap allocation in the process, including the shared library, Qt containers
// (which use malloc directly) and operator new (which calls malloc in libstdc++).
// posix_memalign / aligned_alloc / memalign are not counted.
// ---------------------------------------------------------------------------------------------
#if defined(__GLIBC__) && !defined(__SANITIZE_ADDRESS__)
#define BENCH_COUNT_ALLOCATIONS 1
extern "C" {
void *__libc_malloc(std::size_t size);
void *__libc_calloc(std::size_t count, std::size_t size);
void *__libc_realloc(void *ptr, std::size_t size);
}

namespace {
std::atomic<bool> g_countAllocations{false};
std::atomic<std::uint64_t> g_allocationCount{0};
std::atomic<std::uint64_t> g_allocationBytes{0};

inline void recordAllocation(std::size_t bytes)
{
    if (g_countAllocations.load(std::memory_order_relaxed)) {
        g_allocationCount.fetch_add(1, std::memory_order_relaxed);
        g_allocationBytes.fetch_add(bytes, std::memory_order_relaxed);
    }
}
} // namespace

extern "C" void *malloc(std::size_t size) noexcept
{
    recordAllocation(size);
    return __libc_malloc(size);
}

extern "C" void *calloc(std::size_t count, std::size_t size) noexcept
{
    recordAllocation(count * size);
    return __libc_calloc(count, size);
}

extern "C" void *realloc(void *ptr, std::size_t size) noexcept
{
    recordAllocation(size);
    return __libc_realloc(ptr, size);
}
#else
#define BENCH_COUNT_ALLOCATIONS 0
#endif

namespace {

struct AllocationDelta {
    std::uint64_t count{0};
    std::uint64_t bytes{0};
};

void beginAllocationCount()
{
#if BENCH_COUNT_ALLOCATIONS
    g_allocationCount.store(0, std::memory_order_relaxed);
    g_allocationBytes.store(0, std::memory_order_relaxed);
    g_countAllocations.store(true, std::memory_order_relaxed);
#endif
}

AllocationDelta endAllocationCount()
{
    AllocationDelta d;
#if BENCH_COUNT_ALLOCATIONS
    g_countAllocations.store(false, std::memory_order_relaxed);
    d.count = g_allocationCount.load(std::memory_order_relaxed);
    d.bytes = g_allocationBytes.load(std::memory_order_relaxed);
#endif
    return d;
}

// ---------------------------------------------------------------------------------------------
// Method constants
// ---------------------------------------------------------------------------------------------
constexpr int kWarmupRenders = 5;
constexpr int kMeasuredRenders = 30;
constexpr int kResizeTrials = 10;
constexpr int kWidgetWidth = 1974;
constexpr int kWidgetHeight = 1080;
// StripChart::plotArea() uses a 42 px left and 12 px right margin (identical in 2.0.1 and 2.1.x).
constexpr int kPlotMarginsX = 42 + 12;
constexpr qint64 kTimeSpanNs = 10000000000LL; // 10 s visible window
constexpr double kFramePeriodS = 1.0 / 60.0;   // 16.7 ms of acquisition per streamed frame

int envInt(const char *name, int fallback)
{
    bool ok = false;
    const int v = qEnvironmentVariableIntValue(name, &ok);
    return (ok && v > 0) ? v : fallback;
}

// ---------------------------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------------------------
struct Stats {
    double min{0.0};
    double median{0.0};
    double p95{0.0};
    double max{0.0};
};

Stats computeStats(std::vector<double> samples)
{
    Stats s;
    if (samples.empty()) {
        return s;
    }
    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    s.min = samples.front();
    s.max = samples.back();
    s.median = (n % 2 == 1) ? samples[n / 2] : 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
    // Nearest-rank p95
    const size_t rank = static_cast<size_t>(std::ceil(0.95 * static_cast<double>(n)));
    s.p95 = samples[std::max<size_t>(rank, 1) - 1];
    return s;
}

// ---------------------------------------------------------------------------------------------
// Result storage (printed as Markdown at the end of the run)
// ---------------------------------------------------------------------------------------------
struct PaintRow {
    int channels{0};
    QString samples;
    QString signal;
    QString axis;
    Stats renderMs;
    qint64 tracePixels{0};
};

struct StreamRow {
    int channels{0};
    QString samples;
    QString signal;
    QString axis;
    int chunkPerChannel{0};
    Stats renderMs;
    Stats appendMs;
    Stats allocsPerRender;
    Stats kibPerRender;
    Stats allocsPerAppend;
    qint64 tracePixels{0}; // last measured frame
};

// SampleIndex and Time charts streamed and rendered alternately in the same loop, so that both
// modes see the same machine state (this machine's timings are bimodal between runs).
struct InterleavedRow {
    int channels{0};
    QString samples;
    QString signal;
    int chunkPerChannel{0};
    Stats sampleIndexMs;
    Stats timeMs;
};

// 1 ch x 100k and 1 ch x 4M rendered alternately (steady state); growth = per-pair ratio - 1
struct GrowthRow {
    QString signal;
    QString axis;
    qint64 smallPixels{0};
    qint64 largePixels{0};
    Stats smallMs;
    Stats largeMs;
    Stats growthPercent;
};

struct TimedRow {
    QString name;
    QString unitsPerTrial;
    int trials{0};
    Stats totalMs;
    double medianPerUnitUs{-1.0}; // < 0: not applicable
};

std::vector<PaintRow> g_steadyRows;
std::vector<StreamRow> g_streamRows;
std::vector<InterleavedRow> g_interleavedRows;
std::vector<GrowthRow> g_growthRows;
std::vector<TimedRow> g_resizeRows;
std::vector<TimedRow> g_insertRows;
QStringList g_environment;

const char *sampleLabel(int samples)
{
    switch (samples) {
        case 10000: return "10k";
        case 100000: return "100k";
        case 1000000: return "1M";
        case 4000000: return "4M";
        default: return "?";
    }
}

const char *axisLabel(StripChart::XAxisMode mode)
{
    return (mode == StripChart::XAxisMode::Time) ? "Time" : "SampleIndex";
}

// ---------------------------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------------------------
QString cpuModel()
{
#if defined(Q_OS_LINUX)
    QFile f(QStringLiteral("/proc/cpuinfo"));
    // /proc files report size 0, so read everything instead of looping on atEnd().
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromLatin1(f.readAll()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.startsWith(QLatin1String("model name"))) {
                return line.section(QLatin1Char(':'), 1).trimmed();
            }
        }
    }
#elif defined(Q_OS_MACOS)
    QProcess p;
    p.start(QStringLiteral("sysctl"), {QStringLiteral("-n"), QStringLiteral("machdep.cpu.brand_string")});
    if (p.waitForFinished(5000)) {
        return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
    }
#elif defined(Q_OS_WIN)
    const QString id = qEnvironmentVariable("PROCESSOR_IDENTIFIER");
    if (!id.isEmpty()) {
        return id;
    }
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
#if defined(__AVX2__)
    parts << QStringLiteral("AVX2 enabled");
#endif
    return parts.join(QStringLiteral(", "));
}

QString runGit(const QString &dir, const QStringList &args)
{
    QProcess p;
    p.start(QStringLiteral("git"), QStringList{QStringLiteral("-C"), dir} + args);
    if (!p.waitForFinished(10000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        return QString();
    }
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

QString gitDescription()
{
    const QString dir = QFileInfo(QStringLiteral(__FILE__)).absolutePath();
    const QString hash = runGit(dir, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    if (hash.isEmpty()) {
        return QStringLiteral("unknown (source not in a git checkout)");
    }
    QString result = hash;
    const QString dirty = runGit(dir, {QStringLiteral("status"), QStringLiteral("--porcelain"),
                                       QStringLiteral("--untracked-files=no")});
    if (!dirty.isEmpty()) {
        result += QStringLiteral(" (working tree modified)");
    }
    // e.g. this file copied into an older checkout to measure a baseline
    const QString self = runGit(dir, {QStringLiteral("status"), QStringLiteral("--porcelain"),
                                      QStringLiteral("--untracked-files=all"), QStringLiteral("--"),
                                      QStringLiteral(__FILE__)});
    if (!self.isEmpty()) {
        result += QStringLiteral(" (bench_StripChart.cpp differs from this commit)");
    }
    return result;
}

// ---------------------------------------------------------------------------------------------
// Markdown output
// ---------------------------------------------------------------------------------------------
void out(const QString &s)
{
    std::fputs(s.toUtf8().constData(), stdout);
    std::fputc('\n', stdout);
}

QString ms(double v)
{
    return QString::number(v, 'f', 2);
}

QString countCell(const Stats &s)
{
    if (!BENCH_COUNT_ALLOCATIONS) {
        return QStringLiteral("n/a");
    }
    return QStringLiteral("%1 / %2").arg(s.median, 0, 'f', 0).arg(s.max, 0, 'f', 0);
}

void printMarkdown()
{
    out(QStringLiteral("<!-- bench_StripChart results: begin -->"));
    out(QString());
    out(QStringLiteral("#### Environment"));
    out(QString());
    out(QStringLiteral("| Item | Value |"));
    out(QStringLiteral("|---|---|"));
    for (const QString &line : std::as_const(g_environment)) {
        out(line);
    }

    if (!g_steadyRows.empty()) {
        out(QString());
        out(QStringLiteral("#### Paint at steady state (%1 warm-up + %2 measured renders, ms)")
                .arg(kWarmupRenders).arg(kMeasuredRenders));
        out(QString());
        out(QStringLiteral("| Channels | Samples / ch | Signal | X axis | Trace px | Min | Median | p95 |"));
        out(QStringLiteral("|---:|---:|---|---|---:|---:|---:|---:|"));
        for (const PaintRow &r : g_steadyRows) {
            out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 |")
                    .arg(r.channels).arg(r.samples, r.signal, r.axis).arg(r.tracePixels)
                    .arg(ms(r.renderMs.min), ms(r.renderMs.median), ms(r.renderMs.p95)));
        }
    }

    if (!g_growthRows.empty()) {
        out(QString());
        out(QStringLiteral("#### Paint at steady state, 1 ch x 100k and 1 ch x 4M interleaved (rendered alternately in the "
                           "same loop; %1 warm-up + %2 measured pairs, ms; growth = per-pair ratio 4M / 100k - 1)")
                .arg(kWarmupRenders).arg(kMeasuredRenders));
        out(QString());
        out(QStringLiteral("| Channels | Signal | X axis | Trace px 100k | Trace px 4M | 100k median | 100k p95 | "
                           "4M median | 4M p95 | Growth, median of per-pair ratios (%) |"));
        out(QStringLiteral("|---:|---|---|---:|---:|---:|---:|---:|---:|---:|"));
        for (const GrowthRow &r : g_growthRows) {
            out(QStringLiteral("| 1 | %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 |")
                    .arg(r.signal, r.axis).arg(r.smallPixels).arg(r.largePixels)
                    .arg(ms(r.smallMs.median), ms(r.smallMs.p95), ms(r.largeMs.median), ms(r.largeMs.p95))
                    .arg(QString::number(r.growthPercent.median, 'f', 1)));
        }
    }

    if (!g_streamRows.empty()) {
        out(QString());
        out(QStringLiteral("#### Paint with streaming (1/60 s of data appended to every channel before each render; "
                           "%1 warm-up + %2 measured frames, ms)")
                .arg(kWarmupRenders).arg(kMeasuredRenders));
        out(QString());
        out(QStringLiteral("| Channels | Samples / ch | Signal | X axis | New samples / ch / frame | Trace px | "
                           "Render min | Render median | Render p95 | Append median (all ch) |"));
        out(QStringLiteral("|---:|---:|---|---|---:|---:|---:|---:|---:|---:|"));
        for (const StreamRow &r : g_streamRows) {
            out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 | %10 |")
                    .arg(r.channels).arg(r.samples, r.signal, r.axis).arg(r.chunkPerChannel).arg(r.tracePixels)
                    .arg(ms(r.renderMs.min), ms(r.renderMs.median), ms(r.renderMs.p95), ms(r.appendMs.median)));
        }

        out(QString());
        out(QStringLiteral("#### Heap allocations in the streaming case (per measured frame; median / max over %1 frames)")
                .arg(kMeasuredRenders));
        out(QString());
        if (!BENCH_COUNT_ALLOCATIONS) {
            out(QStringLiteral("Allocation counting is only available on glibc builds."));
            out(QString());
        }
        out(QStringLiteral("| Channels | Samples / ch | Signal | X axis | Allocations per render | "
                           "KiB allocated per render | Allocations per append (all ch) |"));
        out(QStringLiteral("|---:|---:|---|---|---:|---:|---:|"));
        for (const StreamRow &r : g_streamRows) {
            const QString kib = BENCH_COUNT_ALLOCATIONS
                ? QStringLiteral("%1 / %2").arg(r.kibPerRender.median, 0, 'f', 1).arg(r.kibPerRender.max, 0, 'f', 1)
                : QStringLiteral("n/a");
            out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 |")
                    .arg(r.channels).arg(r.samples, r.signal, r.axis)
                    .arg(countCell(r.allocsPerRender), kib, countCell(r.allocsPerAppend)));
        }
    }

    if (!g_interleavedRows.empty()) {
        out(QString());
        out(QStringLiteral("#### Paint with streaming, SampleIndex and Time interleaved (both charts receive 1/60 s of data "
                           "and are rendered alternately in the same frame; %1 warm-up + %2 measured frames, ms)")
                .arg(kWarmupRenders).arg(kMeasuredRenders));
        out(QString());
        out(QStringLiteral("| Channels | Samples / ch | Signal | New samples / ch / frame | SampleIndex render median | "
                           "SampleIndex render p95 | Time render median | Time render p95 | p95 ratio SampleIndex / Time |"));
        out(QStringLiteral("|---:|---:|---|---:|---:|---:|---:|---:|---:|"));
        for (const InterleavedRow &r : g_interleavedRows) {
            out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 |")
                    .arg(r.channels).arg(r.samples, r.signal).arg(r.chunkPerChannel)
                    .arg(ms(r.sampleIndexMs.median), ms(r.sampleIndexMs.p95), ms(r.timeMs.median), ms(r.timeMs.p95),
                         ms(r.sampleIndexMs.p95 / r.timeMs.p95)));
        }
    }

    auto printTimed = [](const QString &title, const std::vector<TimedRow> &rows) {
        if (rows.empty()) {
            return;
        }
        out(QString());
        out(title);
        out(QString());
        out(QStringLiteral("| Case | Work per trial | Trials | Min (ms) | Median (ms) | p95 (ms) | Median per call (µs) |"));
        out(QStringLiteral("|---|---:|---:|---:|---:|---:|---:|"));
        for (const TimedRow &r : rows) {
            const QString perCall = (r.medianPerUnitUs < 0.0)
                ? QStringLiteral("–")
                : QString::number(r.medianPerUnitUs, 'f', 4);
            out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 |")
                    .arg(r.name, r.unitsPerTrial).arg(r.trials)
                    .arg(ms(r.totalMs.min), ms(r.totalMs.median), ms(r.totalMs.p95), perCall));
        }
    };
    printTimed(QStringLiteral("#### First paint after resize (cold path)"), g_resizeRows);
    printTimed(QStringLiteral("#### Insertion"), g_insertRows);

    out(QString());
    out(QStringLiteral("<!-- bench_StripChart results: end -->"));
    std::fflush(stdout);
}

const QColor kColors[] = {QColor(0, 229, 255), QColor(255, 170, 0), QColor(0, 230, 118), QColor(235, 59, 90)};

} // namespace

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
    // Signals are defined as functions of time t = i * dtNs, so the drawn waveform covers the same
    // shape in the 10 s window for every sample count N. The reference interval kRefDtNs = 10 µs
    // (N = 1M in 10 s) reproduces the earlier index-based generator sample for sample, so 1M cases
    // remain comparable with measurements made with that generator.
    static std::vector<double> generateSignal(SignalType type, int count, int channelIndex, qint64 dtNs) {
        constexpr double kRefDtNs = 10000.0;
        constexpr qint64 kSpikePeriodNs = 100000000;   // one-sample spike every 100 ms
        constexpr qint64 kDropoutPeriodNs = 250000000; // NaN dropout every 250 ms ...
        constexpr qint64 kDropoutLengthNs = 50000;     // ... lasting 50 µs
        std::vector<double> data(count);
        double phaseOffset = channelIndex * 1.57079632679;
        for (int i = 0; i < count; ++i) {
            const qint64 tNs = static_cast<qint64>(i) * dtNs;
            const double phase = 0.001 * (static_cast<double>(tNs) / kRefDtNs);
            double baseSine = std::sin(phase + phaseOffset) * 50.0 + 50.0;
            switch (type) {
                case SignalType::SmoothSine:
                    data[i] = baseSine;
                    break;
                case SignalType::BroadbandNoise: {
                    // Carrier sine + broadband noise ensuring every column has a min/max envelope
                    uint64_t rng = static_cast<uint64_t>(i + channelIndex * 1000003) * 6364136223846793005ULL + 1442695040888963407ULL;
                    double rand01 = static_cast<double>(rng >> 32) / 4294967296.0;
                    double noise = (rand01 - 0.5) * 10.0 + ((i % 2 == 0) ? 3.0 : -3.0);
                    data[i] = std::sin(phase + phaseOffset) * 30.0 + 50.0 + noise;
                    break;
                }
                case SignalType::SpikesAndNaN:
                    if (tNs % kSpikePeriodNs < dtNs) {
                        data[i] = 500.0; // Single-sample spike (first sample of each period)
                    } else if (tNs % kDropoutPeriodNs < kDropoutLengthNs) {
                        data[i] = std::numeric_limits<double>::quiet_NaN(); // Dropout
                    } else {
                        data[i] = baseSine;
                    }
                    break;
            }
        }
        return data;
    }

    // Trace pixels in the rendered frame: decimated traces are drawn with a cosmetic pen without
    // antialiasing in the exact channel colour, and clipped to the plot rectangle. Pixels inside the
    // plot rectangle whose RGB equals one of the channel colours are counted (the legend swatches
    // sit above the plot rectangle; the glow dot uses a lighter colour and is not counted).
    static qint64 countTracePixels(const QPixmap &pix, int channels, StripChart::XAxisMode mode)
    {
        const QImage img = pix.toImage().convertToFormat(QImage::Format_RGB32);
        // StripChart::plotArea() margins with the legend visible (identical in 2.0.1 and 2.1.x)
        const int left = 42;
        const int right = img.width() - 12;
        const int top = 30;
        const int bottom = img.height() - ((mode == StripChart::XAxisMode::Time) ? 26 : 16);
        std::vector<QRgb> colors;
        for (int c = 0; c < channels; ++c) {
            colors.push_back(kColors[c % 4].rgb() & 0x00ffffff);
        }
        qint64 count = 0;
        for (int y = top; y < bottom; ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            for (int x = left; x < right; ++x) {
                const QRgb px = line[x] & 0x00ffffff;
                if (std::find(colors.begin(), colors.end(), px) != colors.end()) {
                    ++count;
                }
            }
        }
        return count;
    }

    static void configureChart(StripChart &chart, int samples, StripChart::XAxisMode mode)
    {
        chart.setXAxisMode(mode);
        chart.setCapacity(samples);
        chart.setDecimationMode(StripChart::DecimationMode::Auto);
        if (mode == StripChart::XAxisMode::Time) {
            chart.setTimeSpan(std::chrono::nanoseconds(kTimeSpanNs));
        }
    }

    static qint64 sampleIntervalNs(int samples)
    {
        return std::max<qint64>(1, kTimeSpanNs / samples);
    }

    static void addPaintMatrixRows();
    static void runAddSampleCase(bool autoScale);

private Q_SLOTS:
    void initTestCase();
    void paintSteady_data();
    void paintSteady();
    void paintSteadyGrowthInterleaved_data();
    void paintSteadyGrowthInterleaved();
    void paintStreaming_data();
    void paintStreaming();
    void paintStreamingInterleaved_data();
    void paintStreamingInterleaved();
    void firstPaintAfterResize();
    void insertionThroughput_addSample_autoscaleOn();
    void insertionThroughput_addSample_autoscaleOff();
    void insertionThroughput_addUniformSamples_1M();
    void insertionThroughput_addDataPoint_legacy_100k();
    void cleanupTestCase();
};

void BenchStripChart::initTestCase()
{
    // QtTest's default watchdog aborts a test function after 5 minutes, and Qt treats 0 as
    // "use the default". A slow baseline (e.g. 2.0.1) needs much longer, so keep a value set by
    // the caller and otherwise allow 24 h.
    if (qEnvironmentVariableIsEmpty("QTEST_FUNCTION_TIMEOUT")) {
        qputenv("QTEST_FUNCTION_TIMEOUT", "86400000");
    }

    // Verify environment: offscreen, DPR 1
    StripChart testChart;
    testChart.resize(kWidgetWidth, kWidgetHeight);
    const double dpr = testChart.devicePixelRatioF();
    QCOMPARE(dpr, 1.0);

    const int plotWidthDev = static_cast<int>(std::lround((kWidgetWidth - kPlotMarginsX) * dpr));

    auto row = [](const QString &k, const QString &v) {
        g_environment << QStringLiteral("| %1 | %2 |").arg(k, v);
    };
    row(QStringLiteral("CPU"), QStringLiteral("%1 (%2 logical CPUs, %3)")
            .arg(cpuModel()).arg(QThread::idealThreadCount()).arg(QSysInfo::currentCpuArchitecture()));
    row(QStringLiteral("OS"), QSysInfo::prettyProductName());
    row(QStringLiteral("Kernel"), QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion());
    row(QStringLiteral("Compiler"), compilerDescription());
    row(QStringLiteral("Build"), buildDescription());
    row(QStringLiteral("Qt"), QStringLiteral("%1 (compiled against %2)").arg(QLatin1String(qVersion()), QLatin1String(QT_VERSION_STR)));
    row(QStringLiteral("QPA platform"), QGuiApplication::platformName());
    row(QStringLiteral("Render target"), QStringLiteral("QPixmap via QWidget::render() (CPU raster)"));
    row(QStringLiteral("Device pixel ratio"), QString::number(dpr));
    row(QStringLiteral("Widget size"), QStringLiteral("%1 × %2 px").arg(kWidgetWidth).arg(kWidgetHeight));
    row(QStringLiteral("Plot width W_dev"), QStringLiteral("%1 device px").arg(plotWidthDev));
    row(QStringLiteral("Library version"), QStringLiteral(QTINDUSTRIALWIDGETS_VERSION_STR));
    row(QStringLiteral("Benchmark source commit"), gitDescription());
    row(QStringLiteral("Heap allocation counting"), BENCH_COUNT_ALLOCATIONS
            ? QStringLiteral("malloc/calloc/realloc interposed (glibc)")
            : QStringLiteral("not available on this platform"));

    for (const QString &line : std::as_const(g_environment)) {
        std::fprintf(stderr, "%s\n", line.toUtf8().constData());
    }
}

void BenchStripChart::cleanupTestCase()
{
    printMarkdown();
}

void BenchStripChart::addPaintMatrixRows()
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
                    QString tag = QStringLiteral("%1ch_%2_%3_%4")
                                      .arg(ch)
                                      .arg(QLatin1String(sampleLabel(s)))
                                      .arg(QLatin1String(signalTypeName(sig)))
                                      .arg(QLatin1String(axisLabel(m)));
                    QTest::newRow(tag.toUtf8().constData())
                        << ch << s << static_cast<int>(sig) << static_cast<int>(m);
                }
            }
        }
    }
}

void BenchStripChart::paintSteady_data()
{
    addPaintMatrixRows();
}

void BenchStripChart::paintSteady()
{
    QFETCH(int, channels);
    QFETCH(int, samples);
    QFETCH(int, signalType);
    QFETCH(int, axisMode);

    const auto sig = static_cast<SignalType>(signalType);
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);

    StripChart chart;
    configureChart(chart, samples, mode);
    const qint64 dtNs = sampleIntervalNs(samples);

    for (int c = 0; c < channels; ++c) {
        int chId = chart.addChannel(QStringLiteral("Ch%1").arg(c), kColors[c % 4]);
        std::vector<double> sigData = generateSignal(sig, samples, c, dtNs);
        chart.addUniformSamples(chId, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs),
                                sigData.data(), samples);
    }

    chart.resize(kWidgetWidth, kWidgetHeight);
    QPixmap pix(chart.size());

    for (int i = 0; i < kWarmupRenders; ++i) {
        chart.render(&pix);
    }

    std::vector<double> timings;
    timings.reserve(kMeasuredRenders);
    for (int i = 0; i < kMeasuredRenders; ++i) {
        QElapsedTimer timer;
        timer.start();
        chart.render(&pix);
        timings.push_back(timer.nsecsElapsed() / 1e6);
    }

    PaintRow r;
    r.channels = channels;
    r.samples = QLatin1String(sampleLabel(samples));
    r.signal = QLatin1String(signalTypeName(sig));
    r.axis = QLatin1String(axisLabel(mode));
    r.renderMs = computeStats(timings);
    r.tracePixels = countTracePixels(pix, channels, mode);
    g_steadyRows.push_back(r);

    std::fprintf(stderr, "[PAINT_STEADY] %d ch | %4s | %-14s | %-11s | %8lld px | min %8.2f | median %8.2f | p95 %8.2f ms\n",
                 channels, sampleLabel(samples), signalTypeName(sig), axisLabel(mode),
                 static_cast<long long>(r.tracePixels), r.renderMs.min, r.renderMs.median, r.renderMs.p95);
}

void BenchStripChart::paintSteadyGrowthInterleaved_data()
{
    QTest::addColumn<int>("axisMode");
    for (auto m : {StripChart::XAxisMode::Time, StripChart::XAxisMode::SampleIndex}) {
        const QString tag = QStringLiteral("1ch_100k_4M_BroadbandNoise_%1").arg(QLatin1String(axisLabel(m)));
        QTest::newRow(tag.toUtf8().constData()) << static_cast<int>(m);
    }
}

// #6 (merged into #18): paint-time growth from 100k to 4M samples, 1 ch BroadbandNoise, steady state. The two charts
// are rendered alternately in the same loop (the order alternates every pair), so both see the same machine state
// and the per-pair ratio is not affected by this machine's bimodal timings.
void BenchStripChart::paintSteadyGrowthInterleaved()
{
    QFETCH(int, axisMode);
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);
    constexpr int kSmall = 100000;
    constexpr int kLarge = 4000000;

    StripChart smallChart;
    StripChart largeChart;
    for (auto [chart, samples] : {std::pair<StripChart *, int>{&smallChart, kSmall}, {&largeChart, kLarge}}) {
        configureChart(*chart, samples, mode);
        const qint64 dtNs = sampleIntervalNs(samples);
        const int id = chart->addChannel(QStringLiteral("Ch0"), kColors[0]);
        const std::vector<double> data = generateSignal(SignalType::BroadbandNoise, samples, 0, dtNs);
        chart->addUniformSamples(id, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs), data.data(), samples);
        chart->resize(kWidgetWidth, kWidgetHeight);
    }
    QPixmap pix(smallChart.size());

    auto timedRender = [&pix](StripChart &chart) {
        QElapsedTimer timer;
        timer.start();
        chart.render(&pix);
        return timer.nsecsElapsed() / 1e6;
    };

    std::vector<double> smallMs;
    std::vector<double> largeMs;
    std::vector<double> growth;
    for (int f = 0; f < kWarmupRenders + kMeasuredRenders; ++f) {
        double a = 0.0;
        double b = 0.0;
        if (f % 2 == 0) {
            a = timedRender(smallChart);
            b = timedRender(largeChart);
        } else {
            b = timedRender(largeChart);
            a = timedRender(smallChart);
        }
        if (f >= kWarmupRenders) {
            smallMs.push_back(a);
            largeMs.push_back(b);
            growth.push_back(100.0 * (b / a - 1.0));
        }
    }

    GrowthRow r;
    r.signal = QLatin1String(signalTypeName(SignalType::BroadbandNoise));
    r.axis = QLatin1String(axisLabel(mode));
    smallChart.render(&pix);
    r.smallPixels = countTracePixels(pix, 1, mode);
    largeChart.render(&pix);
    r.largePixels = countTracePixels(pix, 1, mode);
    r.smallMs = computeStats(smallMs);
    r.largeMs = computeStats(largeMs);
    r.growthPercent = computeStats(growth);
    g_growthRows.push_back(r);

    std::fprintf(stderr, "[PAINT_GROWTH_INTERLEAVED] 1 ch | 100k / 4M | BroadbandNoise | %-11s | %lld / %lld px | "
                         "median %.2f / %.2f ms | growth (median of per-pair ratios) %+.1f %%\n",
                 axisLabel(mode), static_cast<long long>(r.smallPixels), static_cast<long long>(r.largePixels),
                 r.smallMs.median, r.largeMs.median, r.growthPercent.median);
}

void BenchStripChart::paintStreaming_data()
{
    addPaintMatrixRows();
}

void BenchStripChart::paintStreamingInterleaved_data()
{
    QTest::addColumn<int>("channels");
    QTest::addColumn<int>("samples");
    QTest::addColumn<int>("signalType");

    const int channelCounts[] = {1, 4};
    const int sampleCounts[] = {10000, 100000, 1000000, 4000000};
    const SignalType signalTypes[] = {SignalType::SmoothSine, SignalType::BroadbandNoise, SignalType::SpikesAndNaN};
    for (int ch : channelCounts) {
        for (int s : sampleCounts) {
            for (auto sig : signalTypes) {
                const QString tag = QStringLiteral("%1ch_%2_%3").arg(ch).arg(QLatin1String(sampleLabel(s)))
                                        .arg(QLatin1String(signalTypeName(sig)));
                QTest::newRow(tag.toUtf8().constData()) << ch << s << static_cast<int>(sig);
            }
        }
    }
}

void BenchStripChart::paintStreamingInterleaved()
{
    QFETCH(int, channels);
    QFETCH(int, samples);
    QFETCH(int, signalType);

    const auto sig = static_cast<SignalType>(signalType);
    const double sampleRateHz = static_cast<double>(samples) / (static_cast<double>(kTimeSpanNs) * 1e-9);
    const int chunk = std::max(1, static_cast<int>(std::lround(sampleRateHz * kFramePeriodS)));
    const int frames = kWarmupRenders + kMeasuredRenders;
    const int totalSamples = samples + frames * chunk;
    const qint64 dtNs = sampleIntervalNs(samples);

    // Same data and geometry in both charts; only the X axis mode differs
    StripChart indexChart;
    StripChart timeChart;
    configureChart(indexChart, samples, StripChart::XAxisMode::SampleIndex);
    configureChart(timeChart, samples, StripChart::XAxisMode::Time);
    std::vector<std::vector<double>> data;
    for (int c = 0; c < channels; ++c) {
        data.push_back(generateSignal(sig, totalSamples, c, dtNs));
        for (StripChart *chart : {&indexChart, &timeChart}) {
            const int id = chart->addChannel(QStringLiteral("Ch%1").arg(c), kColors[c % 4]);
            chart->addUniformSamples(id, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs),
                                     data.back().data(), samples);
        }
    }
    indexChart.resize(kWidgetWidth, kWidgetHeight);
    timeChart.resize(kWidgetWidth, kWidgetHeight);
    QPixmap pix(indexChart.size());
    indexChart.render(&pix); // first paints at this size (cold path, not measured)
    timeChart.render(&pix);

    auto timedRender = [&pix](StripChart &chart) {
        QElapsedTimer timer;
        timer.start();
        chart.render(&pix);
        return timer.nsecsElapsed() / 1e6;
    };

    std::vector<double> indexMs;
    std::vector<double> timeMs;
    for (int f = 0; f < frames; ++f) {
        const qint64 firstIndex = static_cast<qint64>(samples) + static_cast<qint64>(f) * chunk;
        for (int c = 0; c < channels; ++c) {
            for (StripChart *chart : {&indexChart, &timeChart}) {
                chart->addUniformSamples(c, std::chrono::nanoseconds(firstIndex * dtNs), std::chrono::nanoseconds(dtNs),
                                         data[c].data() + firstIndex, chunk);
            }
        }
        // Alternate which mode renders first, so neither is favoured by the order
        double a = 0.0;
        double b = 0.0;
        if (f % 2 == 0) {
            a = timedRender(indexChart);
            b = timedRender(timeChart);
        } else {
            b = timedRender(timeChart);
            a = timedRender(indexChart);
        }
        if (f >= kWarmupRenders) {
            indexMs.push_back(a);
            timeMs.push_back(b);
        }
    }

    InterleavedRow r;
    r.channels = channels;
    r.samples = QLatin1String(sampleLabel(samples));
    r.signal = QLatin1String(signalTypeName(sig));
    r.chunkPerChannel = chunk;
    r.sampleIndexMs = computeStats(indexMs);
    r.timeMs = computeStats(timeMs);
    g_interleavedRows.push_back(r);

    std::fprintf(stderr, "[PAINT_STREAM_INTERLEAVED] %d ch | %4s | %-14s | +%d/ch/frame | SampleIndex median %8.2f p95 %8.2f"
                         " | Time median %8.2f p95 %8.2f ms | p95 ratio %.2f\n",
                 channels, sampleLabel(samples), signalTypeName(sig), chunk,
                 r.sampleIndexMs.median, r.sampleIndexMs.p95, r.timeMs.median, r.timeMs.p95,
                 r.sampleIndexMs.p95 / r.timeMs.p95);
}

void BenchStripChart::paintStreaming()
{
    QFETCH(int, channels);
    QFETCH(int, samples);
    QFETCH(int, signalType);
    QFETCH(int, axisMode);

    const auto sig = static_cast<SignalType>(signalType);
    const auto mode = static_cast<StripChart::XAxisMode>(axisMode);

    // Sample rate of the case: `samples` samples cover the 10 s window.
    const double sampleRateHz = static_cast<double>(samples) / (static_cast<double>(kTimeSpanNs) * 1e-9);
    const int chunk = std::max(1, static_cast<int>(std::lround(sampleRateHz * kFramePeriodS)));
    const int frames = kWarmupRenders + kMeasuredRenders;
    const int totalSamples = samples + frames * chunk;

    StripChart chart;
    configureChart(chart, samples, mode);
    const qint64 dtNs = sampleIntervalNs(samples);

    std::vector<int> ids;
    std::vector<std::vector<double>> data;
    for (int c = 0; c < channels; ++c) {
        ids.push_back(chart.addChannel(QStringLiteral("Ch%1").arg(c), kColors[c % 4]));
        data.push_back(generateSignal(sig, totalSamples, c, dtNs));
        chart.addUniformSamples(ids.back(), std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs),
                                data.back().data(), samples);
    }

    chart.resize(kWidgetWidth, kWidgetHeight);
    QPixmap pix(chart.size());
    chart.render(&pix); // first paint at this size (cold path, not measured)

    std::vector<double> renderMs;
    std::vector<double> appendMs;
    std::vector<double> allocsPerRender;
    std::vector<double> kibPerRender;
    std::vector<double> allocsPerAppend;

    for (int f = 0; f < frames; ++f) {
        const bool measured = (f >= kWarmupRenders);
        const qint64 firstIndex = static_cast<qint64>(samples) + static_cast<qint64>(f) * chunk;

        beginAllocationCount();
        QElapsedTimer appendTimer;
        appendTimer.start();
        for (int c = 0; c < channels; ++c) {
            chart.addUniformSamples(ids[c], std::chrono::nanoseconds(firstIndex * dtNs),
                                    std::chrono::nanoseconds(dtNs),
                                    data[c].data() + firstIndex, chunk);
        }
        const double appendElapsedMs = appendTimer.nsecsElapsed() / 1e6;
        const AllocationDelta appendAllocs = endAllocationCount();

        beginAllocationCount();
        QElapsedTimer renderTimer;
        renderTimer.start();
        chart.render(&pix);
        const double renderElapsedMs = renderTimer.nsecsElapsed() / 1e6;
        const AllocationDelta renderAllocs = endAllocationCount();

        if (measured) {
            renderMs.push_back(renderElapsedMs);
            appendMs.push_back(appendElapsedMs);
            allocsPerRender.push_back(static_cast<double>(renderAllocs.count));
            kibPerRender.push_back(static_cast<double>(renderAllocs.bytes) / 1024.0);
            allocsPerAppend.push_back(static_cast<double>(appendAllocs.count));
        }
    }

    StreamRow r;
    r.channels = channels;
    r.samples = QLatin1String(sampleLabel(samples));
    r.signal = QLatin1String(signalTypeName(sig));
    r.axis = QLatin1String(axisLabel(mode));
    r.chunkPerChannel = chunk;
    r.renderMs = computeStats(renderMs);
    r.appendMs = computeStats(appendMs);
    r.allocsPerRender = computeStats(allocsPerRender);
    r.kibPerRender = computeStats(kibPerRender);
    r.allocsPerAppend = computeStats(allocsPerAppend);
    r.tracePixels = countTracePixels(pix, channels, mode);
    g_streamRows.push_back(r);

    std::fprintf(stderr, "[PAINT_STREAM] %d ch | %4s | %-14s | %-11s | +%d/ch/frame | %8lld px | min %8.2f | median %8.2f | p95 %8.2f ms"
                         " | append %.3f ms | allocs/render %.0f (max %.0f) | allocs/append max %.0f\n",
                 channels, sampleLabel(samples), signalTypeName(sig), axisLabel(mode), chunk,
                 static_cast<long long>(r.tracePixels),
                 r.renderMs.min, r.renderMs.median, r.renderMs.p95, r.appendMs.median,
                 r.allocsPerRender.median, r.allocsPerRender.max, r.allocsPerAppend.max);
}

void BenchStripChart::firstPaintAfterResize()
{
    // Stream rebuild cost: 4 channels x 1M noisy
    constexpr int channels = 4;
    constexpr int samples = 1000000;
    const qint64 dtNs = sampleIntervalNs(samples);

    std::vector<std::vector<double>> data;
    for (int c = 0; c < channels; ++c) {
        data.push_back(generateSignal(SignalType::BroadbandNoise, samples, c, dtNs));
    }

    std::vector<double> timings;
    timings.reserve(kResizeTrials);

    for (int trial = 0; trial < kResizeTrials; ++trial) {
        StripChart chart;
        configureChart(chart, samples, StripChart::XAxisMode::Time);

        for (int c = 0; c < channels; ++c) {
            int chId = chart.addChannel(QStringLiteral("Ch%1").arg(c), kColors[c % 4]);
            chart.addUniformSamples(chId, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs),
                                    data[c].data(), samples);
        }

        // Render at initial size
        chart.resize(1000, 800);
        QPixmap pixInitial(chart.size());
        chart.render(&pixInitial);

        // Resize to 1974 x 1080 (W_dev = 1920 at DPR 1)
        chart.resize(kWidgetWidth, kWidgetHeight);
        QPixmap pix(chart.size());

        QElapsedTimer timer;
        timer.start();
        chart.render(&pix);
        timings.push_back(timer.nsecsElapsed() / 1e6);
    }

    TimedRow r;
    r.name = QStringLiteral("4 ch × 1M BroadbandNoise, Time; render at 1000×800, resize to %1×%2, time the next render")
                 .arg(kWidgetWidth).arg(kWidgetHeight);
    r.unitsPerTrial = QStringLiteral("1 render");
    r.trials = kResizeTrials;
    r.totalMs = computeStats(timings);
    g_resizeRows.push_back(r);

    std::fprintf(stderr, "[RESIZE] 4x1M noisy | min %8.2f | median %8.2f | p95 %8.2f ms\n",
                 r.totalMs.min, r.totalMs.median, r.totalMs.p95);
}

void BenchStripChart::runAddSampleCase(bool autoScale)
{
    // Ring buffer full with 1M samples; every addSample() evicts the oldest sample,
    // so the buffer holds exactly 1M samples for every measured call.
    constexpr int baseSamples = 1000000;
    const int calls = envInt("BENCH_ADDSAMPLE_CALLS", 100000);
    const int trials = envInt("BENCH_INSERT_TRIALS", 10);
    constexpr qint64 dtNs = 10000;

    StripChart chart;
    chart.setCapacity(baseSamples);
    chart.setXAxisMode(StripChart::XAxisMode::Time);
    chart.setTimeSpan(std::chrono::seconds(10));
    chart.setAutoScaleY(autoScale);

    int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);
    std::vector<double> baseData = generateSignal(SignalType::BroadbandNoise, baseSamples, 0, dtNs);
    chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(dtNs), baseData.data(), baseSamples);

    std::vector<double> timingsMs;
    timingsMs.reserve(trials);

    qint64 nextT = static_cast<qint64>(baseSamples) * dtNs;
    for (int trial = 0; trial < trials; ++trial) {
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < calls; ++i) {
            chart.addSample(ch, std::chrono::nanoseconds(nextT), 50.0 + (i % 20));
            nextT += dtNs;
        }
        timingsMs.push_back(timer.nsecsElapsed() / 1e6);
    }

    TimedRow r;
    r.name = autoScale ? QStringLiteral("`addSample`, autoscale ON, 1M samples in buffer")
                       : QStringLiteral("`addSample`, autoscale OFF, 1M samples in buffer");
    r.unitsPerTrial = QStringLiteral("%1 calls").arg(calls);
    r.trials = trials;
    r.totalMs = computeStats(timingsMs);
    r.medianPerUnitUs = r.totalMs.median * 1e3 / calls;
    g_insertRows.push_back(r);

    std::fprintf(stderr, "[INSERT] addSample autoscale %s | %d calls x %d trials | median %.2f ms (%.4f us/call)\n",
                 autoScale ? "ON" : "OFF", calls, trials, r.totalMs.median, r.medianPerUnitUs);
}

void BenchStripChart::insertionThroughput_addSample_autoscaleOn()
{
    runAddSampleCase(true);
}

void BenchStripChart::insertionThroughput_addSample_autoscaleOff()
{
    runAddSampleCase(false);
}

void BenchStripChart::insertionThroughput_addUniformSamples_1M()
{
    constexpr int samples = 1000000;
    const int trials = envInt("BENCH_INSERT_TRIALS", 10);
    std::vector<double> data = generateSignal(SignalType::BroadbandNoise, samples, 0, 1000);

    std::vector<double> timingsMs;
    timingsMs.reserve(trials);

    for (int trial = 0; trial < trials; ++trial) {
        StripChart chart;
        chart.setCapacity(samples);
        chart.setXAxisMode(StripChart::XAxisMode::Time);
        int ch = chart.addChannel(QStringLiteral("Ch0"), Qt::cyan);

        QElapsedTimer timer;
        timer.start();
        chart.addUniformSamples(ch, std::chrono::nanoseconds(0), std::chrono::nanoseconds(1000), data.data(), samples);
        timingsMs.push_back(timer.nsecsElapsed() / 1e6);
    }

    TimedRow r;
    r.name = QStringLiteral("`addUniformSamples`, one 1M-sample block into an empty channel");
    r.unitsPerTrial = QStringLiteral("1 call (1M samples)");
    r.trials = trials;
    r.totalMs = computeStats(timingsMs);
    g_insertRows.push_back(r);

    std::fprintf(stderr, "[INSERT] addUniformSamples 1M | %d trials | median %.2f ms\n", trials, r.totalMs.median);
}

void BenchStripChart::insertionThroughput_addDataPoint_legacy_100k()
{
    constexpr int targetSamples = 100000;
    const int trials = envInt("BENCH_INSERT_TRIALS", 10);

    std::vector<double> timingsMs;
    timingsMs.reserve(trials);

    for (int trial = 0; trial < trials; ++trial) {
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

    TimedRow r;
    r.name = QStringLiteral("`addDataPoint` (legacy), SampleIndex, autoscale OFF, empty channel");
    r.unitsPerTrial = QStringLiteral("%1 calls").arg(targetSamples);
    r.trials = trials;
    r.totalMs = computeStats(timingsMs);
    r.medianPerUnitUs = r.totalMs.median * 1e3 / targetSamples;
    g_insertRows.push_back(r);

    std::fprintf(stderr, "[INSERT] addDataPoint legacy | %d calls x %d trials | median %.2f ms (%.4f us/call)\n",
                 targetSamples, trials, r.totalMs.median, r.medianPerUnitUs);
}

QTEST_MAIN(BenchStripChart)
#include "bench_StripChart.moc"
