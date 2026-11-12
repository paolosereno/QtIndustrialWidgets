// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #190: the theme colours of the gallery (examples/gallery/GalleryTheme.cpp) are set in both themes. A widget switched
// from one theme to the other ends with the same colours as a new widget given only the second theme, as the live
// gallery does when the theme button is pressed. A colour set in one theme only would stay behind after a switch (the
// scale of the gauges did, in the dark theme, before #190).

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

#include "ColourPropertyTestUtils.h"
#include "GalleryTheme.h"

#include <functional>
#include <map>
#include <memory>

using namespace QtIndustrialWidgets;
using GalleryShowcase::Theme;

namespace {

using ColourPropertyTest::Colours;
using ColourPropertyTest::colours;

QString differences(const Colours &got, const Colours &expected)
{
    QStringList out;
    for (const auto &[name, colour] : expected) {
        const auto it = got.find(name);
        const QColor value = it == got.end() ? QColor() : it->second;
        if (value != colour) {
            out << QStringLiteral("%1: %2, expected %3").arg(name, value.name(QColor::HexArgb), colour.name(QColor::HexArgb));
        }
    }
    return out.join(QStringLiteral("; "));
}

QString themeName(Theme theme) { return theme == Theme::Dark ? QStringLiteral("dark") : QStringLiteral("light"); }

// Creates a new widget with the given themes applied in order
using Make = std::function<std::unique_ptr<QWidget>(const std::vector<Theme> &)>;

template <typename Widget>
Make make()
{
    return [](const std::vector<Theme> &themes) {
        auto w = std::make_unique<Widget>();
        for (Theme theme : themes) {
            GalleryShowcase::applyGalleryTheme(*w, theme);
        }
        return std::unique_ptr<QWidget>(std::move(w));
    };
}

} // namespace

class tst_GalleryTheme : public QObject
{
    Q_OBJECT

private slots:
    void switchEqualsSecondThemeOnly_data();
    void switchEqualsSecondThemeOnly();
};

void tst_GalleryTheme::switchEqualsSecondThemeOnly_data()
{
    QTest::addColumn<Make>("create");
    QTest::addColumn<Theme>("first");
    QTest::addColumn<Theme>("second");
    const std::vector<std::pair<const char *, Make>> widgets = {
        {"RadialGauge", make<RadialGauge>()},           {"LinearGauge", make<LinearGauge>()},
        {"IndustrialKnob", make<IndustrialKnob>()},     {"Compass", make<Compass>()},
        {"CircularBargraph", make<CircularBargraph>()}, {"TankDisplay", make<TankDisplay>()},
        {"ValveIndicator", make<ValveIndicator>()},     {"ThumbwheelSwitch", make<ThumbwheelSwitch>()},
        {"IndustrialSlider", make<IndustrialSlider>()},
    };
    for (const auto &[name, create] : widgets) {
        QTest::addRow("%s, light then dark", name) << create << Theme::Light << Theme::Dark;
        QTest::addRow("%s, dark then light", name) << create << Theme::Dark << Theme::Light;
    }
}

void tst_GalleryTheme::switchEqualsSecondThemeOnly()
{
    QFETCH(Make, create);
    QFETCH(Theme, first);
    QFETCH(Theme, second);
    const Colours switched = colours(*create({first, second}));
    const Colours secondOnly = colours(*create({second}));
    const Colours firstOnly = colours(*create({first}));
    // The check means something only if the two themes differ in some colour of this widget
    QVERIFY2(!secondOnly.empty(), "no QColor property");
    QVERIFY2(firstOnly != secondOnly, "the two themes give the same colours");
    const QString diff = differences(switched, secondOnly);
    QVERIFY2(diff.isEmpty(), qPrintable(QStringLiteral("%1 then %2 differs from %2 only: %3")
                                            .arg(themeName(first), themeName(second), diff)));
}

QTEST_MAIN(tst_GalleryTheme)
#include "tst_GalleryTheme.moc"
