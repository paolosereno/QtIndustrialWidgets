// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// minimumSizeHint() benchmark (#177): the cost of the first minimumSizeHint() of a new IndustrialKnob, RadialGauge
// and CircularBargraph, whose minimum is computed from the font and the configuration (#60, #167, #170).
//
// Targets (approved in #177), on the median of the first call:
//  - default configuration: <= 2 ms per widget (50 widgets add <= 100 ms to the start of a window);
//  - worst case: <= 16.7 ms, one frame at 60 Hz (the hint is computed on the GUI thread during layout).
//
// Worst case: for each widget, the configuration with the largest cost over a fixed grid:
//  ranges 0-1e2 ... 0-1e12 and -1e12-1e12, precisions 0-3, and 10 or 20 major ticks (knob and gauge; the bargraph has
//  no major ticks). It is chosen once, before the timing runs, with --select, and passed to every run in BENCH_WORST.
//
// Method of a run: 5 warm-up trials, then 30 measured trials. A trial creates a new widget for every case, configures
// it and times its first minimumSizeHint(); the cases are interleaved and their order is reversed every other trial,
// so all cases see the same machine state; BENCH_ROUND alternates the starting order between the rounds of a session. min, median and p95 (nearest rank) of the 30 times are reported.
//
// Usage (QT_QPA_PLATFORM=offscreen, DPR 1):
//   ./bench_MinimumSizeHint --select > select.md                (the grid, and BENCH_WORST to use)
//   BENCH_ROUND=1 BENCH_WORST='<printed value>' ./bench_MinimumSizeHint > run1.md
// Progress lines go to stderr.

#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtCore/QThread>
#include <QtGui/QFontInfo>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

using namespace QtIndustrialWidgets;

