// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "QtIndustrialWidgetsPlugin.h"

#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/SevenSegmentDisplay.h>
#include <QtIndustrialWidgets/LedIndicator.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/IndustrialSlider.h>
#include <QtIndustrialWidgets/ThumbwheelSwitch.h>
#include <QtIndustrialWidgets/StripChart.h>
#include <QtIndustrialWidgets/AnalysisChart.h>
#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include <QtIndustrialWidgets/LevelMeter.h>
#include <QtIndustrialWidgets/AnnunciatorPanel.h>
#include <QtIndustrialWidgets/Compass.h>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/TankDisplay.h>
#include <QtIndustrialWidgets/ValveIndicator.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>

// ============================================================================
// IndustrialWidgetPlugin: one entry of the widget box
// ============================================================================

IndustrialWidgetPlugin::IndustrialWidgetPlugin(const Entry &entry, QObject *parent)
    : QObject(parent)
    , m_entry(entry)
{
}

void IndustrialWidgetPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *IndustrialWidgetPlugin::createWidget(QWidget *parent) { return m_entry.createWidget(parent); }
QString IndustrialWidgetPlugin::group() const { return QStringLiteral("Industrial Widgets"); }
QString IndustrialWidgetPlugin::includeFile() const { return m_entry.includeFile(); }
QString IndustrialWidgetPlugin::name() const { return m_entry.name(); }
QString IndustrialWidgetPlugin::toolTip() const { return m_entry.toolTip(); }
QString IndustrialWidgetPlugin::whatsThis() const { return m_entry.whatsThis(); }
QString IndustrialWidgetPlugin::domXml() const { return m_entry.domXml(); }
QIcon IndustrialWidgetPlugin::icon() const { return m_entry.icon(); }

