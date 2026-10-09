// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include <QtGui/QPixmap>

using namespace QtIndustrialWidgets;

class tst_IndustrialSwitch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void twoPositionToggle();
    void threePositionSwitch();
    void safetyGuardBehavior();
    void switchStylesAndOrientations();
    void signalEmission();
    void keyboardInteraction();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_IndustrialSwitch::defaultValues()
{
    IndustrialSwitch sw;
    QCOMPARE(sw.positionCount(), 2);
    QCOMPARE(sw.position(), 0);
    QCOMPARE(sw.isChecked(), false);
    QCOMPARE(sw.switchType(), IndustrialSwitch::SwitchType::ToggleLever);
    QCOMPARE(sw.hasSafetyGuard(), false);
}

void tst_IndustrialSwitch::twoPositionToggle()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);

    sw.setChecked(true);
    QCOMPARE(sw.position(), 1);
    QCOMPARE(sw.isChecked(), true);

    sw.toggle();
    QCOMPARE(sw.position(), 0);
    QCOMPARE(sw.isChecked(), false);
}

void tst_IndustrialSwitch::threePositionSwitch()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);
    sw.setPositionCount(3);

    QCOMPARE(sw.positionCount(), 3);
    sw.setPosition(2);
    QCOMPARE(sw.position(), 2);
    QCOMPARE(sw.isChecked(), true);

    sw.setPosition(1);
    QCOMPARE(sw.position(), 1);
    QCOMPARE(sw.isChecked(), false);
}

void tst_IndustrialSwitch::safetyGuardBehavior()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);
    sw.setHasSafetyGuard(true);

    QCOMPARE(sw.hasSafetyGuard(), true);
    QCOMPARE(sw.isGuardOpen(), false);

    QSignalSpy guardSpy(&sw, &IndustrialSwitch::guardToggled);
    sw.setGuardOpen(true);

    QCOMPARE(guardSpy.count(), 1);
    QCOMPARE(sw.isGuardOpen(), true);

    sw.setGuardOpen(false);
    QCOMPARE(sw.isGuardOpen(), false);
}

void tst_IndustrialSwitch::switchStylesAndOrientations()
{
    IndustrialSwitch sw;
    sw.setSwitchType(IndustrialSwitch::SwitchType::Rocker);
    QCOMPARE(sw.switchType(), IndustrialSwitch::SwitchType::Rocker);

    sw.setOrientation(Qt::Horizontal);
    QCOMPARE(sw.orientation(), Qt::Horizontal);
}

void tst_IndustrialSwitch::signalEmission()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);

    QSignalSpy posSpy(&sw, &IndustrialSwitch::positionChanged);
    QSignalSpy toggleSpy(&sw, &IndustrialSwitch::toggled);

    sw.setPosition(1);
    QCOMPARE(posSpy.count(), 1);
    QCOMPARE(toggleSpy.count(), 1);
}

void tst_IndustrialSwitch::keyboardInteraction()
{
    IndustrialSwitch sw;
    sw.setAnimated(false);

    QTest::keyClick(&sw, Qt::Key_Space);
    QCOMPARE(sw.position(), 1);

    QTest::keyClick(&sw, Qt::Key_Space);
    QCOMPARE(sw.position(), 0);
}

void tst_IndustrialSwitch::renderOffscreen()
{
    IndustrialSwitch sw;
    sw.setLabel(QStringLiteral("TEST"));
    sw.setHasSafetyGuard(true);
    sw.setChecked(true);
    sw.resize(80, 130);

    QPixmap pix(sw.size());
    sw.render(&pix);
    QVERIFY(!pix.isNull());

    // Rocker mode
    sw.setSwitchType(IndustrialSwitch::SwitchType::Rocker);
    QPixmap pixRocker(sw.size());
    sw.render(&pixRocker);
    QVERIFY(!pixRocker.isNull());
}

void tst_IndustrialSwitch::extremeResizeNoCrash()
{
    IndustrialSwitch sw;
    sw.resize(2, 2);
    QPixmap pixSmall(sw.size());
    sw.render(&pixSmall);

    sw.resize(2560, 1440);
    QPixmap pixLarge(sw.size());
    sw.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_IndustrialSwitch)
#include "tst_IndustrialSwitch.moc"


