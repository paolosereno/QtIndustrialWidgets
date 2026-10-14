// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>

#include "internal/RadialGaugeLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets::internal;

namespace {

LayoutRect expanded(const LayoutRect &r, double m)
{
    return {r.x - m, r.y - m, r.w + 2.0 * m, r.h + 2.0 * m};
}

// Label rectangle of width w and height h centred at (cx, cy)
LayoutRect labelAt(double cx, double cy, double w, double h)
{
    return {cx - w * 0.5, cy - h * 0.5, w, h};
}

// Input for a 300 px dial (r = 146) centred at (150, 150), with the end labels of the default
// 270-degree arc at 135 degrees on both sides of 12 o'clock
PodLayoutInput dial300(double startLabelWidth, double endLabelWidth)
{
    PodLayoutInput in;
    in.centerX = 150.0;
    in.centerY = 150.0;
    in.radius = 146.0;
    const double labelRadius = radialScaleGeometry(300.0).labelRadius;
    const double d = labelRadius * std::sqrt(0.5);
    in.labelRects = {labelAt(150.0 - d, 150.0 + d, startLabelWidth, 14.0), labelAt(150.0 + d, 150.0 + d, endLabelWidth, 14.0)};
    in.minPodWidth = 40.0;
    in.minPodHeight = 14.0;
    in.unitWidth = 20.0;
    return in;
}

bool clearOf(const LayoutRect &box, const std::vector<LayoutRect> &labels, double margin)
{
    return std::none_of(labels.begin(), labels.end(), [&](const LayoutRect &l) { return expanded(box, margin).intersects(l); });
}

} // namespace

class tst_RadialGaugeLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scaleGeometryUnchanged_data();
    void scaleGeometryUnchanged();
    void podKeepsPreferredPlaceWithoutLabels();
    void podNarrowedBetweenEndLabels_data();
    void podNarrowedBetweenEndLabels();
    void unitBandChecked();
    void podLoweredBelowBlockingLabel();
    void valueHasPrecedenceOverLabels();
    void podHeightHoldsValue();
    void nonFiniteInputGivesEmptyPod();
    void fontFitPicksLargestFittingSize();
    void fontFitFallsBackToMinimum();
};

// #43: the scale radius and the label font are those of main (formulas copied from 84ada40)
void tst_RadialGaugeLayout::scaleGeometryUnchanged_data()
{
    QTest::addColumn<double>("side");
    for (double side : {20.0, 100.0, 200.0, 260.0, 300.0, 400.0, 1000.0}) {
        QTest::addRow("%g px", side) << side;
    }
}

void tst_RadialGaugeLayout::scaleGeometryUnchanged()
{
    QFETCH(double, side);
    const double radius = (side * 0.5) - 4.0;
    const double arcRadius = radius * 0.82;
    const double arcWidth = std::max(3.0, radius * 0.055);
    const double tickOuter = arcRadius - (arcWidth * 0.6);
    const double majorTickInner = tickOuter - (radius * 0.09);
    const double labelRadius = majorTickInner - (radius * 0.12);
    const int labelFont = std::max(7, static_cast<int>(radius * 0.082));

    const RadialScaleGeometry g = radialScaleGeometry(side);
    QCOMPARE(g.radius, radius);
    QCOMPARE(g.tickOuter, tickOuter);
    QCOMPARE(g.majorTickInner, majorTickInner);
    QCOMPARE(g.minorTickInner, tickOuter - (radius * 0.045));
    QCOMPARE(g.labelRadius, labelRadius);
    QCOMPARE(g.labelFontPx, labelFont);
}

void tst_RadialGaugeLayout::podKeepsPreferredPlaceWithoutLabels()
{
    PodLayoutInput in = dial300(0.0, 0.0);
    in.labelRects.clear();
    const PodLayout out = layoutDigitalPod(in);
    QVERIFY(out.clearOfLabels);
    QVERIFY(!out.lowered);
    QCOMPARE(out.pod.w, 146.0 * 0.68);
    QCOMPARE(out.pod.h, 146.0 * 0.28);
    QCOMPARE(out.pod.y, 150.0 + 146.0 * 0.28);
    QCOMPARE(out.pod.x + out.pod.w * 0.5, 150.0);
}

void tst_RadialGaugeLayout::podNarrowedBetweenEndLabels_data()
{
    QTest::addColumn<double>("startWidth");
    QTest::addColumn<double>("endWidth");
    QTest::addRow("0 / 8000") << 8.0 << 31.0;
    QTest::addRow("-1000 / 1000") << 36.0 << 30.0;
    QTest::addRow("0 / 100") << 8.0 << 24.0;
}

void tst_RadialGaugeLayout::podNarrowedBetweenEndLabels()
{
    QFETCH(double, startWidth);
    QFETCH(double, endWidth);
    const PodLayoutInput in = dial300(startWidth, endWidth);
    const PodLayout out = layoutDigitalPod(in);
    const double m = podLabelMargin(in.radius);

    QVERIFY(out.clearOfLabels);
    QVERIFY(!out.lowered);
    QVERIFY(out.pod.w < 146.0 * 0.68);
    QVERIFY(out.pod.w >= in.minPodWidth);
    QCOMPARE(out.pod.x + out.pod.w * 0.5, 150.0);
    QVERIFY2(clearOf(out.pod, in.labelRects, m - 1e-9), "pod closer than the margin to a label");
    // As wide as the labels allow: the nearer label is exactly at the margin
    const double gapToStart = out.pod.x - in.labelRects[0].right();
    const double gapToEnd = in.labelRects[1].x - out.pod.right();
    QVERIFY(std::abs(std::min(gapToStart, gapToEnd) - m) < 1e-9);
}

