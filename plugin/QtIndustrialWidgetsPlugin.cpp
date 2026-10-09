#include "QtIndustrialWidgetsPlugin.h"

#include <QtIndustrialWidgets/QRadialGauge.h>
#include <QtIndustrialWidgets/QLinearGauge.h>
#include <QtIndustrialWidgets/QSevenSegmentDisplay.h>
#include <QtIndustrialWidgets/QLedIndicator.h>
#include <QtIndustrialWidgets/QIndustrialKnob.h>
#include <QtIndustrialWidgets/QStripChart.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>

// ============================================================================
// QRadialGaugePlugin
// ============================================================================

QRadialGaugePlugin::QRadialGaugePlugin(QObject *parent)
    : QObject(parent)
{
}

void QRadialGaugePlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *QRadialGaugePlugin::createWidget(QWidget *parent)
{
    return new QRadialGauge(parent);
}

QString QRadialGaugePlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString QRadialGaugePlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/QRadialGauge.h");
}

QString QRadialGaugePlugin::name() const
{
    return QStringLiteral("QRadialGauge");
}

QString QRadialGaugePlugin::toolTip() const
{
    return QStringLiteral("Industrial circular dial gauge with cached scale and vector needle");
}

QString QRadialGaugePlugin::whatsThis() const
{
    return QStringLiteral("A circular gauge supporting custom angles, threshold bands, major/minor ticks and Hi-DPI caching.");
}