// The entries: what each widget shows in Designer. Internal linkage: only the collection below uses them
namespace {

// ============================================================================
// RadialGauge
// ============================================================================

namespace RadialGaugeEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::RadialGauge(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/RadialGauge.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::RadialGauge");
}

QString toolTip()
{
    return QStringLiteral("Industrial circular dial gauge with cached scale and vector needle");
}

QString whatsThis()
{
    return QStringLiteral("A circular gauge supporting custom angles, threshold bands, major/minor ticks and Hi-DPI caching.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::RadialGauge\" name=\"radialGauge\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>220</width>\n"
        "   <height>220</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Outer circle
    p.setPen(QPen(QColor(60, 68, 80), 2.0));
    p.setBrush(QColor(25, 30, 40));
    p.drawEllipse(2, 2, 28, 28);

    // Arc
    p.setPen(QPen(QColor(46, 204, 113), 2.5));
    p.drawArc(6, 6, 20, 20, -30 * 16, 240 * 16);

    // Needle
    p.setPen(QPen(QColor(235, 59, 90), 2.0));
    p.drawLine(16, 16, 22, 9);

    // Center hub
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(220, 225, 230));
    p.drawEllipse(14, 14, 4, 4);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace RadialGaugeEntry

// ============================================================================
// LinearGauge
// ============================================================================

namespace LinearGaugeEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::LinearGauge(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/LinearGauge.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::LinearGauge");
}

QString toolTip()
{
    return QStringLiteral("Industrial linear column gauge and thermometer");
}

QString whatsThis()
{
    return QStringLiteral("Linear gauge supporting both vertical and horizontal layouts, thermometer bulb mode, and dynamic fluid color.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::LinearGauge\" name=\"linearGauge\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>100</width>\n"
        "   <height>260</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Tube background
    p.setPen(QPen(QColor(60, 68, 80), 1.5));
    p.setBrush(QColor(25, 30, 40));
    p.drawRoundedRect(10, 3, 12, 22, 3, 3);

    // Bulb
    p.drawEllipse(7, 19, 18, 11);

    // Fill fluid
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(235, 59, 90));
    p.drawEllipse(9, 20, 14, 9);
    p.drawRect(12, 10, 8, 12);

    // Tick lines
    p.setPen(QPen(QColor(200, 205, 215), 1.2));
    p.drawLine(24, 7, 28, 7);
    p.drawLine(24, 13, 27, 13);
    p.drawLine(24, 18, 28, 18);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace LinearGaugeEntry

// ============================================================================
// SevenSegmentDisplay
// ============================================================================

namespace SevenSegmentDisplayEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::SevenSegmentDisplay(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/SevenSegmentDisplay.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::SevenSegmentDisplay");
}

QString toolTip()
{
    return QStringLiteral("Industrial vector 7-segment LED/LCD display");
}

QString whatsThis()
{
    return QStringLiteral("Scalable vector 7-segment display with italic slant, decimal point, and customizable LED colors.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::SevenSegmentDisplay\" name=\"sevenSegmentDisplay\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>220</width>\n"
        "   <height>70</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Bezel
    p.setPen(QPen(QColor(55, 65, 75), 1.5));
    p.setBrush(QColor(16, 20, 28));
    p.drawRoundedRect(2, 6, 28, 20, 3, 3);

    // Digits in cyan
    p.setPen(QPen(QColor(0, 229, 255), 1.8, Qt::SolidLine, Qt::RoundCap));
    // Draw '8'
    p.drawLine(8, 10, 14, 10);
    p.drawLine(8, 16, 14, 16);
    p.drawLine(8, 22, 14, 22);
    p.drawLine(8, 10, 8, 16);
    p.drawLine(14, 10, 14, 16);
    p.drawLine(8, 16, 8, 22);
    p.drawLine(14, 16, 14, 22);

    // Draw '8'
    p.drawLine(18, 10, 24, 10);
    p.drawLine(18, 16, 24, 16);
    p.drawLine(18, 22, 24, 22);
    p.drawLine(18, 10, 18, 16);
    p.drawLine(24, 10, 24, 16);
    p.drawLine(18, 16, 18, 22);
    p.drawLine(24, 16, 24, 22);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace SevenSegmentDisplayEntry

// ============================================================================
// LedIndicator
// ============================================================================

namespace LedIndicatorEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::LedIndicator(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/LedIndicator.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::LedIndicator");
}

QString toolTip()
{
    return QStringLiteral("Industrial LED panel indicator with 3D lens and blinking");
}

QString whatsThis()
{
    return QStringLiteral("A customizable LED indicator supporting circular or rectangular shapes, metallic bezel, customizable colors, and blinking.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::LedIndicator\" name=\"ledIndicator\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>32</width>\n"
        "   <height>32</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Bezel
    p.setPen(QPen(QColor(60, 70, 85), 2.0));
    p.setBrush(QColor(30, 35, 45));
    p.drawEllipse(3, 3, 26, 26);

    // Green lens
    QRadialGradient grad(QPointF(14, 14), 10);
    grad.setColorAt(0.0, QColor(70, 240, 140));
    grad.setColorAt(0.7, QColor(46, 204, 113));
    grad.setColorAt(1.0, QColor(30, 140, 75));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawEllipse(6, 6, 20, 20);

    // Specular highlight
    p.setBrush(QColor(255, 255, 255, 180));
    p.drawEllipse(10, 8, 8, 4);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace LedIndicatorEntry

// ============================================================================
// IndustrialKnob
// ============================================================================

namespace IndustrialKnobEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::IndustrialKnob(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/IndustrialKnob.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::IndustrialKnob");
}

QString toolTip()
{
    return QStringLiteral("Industrial rotary knob potentiometer and selector switch");
}

QString whatsThis()
{
    return QStringLiteral("A rotary control knob with CNC knurled grip, graduated circular scale, continuous and discrete modes, mouse and keyboard input, and an optional mouse wheel.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::IndustrialKnob\" name=\"industrialKnob\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>160</width>\n"
        "   <height>180</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Scale arc
    p.setPen(QPen(QColor(0, 229, 255), 2.0));
    p.drawArc(3, 3, 26, 26, -45 * 16, 270 * 16);

    // Outer knob
    p.setPen(QPen(QColor(60, 70, 85), 1.5));
    p.setBrush(QColor(35, 42, 54));
    p.drawEllipse(7, 7, 18, 18);

    // Face
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(48, 56, 70));
    p.drawEllipse(9, 9, 14, 14);

    // Pointer notch
    p.setPen(QPen(QColor(0, 229, 255), 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(16, 16, 20, 11);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace IndustrialKnobEntry

// ============================================================================
// StripChart
// ============================================================================

namespace StripChartEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::StripChart(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/StripChart.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::StripChart");
}

QString toolTip()
{
    return QStringLiteral("High-performance real-time telemetry strip chart and oscilloscope");
}

QString whatsThis()
{
    return QStringLiteral("A real-time scrolling multi-channel oscilloscope / strip chart with ring buffers, a cached grid reticle and M4 min/max decimation (measured paint times: docs/benchmarks.md).");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::StripChart\" name=\"stripChart\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>380</width>\n"
        "   <height>220</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Chassis & Screen
    p.setPen(QPen(QColor(60, 70, 85), 1.5));
    p.setBrush(QColor(16, 20, 28));
    p.drawRoundedRect(2, 4, 28, 24, 3, 3);

    // Fine grid
    p.setPen(QPen(QColor(38, 50, 68), 1.0, Qt::DotLine));
    p.drawLine(2, 16, 30, 16);
    p.drawLine(16, 4, 16, 28);

    // Sine waveform
    p.setPen(QPen(QColor(0, 229, 255), 1.8, Qt::SolidLine, Qt::RoundCap));
    QPolygonF poly;
    poly << QPointF(4, 22) << QPointF(10, 8) << QPointF(18, 24) << QPointF(24, 12) << QPointF(28, 16);
    p.drawPolyline(poly);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace StripChartEntry

// ============================================================================
// AnalysisChart
// ============================================================================

namespace AnalysisChartEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::AnalysisChart(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/AnalysisChart.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::AnalysisChart");
}

QString toolTip()
{
    return QStringLiteral("Offline review chart for recorded multi-channel data");
}

QString whatsThis()
{
    return QStringLiteral("A chart for reviewing recorded multi-channel timestamp + value data, with dropouts and gaps shown as in StripChart.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::AnalysisChart\" name=\"analysisChart\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>640</width>\n"
        "   <height>360</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Screen with a trace and a dropout gap
    p.setPen(QPen(QColor(60, 70, 85), 1.5));
    p.setBrush(QColor(16, 20, 28));
    p.drawRoundedRect(2, 4, 28, 24, 3, 3);
    p.setPen(QPen(QColor(0, 229, 255), 1.8, Qt::SolidLine, Qt::RoundCap));
    QPolygonF left;
    left << QPointF(4, 20) << QPointF(8, 12) << QPointF(13, 18);
    QPolygonF right;
    right << QPointF(18, 10) << QPointF(23, 22) << QPointF(28, 14);
    p.drawPolyline(left);
    p.drawPolyline(right);
    // Magnifier
    p.setPen(QPen(QColor(220, 225, 235), 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(22, 11), 4.5, 4.5);
    p.drawLine(QPointF(25.2, 14.2), QPointF(29, 18));

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace AnalysisChartEntry

// ============================================================================
// IndustrialSwitch
// ============================================================================

namespace IndustrialSwitchEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::IndustrialSwitch(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/IndustrialSwitch.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::IndustrialSwitch");
}

QString toolTip()
{
    return QStringLiteral("Heavy-duty industrial toggle lever and rocker switch with safety guard");
}

QString whatsThis()
{
    return QStringLiteral("Industrial panel switch supporting bat toggle lever, rocker mode, 2 or 3 positions, and optional safety lock guard.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::IndustrialSwitch\" name=\"industrialSwitch\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>75</width>\n"
        "   <height>125</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Dark plate
    p.setPen(QPen(QColor(60, 70, 85), 1.2));
    p.setBrush(QColor(35, 40, 48));
    p.drawRoundedRect(4, 2, 24, 28, 3, 3);

    // Collar
    p.setPen(QPen(QColor(180, 190, 200), 1.0));
    p.setBrush(QColor(80, 85, 95));
    p.drawEllipse(QPointF(16, 18), 7, 7);

    // Toggle lever pointing up
    p.setPen(QPen(QColor(40, 45, 50), 0.8));
    p.setBrush(QColor(220, 225, 230));
    p.drawRoundedRect(QRectF(14.5, 6, 3, 12), 1, 1);
    p.drawEllipse(QPointF(16, 6), 3, 3);

    // Green indicator dot
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(46, 204, 113));
    p.drawEllipse(QPointF(16, 26), 1.8, 1.8);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace IndustrialSwitchEntry

// ============================================================================
// LevelMeter
// ============================================================================

namespace LevelMeterEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::LevelMeter(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/LevelMeter.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::LevelMeter");
}

QString toolTip()
{
    return QStringLiteral("Multi-channel industrial VU and level meter with peak hold");
}

QString whatsThis()
{
    return QStringLiteral("A high-performance multi-channel VU and level meter with discrete LED segments, smooth bar, peak hold decay, and customizable thresholds.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::LevelMeter\" name=\"levelMeter\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>75</width>\n"
        "   <height>220</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Dark chassis
    p.setPen(QPen(QColor(60, 70, 85), 1.2));
    p.setBrush(QColor(22, 25, 30));
    p.drawRoundedRect(4, 2, 24, 28, 3, 3);

    // Two LED bar ladders (L and R)
    auto drawMiniBar = [&](int x) {
        // Green segments
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(46, 204, 113));
        p.drawRect(x, 20, 4, 2);
        p.drawRect(x, 17, 4, 2);
        p.drawRect(x, 14, 4, 2);
        // Yellow segments
        p.setBrush(QColor(254, 211, 48));
        p.drawRect(x, 11, 4, 2);
        p.drawRect(x, 8, 4, 2);
        // Peak white segment
        p.setBrush(QColor(255, 255, 255));
        p.drawRect(x, 5, 4, 1);
    };

    drawMiniBar(9);
    drawMiniBar(16);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace LevelMeterEntry

// ============================================================================
// AnnunciatorPanel
// ============================================================================

namespace AnnunciatorPanelEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::AnnunciatorPanel(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/AnnunciatorPanel.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::AnnunciatorPanel");
}

QString toolTip()
{
    return QStringLiteral("Industrial alarm annunciator window matrix");
}

QString whatsThis()
{
    return QStringLiteral("A matrix of backlit alarm indicator windows with automatic-reset and manual-reset sequences and engraved legends.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::AnnunciatorPanel\" name=\"annunciatorPanel\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>320</width>\n"
        "   <height>160</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Dark chassis
    p.setPen(QPen(QColor(60, 70, 85), 1.2));
    p.setBrush(QColor(24, 28, 36));
    p.drawRoundedRect(2, 4, 28, 24, 3, 3);

    // 2x2 grid of annunciator tiles
    // Tile 1: Red lit
    p.setPen(QPen(QColor(235, 59, 90), 1.0));
    p.setBrush(QColor(235, 59, 90));
    p.drawRoundedRect(5, 7, 10, 8, 1, 1);

    // Tile 2: Amber lit
    p.setPen(QPen(QColor(254, 211, 48), 1.0));
    p.setBrush(QColor(254, 211, 48));
    p.drawRoundedRect(17, 7, 10, 8, 1, 1);

    // Tile 3: Dark unlit
    p.setPen(QPen(QColor(45, 55, 68), 1.0));
    p.setBrush(QColor(35, 42, 52));
    p.drawRoundedRect(5, 17, 10, 8, 1, 1);

    // Tile 4: Cyan lit
    p.setPen(QPen(QColor(0, 229, 255), 1.0));
    p.setBrush(QColor(0, 229, 255));
    p.drawRoundedRect(17, 17, 10, 8, 1, 1);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace AnnunciatorPanelEntry

// ============================================================================
// Compass
// ============================================================================

namespace CompassEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::Compass(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/Compass.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::Compass");
}

QString toolTip()
{
    return QStringLiteral("Marine gyrocompass and aeronautical heading indicator");
}

QString whatsThis()
{
    return QStringLiteral("A 360-degree navigational instrument with HeadingUp and NorthUp modes, target heading bug, and course deviation indicator.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::Compass\" name=\"compass\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>220</width>\n"
        "   <height>220</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Outer dark bezel ring
    p.setPen(QPen(QColor(60, 70, 85), 1.5));
    p.setBrush(QColor(20, 24, 32));
    p.drawEllipse(QPointF(16, 16), 14, 14);

    // Inner dial ring
    p.setPen(QPen(QColor(0, 229, 255, 60), 1.0));
    p.setBrush(QColor(14, 18, 25));
    p.drawEllipse(QPointF(16, 16), 11, 11);

    // North marker (red arrow)
    QPainterPath north;
    north.moveTo(16, 16);
    north.lineTo(13.5, 16);
    north.lineTo(16, 5);
    north.lineTo(18.5, 16);
    north.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(235, 59, 90));
    p.drawPath(north);

    // South marker (slate arrow)
    QPainterPath south;
    south.moveTo(16, 16);
    south.lineTo(13.5, 16);
    south.lineTo(16, 27);
    south.lineTo(18.5, 16);
    south.closeSubpath();
    p.setBrush(QColor(160, 175, 195));
    p.drawPath(south);

    // Center pivot
    p.setPen(QPen(QColor(30, 36, 46), 1.0));
    p.setBrush(QColor(240, 244, 250));
    p.drawEllipse(QPointF(16, 16), 2.5, 2.5);

    // Heading bug notch at 45 deg (orange)
    p.setPen(QPen(QColor(254, 130, 40), 2.0));
    p.drawLine(QPointF(24, 8), QPointF(26, 6));

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace CompassEntry

// ============================================================================
// CircularBargraph
// ============================================================================

namespace CircularBargraphEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::CircularBargraph(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/CircularBargraph.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::CircularBargraph");
}

QString toolTip()
{
    return QStringLiteral("Circular arc bargraph indicator with discrete curved LED segments and peak hold");
}

QString whatsThis()
{
    return QStringLiteral("An industrial circular LED bargraph indicator with progressive Bar or Dot mode, threshold zones, and ballistic peak decay.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::CircularBargraph\" name=\"circularBargraph\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>180</width>\n"
        "   <height>180</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Chassis background
    p.setPen(QPen(QColor(60, 70, 85), 1.0));
    p.setBrush(QColor(24, 28, 36));
    p.drawEllipse(QPointF(16, 16), 14, 14);

    // Discrete arc segments in miniature
    const double rOut = 13.0;
    const double rIn = 9.0;
    const int numSegs = 8;
    const double startA = -135.0;
    const double spanA = 270.0;
    const double stepA = spanA / numSegs;
    const double gapA = 3.0;

    p.setPen(Qt::NoPen);
    for (int i = 0; i < numSegs; ++i) {
        double a1 = startA + i * stepA + gapA * 0.5;
        double a2 = a1 + stepA - gapA;
        QPainterPath path;
        for (int s = 0; s <= 2; ++s) {
            double ang = a1 + (a2 - a1) * (s / 2.0);
            double rad = (ang - 90.0) * 3.14159265358979323846 / 180.0;
            double px = 16.0 + rOut * std::cos(rad);
            double py = 16.0 + rOut * std::sin(rad);
            if (s == 0) path.moveTo(px, py); else path.lineTo(px, py);
        }
        for (int s = 2; s >= 0; --s) {
            double ang = a1 + (a2 - a1) * (s / 2.0);
            double rad = (ang - 90.0) * 3.14159265358979323846 / 180.0;
            double px = 16.0 + rIn * std::cos(rad);
            double py = 16.0 + rIn * std::sin(rad);
            path.lineTo(px, py);
        }
        path.closeSubpath();
        QColor col = (i < 5) ? QColor(46, 204, 113) : ((i == 5) ? QColor(254, 211, 48) : QColor(38, 43, 53));
        p.fillPath(path, col);
    }

    // Inner center pod
    p.setBrush(QColor(18, 21, 28));
    p.drawEllipse(QPointF(16, 16), 7, 7);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace CircularBargraphEntry

// ============================================================================
// TankDisplay
// ============================================================================

namespace TankDisplayEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::TankDisplay(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/TankDisplay.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::TankDisplay");
}

QString toolTip()
{
    return QStringLiteral("Tank and silo level indicator with high/low limits");
}

QString whatsThis()
{
    return QStringLiteral("A vessel (vertical or horizontal cylinder, hopper or sphere) filled with a liquid or a "
                          "granular solid up to the value, with a vertical scale, a readout and four limits "
                          "(low-low, low, high, high-high) whose crossings it reports.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::TankDisplay\" name=\"tankDisplay\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>160</width>\n"
        "   <height>260</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Vertical cylinder with dished heads, two-thirds full
    QPainterPath vessel;
    vessel.moveTo(8, 7);
    vessel.arcTo(QRectF(8, 3, 16, 8), 180, -180);
    vessel.lineTo(24, 25);
    vessel.arcTo(QRectF(8, 21, 16, 8), 0, -180);
    vessel.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 34, 38));
    p.drawPath(vessel);
    QPainterPath below;
    below.addRect(QRectF(7, 13, 18, 17));
    p.setBrush(QColor(52, 152, 219));
    p.drawPath(vessel.intersected(below));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(120, 134, 150), 1.5));
    p.drawPath(vessel);

    // High limit marker
    QPolygonF marker;
    marker << QPointF(2, 6) << QPointF(6, 9) << QPointF(2, 12);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(231, 76, 60));
    p.drawPolygon(marker);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace TankDisplayEntry

// ============================================================================
// ValveIndicator
// ============================================================================

namespace ValveIndicatorEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::ValveIndicator(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/ValveIndicator.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::ValveIndicator");
}

QString toolTip()
{
    return QStringLiteral("Valve symbol showing the status and the opening of a valve");
}

QString whatsThis()
{
    return QStringLiteral("A gate, ball, butterfly or control valve symbol whose shape shows the status (unknown, "
                          "closed, open, in transit, fault), with an optional opening bar and readout. An indicator "
                          "only: it sends no command.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::ValveIndicator\" name=\"valveIndicator\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>120</width>\n"
        "   <height>120</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Control valve: two triangles tip to tip, the left one filled (in transit), a stem and a dome
    QPainterPath left;
    left.moveTo(3, 15);
    left.lineTo(16, 22);
    left.lineTo(3, 29);
    left.closeSubpath();
    QPainterPath right;
    right.moveTo(29, 15);
    right.lineTo(16, 22);
    right.lineTo(29, 29);
    right.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(241, 196, 15));
    p.drawPath(left);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(241, 196, 15), 1.5));
    p.drawPath(left);
    p.drawPath(right);
    p.setPen(QPen(QColor(120, 134, 150), 1.5));
    p.drawLine(QPointF(16, 22), QPointF(16, 10));
    QPainterPath dome;
    dome.moveTo(9, 10);
    dome.arcTo(QRectF(9, 3, 14, 14), 180, -180);
    dome.closeSubpath();
    p.drawPath(dome);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace ValveIndicatorEntry

// ============================================================================
// IndustrialSlider
// ============================================================================

namespace IndustrialSliderEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::IndustrialSlider(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/IndustrialSlider.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::IndustrialSlider");
}

QString toolTip()
{
    return QStringLiteral("Linear setpoint control with actuator position feedback");
}

QString whatsThis()
{
    return QStringLiteral("A linear control for an actuator command (a valve, a damper, a speed reference): a "
                          "setpoint handle and a feedback bar for the actual position on one scale, with readouts, "
                          "a deviation indication, setpoint limits and read-only mode.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::IndustrialSlider\" name=\"industrialSlider\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>300</width>\n"
        "   <height>92</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Track with a scale under it
    p.setPen(QPen(QColor(120, 134, 150), 1.0));
    p.setBrush(QColor(30, 36, 45));
    p.drawRect(QRectF(2.5, 14.5, 27, 5));
    for (int i = 0; i <= 4; ++i) {
        const double x = 3.0 + i * 6.5;
        p.drawLine(QPointF(x, 22), QPointF(x, i % 2 == 0 ? 27 : 25));
    }

    // Feedback bar across the track, behind the setpoint
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 159, 67));
    p.drawRect(QRectF(11, 13, 2.5, 8));

    // Setpoint handle, its tip on the track
    QPolygonF handle;
    handle << QPointF(17, 4) << QPointF(25, 4) << QPointF(25, 10) << QPointF(21, 14) << QPointF(17, 10);
    p.setBrush(QColor(0, 188, 212));
    p.drawPolygon(handle);

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace IndustrialSliderEntry

// ============================================================================
// ThumbwheelSwitch
// ============================================================================

namespace ThumbwheelSwitchEntry {

QWidget *createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::ThumbwheelSwitch(parent);
}

QString includeFile()
{
    return QStringLiteral("QtIndustrialWidgets/ThumbwheelSwitch.h");
}

QString name()
{
    return QStringLiteral("QtIndustrialWidgets::ThumbwheelSwitch");
}

QString toolTip()
{
    return QStringLiteral("Push-wheel numeric selector, one drum per decade");
}

QString whatsThis()
{
    return QStringLiteral("A thumbwheel switch for calibration racks and manual setpoint panels: one digit drum per "
                          "decade with + and − buttons, a fixed decimal point, an optional sign drum, value limits, "
                          "an overflow mark, and a pending value applied with SET or Enter.");
}

QString domXml()
{
    return QStringLiteral(
        "<widget class=\"QtIndustrialWidgets::ThumbwheelSwitch\" name=\"thumbwheelSwitch\">\n"
        " <property name=\"geometry\">\n"
        "  <rect>\n"
        "   <x>0</x>\n"
        "   <y>0</y>\n"
        "   <width>260</width>\n"
        "   <height>120</height>\n"
        "  </rect>\n"
        " </property>\n"
        "</widget>\n"
    );
}

QIcon icon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Three drums, each with a + button above and a − button below
    for (int i = 0; i < 3; ++i) {
        const double x = 2.5 + i * 9.5;
        p.setPen(QPen(QColor(120, 134, 150), 1.0));
        p.setBrush(QColor(30, 36, 45));
        p.drawRect(QRectF(x, 10.5, 8, 11));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(200, 208, 218));
        QPolygonF up;
        up << QPointF(x + 1.5, 8) << QPointF(x + 6.5, 8) << QPointF(x + 4, 4);
        p.drawPolygon(up);
        QPolygonF down;
        down << QPointF(x + 1.5, 24) << QPointF(x + 6.5, 24) << QPointF(x + 4, 28);
        p.drawPolygon(down);
        // The digit, as a bar
        p.setBrush(QColor(240, 244, 248));
        p.drawRect(QRectF(x + 3, 13, 2, 6));
    }

    return QIcon(pixmap);
}

const IndustrialWidgetPlugin::Entry entry{&name, &includeFile, &toolTip, &whatsThis, &domXml, &icon, &createWidget};

} // namespace ThumbwheelSwitchEntry

} // namespace

// ============================================================================
// QtIndustrialWidgetsPlugin Collection
// ============================================================================

QtIndustrialWidgetsPlugin::QtIndustrialWidgetsPlugin(QObject *parent)
    : QObject(parent)
{
    // The order of the Designer widget box
    const IndustrialWidgetPlugin::Entry *entries[] = {
        &RadialGaugeEntry::entry,
        &LinearGaugeEntry::entry,
        &SevenSegmentDisplayEntry::entry,
        &LedIndicatorEntry::entry,
        &IndustrialKnobEntry::entry,
        &StripChartEntry::entry,
        &AnalysisChartEntry::entry,
        &IndustrialSwitchEntry::entry,
        &LevelMeterEntry::entry,
        &AnnunciatorPanelEntry::entry,
        &CompassEntry::entry,
        &CircularBargraphEntry::entry,
        &TankDisplayEntry::entry,
        &ValveIndicatorEntry::entry,
        &IndustrialSliderEntry::entry,
        &ThumbwheelSwitchEntry::entry,
    };
    for (const IndustrialWidgetPlugin::Entry *entry : entries) {
        m_widgets.append(new IndustrialWidgetPlugin(*entry, this));
    }
}

QList<QDesignerCustomWidgetInterface *> QtIndustrialWidgetsPlugin::customWidgets() const
{
    return m_widgets;
}
