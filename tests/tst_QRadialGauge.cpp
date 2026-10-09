#include <QtTest/QtTest>
#include <QtIndustrialWidgets/QRadialGauge.h>
#include <QtGui/QPixmap>

class tst_QRadialGauge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void defaultValues();
    void rangeAndClamping();
    void invalidRangeIgnored();
    void signalEmission();
    void noDuplicateSignal();
    void thresholdZones();
    void renderOffscreen();
    void extremeResizeNoCrash();
};

void tst_QRadialGauge::initTestCase()
{
}

void tst_QRadialGauge::defaultValues()
{
    QRadialGauge gauge;
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
    QCOMPARE(gauge.value(), 0.0);
    QCOMPARE(gauge.spanAngle(), 270.0);
    QCOMPARE(gauge.precision(), 1);
}

void tst_QRadialGauge::rangeAndClamping()
{
    QRadialGauge gauge;
    gauge.setRange(0.0, 200.0);
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 200.0);

    // Value above maximum should be clamped to maximum
    gauge.setValue(250.0);
    QCOMPARE(gauge.value(), 200.0);

    // Value below minimum should be clamped to minimum
    gauge.setValue(-50.0);
    QCOMPARE(gauge.value(), 0.0);

    // Valid value inside range
    gauge.setValue(125.0);
    QCOMPARE(gauge.value(), 125.0);
}

void tst_QRadialGauge::invalidRangeIgnored()
{
    QRadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    // Setting min >= max should be safely rejected
    gauge.setRange(150.0, 50.0);
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
}

void tst_QRadialGauge::signalEmission()
{
    QRadialGauge gauge;
    gauge.setRange(0.0, 100.0);

    QSignalSpy valueSpy(&gauge, &QRadialGauge::valueChanged);
    gauge.setValue(45.0);

    QCOMPARE(valueSpy.count(), 1);
    QCOMPARE(valueSpy.takeFirst().at(0).toDouble(), 45.0);
}

void tst_QRadialGauge::noDuplicateSignal()
{
    QRadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(45.0);

    QSignalSpy valueSpy(&gauge, &QRadialGauge::valueChanged);
    // Setting identical value should not emit duplicate signal
    gauge.setValue(45.0);
    QCOMPARE(valueSpy.count(), 0);
}

void tst_QRadialGauge::thresholdZones()
{
    QRadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setWarningThreshold(75.0);
    gauge.setErrorThreshold(90.0);

    QCOMPARE(gauge.warningThreshold(), 75.0);
    QCOMPARE(gauge.errorThreshold(), 90.0);
}

void tst_QRadialGauge::renderOffscreen()
{
    QRadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(60.0);
    gauge.resize(250, 250);

    QPixmap pix(gauge.size());
    pix.fill(Qt::transparent);
    gauge.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_QRadialGauge::extremeResizeNoCrash()
{
    QRadialGauge gauge;
    // Tiny size
    gauge.resize(2, 2);
    QPixmap pixSmall(gauge.size());
    gauge.render(&pixSmall);

    // 4K Ultra HD size
    gauge.resize(3840, 2160);
    QPixmap pix4k(gauge.size());
    gauge.render(&pix4k);
    QVERIFY(!pix4k.isNull());
}

QTEST_MAIN(tst_QRadialGauge)
#include "tst_QRadialGauge.moc"
