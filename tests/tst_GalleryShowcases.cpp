// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// Every public widget must have a showcase in examples/gallery/Showcases.cpp, the list the
// documentation image export (--export-gallery) renders. A new widget comes with its showcase.
#include <QtTest/QtTest>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/Compass.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/IndustrialSlider.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/TankDisplay.h>
#include <QtIndustrialWidgets/ThumbwheelSwitch.h>
#include <QtIndustrialWidgets/ValveIndicator.h>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtWidgets/QWidget>

#include "ColourPropertyTestUtils.h"
#include "Showcases.h"

#include <map>
#include <memory>

class tst_GalleryShowcases : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyPublicWidgetHasAShowcase();
    void showcasesBuildTheirWidget_data();
    void showcasesBuildTheirWidget();
    void fileNamesAreStableAndUnique();
    void showcasesUseTheGalleryTheme_data();
    void showcasesUseTheGalleryTheme();
};

namespace {

// Widget classes declared in the public headers, read from the source tree at run time
QStringList publicWidgetClasses()
{
    const QDir dir(QStringLiteral(QIW_PUBLIC_HEADER_DIR));
    const QRegularExpression declaration(
        QStringLiteral(R"(class\s+QTINDUSTRIALWIDGETS_EXPORT\s+(\w+)\s*:\s*public\s+QWidget\b)"));
    QStringList classes;
    for (const QString &name : dir.entryList({QStringLiteral("*.h")}, QDir::Files, QDir::Name)) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString text = QString::fromUtf8(file.readAll());
        auto it = declaration.globalMatch(text);
        while (it.hasNext()) {
            classes << it.next().captured(1);
        }
    }
    return classes;
}

using ColourPropertyTest::Colours;
using ColourPropertyTest::colours;

// GalleryShowcase::applyGalleryTheme() for the widgets that GalleryTheme.cpp styles; false for the others
bool applyGalleryTheme(QWidget &w, GalleryShowcase::Theme theme)
{
    using namespace QtIndustrialWidgets;
    using GalleryShowcase::applyGalleryTheme;
    if (auto *x = qobject_cast<RadialGauge *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<LinearGauge *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<IndustrialKnob *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<IndustrialSlider *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<Compass *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<CircularBargraph *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<TankDisplay *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<ValveIndicator *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    if (auto *x = qobject_cast<ThumbwheelSwitch *>(&w)) { applyGalleryTheme(*x, theme); return true; }
    return false;
}

const QStringList &themedClasses()
{
    static const QStringList classes = {
        QStringLiteral("RadialGauge"), QStringLiteral("LinearGauge"),      QStringLiteral("IndustrialKnob"),
        QStringLiteral("Compass"),     QStringLiteral("CircularBargraph"), QStringLiteral("TankDisplay"),
        QStringLiteral("ValveIndicator"), QStringLiteral("IndustrialSlider"), QStringLiteral("ThumbwheelSwitch"),
    };
    return classes;
}

} // namespace

void tst_GalleryShowcases::everyPublicWidgetHasAShowcase()
{
    const QStringList classes = publicWidgetClasses();
    QVERIFY2(classes.size() >= 11, qPrintable(QStringLiteral("only %1 widget classes found in %2")
                                                  .arg(classes.size()).arg(QStringLiteral(QIW_PUBLIC_HEADER_DIR))));
    QSet<QString> covered;
    for (const GalleryShowcase::Showcase &s : GalleryShowcase::showcases()) {
        covered.insert(s.widgetClass);
    }
    QStringList missing;
    for (const QString &c : classes) {
        if (!covered.contains(c)) {
            missing << c;
        }
    }
    QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("no showcase in examples/gallery/Showcases.cpp for: %1")
                                               .arg(missing.join(QStringLiteral(", ")))));
}

void tst_GalleryShowcases::showcasesBuildTheirWidget_data()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<bool>("dark");
    const auto &all = GalleryShowcase::showcases();
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        for (bool dark : {true, false}) {
            QTest::newRow(qPrintable(all[static_cast<size_t>(i)].fileStem(dark ? GalleryShowcase::Theme::Dark
                                                                                 : GalleryShowcase::Theme::Light)))
                << i << dark;
        }
    }
}

