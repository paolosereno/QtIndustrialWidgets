// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "QtIndustrialWidgetsPlugin.h"

#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/SevenSegmentDisplay.h>
#include <QtIndustrialWidgets/LedIndicator.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/StripChart.h>
#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include <QtIndustrialWidgets/LevelMeter.h>
#include <QtIndustrialWidgets/AnnunciatorPanel.h>
#include <QtIndustrialWidgets/Compass.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>

// ============================================================================
// RadialGaugePlugin
// ============================================================================

RadialGaugePlugin::RadialGaugePlugin(QObject *parent)
    : QObject(parent)
{
}

void RadialGaugePlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *RadialGaugePlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::RadialGauge(parent);
}

QString RadialGaugePlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString RadialGaugePlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/RadialGauge.h");
}

QString RadialGaugePlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::RadialGauge");
}

QString RadialGaugePlugin::toolTip() const
{
    return QStringLiteral("Industrial circular dial gauge with cached scale and vector needle");
}

QString RadialGaugePlugin::whatsThis() const
{
    return QStringLiteral("A circular gauge supporting custom angles, threshold bands, major/minor ticks and Hi-DPI caching.");
}

QString RadialGaugePlugin::domXml() const
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

QIcon RadialGaugePlugin::icon() const
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

// ============================================================================
// LinearGaugePlugin
// ============================================================================

LinearGaugePlugin::LinearGaugePlugin(QObject *parent)
    : QObject(parent)
{
}

void LinearGaugePlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *LinearGaugePlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::LinearGauge(parent);
}

QString LinearGaugePlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString LinearGaugePlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/LinearGauge.h");
}

QString LinearGaugePlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::LinearGauge");
}

QString LinearGaugePlugin::toolTip() const
{
    return QStringLiteral("Industrial linear column gauge and thermometer");
}

QString LinearGaugePlugin::whatsThis() const
{
    return QStringLiteral("Linear gauge supporting both vertical and horizontal layouts, thermometer bulb mode, and dynamic fluid color.");
}

QString LinearGaugePlugin::domXml() const
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

QIcon LinearGaugePlugin::icon() const
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

// ============================================================================
// SevenSegmentDisplayPlugin
// ============================================================================

SevenSegmentDisplayPlugin::SevenSegmentDisplayPlugin(QObject *parent)
    : QObject(parent)
{
}

void SevenSegmentDisplayPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *SevenSegmentDisplayPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::SevenSegmentDisplay(parent);
}

QString SevenSegmentDisplayPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString SevenSegmentDisplayPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/SevenSegmentDisplay.h");
}

QString SevenSegmentDisplayPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::SevenSegmentDisplay");
}

QString SevenSegmentDisplayPlugin::toolTip() const
{
    return QStringLiteral("Industrial vector 7-segment LED/LCD display");
}

QString SevenSegmentDisplayPlugin::whatsThis() const
{
    return QStringLiteral("Scalable vector 7-segment display with italic slant, decimal point, and customizable LED colors.");
}

QString SevenSegmentDisplayPlugin::domXml() const
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

QIcon SevenSegmentDisplayPlugin::icon() const
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

// ============================================================================
// LedIndicatorPlugin
// ============================================================================

LedIndicatorPlugin::LedIndicatorPlugin(QObject *parent)
    : QObject(parent)
{
}

void LedIndicatorPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *LedIndicatorPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::LedIndicator(parent);
}

QString LedIndicatorPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString LedIndicatorPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/LedIndicator.h");
}

QString LedIndicatorPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::LedIndicator");
}

QString LedIndicatorPlugin::toolTip() const
{
    return QStringLiteral("Industrial LED panel indicator with 3D lens and blinking");
}

QString LedIndicatorPlugin::whatsThis() const
{
    return QStringLiteral("A customizable LED indicator supporting circular or rectangular shapes, metallic bezel, customizable colors, and blinking.");
}

QString LedIndicatorPlugin::domXml() const
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

QIcon LedIndicatorPlugin::icon() const
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

// ============================================================================
// IndustrialKnobPlugin
// ============================================================================

IndustrialKnobPlugin::IndustrialKnobPlugin(QObject *parent)
    : QObject(parent)
{
}

void IndustrialKnobPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *IndustrialKnobPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::IndustrialKnob(parent);
}

QString IndustrialKnobPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString IndustrialKnobPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/IndustrialKnob.h");
}

QString IndustrialKnobPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::IndustrialKnob");
}

QString IndustrialKnobPlugin::toolTip() const
{
    return QStringLiteral("Industrial rotary knob potentiometer and selector switch");
}

QString IndustrialKnobPlugin::whatsThis() const
{
    return QStringLiteral("A rotary control knob with CNC knurled grip, graduated circular scale, continuous and discrete modes, and mouse/wheel interaction.");
}

QString IndustrialKnobPlugin::domXml() const
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

QIcon IndustrialKnobPlugin::icon() const
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

// ============================================================================
// StripChartPlugin
// ============================================================================

StripChartPlugin::StripChartPlugin(QObject *parent)
    : QObject(parent)
{
}

void StripChartPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *StripChartPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::StripChart(parent);
}

QString StripChartPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString StripChartPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/StripChart.h");
}

