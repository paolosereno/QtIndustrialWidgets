// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtGui/QPixmap>

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_CircularBargraph : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultValues();
    void settersAndClamping();
    void signalEmission();
    void nanAndInfinityResilience();
    void displayModes();
    void peakHoldAndDecay();
    void digitalDisplay();
    void extremeResizing();
    void offscreenRendering();
};

void tst_CircularBargraph::defaultValues()
{
    CircularBargraph bargraph;
    QCOMPARE(bargraph.minimum(), 0.0);
    QCOMPARE(bargraph.maximum(), 100.0);
    QCOMPARE(bargraph.value(), 0.0);
    QCOMPARE(bargraph.segmentCount(), 24);
    QCOMPARE(bargraph.startAngle(), -135.0);
    QCOMPARE(bargraph.spanAngle(), 270.0);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);
    QCOMPARE(bargraph.warningThreshold(), 70.0);
    QCOMPARE(bargraph.errorThreshold(), 85.0);
    QCOMPARE(bargraph.isPeakHoldEnabled(), true);
    QCOMPARE(bargraph.peakHoldTimeMs(), 1200);
    QCOMPARE(bargraph.isDigitalDisplayVisible(), true);
    QCOMPARE(bargraph.precision(), 1);
    QCOMPARE(bargraph.unit(), QStringLiteral("%"));
}

void tst_CircularBargraph::settersAndClamping()
{
    CircularBargraph bargraph;
    bargraph.setRange(10.0, 50.0);
    QCOMPARE(bargraph.minimum(), 10.0);
    QCOMPARE(bargraph.maximum(), 50.0);

    // Below minimum clamp
    bargraph.setValue(5.0);
    QCOMPARE(bargraph.value(), 10.0);

    // Above maximum clamp
    bargraph.setValue(60.0);
    QCOMPARE(bargraph.value(), 50.0);

    // Valid intermediate value
    bargraph.setValue(30.0);
    QCOMPARE(bargraph.value(), 30.0);

    // Inverted range rejection
    bargraph.setRange(80.0, 20.0);
    QCOMPARE(bargraph.minimum(), 10.0);
    QCOMPARE(bargraph.maximum(), 50.0);

    // Segment count
    bargraph.setSegmentCount(32);
    QCOMPARE(bargraph.segmentCount(), 32);
    bargraph.setSegmentCount(2); // Too small (< 4), must be rejected
    QCOMPARE(bargraph.segmentCount(), 32);

    // Span angle
    bargraph.setSpanAngle(180.0);
    QCOMPARE(bargraph.spanAngle(), 180.0);
    bargraph.setSpanAngle(-10.0); // Invalid <= 0
    QCOMPARE(bargraph.spanAngle(), 180.0);
    bargraph.setSpanAngle(400.0); // Invalid > 360
    QCOMPARE(bargraph.spanAngle(), 180.0);
}

void tst_CircularBargraph::signalEmission()
{
    CircularBargraph bargraph;
    QSignalSpy spyValue(&bargraph, &CircularBargraph::valueChanged);
    QSignalSpy spyRange(&bargraph, &CircularBargraph::rangeChanged);
    QSignalSpy spyThreshold(&bargraph, &CircularBargraph::thresholdChanged);
    QSignalSpy spyAppearance(&bargraph, &CircularBargraph::appearanceChanged);

    // 1. Value change
    bargraph.setValue(45.0);
    QCOMPARE(spyValue.count(), 1);
    QCOMPARE(spyValue.takeFirst().at(0).toDouble(), 45.0);

    // Duplicate value must not emit signal
    bargraph.setValue(45.0);
    QCOMPARE(spyValue.count(), 0);

    // 2. Range change
    bargraph.setRange(0.0, 200.0);
    QCOMPARE(spyRange.count(), 1);
    auto rangeArgs = spyRange.takeFirst();
    QCOMPARE(rangeArgs.at(0).toDouble(), 0.0);
    QCOMPARE(rangeArgs.at(1).toDouble(), 200.0);

    // 3. Threshold change
    bargraph.setWarningThreshold(140.0);
    QCOMPARE(spyThreshold.count(), 1);
    bargraph.setErrorThreshold(180.0);
    QCOMPARE(spyThreshold.count(), 2);

    // 4. Appearance change on visual setters
    spyAppearance.clear();
    bargraph.setSegmentCount(48);
    QCOMPARE(spyAppearance.count(), 1);
    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Dot);
    QCOMPARE(spyAppearance.count(), 2);
}