namespace {

constexpr int kWarmupTrials = 5;
constexpr int kMeasuredTrials = 30;
constexpr int kSelectTrials = 5;
constexpr double kDefaultTargetMs = 2.0;
constexpr double kWorstTargetMs = 16.7;

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
    const std::size_t rank = (95 * n + 99) / 100; // nearest rank
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

void out(const QString &s)
{
    std::fputs(s.toUtf8().constData(), stdout);
    std::fputc('\n', stdout);
}

QString msCell(double v) { return QString::number(v, 'f', 3); }

enum class Kind { Knob, Gauge, Bargraph };

const char *kindName(Kind k)
{
    switch (k) {
    case Kind::Knob: return "IndustrialKnob";
    case Kind::Gauge: return "RadialGauge";
    case Kind::Bargraph: return "CircularBargraph";
    }
    return "?";
}

// A configuration of the grid; isDefault: the widget as constructed
struct Config {
    bool isDefault{true};
    double minimum{0.0};
    double maximum{100.0};
    int precision{0};
    int majorTicks{10};
};

QString describe(Kind kind, const Config &c)
{
    if (c.isDefault) return QStringLiteral("default");
    QString s = QStringLiteral("%1 to %2, precision %3").arg(c.minimum, 0, 'g', 3).arg(c.maximum, 0, 'g', 3).arg(c.precision);
    if (kind != Kind::Bargraph) s += QStringLiteral(", %1 major ticks").arg(c.majorTicks);
    return s;
}

std::unique_ptr<QWidget> create(Kind kind, const Config &c)
{
    switch (kind) {
    case Kind::Knob: {
        auto w = std::make_unique<IndustrialKnob>();
        if (!c.isDefault) { w->setRange(c.minimum, c.maximum); w->setPrecision(c.precision); w->setMajorTicks(c.majorTicks); }
        return w;
    }
    case Kind::Gauge: {
        auto w = std::make_unique<RadialGauge>();
        if (!c.isDefault) { w->setRange(c.minimum, c.maximum); w->setPrecision(c.precision); w->setMajorTicks(c.majorTicks); }
        return w;
    }
    case Kind::Bargraph: {
        auto w = std::make_unique<CircularBargraph>();
        if (!c.isDefault) { w->setRange(c.minimum, c.maximum); w->setPrecision(c.precision); }
        return w;
    }
    }
    return nullptr;
}

// Time of the first minimumSizeHint() of a new widget, in ms; \a side receives its width
double timeFirstHint(Kind kind, const Config &c, int *side)
{
    const std::unique_ptr<QWidget> w = create(kind, c);
    QElapsedTimer t;
    t.start();
    const QSize hint = w->minimumSizeHint();
    const double ms = static_cast<double>(t.nsecsElapsed()) / 1e6;
    *side = hint.width();
    return ms;
}

std::vector<Config> grid(Kind kind)
{
    std::vector<std::pair<double, double>> ranges;
    double top = 100.0;
    for (int k = 2; k <= 12; ++k, top *= 10.0) ranges.emplace_back(0.0, top);
    ranges.emplace_back(-1e12, 1e12);
    std::vector<Config> g;
    for (const auto &r : ranges) {
        for (int p = 0; p <= 3; ++p) {
            for (int ticks : {10, 20}) {
                if (kind == Kind::Bargraph && ticks != 10) continue;
                g.push_back(Config{false, r.first, r.second, p, ticks});
            }
        }
    }
    return g;
}

QString encode(const Config &c)
{
    return QStringLiteral("%1,%2,%3,%4").arg(c.minimum, 0, 'g', 17).arg(c.maximum, 0, 'g', 17).arg(c.precision).arg(c.majorTicks);
}

bool decode(const QString &text, Config *c)
{
    const QStringList p = text.split(QLatin1Char(','));
    if (p.size() != 4) return false;
    bool ok[4] = {false, false, false, false};
    *c = Config{false, p[0].toDouble(&ok[0]), p[1].toDouble(&ok[1]), p[2].toInt(&ok[2]), p[3].toInt(&ok[3])};
    return ok[0] && ok[1] && ok[2] && ok[3];
}

void environment(const QString &extraKey = QString(), const QString &extraValue = QString())
{
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
    const QFontInfo font(QApplication::font());
    row(QStringLiteral("Application font"), QStringLiteral("%1, %2 px").arg(font.family()).arg(font.pixelSize()));
    row(QStringLiteral("Library version"), QStringLiteral(QTINDUSTRIALWIDGETS_VERSION_STR));
    row(QStringLiteral("Benchmark source commit"), gitDescription());
    if (!extraKey.isEmpty()) row(extraKey, extraValue);
    out(QString());
}

int select()
{
    out(QStringLiteral("<!-- bench_MinimumSizeHint selection: begin -->"));
    out(QString());
    environment();
    QStringList worst;
    for (Kind kind : {Kind::Knob, Kind::Gauge, Kind::Bargraph}) {
        out(QStringLiteral("#### %1: grid, median of %2 first calls").arg(QLatin1String(kindName(kind))).arg(kSelectTrials));
        out(QString());
        out(QStringLiteral("| Configuration | Minimum size | median (ms) |"));
        out(QStringLiteral("|---|---|---|"));
        double worstMs = -1.0;
        Config worstConfig;
        int side = 0;
        for (const Config &c : grid(kind)) {
            for (int i = 0; i < 2; ++i) timeFirstHint(kind, c, &side); // warm-up
            std::vector<double> ms;
            for (int i = 0; i < kSelectTrials; ++i) ms.push_back(timeFirstHint(kind, c, &side));
            const double median = computeStats(ms).median;
            out(QStringLiteral("| %1 | %2 | %3 |").arg(describe(kind, c)).arg(side).arg(msCell(median)));
            if (median > worstMs) { worstMs = median; worstConfig = c; }
        }
        out(QString());
        out(QStringLiteral("Worst case of %1: %2 (%3 ms).").arg(QLatin1String(kindName(kind)), describe(kind, worstConfig), msCell(worstMs)));
        out(QString());
        worst << encode(worstConfig);
        std::fprintf(stderr, "[SELECT] %s: %s\n", kindName(kind), qPrintable(describe(kind, worstConfig)));
    }
    out(QStringLiteral("BENCH_WORST='%1'").arg(worst.join(QLatin1Char(';'))));
    out(QString());
    out(QStringLiteral("<!-- bench_MinimumSizeHint selection: end -->"));
    return 0;
}

struct Case {
    Kind kind;
    Config config;
    bool worst;
    std::vector<double> ms;
    int side{0};
};

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    {
        IndustrialKnob probe;
        if (probe.devicePixelRatioF() != 1.0) {
            std::fprintf(stderr, "error: the benchmark needs DPR 1 (got %g)\n", probe.devicePixelRatioF());
            return 1;
        }
    }
    if (app.arguments().contains(QStringLiteral("--select"))) return select();

