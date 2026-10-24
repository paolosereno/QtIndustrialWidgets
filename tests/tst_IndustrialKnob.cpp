// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialKnob.h>

#include "FontTestUtils.h"

#include <memory>
#include <QtGui/QPixmap>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_IndustrialKnob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void textSizeIgnoresWidgetFontSize();
    void fontChangeRefreshesCache();
    void labelsFollowWidgetFont();
    void defaultValues();
    void rangeAndClamping();
    void modes();
    void keyboardInteraction();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void nanAndInfinityResilience();
};

void tst_IndustrialKnob::defaultValues()
{
    IndustrialKnob knob;
    QCOMPARE(knob.minimum(), 0.0);
    QCOMPARE(knob.maximum(), 100.0);
    QCOMPARE(knob.value(), 25.0);
    QCOMPARE(knob.mode(), IndustrialKnob::KnobMode::Continuous);
}

void tst_IndustrialKnob::rangeAndClamping()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 50.0);

    // Below minimum
    knob.setValue(-25.0);
    QCOMPARE(knob.value(), 0.0);

    // Above maximum
    knob.setValue(75.0);
    QCOMPARE(knob.value(), 50.0);

    // Valid value
    knob.setValue(25.0);
    QCOMPARE(knob.value(), 25.0);
}

void tst_IndustrialKnob::modes()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setMode(IndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5);

    QCOMPARE(knob.mode(), IndustrialKnob::KnobMode::Discrete);
    QCOMPARE(knob.discreteSteps(), 5);

    // In discrete 5 steps over [0, 100], steps are 0, 25, 50, 75, 100
    knob.setValue(22.0);
    QCOMPARE(knob.value(), 25.0);

    knob.setValue(68.0);
    QCOMPARE(knob.value(), 75.0);
}

void tst_IndustrialKnob::keyboardInteraction()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(10.0);
    knob.setStep(5.0);

    QTest::keyClick(&knob, Qt::Key_Up);
    QCOMPARE(knob.value(), 15.0);

    QTest::keyClick(&knob, Qt::Key_Down);
    QCOMPARE(knob.value(), 10.0);
}

void tst_IndustrialKnob::signalEmission()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);

    QSignalSpy spy(&knob, &IndustrialKnob::valueChanged);
    knob.setValue(30.0);
    QCOMPARE(spy.count(), 1);

    // Duplicate value
    knob.setValue(30.0);
    QCOMPARE(spy.count(), 1);
}

void tst_IndustrialKnob::renderOffscreen()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(45.0);
    knob.resize(120, 120);

    QPixmap pix(knob.size());
    knob.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_IndustrialKnob::extremeResizeNoCrash()
{
    IndustrialKnob knob;
    knob.resize(1, 1);
    QPixmap pixSmall(knob.size());
    knob.render(&pixSmall);

    knob.resize(2560, 1440);
    QPixmap pixLarge(knob.size());
    knob.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

void tst_IndustrialKnob::nanAndInfinityResilience()
{
    IndustrialKnob knob;
    knob.setRange(0.0, 100.0);
    knob.setValue(50.0);
    QCOMPARE(knob.value(), 50.0);

    // Continuous mode: NaN ignored
    knob.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(knob.value(), 50.0);

    // +Inf and -Inf clamped
    knob.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(knob.value(), 100.0);
    knob.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(knob.value(), 0.0);

    // Discrete mode: NaN must not cause undefined behavior / crash
    knob.setMode(IndustrialKnob::KnobMode::Discrete);
    knob.setDiscreteSteps(5);
    knob.setValue(50.0);
    knob.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(knob.value(), 50.0);

    // Range with NaN/Inf ignored
    knob.setRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
    QCOMPARE(knob.minimum(), 0.0);

    // Offscreen render check
    knob.resize(150, 150);
    QPixmap pix(knob.size());
    knob.render(&pix);
    QVERIFY(!pix.isNull());
}

// #51: the scale labels (value display hidden) are drawn into the static cache with the widget font (family, style, spacing),
// not with the application font: a widget created with another font of the same size draws other
// text pixels. Before #51 the cached text used painter.font() (the application font on a pixmap)
void tst_IndustrialKnob::labelsFollowWidgetFont()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<IndustrialKnob>();
        if (font) g->setFont(*font);
        g->setValueDisplayVisible(false);
        g->resize(220, 240);
        return g;
    };
    auto standard = make(nullptr);
    const QFont other = FontTest::otherFont(standard->font());
    auto changed = make(&other);
    const int pixels = FontTest::differingPixelCount(FontTest::render(*standard), FontTest::render(*changed));
    QVERIFY2(pixels > 100, qPrintable(QStringLiteral("only %1 pixels change with the widget font").arg(pixels)));
}

// #51: setFont() on a widget already painted refreshes its cached background: the next render is
// identical to that of a widget created with the new font. Before #51 the cache was invalidated only
// by the widget's own setters, resizeEvent() and (some widgets) PaletteChange, not by FontChange
void tst_IndustrialKnob::fontChangeRefreshesCache()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<IndustrialKnob>();
        if (font) g->setFont(*font);
        g->resize(220, 240);
        return g;
    };
    // Shown widgets: grab() on a hidden one delivers a pending resize event, which refreshes the
    // cache by itself and hides the defect
    auto painted = make(nullptr);
    painted->show();
    QVERIFY(QTest::qWaitForWindowExposed(painted.get()));
    const QFont other = FontTest::otherFont(painted->font());
    const QImage before = FontTest::render(*painted);
    painted->setFont(other);
    QCoreApplication::processEvents();
    const QImage after = FontTest::render(*painted);
    // One window at a time: the active one has the focus (IndustrialKnob draws a focus ring)
    painted->hide();
    auto fresh = make(&other);
    fresh->show();
    QVERIFY(QTest::qWaitForWindowExposed(fresh.get()));
    const QImage expected = FontTest::render(*fresh);
    QVERIFY2(FontTest::differingPixelCount(before, expected) > 100, "the font change is not visible: the test checks nothing");
    const int stale = FontTest::differingPixelCount(after, expected);
    QVERIFY2(stale == 0, qPrintable(QStringLiteral("%1 pixels differ from a widget created with the new font").arg(stale)));
}

// #51: this widget follows rule (a): its texts take family and style from the widget font, their
// size from the widget geometry, until its layout makes room for larger text. A widget font twice
// as large leaves the tallest glyph drawn in textColor unchanged
void tst_IndustrialKnob::textSizeIgnoresWidgetFontSize()
{
    auto make = [](const QFont *font) {
        auto g = std::make_unique<IndustrialKnob>();
        if (font) g->setFont(*font);
        g->setValueDisplayVisible(false);
        g->resize(220, 240);
        return g;
    };
    auto standard = make(nullptr);
    const QFont doubled = FontTest::doubledFont();
    auto large = make(&doubled);
    const int base = FontTest::textGlyphHeight(*standard);
    const int grown = FontTest::textGlyphHeight(*large);
    QVERIFY2(base > 0, "no text drawn in textColor");
    QVERIFY2(grown == base, qPrintable(QStringLiteral("tallest glyph %1 px with the default font, %2 px with a font twice as large").arg(base).arg(grown)));
}

QTEST_MAIN(tst_IndustrialKnob)
#include "tst_IndustrialKnob.moc"