QString StripChartPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::StripChart");
}

QString StripChartPlugin::toolTip() const
{
    return QStringLiteral("High-performance real-time telemetry strip chart and oscilloscope");
}

QString StripChartPlugin::whatsThis() const
{
    return QStringLiteral("A real-time scrolling multi-channel oscilloscope / strip chart with ring buffers, cached grid reticle, and 60+ FPS performance.");
}

QString StripChartPlugin::domXml() const
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

QIcon StripChartPlugin::icon() const
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

// ============================================================================
// IndustrialSwitchPlugin
// ============================================================================

IndustrialSwitchPlugin::IndustrialSwitchPlugin(QObject *parent)
    : QObject(parent)
{
}

void IndustrialSwitchPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *IndustrialSwitchPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::IndustrialSwitch(parent);
}

QString IndustrialSwitchPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString IndustrialSwitchPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/IndustrialSwitch.h");
}

QString IndustrialSwitchPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::IndustrialSwitch");
}

QString IndustrialSwitchPlugin::toolTip() const
{
    return QStringLiteral("Heavy-duty industrial toggle lever and rocker switch with safety guard");
}

QString IndustrialSwitchPlugin::whatsThis() const
{
    return QStringLiteral("Industrial panel switch supporting bat toggle lever, rocker mode, 2 or 3 positions, and optional safety lock guard.");
}

QString IndustrialSwitchPlugin::domXml() const
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

QIcon IndustrialSwitchPlugin::icon() const
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

// ============================================================================
// LevelMeterPlugin
// ============================================================================

LevelMeterPlugin::LevelMeterPlugin(QObject *parent)
    : QObject(parent)
{
}

void LevelMeterPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *LevelMeterPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::LevelMeter(parent);
}

QString LevelMeterPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString LevelMeterPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/LevelMeter.h");
}

QString LevelMeterPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::LevelMeter");
}

QString LevelMeterPlugin::toolTip() const
{
    return QStringLiteral("Multi-channel industrial VU and level meter with peak hold");
}

QString LevelMeterPlugin::whatsThis() const
{
    return QStringLiteral("A high-performance multi-channel VU and level meter with discrete LED segments, smooth bar, peak hold decay, and customizable thresholds.");
}

QString LevelMeterPlugin::domXml() const
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

QIcon LevelMeterPlugin::icon() const
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

// ============================================================================
// AnnunciatorPanelPlugin
// ============================================================================

AnnunciatorPanelPlugin::AnnunciatorPanelPlugin(QObject *parent)
    : QObject(parent)
{
}

void AnnunciatorPanelPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *AnnunciatorPanelPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::AnnunciatorPanel(parent);
}

QString AnnunciatorPanelPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString AnnunciatorPanelPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/AnnunciatorPanel.h");
}

QString AnnunciatorPanelPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::AnnunciatorPanel");
}

QString AnnunciatorPanelPlugin::toolTip() const
{
    return QStringLiteral("ANSI/ISA-18.1 industrial alarm annunciator window matrix");
}

QString AnnunciatorPanelPlugin::whatsThis() const
{
    return QStringLiteral("A matrix of backlit alarm indicator windows with standard ISA-18.1 sequence logic and engraved legends.");
}

QString AnnunciatorPanelPlugin::domXml() const
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

QIcon AnnunciatorPanelPlugin::icon() const
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

// ============================================================================
// CompassPlugin
// ============================================================================

CompassPlugin::CompassPlugin(QObject *parent)
    : QObject(parent)
{
}

void CompassPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *CompassPlugin::createWidget(QWidget *parent)
{
    return new QtIndustrialWidgets::Compass(parent);
}

QString CompassPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString CompassPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/Compass.h");
}

QString CompassPlugin::name() const
{
    return QStringLiteral("QtIndustrialWidgets::Compass");
}

QString CompassPlugin::toolTip() const
{
    return QStringLiteral("Marine gyrocompass and aeronautical heading indicator");
}

QString CompassPlugin::whatsThis() const
{
    return QStringLiteral("A 360-degree navigational instrument with HeadingUp and NorthUp modes, target heading bug, and course deviation indicator.");
}

QString CompassPlugin::domXml() const
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

QIcon CompassPlugin::icon() const
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

// ============================================================================
// QtIndustrialWidgetsPlugin Collection
// ============================================================================

QtIndustrialWidgetsPlugin::QtIndustrialWidgetsPlugin(QObject *parent)
    : QObject(parent)
{
    m_widgets.append(new RadialGaugePlugin(this));
    m_widgets.append(new LinearGaugePlugin(this));
    m_widgets.append(new SevenSegmentDisplayPlugin(this));
    m_widgets.append(new LedIndicatorPlugin(this));
    m_widgets.append(new IndustrialKnobPlugin(this));
    m_widgets.append(new StripChartPlugin(this));
    m_widgets.append(new IndustrialSwitchPlugin(this));
    m_widgets.append(new LevelMeterPlugin(this));
    m_widgets.append(new AnnunciatorPanelPlugin(this));
    m_widgets.append(new CompassPlugin(this));
}

QList<QDesignerCustomWidgetInterface *> QtIndustrialWidgetsPlugin::customWidgets() const
{
    return m_widgets;
}
