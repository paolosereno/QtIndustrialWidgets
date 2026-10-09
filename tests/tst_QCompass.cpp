// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QCompass.h>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>

class tst_QCompass : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void defaultValues();
    void headingNormalization();
    void targetHeadingAndBug();
    void courseDeviationCalculation();
    void displayModeSwitch();
    void colorSetters();
    void mouseInteractionBug();
    void renderOffscreen();
    void extremeResizeNoCrash();
    void cleanupTestCase();
};

void tst_QCompass::initTestCase()
{
}

void tst_QCompass::cleanupTestCase()
{
}

void tst_QCompass::defaultValues()
{
    QCompass compass;
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(compass.targetHeading(), 0.0);
    QCOMPARE(compass.displayMode(), QCompass::DisplayMode::HeadingUp);
    QVERIFY(compass.isHeadingBugVisible());
    QVERIFY(compass.isHeadingBugInteractive());
    QVERIFY(compass.isLubberLineVisible());
    QVERIFY(compass.isDigitalReadoutVisible());
    QCOMPARE(compass.courseDeviation(), 0.0);
    QVERIFY(compass.minimumSizeHint().width() > 0);
    QVERIFY(compass.sizeHint().width() >= compass.minimumSizeHint().width());
}

void tst_QCompass::headingNormalization()
{
    QCompass compass;
    QSignalSpy spy(&compass, &QCompass::headingChanged);

    // Standard heading within [0, 360)
    compass.setHeading(45.0);
    QCOMPARE(compass.heading(), 45.0);
    QCOMPARE(spy.count(), 1);

    // 360.0 wraps to 0.0
    compass.setHeading(360.0);
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(spy.count(), 2);

    // Multiple revolutions
    compass.setHeading(720.0);
    QCOMPARE(compass.heading(), 0.0);
    QCOMPARE(spy.count(), 2); // No change from 0.0, no new emission

    compass.setHeading(450.0);
    QCOMPARE(compass.heading(), 90.0);
    QCOMPARE(spy.count(), 3);

    // Negative angle wraps properly
    compass.setHeading(-90.0);
    QCOMPARE(compass.heading(), 270.0);
    QCOMPARE(spy.count(), 4);

    compass.setHeading(-370.0);
    QCOMPARE(compass.heading(), 350.0);
    QCOMPARE(spy.count(), 5);

    // Static helper
    QCOMPARE(QCompass::normalizeDegrees(0.0), 0.0);
    QCOMPARE(QCompass::normalizeDegrees(360.0), 0.0);
    QCOMPARE(QCompass::normalizeDegrees(-45.0), 315.0);
}

void tst_QCompass::targetHeadingAndBug()
{
    QCompass compass;
    QSignalSpy spyTarget(&compass, &QCompass::targetHeadingChanged);
    QSignalSpy spyApp(&compass, &QCompass::appearanceChanged);

    compass.setTargetHeading(120.0);
    QCOMPARE(compass.targetHeading(), 120.0);
    QCOMPARE(spyTarget.count(), 1);

    compass.setTargetHeading(-45.0);
    QCOMPARE(compass.targetHeading(), 315.0);
    QCOMPARE(spyTarget.count(), 2);

    // Setting same value does not emit
    compass.setTargetHeading(315.0);
    QCOMPARE(spyTarget.count(), 2);

    // Bug visibility and interaction
    compass.setHeadingBugVisible(false);
    QVERIFY(!compass.isHeadingBugVisible());
    QVERIFY(spyApp.count() >= 1);

    compass.setHeadingBugInteractive(false);
    QVERIFY(!compass.isHeadingBugInteractive());

    compass.setLubberLineVisible(false);
    QVERIFY(!compass.isLubberLineVisible());

    compass.setDigitalReadoutVisible(false);
    QVERIFY(!compass.isDigitalReadoutVisible());
}

void tst_QCompass::courseDeviationCalculation()
{
    QCompass compass;

    // On course
    compass.setHeading(45.0);
    compass.setTargetHeading(45.0);
    QCOMPARE(compass.courseDeviation(), 0.0);

    // 10 degrees to starboard (+10°)
    compass.setHeading(50.0);
    compass.setTargetHeading(40.0);
    QCOMPARE(compass.courseDeviation(), 10.0);

    // 10 degrees to port (-10°)
    compass.setHeading(30.0);
    compass.setTargetHeading(40.0);
    QCOMPARE(compass.courseDeviation(), -10.0);

    // Crossing 0° / 360° north meridian
    compass.setHeading(10.0);
    compass.setTargetHeading(350.0);
    QCOMPARE(compass.courseDeviation(), 20.0);

    compass.setHeading(350.0);
    compass.setTargetHeading(10.0);
    QCOMPARE(compass.courseDeviation(), -20.0);
}

