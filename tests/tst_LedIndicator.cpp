// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/LedIndicator.h>
#include <QtGui/QPixmap>

using namespace QtIndustrialWidgets;

class tst_LedIndicator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void stateAndToggling();
    void signalEmission();
    void shapes();
    void blinking();
    void clickableMouseInteraction();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_LedIndicator::defaultValues()
{
    LedIndicator led;
    QCOMPARE(led.isOn(), true);
    QCOMPARE(led.isBlinking(), false);
    QCOMPARE(led.shape(), LedIndicator::LedShape::Circular);
    QCOMPARE(led.isClickable(), false);
}

void tst_LedIndicator::stateAndToggling()
{
    LedIndicator led;
    led.setOn(false);
    QCOMPARE(led.isOn(), false);

    led.toggle();
    QCOMPARE(led.isOn(), true);

    led.setOff();
    QCOMPARE(led.isOn(), false);
}

void tst_LedIndicator::signalEmission()
{
    LedIndicator led;
    led.setOn(false);

    QSignalSpy spy(&led, &LedIndicator::stateChanged);
    led.setOn(true);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
}

void tst_LedIndicator::shapes()
{
    LedIndicator led;
    led.setShape(LedIndicator::LedShape::Rectangular);
    QCOMPARE(led.shape(), LedIndicator::LedShape::Rectangular);

    led.setShape(LedIndicator::LedShape::Circular);
    QCOMPARE(led.shape(), LedIndicator::LedShape::Circular);
}

void tst_LedIndicator::blinking()
{
    LedIndicator led;
    led.setBlinking(true);
    QVERIFY(led.isBlinking());

    led.setBlinkRateMs(150);
    QCOMPARE(led.blinkRateMs(), 150);

    led.setBlinking(false);
    QVERIFY(!led.isBlinking());
}

void tst_LedIndicator::clickableMouseInteraction()
{
    LedIndicator led;
    led.setClickable(true);
    led.setOn(false);

    QSignalSpy clickSpy(&led, &LedIndicator::clicked);
    QSignalSpy stateSpy(&led, &LedIndicator::stateChanged);

    QTest::mouseClick(&led, Qt::LeftButton);

    QCOMPARE(clickSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(led.isOn(), true);
}

void tst_LedIndicator::renderOffscreen()
{
    LedIndicator led;
    led.setOn(true);
    led.setShape(LedIndicator::LedShape::Circular);
    led.setLabelText(QStringLiteral("RUN"));
    led.resize(60, 60);

    QPixmap pix(led.size());
    led.render(&pix);
    QVERIFY(!pix.isNull());

    led.setShape(LedIndicator::LedShape::Rectangular);
    QPixmap pixRect(led.size());
    led.render(&pixRect);
    QVERIFY(!pixRect.isNull());
}

void tst_LedIndicator::extremeResizeNoCrash()
{
    LedIndicator led;
    led.resize(1, 1);
    QPixmap pixSmall(led.size());
    led.render(&pixSmall);

    led.resize(1920, 1080);
    QPixmap pixLarge(led.size());
    led.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_LedIndicator)
#include "tst_LedIndicator.moc"