void tst_CircularBargraph::nanAndInfinityResilience()
{
    CircularBargraph bargraph;
    bargraph.setRange(0.0, 100.0);
    bargraph.setValue(42.0);
    QCOMPARE(bargraph.value(), 42.0);

    // NaN must trigger fail-safe hold (value unchanged)
    bargraph.setValue(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(bargraph.value(), 42.0);

    // Positive infinity saturates to maximum
    bargraph.setValue(std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.value(), 100.0);

    // Negative infinity saturates to minimum
    bargraph.setValue(-std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.value(), 0.0);

    // Non-finite range bounds rejected
    bargraph.setRange(std::numeric_limits<double>::quiet_NaN(), 100.0);
    QCOMPARE(bargraph.minimum(), 0.0);
    bargraph.setRange(0.0, std::numeric_limits<double>::infinity());
    QCOMPARE(bargraph.maximum(), 100.0);
}

void tst_CircularBargraph::displayModes()
{
    CircularBargraph bargraph;
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);

    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Dot);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Dot);

    bargraph.setDisplayMode(CircularBargraph::DisplayMode::Bar);
    QCOMPARE(bargraph.displayMode(), CircularBargraph::DisplayMode::Bar);
}

void tst_CircularBargraph::peakHoldAndDecay()
{
    CircularBargraph bargraph;
    bargraph.setRange(0.0, 100.0);
    bargraph.setPeakHoldEnabled(true);

    bargraph.setValue(80.0);
    QCOMPARE(bargraph.peakValue(), 80.0);

    // Value drops, peak stays at 80.0
    bargraph.setValue(30.0);
    QCOMPARE(bargraph.value(), 30.0);
    QCOMPARE(bargraph.peakValue(), 80.0);

    // Manual reset returns peak to current value
    bargraph.resetPeak();
    QCOMPARE(bargraph.peakValue(), 30.0);

    // Disabling peak hold resets peak value
    bargraph.setValue(90.0);
    QCOMPARE(bargraph.peakValue(), 90.0);
    bargraph.setValue(50.0);
    bargraph.setPeakHoldEnabled(false);
    QCOMPARE(bargraph.peakValue(), 50.0);
}

void tst_CircularBargraph::digitalDisplay()
{
    CircularBargraph bargraph;
    bargraph.setPrecision(2);
    QCOMPARE(bargraph.precision(), 2);

    bargraph.setUnit(QStringLiteral("bar"));
    QCOMPARE(bargraph.unit(), QStringLiteral("bar"));

    bargraph.setDigitalDisplayVisible(false);
    QCOMPARE(bargraph.isDigitalDisplayVisible(), false);
}

void tst_CircularBargraph::extremeResizing()
{
    CircularBargraph bargraph;
    bargraph.show();

    const QList<QSize> sizes = {
        QSize(0, 0),
        QSize(1, 1),
        QSize(2, 2),
        QSize(4, 4),
        QSize(10, 1000),
        QSize(1000, 10),
        QSize(3840, 2160)
    };

    for (const QSize &sz : sizes) {
        bargraph.resize(sz);
        bargraph.repaint();
    }
}

void tst_CircularBargraph::offscreenRendering()
{
    CircularBargraph bargraph;
    bargraph.setRange(0.0, 100.0);
    bargraph.setValue(75.0);
    bargraph.setWarningThreshold(60.0);
    bargraph.setErrorThreshold(80.0);
    bargraph.resize(200, 200);

    QPixmap pixmap(200, 200);
    pixmap.fill(Qt::transparent);
    bargraph.render(&pixmap);

    QVERIFY(!pixmap.isNull());
    QCOMPARE(pixmap.size(), QSize(200, 200));

    // HiDPI offscreen check
    pixmap.setDevicePixelRatio(2.0);
    bargraph.render(&pixmap);
    QVERIFY(!pixmap.isNull());
}

QTEST_MAIN(tst_CircularBargraph)
#include "tst_CircularBargraph.moc"