void tst_QCompass::displayModeSwitch()
{
    QCompass compass;
    QSignalSpy spyMode(&compass, &QCompass::displayModeChanged);

    QCOMPARE(compass.displayMode(), QCompass::DisplayMode::HeadingUp);

    compass.setDisplayMode(QCompass::DisplayMode::NorthUp);
    QCOMPARE(compass.displayMode(), QCompass::DisplayMode::NorthUp);
    QCOMPARE(spyMode.count(), 1);

    // Re-setting same mode does not emit duplicate
    compass.setDisplayMode(QCompass::DisplayMode::NorthUp);
    QCOMPARE(spyMode.count(), 1);

    compass.setDisplayMode(QCompass::DisplayMode::HeadingUp);
    QCOMPARE(compass.displayMode(), QCompass::DisplayMode::HeadingUp);
    QCOMPARE(spyMode.count(), 2);
}

void tst_QCompass::colorSetters()
{
    QCompass compass;
    QSignalSpy spy(&compass, &QCompass::appearanceChanged);

    compass.setDialColor(QColor(10, 10, 10));
    QCOMPARE(compass.dialColor(), QColor(10, 10, 10));

    compass.setBezelColor(QColor(40, 50, 60));
    QCOMPARE(compass.bezelColor(), QColor(40, 50, 60));

    compass.setTextColor(QColor(255, 255, 255));
    QCOMPARE(compass.textColor(), QColor(255, 255, 255));

    compass.setCardinalColor(QColor(0, 200, 255));
    QCOMPARE(compass.cardinalColor(), QColor(0, 200, 255));

    compass.setNeedleColor(QColor(255, 0, 0));
    QCOMPARE(compass.needleColor(), QColor(255, 0, 0));

    compass.setNeedleTailColor(QColor(200, 200, 200));
    QCOMPARE(compass.needleTailColor(), QColor(200, 200, 200));

    compass.setBugColor(QColor(255, 128, 0));
    QCOMPARE(compass.bugColor(), QColor(255, 128, 0));

    compass.setLubberColor(QColor(255, 255, 0));
    QCOMPARE(compass.lubberColor(), QColor(255, 255, 0));

    QVERIFY(spy.count() >= 8);
}

void tst_QCompass::mouseInteractionBug()
{
    QCompass compass;
    compass.resize(300, 300);
    compass.show();
    QVERIFY(QTest::qWaitForWindowExposed(&compass));

    compass.setDisplayMode(QCompass::DisplayMode::NorthUp);
    compass.setHeadingBugInteractive(true);

    // Center is (150, 150).
    // Click at 3 o'clock: (250, 150) -> angle = 90° (East)
    QPoint eastPoint(250, 150);
    QTest::mousePress(&compass, Qt::LeftButton, Qt::NoModifier, eastPoint);
    QTest::mouseRelease(&compass, Qt::LeftButton, Qt::NoModifier, eastPoint);

    // Allow slight float tolerance around 90°
    QVERIFY(std::abs(compass.targetHeading() - 90.0) < 3.0);

    // Drag to 6 o'clock: (150, 250) -> angle = 180° (South)
    QPoint southPoint(150, 250);
    QTest::mousePress(&compass, Qt::LeftButton, Qt::NoModifier, eastPoint);
    QTest::mouseMove(&compass, southPoint);
    QTest::mouseRelease(&compass, Qt::LeftButton, Qt::NoModifier, southPoint);

    QVERIFY(std::abs(compass.targetHeading() - 180.0) < 3.0);
}

void tst_QCompass::renderOffscreen()
{
    QCompass compass;
    compass.resize(250, 250);
    compass.setHeading(45.0);
    compass.setTargetHeading(90.0);

    // Render HeadingUp mode
    compass.setDisplayMode(QCompass::DisplayMode::HeadingUp);
    QPixmap pixmap1(250, 250);
    pixmap1.fill(Qt::transparent);
    compass.render(&pixmap1);
    QVERIFY(!pixmap1.isNull());

    // Render NorthUp mode
    compass.setDisplayMode(QCompass::DisplayMode::NorthUp);
    QPixmap pixmap2(250, 250);
    pixmap2.fill(Qt::transparent);
    compass.render(&pixmap2);
    QVERIFY(!pixmap2.isNull());
}

void tst_QCompass::extremeResizeNoCrash()
{
    QCompass compass;

    const QList<QSize> extremeSizes = {
        QSize(0, 0),
        QSize(1, 1),
        QSize(5, 5),
        QSize(10, 10),
        QSize(20, 20),
        QSize(100, 100),
        QSize(800, 800),
        QSize(1920, 1080),
        QSize(3840, 2160)
    };

    for (const auto &sz : extremeSizes) {
        compass.resize(sz);
        if (sz.width() > 0 && sz.height() > 0) {
            QPixmap pixmap(sz);
            compass.render(&pixmap);
        }
    }
}

QTEST_MAIN(tst_QCompass)
#include "tst_QCompass.moc"