    const QStringList worst = qEnvironmentVariable("BENCH_WORST").split(QLatin1Char(';'));
    std::vector<Case> cases;
    const Kind kinds[] = {Kind::Knob, Kind::Gauge, Kind::Bargraph};
    for (int i = 0; i < 3; ++i) {
        Config w;
        if (worst.size() != 3 || !decode(worst[i], &w)) {
            std::fprintf(stderr, "error: BENCH_WORST must hold the three configurations printed by --select\n");
            return 1;
        }
        cases.push_back(Case{kinds[i], Config{}, false, {}, 0});
        cases.push_back(Case{kinds[i], w, true, {}, 0});
    }

    // BENCH_ROUND: the round of the session; even rounds start each pair of trials with the reversed order
    const bool evenRound = qEnvironmentVariableIntValue("BENCH_ROUND") % 2 == 0;
    for (int trial = 0; trial < kWarmupTrials + kMeasuredTrials; ++trial) {
        const bool reversed = (trial % 2 == 1) != evenRound;
        for (std::size_t k = 0; k < cases.size(); ++k) {
            Case &c = cases[reversed ? cases.size() - 1 - k : k];
            const double ms = timeFirstHint(c.kind, c.config, &c.side);
            if (trial >= kWarmupTrials) c.ms.push_back(ms);
        }
    }

    out(QStringLiteral("<!-- bench_MinimumSizeHint results: begin -->"));
    out(QString());
    environment(QStringLiteral("BENCH_WORST"), qEnvironmentVariable("BENCH_WORST"));
    out(QStringLiteral("#### First minimumSizeHint() of a new widget (targets on the median: default ≤ %1 ms, worst case ≤ %2 ms)")
            .arg(kDefaultTargetMs).arg(kWorstTargetMs));
    out(QString());
    out(QStringLiteral("| Case | Minimum size | Trials | min (ms) | median (ms) | p95 (ms) | Target |"));
    out(QStringLiteral("|---|---|---|---|---|---|---|"));
    for (const Case &c : cases) {
        const Stats s = computeStats(c.ms);
        const double target = c.worst ? kWorstTargetMs : kDefaultTargetMs;
        const QString name = QStringLiteral("%1, %2%3").arg(QLatin1String(kindName(c.kind)), c.worst ? QStringLiteral("worst case: ") : QString(),
                                                         describe(c.kind, c.config));
        out(QStringLiteral("| %1 | %2 | %3 | %4 | %5 | %6 | %7 |").arg(name).arg(c.side).arg(c.ms.size())
                .arg(msCell(s.min), msCell(s.median), msCell(s.p95), s.median <= target ? QStringLiteral("PASS") : QStringLiteral("FAIL")));
        std::fprintf(stderr, "[HINT] %-60s | min %7.3f | median %7.3f | p95 %7.3f ms\n", qPrintable(name), s.min, s.median, s.p95);
    }
    out(QString());
    out(QStringLiteral("<!-- bench_MinimumSizeHint results: end -->"));
    return 0;
}
