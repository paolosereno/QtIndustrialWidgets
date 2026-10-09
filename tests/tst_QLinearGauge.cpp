#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QLinearGauge.h>
#include <QtGui/QPixmap>

class tst_QLinearGauge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void rangeAndClamping();
    void orientationSwitching();
    void thermometerMode();
    void signalEmission();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_QLinearGauge::defaultValues()
{
    QLinearGauge gauge;
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
    QCOMPARE(gauge.value(), 0.0);
    QCOMPARE(gauge.orientation(), Qt::Vertical);
}

void tst_QLinearGauge::rangeAndClamping()
{
    QLinearGauge gauge;
    gauge.setRange(-20.0, 80.0);
    QCOMPARE(gauge.minimum(), -20.0);
    QCOMPARE(gauge.maximum(), 80.0);

    // Below min
    gauge.setValue(-50.0);
    QCOMPARE(gauge.value(), -20.0);

    // Above max
    gauge.setValue(120.0);
    QCOMPARE(gauge.value(), 80.0);

    // Inside range
    gauge.setValue(25.0);
    QCOMPARE(gauge.value(), 25.0);
}

void tst_QLinearGauge::orientationSwitching()
{
    QLinearGauge gauge;
    gauge.setOrientation(Qt::Horizontal);
    QCOMPARE(gauge.orientation(), Qt::Horizontal);

    gauge.setOrientation(Qt::Vertical);
    QCOMPARE(gauge.orientation(), Qt::Vertical);
}

void tst_QLinearGauge::thermometerMode()
{
    QLinearGauge gauge;
    gauge.setThermometerMode(true);
    QVERIFY(gauge.isThermometerMode());

    gauge.setThermometerMode(false);
    QVERIFY(!gauge.isThermometerMode());
}

void tst_QLinearGauge::signalEmission()
{
    QLinearGauge gauge;
    QSignalSpy spy(&gauge, &QLinearGauge::valueChanged);

    gauge.setValue(35.0);
    QCOMPARE(spy.count(), 1);

    // Duplicate value
    gauge.setValue(35.0);
    QCOMPARE(spy.count(), 1);
}

void tst_QLinearGauge::renderOffscreen()
{
    QLinearGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(72.0);
    gauge.setOrientation(Qt::Vertical);
    gauge.setThermometerMode(true);
    gauge.resize(80, 220);

    QPixmap pix(gauge.size());
    gauge.render(&pix);
    QVERIFY(!pix.isNull());

    // Horizontal test
    gauge.setOrientation(Qt::Horizontal);
    gauge.resize(220, 80);
    QPixmap pixH(gauge.size());
    gauge.render(&pixH);
    QVERIFY(!pixH.isNull());
}

void tst_QLinearGauge::extremeResizeNoCrash()
{
    QLinearGauge gauge;
    gauge.resize(1, 1);
    QPixmap pixSmall(gauge.size());
    gauge.render(&pixSmall);

    gauge.resize(2560, 1440);
    QPixmap pixLarge(gauge.size());
    gauge.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

QTEST_MAIN(tst_QLinearGauge)
#include "tst_QLinearGauge.moc"
