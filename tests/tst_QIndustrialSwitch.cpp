#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QIndustrialSwitch.h>
#include <QtGui/QPixmap>

class tst_QIndustrialSwitch : public QObject
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

void tst_QIndustrialSwitch::defaultValues()
{
    QIndustrialSwitch sw;
    QCOMPARE(sw.positionCount(), 2);
    QCOMPARE(sw.position(), 0);
    QCOMPARE(sw.isChecked(), false);
    QCOMPARE(sw.switchType(), QIndustrialSwitch::SwitchType::ToggleLever);
    QCOMPARE(sw.hasSafetyGuard(), false);
}

void tst_QIndustrialSwitch::twoPositionToggle()
{
    QIndustrialSwitch sw;
    sw.setAnimated(false);

    sw.setChecked(true);
    QCOMPARE(sw.position(), 1);
    QCOMPARE(sw.isChecked(), true);

    sw.toggle();
    QCOMPARE(sw.position(), 0);
    QCOMPARE(sw.isChecked(), false);
}

void tst_QIndustrialSwitch::threePositionSwitch()
{
    QIndustrialSwitch sw;
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

void tst_QIndustrialSwitch::safetyGuardBehavior()
{
    QIndustrialSwitch sw;
    sw.setAnimated(false);
    sw.setHasSafetyGuard(true);

    QCOMPARE(sw.hasSafetyGuard(), true);
    QCOMPARE(sw.isGuardOpen(), false);

    QSignalSpy guardSpy(&sw, &QIndustrialSwitch::guardToggled);
    sw.setGuardOpen(true);

    QCOMPARE(guardSpy.count(), 1);
    QCOMPARE(sw.isGuardOpen(), true);

    sw.setGuardOpen(false);
    QCOMPARE(sw.isGuardOpen(), false);
}

void tst_QIndustrialSwitch::switchStylesAndOrientations()
{
    QIndustrialSwitch sw;
    sw.setSwitchType(QIndustrialSwitch::SwitchType::Rocker);
    QCOMPARE(sw.switchType(), QIndustrialSwitch::SwitchType::Rocker);

    sw.setOrientation(Qt::Horizontal);
    QCOMPARE(sw.orientation(), Qt::Horizontal);
}

void tst_QIndustrialSwitch::signalEmission()
{
    QIndustrialSwitch sw;
    sw.setAnimated(false);

    QSignalSpy posSpy(&sw, &QIndustrialSwitch::positionChanged);
    QSignalSpy toggleSpy(&sw, &QIndustrialSwitch::toggled);

    sw.setPosition(1);
    QCOMPARE(posSpy.count(), 1);
    QCOMPARE(toggleSpy.count(), 1);
}

void tst_QIndustrialSwitch::keyboardInteraction()
{
    QIndustrialSwitch sw;
    sw.setAnimated(false);

    QTest::keyClick(&sw, Qt::Key_Space);
    QCOMPARE(sw.position(), 1);

    QTest::keyClick(&sw, Qt::Key_Space);
    QCOMPARE(sw.position(), 0);
}

void tst_QIndustrialSwitch::renderOffscreen()
{
    QIndustrialSwitch sw;
    sw.setLabel(QStringLiteral("TEST"));
    sw.setHasSafetyGuard(true);
    sw.setChecked(true);
    sw.resize(80, 130);

    QPixmap pix(sw.size());
    sw.render(&pix);
    QVERIFY(!pix.isNull());

    // Rocker mode
    sw.setSwitchType(QIndustrialSwitch::SwitchType::Rocker);
    QPixmap pixRocker(sw.size());
    sw.render(&pixRocker);
    QVERIFY(!pixRocker.isNull());
}

void tst_QIndustrialSwitch::extremeResizeNoCrash()
{
    QIndustrialSwitch sw;
    sw.resize(2, 2);
    QPixmap pixSmall(sw.size());
    sw.render(&pixSmall);

    sw.resize(2560, 1440);
    QPixmap pixLarge(sw.size());
    sw.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_QIndustrialSwitch)
#include "tst_QIndustrialSwitch.moc"
