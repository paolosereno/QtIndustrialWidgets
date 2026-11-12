// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// The Qt Designer plugin of the build tree (#154): one entry per widget, in the order of the widget box, each with
// its own name, include, tooltip, What's This, domXml, icon and widget. Written when the entries became one class
// (IndustrialWidgetPlugin) with a table of entries, so that an entry wired to another widget's functions, or a common
// member that stops doing its part, fails here, in every configuration. installed_designer_plugin checks the
// installed plugin, with the run path; this test needs no install.

#include <QtTest/QtTest>
#include <QtCore/QPluginLoader>
#include <QtCore/QSet>
#include <QtGui/QImage>
#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <QtWidgets/QWidget>

#include <memory>

class tst_DesignerPlugin : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void entries_data();
    void entries();
    void entriesAreDistinct();

private:
    QList<QDesignerCustomWidgetInterface *> m_widgets;
};

namespace {

// The widget box, in its order, with the texts of each entry as they were before the entries became one class
struct Expected {
    const char *widgetClass;
    const char *toolTip;
    const char *whatsThis;
};
const Expected kWidgets[] = {
    {"RadialGauge", "Industrial circular dial gauge with cached scale and vector needle",
     "A circular gauge supporting custom angles, threshold bands, major/minor ticks and Hi-DPI caching."},
    {"LinearGauge", "Industrial linear column gauge and thermometer",
     "Linear gauge supporting both vertical and horizontal layouts, thermometer bulb mode, and dynamic fluid color."},
    {"SevenSegmentDisplay", "Industrial vector 7-segment LED/LCD display",
     "Scalable vector 7-segment display with italic slant, decimal point, and customizable LED colors."},
    {"LedIndicator", "Industrial LED panel indicator with 3D lens and blinking",
     "A customizable LED indicator supporting circular or rectangular shapes, metallic bezel, customizable colors, and blinking."},
    {"IndustrialKnob", "Industrial rotary knob potentiometer and selector switch",
     "A rotary control knob with CNC knurled grip, graduated circular scale, continuous and discrete modes, mouse and keyboard input, and an optional mouse wheel."},
    {"StripChart", "High-performance real-time telemetry strip chart and oscilloscope",
     "A real-time scrolling multi-channel oscilloscope / strip chart with ring buffers, a cached grid reticle and M4 min/max decimation (measured paint times: docs/benchmarks.md)."},
    {"AnalysisChart", "Offline review chart for recorded multi-channel data",
     "A chart for reviewing recorded multi-channel timestamp + value data, with dropouts and gaps shown as in StripChart."},
    {"IndustrialSwitch", "Heavy-duty industrial toggle lever and rocker switch with safety guard",
     "Industrial panel switch supporting bat toggle lever, rocker mode, 2 or 3 positions, and optional safety lock guard."},
    {"LevelMeter", "Multi-channel industrial VU and level meter with peak hold",
     "A high-performance multi-channel VU and level meter with discrete LED segments, smooth bar, peak hold decay, and customizable thresholds."},
    {"AnnunciatorPanel", "Industrial alarm annunciator window matrix",
     "A matrix of backlit alarm indicator windows with automatic-reset and manual-reset sequences and engraved legends."},
    {"Compass", "Marine gyrocompass and aeronautical heading indicator",
     "A 360-degree navigational instrument with HeadingUp and NorthUp modes, target heading bug, and course deviation indicator."},
    {"CircularBargraph", "Circular arc bargraph indicator with discrete curved LED segments and peak hold",
     "An industrial circular LED bargraph indicator with progressive Bar or Dot mode, threshold zones, and ballistic peak decay."},
    {"TankDisplay", "Tank and silo level indicator with high/low limits",
     "A vessel (vertical or horizontal cylinder, hopper or sphere) filled with a liquid or a granular solid up to the value, with a vertical scale, a readout and four limits (low-low, low, high, high-high) whose crossings it reports."},
    {"ValveIndicator", "Valve symbol showing the status and the opening of a valve",
     "A gate, ball, butterfly or control valve symbol whose shape shows the status (unknown, closed, open, in transit, fault), with an optional opening bar and readout. An indicator only: it sends no command."},
    {"IndustrialSlider", "Linear setpoint control with actuator position feedback",
     "A linear control for an actuator command (a valve, a damper, a speed reference): a setpoint handle and a feedback bar for the actual position on one scale, with readouts, a deviation indication, setpoint limits and read-only mode."},
    {"ThumbwheelSwitch", "Push-wheel numeric selector, one drum per decade",
     "A thumbwheel switch for calibration racks and manual setpoint panels: one digit drum per decade with + and − buttons, a fixed decimal point, an optional sign drum, value limits, an overflow mark, and a pending value applied with SET or Enter."},
};

int opaquePixels(const QIcon &icon)
{
    const QImage img = icon.pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
    int n = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            n += qAlpha(img.pixel(x, y)) > 0;
        }
    }
    return n;
}

} // namespace

