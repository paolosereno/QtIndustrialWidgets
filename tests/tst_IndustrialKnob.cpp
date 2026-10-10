// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtGui/QPixmap>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_IndustrialKnob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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

QTEST_MAIN(tst_IndustrialKnob)
#include "tst_IndustrialKnob.moc"


