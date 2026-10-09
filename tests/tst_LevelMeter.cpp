// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/LevelMeter.h>
#include <QtGui/QPixmap>

using namespace QtIndustrialWidgets;

class tst_LevelMeter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void channelValuesAndClamping();
    void peakHoldAndReset();
    void overloadSignal();
    void displayModes();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_LevelMeter::defaultValues()
{
    LevelMeter meter;
    QCOMPARE(meter.channelCount(), 2);
    QCOMPARE(meter.minimum(), -60.0);
    QCOMPARE(meter.maximum(), 6.0);
    QCOMPARE(meter.displayMode(), LevelMeter::DisplayMode::Segmented);
    QCOMPARE(meter.orientation(), Qt::Vertical);
    QCOMPARE(meter.isPeakHoldEnabled(), true);
}

void tst_LevelMeter::channelValuesAndClamping()
{
    LevelMeter meter;
    meter.setRange(-40.0, 10.0);

    // Below minimum
    meter.setValue(0, -60.0);
    QCOMPARE(meter.value(0), -40.0);

    // Above maximum
    meter.setValue(1, 25.0);
    QCOMPARE(meter.value(1), 10.0);

    // Valid values
    meter.setValue(0, -12.0);
    meter.setValue(1, -6.0);
    QCOMPARE(meter.value(0), -12.0);
    QCOMPARE(meter.value(1), -6.0);
}

void tst_LevelMeter::peakHoldAndReset()
{
    LevelMeter meter;
    meter.setRange(0.0, 100.0);
    meter.setValue(0, 80.0);

    QCOMPARE(meter.peakValue(0), 80.0);

    // Drop value lower: peak should hold at 80.0
    meter.setValue(0, 40.0);
    QCOMPARE(meter.value(0), 40.0);
    QCOMPARE(meter.peakValue(0), 80.0);

    meter.resetPeaks();
    QCOMPARE(meter.peakValue(0), 40.0);
}

void tst_LevelMeter::overloadSignal()
{
    LevelMeter meter;
    meter.setRange(-20.0, 10.0);
    meter.setErrorThreshold(0.0);

    QSignalSpy overloadSpy(&meter, &LevelMeter::overloadOccurred);
    meter.setValue(0, -5.0);
    QCOMPARE(overloadSpy.count(), 0);

    // Exceed error threshold
    meter.setValue(0, 2.5);
    QCOMPARE(overloadSpy.count(), 1);
    QCOMPARE(overloadSpy.takeFirst().at(0).toInt(), 0);
}

void tst_LevelMeter::displayModes()
{
    LevelMeter meter;
    meter.setDisplayMode(LevelMeter::DisplayMode::Continuous);
    QCOMPARE(meter.displayMode(), LevelMeter::DisplayMode::Continuous);

    meter.setOrientation(Qt::Horizontal);
    QCOMPARE(meter.orientation(), Qt::Horizontal);
}

void tst_LevelMeter::signalEmission()
{
    LevelMeter meter;
    meter.setRange(0.0, 100.0);

    QSignalSpy spy(&meter, &LevelMeter::valueChanged);
    meter.setValue(0, 55.0);

    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 0);
    QCOMPARE(args.at(1).toDouble(), 55.0);
}

void tst_LevelMeter::renderOffscreen()
{
    LevelMeter meter;
    meter.setChannelCount(2);
    meter.setValue(0, -12.0);
    meter.setValue(1, -6.0);
    meter.resize(80, 220);

    QPixmap pix(meter.size());
    meter.render(&pix);
    QVERIFY(!pix.isNull());

    // Continuous and horizontal mode
    meter.setDisplayMode(LevelMeter::DisplayMode::Continuous);
    meter.setOrientation(Qt::Horizontal);
    meter.resize(220, 80);
    QPixmap pixH(meter.size());
    meter.render(&pixH);
    QVERIFY(!pixH.isNull());
}

void tst_LevelMeter::extremeResizeNoCrash()
{
    LevelMeter meter;
    meter.resize(2, 2);
    QPixmap pixSmall(meter.size());
    meter.render(&pixSmall);

    meter.resize(2560, 1440);
    QPixmap pixLarge(meter.size());
    meter.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_LevelMeter)
#include "tst_LevelMeter.moc"


