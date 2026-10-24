// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "Showcases.h"

#include <QtIndustrialWidgets/AnalysisChart.h>
#include <QtIndustrialWidgets/AnnunciatorPanel.h>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/Compass.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include <QtIndustrialWidgets/LedIndicator.h>
#include <QtIndustrialWidgets/LevelMeter.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/SevenSegmentDisplay.h>
#include <QtIndustrialWidgets/StripChart.h>


#include <QtWidgets/QWidget>

#include <chrono>
#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

namespace GalleryShowcase {

namespace {

bool isDark(Theme theme) { return theme == Theme::Dark; }

// LedIndicator label colour, readable on the showcase background (#45)
QColor ledTextColor(Theme theme)
{
    return isDark(theme) ? QColor(225, 231, 236) : QColor(30, 39, 46);
}

// Colours as in MainWindow::applyTheme()
void styleRadialGauge(RadialGauge *g, Theme theme)
{
    if (isDark(theme)) {
        g->setDialColor(QColor(20, 24, 32));
        g->setTextColor(QColor(245, 246, 250));
        g->setBezelColor(QColor(48, 56, 70));
    } else {
        g->setDialColor(QColor(242, 244, 248));
        g->setTextColor(QColor(30, 39, 46));
        g->setScaleColor(QColor(80, 90, 105));
        g->setBezelColor(QColor(200, 208, 218));
    }
}

void styleLinearGauge(LinearGauge *g, Theme theme)
{
    if (isDark(theme)) {
        g->setBezelColor(QColor(36, 43, 56));
        g->setTroughColor(QColor(20, 24, 32));
        g->setTextColor(QColor(245, 246, 250));
    } else {
        g->setBezelColor(QColor(215, 222, 230));
        g->setTroughColor(QColor(235, 240, 245));
        g->setTextColor(QColor(30, 39, 46));
        g->setScaleColor(QColor(80, 90, 105));
    }
}

void styleKnob(IndustrialKnob *k, Theme theme)
{
    if (isDark(theme)) {
        k->setKnobColor(QColor(42, 48, 60));
        k->setScaleColor(QColor(190, 200, 215));
        k->setTextColor(QColor(240, 244, 250));
    } else {
        k->setKnobColor(QColor(220, 226, 235));
        k->setScaleColor(QColor(70, 80, 95));
        k->setTextColor(QColor(30, 39, 46));
    }
}

QWidget *radialGaugeWarning(Theme theme)
{
    auto *g = new RadialGauge;
    g->setRange(0.0, 8000.0);
    g->setWarningThreshold(6000.0);
    g->setErrorThreshold(7000.0);
    g->setPrecision(0);
    g->setUnit(QStringLiteral("RPM"));
    g->setValue(6400.0); // warning zone
    styleRadialGauge(g, theme);
    return g;
}

QWidget *linearGaugeThermometer(Theme theme)
{
    auto *g = new LinearGauge;
    g->setOrientation(Qt::Vertical);
    g->setThermometerMode(true);
    g->setRange(0.0, 120.0);
    g->setWarningThreshold(95.0);
    g->setErrorThreshold(108.0);
    g->setPrecision(1);
    g->setUnit(QStringLiteral("°C"));
    g->setValue(88.0);
    styleLinearGauge(g, theme);
    return g;
}

QWidget *linearGaugeHorizontal(Theme theme)
{
    auto *g = new LinearGauge;
    g->setOrientation(Qt::Horizontal);
    g->setThermometerMode(false);
    g->setRange(0.0, 250.0);
    g->setWarningThreshold(190.0);
    g->setErrorThreshold(225.0);
    g->setPrecision(0);
    g->setUnit(QStringLiteral("bar"));
    g->setValue(140.0);
    styleLinearGauge(g, theme);
    return g;
}

QWidget *sevenSegmentValue(Theme theme)
{
    auto *d = new SevenSegmentDisplay;
    d->setDigitCount(6);
    d->setDecimalPlaces(1);
    d->setSkewAngle(8.0);
    d->setValue(142.6);
    if (isDark(theme)) {
        d->setBackgroundColor(QColor(16, 20, 28));
        d->setBezelColor(QColor(40, 48, 62));
        d->setActiveSegmentColor(QColor(0, 229, 255));
    } else {
        d->setBackgroundColor(QColor(230, 236, 242));
        d->setBezelColor(QColor(190, 200, 210));
        d->setActiveSegmentColor(QColor(0, 130, 200));
        d->setInactiveSegmentColor(QColor(0, 130, 200, 25));
    }
    return d;
}

QWidget *ledOn(Theme theme)
{
    auto *l = new LedIndicator(QColor(46, 204, 113));
    l->setLabelText(QStringLiteral("RUN"));
    l->setOn(true);
    l->setTextColor(ledTextColor(theme));
    return l;
}

QWidget *ledRectangular(Theme theme)
{
    auto *l = new LedIndicator(QColor(254, 211, 48));
    l->setShape(LedIndicator::LedShape::Rectangular);
    l->setLabelText(QStringLiteral("AUX PUMP"));
    l->setOn(true);
    l->setTextColor(ledTextColor(theme));
    return l;
}

QWidget *knobContinuous(Theme theme)
{
    auto *k = new IndustrialKnob;
    k->setRange(0.0, 8000.0);
    k->setPrecision(0);
    k->setUnit(QStringLiteral("RPM"));
    k->setValue(2400.0);
    styleKnob(k, theme);
    return k;
}

QWidget *knobDiscrete(Theme theme)
{
    auto *k = new IndustrialKnob;
    k->setMode(IndustrialKnob::KnobMode::Discrete);
    k->setDiscreteSteps(5);
    k->setValue(25.0); // position 2 of 5 (default range 0..100)
    styleKnob(k, theme);
    return k;
}

QWidget *switchGuardedToggle(Theme)
{
    auto *s = new IndustrialSwitch;
    s->setAnimated(false);
    s->setSwitchType(IndustrialSwitch::SwitchType::ToggleLever);
    s->setHasSafetyGuard(true);
    s->setGuardColor(QColor(220, 53, 69));
    s->setLedColor(QColor(235, 59, 90));
    s->setGuardOpen(false); // closed; the open guard is shown by "guarded-three-position"
    s->setLabel(QStringLiteral("EMERGENCY"));
    s->setLabelOn(QStringLiteral("ARMED"));
    s->setLabelOff(QStringLiteral("SAFE"));
    s->setChecked(true);
    return s;
}

// #44: a guarded 3-position switch at its sizeHint(), guard open
QWidget *switchGuardedThreePosition(Theme)
{
    auto *s = new IndustrialSwitch;
    s->setAnimated(false);
    s->setSwitchType(IndustrialSwitch::SwitchType::ToggleLever);
    s->setPositionCount(3);
    s->setHasSafetyGuard(true);
    s->setGuardColor(QColor(220, 53, 69));
    s->setLedColor(QColor(254, 211, 48));
    s->setLabel(QStringLiteral("SYS MODE"));
    s->setLabelOff(QStringLiteral("MAN"));
    s->setLabelCenter(QStringLiteral("OFF"));
    s->setLabelOn(QStringLiteral("AUTO"));
    s->setGuardOpen(true);
    s->setPosition(2);
    return s;
}

QWidget *switchRocker(Theme)
{
    auto *s = new IndustrialSwitch;
    s->setAnimated(false);
    s->setSwitchType(IndustrialSwitch::SwitchType::Rocker);
    s->setLedColor(QColor(0, 229, 255));
    s->setLabel(QStringLiteral("COOLING"));
    s->setLabelOn(QStringLiteral("HIGH"));
    s->setLabelOff(QStringLiteral("LOW"));
    s->setChecked(false);
    return s;
}

QWidget *levelMeterStereoDb(Theme)
{
    auto *m = new LevelMeter;
    m->setChannelCount(2);
    m->setChannelLabels({QStringLiteral("L"), QStringLiteral("R")});
    m->setTitle(QStringLiteral("AUDIO"));
    m->setUnit(QStringLiteral("dB"));
    m->setRange(-60.0, 6.0);
    m->setWarningThreshold(-6.0);
    m->setErrorThreshold(0.0);
    m->setPeakHoldEnabled(true);
    // Raise both channels first so the held peaks stay above the current levels
    m->setValue(0, -2.0);
    m->setValue(1, -2.0);
    m->setValue(0, -12.0);
    m->setValue(1, -4.0);
    return m;
}

QWidget *annunciatorAlarms(Theme theme)
{
    auto *p = new AnnunciatorPanel;
    p->setGridSize(3, 4);
    struct Tile { int row, col; const char *text; AnnunciatorPanel::Severity severity; };
    static const Tile tiles[] = {
        {0, 0, "TURBINE 1\nOVERSPEED TRIP", AnnunciatorPanel::Severity::Critical},
        {0, 1, "MAIN STEAM\nPRESS HIGH", AnnunciatorPanel::Severity::Critical},
        {0, 2, "BEARING OIL\nPRESS LOW", AnnunciatorPanel::Severity::Critical},
        {0, 3, "GENERATOR\nLOCKOUT TRIP", AnnunciatorPanel::Severity::Critical},
        {1, 0, "FEEDWATER PUMP\nTRIP FAULT", AnnunciatorPanel::Severity::Warning},
        {1, 1, "CONDENSER\nVACUUM LOW", AnnunciatorPanel::Severity::Warning},
        {1, 2, "TRANSFORMER\nTEMP HIGH", AnnunciatorPanel::Severity::Warning},
        {1, 3, "MAIN STEAM\nTEMP HIGH", AnnunciatorPanel::Severity::Warning},
        {2, 0, "FIRE SUPPRESSION\nDISCHARGED", AnnunciatorPanel::Severity::Critical},
        {2, 1, "AUX DIESEL GEN\nRUNNING", AnnunciatorPanel::Severity::Advisory},
        {2, 2, "UPS BATTERY\nON INVERTER", AnnunciatorPanel::Severity::Advisory},
        {2, 3, "SCADA TELEMETRY\nLINK OFFLINE", AnnunciatorPanel::Severity::Warning},
    };
    for (const Tile &t : tiles) {
        p->setTileText(t.row, t.col, QString::fromLatin1(t.text));
        p->setTileSeverity(t.row, t.col, t.severity);
    }
    if (isDark(theme)) {
        p->setFrameColor(QColor(30, 36, 46));
        p->setGridColor(QColor(55, 65, 80));
        p->setWarningColor(QColor(254, 211, 48));
        p->setAdvisoryColor(QColor(0, 229, 255));
        p->setTextColor(QColor(240, 244, 250));
    } else {
        p->setFrameColor(QColor(215, 222, 230));
        p->setGridColor(QColor(180, 190, 205));
        p->setWarningColor(QColor(243, 156, 18));
        p->setAdvisoryColor(QColor(9, 132, 227));
        p->setTextColor(QColor(30, 39, 46));
    }
    p->setAlarmActive(0, 1, true);  // main steam pressure: acknowledged, steady
    p->acknowledge(1);
    p->setAlarmActive(1, 2, true);  // transformer temperature: unacknowledged (first flash phase)
    p->setAlarmActive(2, 1, true);  // aux diesel running: advisory
    p->acknowledge(9);
    return p;
}

QWidget *compassHeadingUp(Theme theme)
{
    auto *c = new Compass;
    c->setDisplayMode(Compass::DisplayMode::HeadingUp);
    c->setHeading(47.0);
    c->setTargetHeading(75.0);
    c->setHeadingBugVisible(true);
    if (isDark(theme)) {
        c->setDialColor(QColor(20, 24, 32));
        c->setBezelColor(QColor(48, 56, 70));
        c->setTextColor(QColor(225, 231, 236));
    } else {
        c->setDialColor(QColor(242, 244, 248));
        c->setBezelColor(QColor(200, 208, 218));
        c->setTextColor(QColor(30, 39, 46));
    }
    return c;
}

QWidget *circularBargraph(Theme theme, double value)
{
    auto *b = new CircularBargraph;
    b->setRange(0.0, 100.0);
    b->setSegmentCount(24);
    b->setWarningThreshold(70.0);
    b->setErrorThreshold(85.0);
    b->setPeakHoldEnabled(true);
    b->setUnit(QStringLiteral("%"));
    b->setValue(82.0); // peak
    b->setValue(value);
    b->setDialColor(isDark(theme) ? QColor(0, 0, 0) : QColor(24, 28, 36)); // as in the gallery
    return b;
}

QWidget *stripChartChannels(Theme theme)
{
    using namespace std::chrono;
    auto *c = new StripChart;
    c->setXAxisMode(StripChart::XAxisMode::Time);
    c->setTimeLabelFormat(StripChart::TimeLabelFormat::Relative);
    c->setTimeSpan(seconds(10));
    c->setYRange(-1.5, 1.5);
    c->setLegendVisible(true);
    c->setCapacity(4096); // default 300 would keep only the last 1.5 s
    if (isDark(theme)) {
        c->setBackgroundColor(QColor(14, 18, 25));
        c->setGridColor(QColor(42, 54, 70));
        c->setBezelColor(QColor(38, 46, 60));
    } else {
        c->setBackgroundColor(QColor(242, 246, 252));
        c->setGridColor(QColor(208, 218, 230));
        c->setBezelColor(QColor(215, 222, 230));
    }
    const int sine = c->addChannel(QStringLiteral("Sine 0.5 Hz"), QColor(0, 229, 255), 1.8);
    const int square = c->addChannel(QStringLiteral("Square 0.2 Hz"), QColor(255, 170, 0), 1.8);
    const int ramp = c->addChannel(QStringLiteral("Ramp"), QColor(46, 204, 113), 1.8);

    // 10 s at 200 Hz of analytic signals; channel 1 has a 0.4 s NaN dropout (sensor fault)
    constexpr int rate = 200;
    constexpr int count = 10 * rate;
    std::vector<double> a(count), b(count), r(count);
    const double pi = std::acos(-1.0);
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / rate;
        a[static_cast<size_t>(i)] = (t >= 4.0 && t < 4.4) ? std::numeric_limits<double>::quiet_NaN()
                                                          : std::sin(2.0 * pi * 0.5 * t);
        b[static_cast<size_t>(i)] = (std::fmod(t, 5.0) < 2.5) ? 0.6 : -0.6;
        r[static_cast<size_t>(i)] = -1.0 + 2.0 * std::fmod(t, 2.5) / 2.5;
    }
    const nanoseconds dt(1000000000LL / rate);
    c->addUniformSamples(sine, nanoseconds(0), dt, a.data(), count);
    c->addUniformSamples(square, nanoseconds(0), dt, b.data(), count);
    c->addUniformSamples(ramp, nanoseconds(0), dt, r.data(), count);
    return c;
}

QWidget *analysisChartReview(Theme theme)
{
    using namespace std::chrono;
    auto *c = new AnalysisChart;
    if (isDark(theme)) {
        c->setBackgroundColor(QColor(14, 18, 25));
        c->setGridColor(QColor(42, 54, 70));
        c->setBezelColor(QColor(38, 46, 60));
        c->setTextColor(QColor(210, 220, 235));
    } else {
        c->setBackgroundColor(QColor(242, 246, 252));
        c->setGridColor(QColor(208, 218, 230));
        c->setBezelColor(QColor(215, 222, 230));
        c->setTextColor(QColor(40, 48, 60));
    }
    const int pressure = c->addChannel(QStringLiteral("Pressure 200 Hz"), QColor(0, 229, 255), 1.8);
    const int flow = c->addChannel(QStringLiteral("Flow 25 Hz"), QColor(255, 170, 0), 1.8);

    // 10 s of recorded data at two rates: Pressure has a 0.4 s NaN dropout, Flow a 1 s recording gap
    const double pi = std::acos(-1.0);
    std::vector<nanoseconds> tp, tf;
    std::vector<double> vp, vf;
    for (int i = 0; i < 2000; ++i) {
        const double t = i / 200.0;
        tp.push_back(nanoseconds(static_cast<qint64>(i) * 5000000));
        vp.push_back((t >= 4.0 && t < 4.4) ? std::numeric_limits<double>::quiet_NaN()
                                            : std::sin(2.0 * pi * 0.4 * t) + 0.15 * std::sin(2.0 * pi * 7.0 * t));
    }
    for (int i = 0; i < 250; ++i) {
        const double t = i / 25.0;
        if (t >= 6.0 && t < 7.0) continue; // nothing recorded
        tf.push_back(nanoseconds(static_cast<qint64>(i) * 40000000));
        vf.push_back(0.6 * std::cos(2.0 * pi * 0.15 * t) - 0.2);
    }
    c->setChannelData(pressure, tp.data(), vp.data(), static_cast<qsizetype>(tp.size()));
    c->setChannelData(flow, tf.data(), vf.data(), static_cast<qsizetype>(tf.size()));
    c->resetView();
    return c;
}

} // namespace

