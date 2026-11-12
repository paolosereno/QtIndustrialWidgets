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

// The widget box, in its order
const char *const kWidgets[] = {"RadialGauge",    "LinearGauge",      "SevenSegmentDisplay", "LedIndicator",
                                "IndustrialKnob", "StripChart",       "AnalysisChart",       "IndustrialSwitch",
                                "LevelMeter",     "AnnunciatorPanel", "Compass",             "CircularBargraph",
                                "TankDisplay",    "ValveIndicator",   "IndustrialSlider",    "ThumbwheelSwitch"};

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
    int i = 0;
    for (const char *w : kWidgets) {
        QTest::newRow(w) << i++ << QString::fromLatin1(w);
    }
}

void tst_DesignerPlugin::entries()
{
    QFETCH(int, index);
    QFETCH(QString, widgetClass);
    QCOMPARE(m_widgets.size(), static_cast<int>(sizeof(kWidgets) / sizeof(kWidgets[0])));
    QDesignerCustomWidgetInterface *w = m_widgets.at(index);
    const QString qualified = QStringLiteral("QtIndustrialWidgets::") + widgetClass;
    QCOMPARE(w->name(), qualified);
    QCOMPARE(w->includeFile(), QStringLiteral("QtIndustrialWidgets/%1.h").arg(widgetClass));
    QCOMPARE(w->group(), QStringLiteral("Industrial Widgets"));
    QVERIFY(!w->isContainer());
    QVERIFY(!w->toolTip().isEmpty());
    QVERIFY(!w->whatsThis().isEmpty());
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