void tst_RadialGaugeLayout::unitBandChecked()
{
    // A label straddling the centre line in the unit band, above the pod band
    PodLayoutInput in = dial300(8.0, 31.0);
    const double podTop = 150.0 + 146.0 * 0.28;
    in.labelRects.push_back(labelAt(150.0, podTop - 146.0 * 0.07, 10.0, 6.0));
    PodLayout out = layoutDigitalPod(in);
    QVERIFY2(out.lowered || !out.clearOfLabels, "the unit band over a label was not detected");

    // Without a unit the same label above the pod band does not matter
    in.unitWidth = 0.0;
    out = layoutDigitalPod(in);
    QVERIFY(out.clearOfLabels);
    QVERIFY(!out.lowered);
}

void tst_RadialGaugeLayout::podLoweredBelowBlockingLabel()
{
    // A label across the centre line in the preferred band: the pod goes below it
    PodLayoutInput in = dial300(8.0, 31.0);
    in.unitWidth = 0.0;
    const LayoutRect blocker = labelAt(150.0, 150.0 + 146.0 * 0.35, 30.0, 14.0);
    in.labelRects.push_back(blocker);
    const PodLayout out = layoutDigitalPod(in);
    const double m = podLabelMargin(in.radius);

    QVERIFY(out.lowered);
    QVERIFY(out.clearOfLabels);
    QVERIFY(out.pod.y >= blocker.bottom() + m - 1e-9);
    QVERIFY(clearOf(out.pod, in.labelRects, m - 1e-9));
    // Still inside the inner dial
    const double innerR = 146.0 * 0.95 - m;
    const double dx = out.pod.w * 0.5;
    const double dy = out.pod.bottom() - 150.0;
    QVERIFY(dx * dx + dy * dy <= innerR * innerR + 1e-6);
}

// #43: the value is never clipped: with no room between the labels the pod is minPodWidth wide anyway
void tst_RadialGaugeLayout::valueHasPrecedenceOverLabels()
{
    PodLayoutInput in = dial300(150.0, 150.0); // labels that leave no room anywhere
    in.minPodWidth = 90.0;
    const PodLayout out = layoutDigitalPod(in);
    QVERIFY(!out.clearOfLabels);
    QCOMPARE(out.pod.w, 90.0);
    QCOMPARE(out.pod.x + out.pod.w * 0.5, 150.0);
    QVERIFY(out.pod.h > 0.0);
}

void tst_RadialGaugeLayout::podHeightHoldsValue()
{
    PodLayoutInput in = dial300(8.0, 31.0);
    in.minPodHeight = 146.0 * 0.28 + 5.0;
    QCOMPARE(layoutDigitalPod(in).pod.h, in.minPodHeight);
}

void tst_RadialGaugeLayout::nonFiniteInputGivesEmptyPod()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (int field = 0; field < 6; ++field) {
        for (double bad : {nan, inf}) {
            PodLayoutInput in = dial300(8.0, 31.0);
            double *targets[] = {&in.centerX, &in.centerY, &in.radius, &in.minPodWidth, &in.minPodHeight, &in.unitWidth};
            *targets[field] = bad;
            const PodLayout out = layoutDigitalPod(in);
            QCOMPARE(out.pod.w, 0.0);
            QCOMPARE(out.pod.h, 0.0);
            QVERIFY(!out.clearOfLabels);
        }
    }
}

void tst_RadialGaugeLayout::fontFitPicksLargestFittingSize()
{
    // Width 3 px per pixel size, height 1.2 px per pixel size
    auto width = [](int px) { return 3.0 * px; };
    auto height = [](int px) { return 1.2 * px; };
    FontFit fit = fitFontPixelSize(21, 11, 45.0, 100.0, width, height);
    QVERIFY(fit.fits);
    QCOMPARE(fit.pixelSize, 15);
    fit = fitFontPixelSize(21, 11, 1000.0, 100.0, width, height);
    QCOMPARE(fit.pixelSize, 21);
    fit = fitFontPixelSize(21, 11, 1000.0, 18.0, width, height);
    QCOMPARE(fit.pixelSize, 15);
}

void tst_RadialGaugeLayout::fontFitFallsBackToMinimum()
{
    auto width = [](int px) { return 3.0 * px; };
    auto height = [](int px) { return 1.2 * px; };
    FontFit fit = fitFontPixelSize(21, 11, 10.0, 100.0, width, height);
    QVERIFY(!fit.fits);
    QCOMPARE(fit.pixelSize, 11);
    // preferred below minimum: the minimum is used
    fit = fitFontPixelSize(5, 8, 1000.0, 100.0, width, height);
    QVERIFY(fit.fits);
    QCOMPARE(fit.pixelSize, 8);
}

QTEST_MAIN(tst_RadialGaugeLayout)
#include "tst_RadialGaugeLayout.moc"