QString Showcase::fileStem(Theme theme) const
{
    QString stem = kebabCase(widgetClass) + QLatin1Char('-') + variant;
    if (theme == Theme::Light) {
        stem += QStringLiteral("-light");
    }
    return stem;
}

QString kebabCase(const QString &className)
{
    QString out;
    for (int i = 0; i < className.size(); ++i) {
        const QChar c = className.at(i);
        if (c.isUpper() && i > 0) {
            out += QLatin1Char('-');
        }
        out += c.toLower();
    }
    return out;
}

QColor backgroundColor(Theme theme)
{
    return isDark(theme) ? QColor(0x18, 0x1d, 0x27) : QColor(0xff, 0xff, 0xff);
}

const std::vector<Showcase> &showcases()
{
    static const std::vector<Showcase> all = {
        {QStringLiteral("AnalysisChart"), QStringLiteral("review"), QSize(640, 300), analysisChartReview},
        {QStringLiteral("AnnunciatorPanel"), QStringLiteral("alarms"), QSize(640, 360), annunciatorAlarms},
        {QStringLiteral("CircularBargraph"), QStringLiteral("valid"), QSize(260, 260),
         [](Theme t) { return circularBargraph(t, 65.0); }},
        {QStringLiteral("CircularBargraph"), QStringLiteral("overrange"), QSize(260, 260),
         [](Theme t) { return circularBargraph(t, 120.0); }},
        {QStringLiteral("Compass"), QStringLiteral("heading-up"), QSize(300, 300), compassHeadingUp},
        {QStringLiteral("IndustrialKnob"), QStringLiteral("continuous"), QSize(220, 240), knobContinuous},
        {QStringLiteral("IndustrialKnob"), QStringLiteral("discrete"), QSize(220, 240), knobDiscrete},
        {QStringLiteral("IndustrialSwitch"), QStringLiteral("guarded-toggle"), QSize(180, 220), switchGuardedToggle},
        {QStringLiteral("IndustrialSwitch"), QStringLiteral("guarded-three-position"), QSize(), switchGuardedThreePosition},
        {QStringLiteral("IndustrialSwitch"), QStringLiteral("rocker"), QSize(120, 190), switchRocker},
        {QStringLiteral("LedIndicator"), QStringLiteral("on"), QSize(140, 56), ledOn},
        {QStringLiteral("LedIndicator"), QStringLiteral("rectangular"), QSize(160, 56), ledRectangular},
        {QStringLiteral("LevelMeter"), QStringLiteral("stereo-db"), QSize(170, 320), levelMeterStereoDb},
        {QStringLiteral("LinearGauge"), QStringLiteral("thermometer"), QSize(120, 320), linearGaugeThermometer},
        {QStringLiteral("LinearGauge"), QStringLiteral("horizontal"), QSize(420, 110), linearGaugeHorizontal},
        {QStringLiteral("RadialGauge"), QStringLiteral("warning"), QSize(300, 300), radialGaugeWarning},
        {QStringLiteral("SevenSegmentDisplay"), QStringLiteral("value"), QSize(300, 110), sevenSegmentValue},
        {QStringLiteral("StripChart"), QStringLiteral("channels"), QSize(640, 300), stripChartChannels},
    };
    return all;
}

} // namespace GalleryShowcase