void tst_DesignerPlugin::initTestCase()
{
    QPluginLoader loader(QStringLiteral(QIW_DESIGNER_PLUGIN));
    QVERIFY2(loader.load(), qPrintable(loader.errorString()));
    auto *collection = qobject_cast<QDesignerCustomWidgetCollectionInterface *>(loader.instance());
    QVERIFY(collection);
    m_widgets = collection->customWidgets();
}

void tst_DesignerPlugin::entries_data()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("widgetClass");
    QTest::addColumn<QString>("toolTip");
    QTest::addColumn<QString>("whatsThis");
    int i = 0;
    for (const Expected &e : kWidgets) {
        QTest::newRow(e.widgetClass) << i++ << QString::fromLatin1(e.widgetClass) << QString::fromUtf8(e.toolTip)
                                     << QString::fromUtf8(e.whatsThis);
    }
}

void tst_DesignerPlugin::entries()
{
    QFETCH(int, index);
    QFETCH(QString, widgetClass);
    QFETCH(QString, toolTip);
    QFETCH(QString, whatsThis);
    QCOMPARE(m_widgets.size(), static_cast<int>(sizeof(kWidgets) / sizeof(kWidgets[0])));
    QDesignerCustomWidgetInterface *w = m_widgets.at(index);
    const QString qualified = QStringLiteral("QtIndustrialWidgets::") + widgetClass;
    QCOMPARE(w->name(), qualified);
    QCOMPARE(w->includeFile(), QStringLiteral("QtIndustrialWidgets/%1.h").arg(widgetClass));
    QCOMPARE(w->group(), QStringLiteral("Industrial Widgets"));
    QVERIFY(!w->isContainer());
    QVERIFY(!w->isInitialized());
    w->initialize(nullptr); // Designer passes its core; the entries do not use it
    QVERIFY(w->isInitialized());
    QCOMPARE(w->toolTip(), toolTip);
    QCOMPARE(w->whatsThis(), whatsThis);
    QVERIFY2(w->domXml().contains(QStringLiteral("<widget class=\"%1\"").arg(qualified)), qPrintable(w->domXml()));
    QVERIFY(opaquePixels(w->icon()) > 0);
    // Designer finds the interface of an entry with qobject_cast
    auto *object = dynamic_cast<QObject *>(w);
    QVERIFY(object);
    QCOMPARE(qobject_cast<QDesignerCustomWidgetInterface *>(object), w);
    QWidget parent;
    QWidget *created = w->createWidget(&parent);
    QVERIFY(created);
    QCOMPARE(created->parentWidget(), &parent);
    QCOMPARE(QString::fromLatin1(created->metaObject()->className()), qualified);
}

// Every entry has its own texts and icon: none borrows those of another
void tst_DesignerPlugin::entriesAreDistinct()
{
    QSet<QString> toolTips, whatsThis, domXml;
    QSet<QByteArray> icons;
    for (QDesignerCustomWidgetInterface *w : m_widgets) {
        toolTips.insert(w->toolTip());
        whatsThis.insert(w->whatsThis());
        domXml.insert(w->domXml());
        const QImage img = w->icon().pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
        icons.insert(QByteArray(reinterpret_cast<const char *>(img.constBits()), img.sizeInBytes()));
    }
    QCOMPARE(toolTips.size(), m_widgets.size());
    QCOMPARE(whatsThis.size(), m_widgets.size());
    QCOMPARE(domXml.size(), m_widgets.size());
    QCOMPARE(icons.size(), m_widgets.size());
}

QTEST_MAIN(tst_DesignerPlugin)
#include "tst_DesignerPlugin.moc"
