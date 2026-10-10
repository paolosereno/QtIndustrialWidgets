// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtGui/QPixmap>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_RadialGauge : public QObject
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
    void nanAndInfinityResilience();
};

void tst_RadialGauge::initTestCase()
{
}

void tst_RadialGauge::defaultValues()
{
    RadialGauge gauge;
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
    QCOMPARE(gauge.value(), 0.0);
    QCOMPARE(gauge.spanAngle(), 270.0);
    QCOMPARE(gauge.precision(), 1);
}

void tst_RadialGauge::rangeAndClamping()
{
    RadialGauge gauge;
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

void tst_RadialGauge::invalidRangeIgnored()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    // Setting min >= max should be safely rejected
    gauge.setRange(150.0, 50.0);
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
}

void tst_RadialGauge::signalEmission()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);

    QSignalSpy valueSpy(&gauge, &RadialGauge::valueChanged);
    gauge.setValue(45.0);

    QCOMPARE(valueSpy.count(), 1);
    QCOMPARE(valueSpy.takeFirst().at(0).toDouble(), 45.0);
}

void tst_RadialGauge::noDuplicateSignal()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(45.0);

    QSignalSpy valueSpy(&gauge, &RadialGauge::valueChanged);
    // Setting identical value should not emit duplicate signal
    gauge.setValue(45.0);
    QCOMPARE(valueSpy.count(), 0);
}

void tst_RadialGauge::thresholdZones()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setWarningThreshold(75.0);
    gauge.setErrorThreshold(90.0);

    QCOMPARE(gauge.warningThreshold(), 75.0);
    QCOMPARE(gauge.errorThreshold(), 90.0);
}

void tst_RadialGauge::renderOffscreen()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(60.0);
    gauge.resize(250, 250);

    QPixmap pix(gauge.size());
    pix.fill(Qt::transparent);
    gauge.render(&pix);
    QVERIFY(!pix.isNull());
}

void tst_RadialGauge::extremeResizeNoCrash()
{
    RadialGauge gauge;
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

void tst_RadialGauge::nanAndInfinityResilience()
{
    RadialGauge gauge;
    gauge.setRange(0.0, 100.0);
    gauge.setValue(50.0);
    QCOMPARE(gauge.value(), 50.0);

    // NaN input must be ignored (fail-safe hold)
    gauge.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(gauge.value(), 50.0);

    // +Infinity must clamp to maximum
    gauge.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.value(), 100.0);

    // -Infinity must clamp to minimum
    gauge.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.value(), 0.0);

    // Invalid range setters with NaN or Inf must be ignored
    gauge.setRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
    QCOMPARE(gauge.minimum(), 0.0);
    gauge.setRange(0.0, std::numeric_limits<double>::infinity());
    QCOMPARE(gauge.maximum(), 100.0);

    // Render offscreen to ensure no NaN warnings or painter faults occur
    gauge.resize(200, 200);
    QPixmap pix(gauge.size());
    gauge.render(&pix);
    QVERIFY(!pix.isNull());
}

QTEST_MAIN(tst_RadialGauge)
#include "tst_RadialGauge.moc"


