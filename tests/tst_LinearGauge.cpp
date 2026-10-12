// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtGui/QPixmap>

#include "internal/LinearGaugeScale.h"

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets;

class tst_LinearGauge : public QObject
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
    void nanAndInfinityResilience();
    void scaleLabelLayout_data();
    void scaleLabelLayout();
    void scaleLabelLayoutNonFinite();
    void scaleLabelMinGap();
};

void tst_LinearGauge::defaultValues()
{
    LinearGauge gauge;
    QCOMPARE(gauge.minimum(), 0.0);
    QCOMPARE(gauge.maximum(), 100.0);
    QCOMPARE(gauge.value(), 0.0);
    QCOMPARE(gauge.orientation(), Qt::Vertical);
}

void tst_LinearGauge::rangeAndClamping()
{
    LinearGauge gauge;
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

void tst_LinearGauge::orientationSwitching()
{
    LinearGauge gauge;
    gauge.setOrientation(Qt::Horizontal);
    QCOMPARE(gauge.orientation(), Qt::Horizontal);

    gauge.setOrientation(Qt::Vertical);
    QCOMPARE(gauge.orientation(), Qt::Vertical);
}

void tst_LinearGauge::thermometerMode()
{
    LinearGauge gauge;
    gauge.setThermometerMode(true);
    QVERIFY(gauge.isThermometerMode());

    gauge.setThermometerMode(false);
    QVERIFY(!gauge.isThermometerMode());
}

void tst_LinearGauge::signalEmission()
{
    LinearGauge gauge;
    QSignalSpy spy(&gauge, &LinearGauge::valueChanged);

    gauge.setValue(35.0);
    QCOMPARE(spy.count(), 1);

    // Duplicate value
    gauge.setValue(35.0);
    QCOMPARE(spy.count(), 1);
}

void tst_LinearGauge::renderOffscreen()
{
    LinearGauge gauge;
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

void tst_LinearGauge::extremeResizeNoCrash()
{
    LinearGauge gauge;
    gauge.resize(1, 1);
    QPixmap pixSmall(gauge.size());
    gauge.render(&pixSmall);

    gauge.resize(2560, 1440);
    QPixmap pixLarge(gauge.size());
    gauge.render(&pixLarge);
    QVERIFY(!pixLarge.isNull());
}

void tst_LinearGauge::nanAndInfinityResilience()
{
    LinearGauge gauge;
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

    // Render offscreen in both orientations
    gauge.resize(80, 250);
    QPixmap pixV(gauge.size());
    gauge.render(&pixV);
    QVERIFY(!pixV.isNull());

    gauge.setOrientation(Qt::Horizontal);
    gauge.resize(250, 80);
    QPixmap pixH(gauge.size());
    gauge.render(&pixH);
    QVERIFY(!pixH.isNull());
}

void tst_LinearGauge::scaleLabelLayout_data()
{
    QTest::addColumn<QVector<double>>("extents"); // size of each major label along the scale
    QTest::addColumn<double>("length");
    QTest::addColumn<double>("minGap");
    QTest::addColumn<int>("step");
    QTest::addColumn<QList<int>>("indices");      // drawn labels

    QTest::newRow("ample space: every label") << QVector<double>(11, 10.0) << 500.0 << 5.0 << 1
                                              << QList<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    // Horizontal 0-250 at its 280x100 size hint, extents and scale length measured from a render:
    // pushing "250" inside the scale leaves 4.3 px after "200" with k = 2, so k = 5
    QTest::newRow("280x100: end push rejects k = 2")
        << QVector<double>{8, 17, 17, 17, 24, 24, 24, 24, 25, 25, 25} << 209.0 << 7.0 << 5 << QList<int>{0, 5, 10};
    // k = 3 or 4 would keep the gap here, but only divisors of 10 are allowed
    QTest::newRow("divisors only: 2 too tight, 5 chosen") << QVector<double>(11, 10.0) << 100.0 << 12.0 << 5
                                                          << QList<int>{0, 5, 10};
    QTest::newRow("prime majorTicks: first and last") << QVector<double>(8, 10.0) << 70.0 << 12.0 << 7
                                                      << QList<int>{0, 7};
    QTest::newRow("prime majorTicks: every label") << QVector<double>(8, 10.0) << 700.0 << 12.0 << 1
                                                   << QList<int>{0, 1, 2, 3, 4, 5, 6, 7};
    QTest::newRow("majorTicks 0: single label") << QVector<double>{10.0} << 100.0 << 5.0 << 1 << QList<int>{0};
    QTest::newRow("majorTicks 1: both labels") << QVector<double>{10.0, 10.0} << 100.0 << 5.0 << 1 << QList<int>{0, 1};
    // Even first and last do not keep the gap: they are drawn anyway (decided in #31)
    QTest::newRow("degenerate: first and last anyway") << QVector<double>(11, 30.0) << 40.0 << 5.0 << 10
                                                       << QList<int>{0, 10};
    QTest::newRow("no labels") << QVector<double>() << 100.0 << 5.0 << 0 << QList<int>();
}

void tst_LinearGauge::scaleLabelLayout()
{
    QFETCH(QVector<double>, extents);
    QFETCH(double, length);
    QFETCH(double, minGap);
    QFETCH(int, step);
    QFETCH(QList<int>, indices);

    const std::vector<double> ext(extents.cbegin(), extents.cend());
    const internal::ScaleLabelLayout layout = internal::layoutScaleLabels(ext, length, minGap);

    QCOMPARE(layout.step, step);
    QList<int> drawn;
    for (const internal::ScaleLabelSlot &slot : layout.labels) {
        drawn.append(slot.index);
        QCOMPARE(slot.end - slot.start, ext[static_cast<size_t>(slot.index)]); // a label keeps its size
    }
    QCOMPARE(drawn, indices);
    if (layout.labels.empty()) {
        return;
    }

    const int n = static_cast<int>(ext.size()) - 1;
    if (n > 0) {
        QCOMPARE(n % layout.step, 0); // the step is a divisor of majorTicks
    }
    const internal::ScaleLabelSlot &first = layout.labels.front();
    const internal::ScaleLabelSlot &last = layout.labels.back();
    QCOMPARE(first.index, 0);
    QCOMPARE(last.index, n);
    if (ext.front() <= length && ext.back() <= length) { // the end labels stay inside the scale
        QVERIFY(first.start >= 0.0);
        QVERIFY(last.end <= length);
    }
    // Interior labels stay centred on their tick
    for (const internal::ScaleLabelSlot &slot : layout.labels) {
        if (slot.index > 0 && slot.index < n) {
            QCOMPARE((slot.start + slot.end) * 0.5, length * slot.index / n);
        }
    }
    // The gap holds between drawn neighbours, except in the degenerate first/last case
    if (layout.step < n || n <= 1) {
        for (size_t i = 1; i < layout.labels.size(); ++i) {
            QVERIFY2(layout.labels[i].start - layout.labels[i - 1].end >= minGap,
                     qPrintable(QStringLiteral("labels %1 and %2").arg(layout.labels[i - 1].index).arg(layout.labels[i].index)));
        }
    }
}

void tst_LinearGauge::scaleLabelLayoutNonFinite()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    // Non-finite extents count as 0: on a finite scale every label fits
    const internal::ScaleLabelLayout finiteScale = internal::layoutScaleLabels({10.0, nan, inf, 10.0}, 300.0, nan);
    QCOMPARE(finiteScale.step, 1);
    QCOMPARE(static_cast<int>(finiteScale.labels.size()), 4);

    // A non-finite scale length counts as 0: all ticks coincide, so only first and last are drawn
    const internal::ScaleLabelLayout zeroScale = internal::layoutScaleLabels({10.0, nan, inf, 10.0}, inf, 5.0);
    QCOMPARE(zeroScale.step, 3);
    QCOMPARE(static_cast<int>(zeroScale.labels.size()), 2);

    for (const internal::ScaleLabelLayout *layout : {&finiteScale, &zeroScale}) {
        for (const internal::ScaleLabelSlot &slot : layout->labels) {
            QVERIFY(std::isfinite(slot.start));
            QVERIFY(std::isfinite(slot.end));
        }
    }
}

void tst_LinearGauge::scaleLabelMinGap()
{
    QCOMPARE(internal::scaleLabelMinGap(13.5), 6.75);
    QCOMPARE(internal::scaleLabelMinGap(8.0), 4.0);
    QCOMPARE(internal::scaleLabelMinGap(6.0), 4.0);
    QCOMPARE(internal::scaleLabelMinGap(std::numeric_limits<double>::quiet_NaN()), 4.0);
}

QTEST_MAIN(tst_LinearGauge)
#include "tst_LinearGauge.moc"