void tst_GalleryShowcases::showcasesBuildTheirWidget()
{
    QFETCH(int, index);
    QFETCH(bool, dark);
    const GalleryShowcase::Showcase &s = GalleryShowcase::showcases()[static_cast<size_t>(index)];
    std::unique_ptr<QWidget> widget(s.create(dark ? GalleryShowcase::Theme::Dark : GalleryShowcase::Theme::Light));
    QVERIFY(widget);
    QCOMPARE(QString::fromLatin1(widget->metaObject()->className()),
             QStringLiteral("QtIndustrialWidgets::") + s.widgetClass);
    const QSize size = s.size.isValid() ? s.size : widget->sizeHint();
    QVERIFY(size.width() > 0 && size.height() > 0);
    // Showcases are rendered at their widget's minimum size or larger (#44)
    const QSize min = widget->minimumSizeHint();
    QVERIFY2(size.width() >= min.width() && size.height() >= min.height(),
             qPrintable(QStringLiteral("%1 is rendered at %2x%3, below its minimumSizeHint %4x%5")
                            .arg(s.fileStem(GalleryShowcase::Theme::Dark)).arg(size.width()).arg(size.height()).arg(min.width()).arg(min.height())));
}

void tst_GalleryShowcases::fileNamesAreStableAndUnique()
{
    const QRegularExpression kebab(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    QSet<QString> stems;
    for (const GalleryShowcase::Showcase &s : GalleryShowcase::showcases()) {
        for (const GalleryShowcase::Theme theme : {GalleryShowcase::Theme::Dark, GalleryShowcase::Theme::Light}) {
            const QString stem = s.fileStem(theme);
            QVERIFY2(kebab.match(stem).hasMatch(), qPrintable(stem));
            QVERIFY2(!stems.contains(stem), qPrintable(QStringLiteral("duplicate file name %1").arg(stem)));
            stems.insert(stem);
        }
    }
    QCOMPARE(GalleryShowcase::kebabCase(QStringLiteral("SevenSegmentDisplay")), QStringLiteral("seven-segment-display"));
}

void tst_GalleryShowcases::showcasesUseTheGalleryTheme_data()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<bool>("dark");
    const auto &all = GalleryShowcase::showcases();
    QSet<QString> covered;
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        const GalleryShowcase::Showcase &s = all[static_cast<size_t>(i)];
        if (!themedClasses().contains(s.widgetClass)) {
            continue;
        }
        covered.insert(s.widgetClass);
        for (bool dark : {true, false}) {
            QTest::newRow(qPrintable(s.fileStem(dark ? GalleryShowcase::Theme::Dark : GalleryShowcase::Theme::Light)))
                << i << dark;
        }
    }
    // A missing class would make this test check less than it says
    QCOMPARE(covered.size(), themedClasses().size());
}

// #190: the showcases of the widgets that GalleryTheme.cpp styles (the colours tst_ReadoutContrast measures) carry
// exactly the colours of the gallery theme: applying the theme again changes none of their QColor properties. A colour
// set in Showcases.cpp instead of GalleryTheme.cpp would be exported but not measured
void tst_GalleryShowcases::showcasesUseTheGalleryTheme()
{
    QFETCH(int, index);
    QFETCH(bool, dark);
    const GalleryShowcase::Theme theme = dark ? GalleryShowcase::Theme::Dark : GalleryShowcase::Theme::Light;
    const GalleryShowcase::Theme other = dark ? GalleryShowcase::Theme::Light : GalleryShowcase::Theme::Dark;
    const GalleryShowcase::Showcase &s = GalleryShowcase::showcases()[static_cast<size_t>(index)];
    std::unique_ptr<QWidget> widget(s.create(theme));
    QVERIFY(widget);
    const Colours shown = colours(*widget);
    QVERIFY(applyGalleryTheme(*widget, theme));
    const Colours themed = colours(*widget);
    QStringList diff;
    for (const auto &[name, colour] : themed) {
        if (shown.at(name) != colour) {
            diff << QStringLiteral("%1: %2 in the showcase, %3 in the theme")
                        .arg(name, shown.at(name).name(QColor::HexArgb), colour.name(QColor::HexArgb));
        }
    }
    QVERIFY2(diff.isEmpty(), qPrintable(diff.join(QStringLiteral("; "))));
    // The theme reaches this widget: the other theme changes its colours
    QVERIFY(applyGalleryTheme(*widget, other));
    QVERIFY(colours(*widget) != themed);
}

QTEST_MAIN(tst_GalleryShowcases)
#include "tst_GalleryShowcases.moc"
