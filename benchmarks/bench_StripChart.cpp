// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

// StripChart benchmark.
//
// Method:
//  - Paint cases: 5 warm-up renders, then 30 measured renders; min / median / p95 are reported.
//  - Streaming cases: before every render, 1/60 s of acquisition (at the case's sample rate) is
//    appended to every channel with addUniformSamples(). Render time, append time and heap
//    allocations per render / per append are reported.
//  - First paint after resize: 10 independent trials (cold path).
//  - Insertion cases: real call counts, never extrapolated.
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
        out(QStringLiteral("| Channels | Samples / ch | Signal | X axis | Min | Median | p95 |"));
        out(QStringLiteral("|---:|---:|---|---|---:|---:|---:|"));
        for (const PaintRow &r : g_steadyRows) {
            out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 |")
                    .arg(r.channels).arg(r.samples, r.signal, r.axis)
                    .arg(ms(r.renderMs.min), ms(r.renderMs.median), ms(r.renderMs.p95)));
        }
    }

    if (!g_streamRows.empty()) {
        out(QString());
        out(QStringLiteral("#### Paint with streaming (1/60 s of data appended to every channel before each render; "
                           "%1 warm-up + %2 measured frames, ms)")
                .arg(kWarmupRenders).arg(kMeasuredRenders));
        out(QString());
        out(QStringLiteral("| Channels | Samples / ch | Signal | X axis | New samples / ch / frame | "
                           "Render min | Render median | Render p95 | Append median (all ch) |"));
        out(QStringLiteral("|---:|---:|---|---|---:|---:|---:|---:|---:|"));
        for (const StreamRow &r : g_streamRows) {
            out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9 |")
                    .arg(r.channels).arg(r.samples, r.signal, r.axis).arg(r.chunkPerChannel)
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
    void paintStreaming_data();
    void paintStreaming();
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
        std::vector<double> sigData = generateSignal(sig, samples, c);
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
    g_steadyRows.push_back(r);

    std::fprintf(stderr, "[PAINT_STEADY] %d ch | %4s | %-14s | %-11s | min %8.2f | median %8.2f | p95 %8.2f ms\n",
                 channels, sampleLabel(samples), signalTypeName(sig), axisLabel(mode),
                 r.renderMs.min, r.renderMs.median, r.renderMs.p95);
}

void BenchStripChart::paintStreaming_data()
{
    addPaintMatrixRows();
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
        data.push_back(generateSignal(sig, totalSamples, c));
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
    g_streamRows.push_back(r);

    std::fprintf(stderr, "[PAINT_STREAM] %d ch | %4s | %-14s | %-11s | +%d/ch/frame | min %8.2f | median %8.2f | p95 %8.2f ms"
                         " | append %.3f ms | allocs/render %.0f (max %.0f) | allocs/append max %.0f\n",
                 channels, sampleLabel(samples), signalTypeName(sig), axisLabel(mode), chunk,
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
        data.push_back(generateSignal(SignalType::BroadbandNoise, samples, c));
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
    std::vector<double> baseData = generateSignal(SignalType::BroadbandNoise, baseSamples, 0);
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
    std::vector<double> data = generateSignal(SignalType::BroadbandNoise, samples, 0);

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
