#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QLevelMeter.h>
#include <QtGui/QPixmap>

class tst_QLevelMeter : public QObject
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

void tst_QLevelMeter::defaultValues()
{
    QLevelMeter meter;
    QCOMPARE(meter.channelCount(), 2);
    QCOMPARE(meter.minimum(), -60.0);
    QCOMPARE(meter.maximum(), 6.0);
    QCOMPARE(meter.displayMode(), QLevelMeter::DisplayMode::Segmented);
    QCOMPARE(meter.orientation(), Qt::Vertical);
    QCOMPARE(meter.isPeakHoldEnabled(), true);
}

void tst_QLevelMeter::channelValuesAndClamping()
{
    QLevelMeter meter;
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

void tst_QLevelMeter::peakHoldAndReset()
{
    QLevelMeter meter;
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

void tst_QLevelMeter::overloadSignal()
{
    QLevelMeter meter;
    meter.setRange(-20.0, 10.0);
    meter.setErrorThreshold(0.0);

    QSignalSpy overloadSpy(&meter, &QLevelMeter::overloadOccurred);
    meter.setValue(0, -5.0);
    QCOMPARE(overloadSpy.count(), 0);

    // Exceed error threshold
    meter.setValue(0, 2.5);
    QCOMPARE(overloadSpy.count(), 1);
    QCOMPARE(overloadSpy.takeFirst().at(0).toInt(), 0);
}

void tst_QLevelMeter::displayModes()
{
    QLevelMeter meter;
    meter.setDisplayMode(QLevelMeter::DisplayMode::Continuous);
    QCOMPARE(meter.displayMode(), QLevelMeter::DisplayMode::Continuous);

    meter.setOrientation(Qt::Horizontal);
    QCOMPARE(meter.orientation(), Qt::Horizontal);
}

void tst_QLevelMeter::signalEmission()
{
    QLevelMeter meter;
    meter.setRange(0.0, 100.0);

    QSignalSpy spy(&meter, &QLevelMeter::valueChanged);
    meter.setValue(0, 55.0);

    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 0);
    QCOMPARE(args.at(1).toDouble(), 55.0);
}

void tst_QLevelMeter::renderOffscreen()
{
    QLevelMeter meter;
    meter.setChannelCount(2);
    meter.setValue(0, -12.0);
    meter.setValue(1, -6.0);
    meter.resize(80, 220);

    QPixmap pix(meter.size());
    meter.render(&pix);
    QVERIFY(!pix.isNull());

    // Continuous and horizontal mode
    meter.setDisplayMode(QLevelMeter::DisplayMode::Continuous);
    meter.setOrientation(Qt::Horizontal);
    meter.resize(220, 80);
    QPixmap pixH(meter.size());
    meter.render(&pixH);
    QVERIFY(!pixH.isNull());
}

void tst_QLevelMeter::extremeResizeNoCrash()
{
    QLevelMeter meter;
    meter.resize(2, 2);
    QPixmap pixSmall(meter.size());
    meter.render(&pixSmall);

    meter.resize(2560, 1440);
    QPixmap pixLarge(meter.size());
    meter.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_QLevelMeter)
#include "tst_QLevelMeter.moc"