QString QRadialGaugePlugin::domXml() const
{
    return QStringLiteral(
        "<widget class=\"QRadialGauge\" name=\"radialGauge\">\n"
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

QIcon QRadialGaugePlugin::icon() const
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
// QLinearGaugePlugin
// ============================================================================

QLinearGaugePlugin::QLinearGaugePlugin(QObject *parent)
    : QObject(parent)
{
}

void QLinearGaugePlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *QLinearGaugePlugin::createWidget(QWidget *parent)
{
    return new QLinearGauge(parent);
}

QString QLinearGaugePlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString QLinearGaugePlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/QLinearGauge.h");
}

QString QLinearGaugePlugin::name() const
{
    return QStringLiteral("QLinearGauge");
}

QString QLinearGaugePlugin::toolTip() const
{
    return QStringLiteral("Industrial linear column gauge and thermometer");
}

QString QLinearGaugePlugin::whatsThis() const
{
    return QStringLiteral("Linear gauge supporting both vertical and horizontal layouts, thermometer bulb mode, and dynamic fluid color.");
}

QString QLinearGaugePlugin::domXml() const
{
    return QStringLiteral(
        "<widget class=\"QLinearGauge\" name=\"linearGauge\">\n"
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

QIcon QLinearGaugePlugin::icon() const
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
// QSevenSegmentDisplayPlugin
// ============================================================================

QSevenSegmentDisplayPlugin::QSevenSegmentDisplayPlugin(QObject *parent)
    : QObject(parent)
{
}

void QSevenSegmentDisplayPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *QSevenSegmentDisplayPlugin::createWidget(QWidget *parent)
{
    return new QSevenSegmentDisplay(parent);
}

QString QSevenSegmentDisplayPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString QSevenSegmentDisplayPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/QSevenSegmentDisplay.h");
}

QString QSevenSegmentDisplayPlugin::name() const
{
    return QStringLiteral("QSevenSegmentDisplay");
}

QString QSevenSegmentDisplayPlugin::toolTip() const
{
    return QStringLiteral("Industrial vector 7-segment LED/LCD display");
}

QString QSevenSegmentDisplayPlugin::whatsThis() const
{
    return QStringLiteral("Scalable vector 7-segment display with italic slant, decimal point, and customizable LED colors.");
}

QString QSevenSegmentDisplayPlugin::domXml() const
{
    return QStringLiteral(
        "<widget class=\"QSevenSegmentDisplay\" name=\"sevenSegmentDisplay\">\n"
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

QIcon QSevenSegmentDisplayPlugin::icon() const
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
// QLedIndicatorPlugin
// ============================================================================

QLedIndicatorPlugin::QLedIndicatorPlugin(QObject *parent)
    : QObject(parent)
{
}

void QLedIndicatorPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *QLedIndicatorPlugin::createWidget(QWidget *parent)
{
    return new QLedIndicator(parent);
}

QString QLedIndicatorPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString QLedIndicatorPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/QLedIndicator.h");
}

QString QLedIndicatorPlugin::name() const
{
    return QStringLiteral("QLedIndicator");
}

QString QLedIndicatorPlugin::toolTip() const
{
    return QStringLiteral("Industrial LED panel indicator with 3D lens and blinking");
}

QString QLedIndicatorPlugin::whatsThis() const
{
    return QStringLiteral("A customizable LED indicator supporting circular or rectangular shapes, metallic bezel, customizable colors, and blinking.");
}

QString QLedIndicatorPlugin::domXml() const
{
    return QStringLiteral(
        "<widget class=\"QLedIndicator\" name=\"ledIndicator\">\n"
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

QIcon QLedIndicatorPlugin::icon() const
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
// QIndustrialKnobPlugin
// ============================================================================

QIndustrialKnobPlugin::QIndustrialKnobPlugin(QObject *parent)
    : QObject(parent)
{
}

void QIndustrialKnobPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *QIndustrialKnobPlugin::createWidget(QWidget *parent)
{
    return new QIndustrialKnob(parent);
}

QString QIndustrialKnobPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString QIndustrialKnobPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/QIndustrialKnob.h");
}

QString QIndustrialKnobPlugin::name() const
{
    return QStringLiteral("QIndustrialKnob");
}

QString QIndustrialKnobPlugin::toolTip() const
{
    return QStringLiteral("Industrial rotary knob potentiometer and selector switch");
}

QString QIndustrialKnobPlugin::whatsThis() const
{
    return QStringLiteral("A rotary control knob with CNC knurled grip, graduated circular scale, continuous and discrete modes, and mouse/wheel interaction.");
}

QString QIndustrialKnobPlugin::domXml() const
{
    return QStringLiteral(
        "<widget class=\"QIndustrialKnob\" name=\"industrialKnob\">\n"
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

QIcon QIndustrialKnobPlugin::icon() const
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
// QStripChartPlugin
// ============================================================================

QStripChartPlugin::QStripChartPlugin(QObject *parent)
    : QObject(parent)
{
}

void QStripChartPlugin::initialize(QDesignerFormEditorInterface *)
{
    if (m_initialized) return;
    m_initialized = true;
}

QWidget *QStripChartPlugin::createWidget(QWidget *parent)
{
    return new QStripChart(parent);
}

QString QStripChartPlugin::group() const
{
    return QStringLiteral("Industrial Widgets");
}

QString QStripChartPlugin::includeFile() const
{
    return QStringLiteral("QtIndustrialWidgets/QStripChart.h");
}

QString QStripChartPlugin::name() const
{
    return QStringLiteral("QStripChart");
}

QString QStripChartPlugin::toolTip() const
{
    return QStringLiteral("High-performance real-time telemetry strip chart and oscilloscope");
}

QString QStripChartPlugin::whatsThis() const
{
    return QStringLiteral("A real-time scrolling multi-channel oscilloscope / strip chart with ring buffers, cached grid reticle, and 60+ FPS performance.");
}

QString QStripChartPlugin::domXml() const
{
    return QStringLiteral(
        "<widget class=\"QStripChart\" name=\"stripChart\">\n"
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

QIcon QStripChartPlugin::icon() const
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
// QtIndustrialWidgetsPlugin Collection
// ============================================================================

QtIndustrialWidgetsPlugin::QtIndustrialWidgetsPlugin(QObject *parent)
    : QObject(parent)
{
    m_widgets.append(new QRadialGaugePlugin(this));
    m_widgets.append(new QLinearGaugePlugin(this));
    m_widgets.append(new QSevenSegmentDisplayPlugin(this));
    m_widgets.append(new QLedIndicatorPlugin(this));
    m_widgets.append(new QIndustrialKnobPlugin(this));
    m_widgets.append(new QStripChartPlugin(this));
}

QList<QDesignerCustomWidgetInterface *> QtIndustrialWidgetsPlugin::customWidgets() const
{
    return m_widgets;
}
