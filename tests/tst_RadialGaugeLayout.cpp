// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtTest/QtTest>

#include "internal/LinearGaugeScale.h"
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

// Synthetic text metrics: every character is 0.6 px wide per font pixel, lines are 1.2 px per pixel
std::function<double(int)> widthOf(int characters)
{
    return [characters](int px) { return 0.6 * px * characters; };
}
double lineHeight(int px) { return 1.2 * px; }

// Input for a 300 px dial (r = 146) centred at (150, 150), with the end labels of the default
// 270-degree arc at 135 degrees on both sides of 12 o'clock. Value fonts 11..21 px (as the widget
// at 300 px), 4-character value, 6 px padding.
PodLayoutInput dial300(double startLabelWidth, double endLabelWidth)
{
    PodLayoutInput in;
    in.centerX = 150.0;
    in.centerY = 150.0;
    in.radius = 146.0;
    const double labelRadius = radialScaleGeometry(300.0).labelRadius;
    const double d = labelRadius * std::sqrt(0.5);
    in.labelRects = {labelAt(150.0 - d, 150.0 + d, startLabelWidth, 14.0), labelAt(150.0 + d, 150.0 + d, endLabelWidth, 14.0)};
    in.unitWidth = 20.0;
    in.padding = 6.0;
    in.preferredFontPx = 21;
    in.minimumFontPx = 11;
    in.valueWidth = widthOf(4);
    in.valueHeight = lineHeight;
    return in;
}

bool clearOf(const LayoutRect &box, const std::vector<LayoutRect> &labels, double margin)
{
    return std::none_of(labels.begin(), labels.end(), [&](const LayoutRect &l) { return expanded(box, margin).intersects(l); });
}

// Boxes of the major labels of a synthetic dial (#50): majorTicks + 1 labels at the label radius
// of a dial of the given side, on an arc from startAngle (0 deg = 12 o'clock, clockwise) over
// spanAngle; label i is characters[i] characters wide (synthetic metrics above) at the label font
std::vector<LayoutRect> arcLabels(double side, double startAngle, double spanAngle, const std::vector<int> &characters)
{
    const RadialScaleGeometry g = radialScaleGeometry(side);
    const int n = static_cast<int>(characters.size()) - 1;
    std::vector<LayoutRect> boxes;
    for (int i = 0; i <= n; ++i) {
        const double rad = (startAngle + spanAngle * (n == 0 ? 0.0 : static_cast<double>(i) / n) - 90.0) * M_PI / 180.0;
        boxes.push_back(labelAt(side * 0.5 + g.labelRadius * std::cos(rad), side * 0.5 + g.labelRadius * std::sin(rad),
                                widthOf(characters[static_cast<size_t>(i)])(g.labelFontPx), lineHeight(g.labelFontPx)));
    }
    return boxes;
}

// Characters of the labels of a linear range with 10 major ticks (integer tick values)
std::vector<int> rangeCharacters(double minimum, double maximum)
{
    std::vector<int> out;
    for (int i = 0; i <= 10; ++i) {
        out.push_back(static_cast<int>(QString::number(static_cast<qint64>(std::round(minimum + (maximum - minimum) * i / 10.0))).size()));
    }
    return out;
}

// True when every pair of the given labels keeps minGap
bool allPairsKeepGap(const std::vector<LayoutRect> &boxes, const std::vector<int> &indices, double minGap)
{
    for (size_t a = 0; a < indices.size(); ++a) {
        for (size_t b = a + 1; b < indices.size(); ++b) {
            if (layoutRectGap(boxes[static_cast<size_t>(indices[a])], boxes[static_cast<size_t>(indices[b])]) < minGap) {
                return false;
            }
        }
    }
    return true;
}

std::vector<int> multiplesOf(int step, int n, bool withLast)
{
    std::vector<int> out;
    for (int i = 0; i <= n; i += step) {
        if (i < n || withLast) {
            out.push_back(i);
        }
    }
    return out;
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
    void placeWithLargerValueFontChosen();
    void tieKeepsPreferredPlace();
    void unitBandChecked();
    void podLoweredBelowBlockingLabel();
    void valueHasPrecedenceOverLabels();
    void podHeightHoldsValue();
    void nonFiniteInputGivesEmptyPod();
    void fontFitPicksLargestFittingSize();
    void fontFitFallsBackToMinimum();
    void labelGapAlongSeparatingAxis();
    void scaleLabelsKeepMinimumGap_data();
    void scaleLabelsKeepMinimumGap();
    void primeMajorTicksKeepOnlyEndLabels();
    void fullCircleDropsLastLabel();
    void fullCircleChecksEveryPair();
    void degenerateLabelInput();
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
    QCOMPARE(out.valueFontPx, 21);
}

void tst_RadialGaugeLayout::podNarrowedBetweenEndLabels_data()
{
    QTest::addColumn<double>("startWidth");
    QTest::addColumn<double>("endWidth");
    QTest::addRow("0 / 8000") << 8.0 << 31.0;
    QTest::addRow("-1000 / 1000") << 36.0 << 30.0;
    QTest::addRow("0 / 100") << 8.0 << 24.0;
}

// A short value fits at the full font at the preferred place: the pod stays there (a tie with the
// lowered place) and is only narrowed
void tst_RadialGaugeLayout::podNarrowedBetweenEndLabels()
{
    QFETCH(double, startWidth);
    QFETCH(double, endWidth);
    PodLayoutInput in = dial300(startWidth, endWidth);
    in.valueWidth = widthOf(3);
    const PodLayout out = layoutDigitalPod(in);
    const double m = podLabelMargin(in.radius);

    QVERIFY(out.clearOfLabels);
    QVERIFY(!out.lowered);
    QCOMPARE(out.valueFontPx, 21);
    QVERIFY(out.pod.w < 146.0 * 0.68);
    QCOMPARE(out.pod.x + out.pod.w * 0.5, 150.0);
    QVERIFY2(clearOf(out.pod, in.labelRects, m - 1e-9), "pod closer than the margin to a label");
    // As wide as the labels allow: the nearer label is exactly at the margin
    const double gapToStart = out.pod.x - in.labelRects[0].right();
    const double gapToEnd = in.labelRects[1].x - out.pod.right();
    QVERIFY(std::abs(std::min(gapToStart, gapToEnd) - m) < 1e-9);
}

// #43 (review of PR #48): the place giving the larger value font is used. With wide end labels a
// 6-character value fits only at a smaller font between them; below them it fits at the full font.
void tst_RadialGaugeLayout::placeWithLargerValueFontChosen()
{
    PodLayoutInput in = dial300(50.0, 50.0);
    in.unitWidth = 0.0;
    in.valueWidth = widthOf(6);

    // At the preferred place the pod fits between the end labels, keeping the margin: the largest
    // font there is below the full 21 px
    const double m = podLabelMargin(in.radius);
    const double labelBottom = in.labelRects[0].bottom();
    const double halfAtPreferred = (in.labelRects[1].x - 150.0) - m;
    const int fontAtPreferred = fitFontPixelSize(21, 11, 2.0 * halfAtPreferred - 2.0 * in.padding, 146.0 * 0.28,
                                                 in.valueWidth, in.valueHeight).pixelSize;
    QVERIFY2(fontAtPreferred < 21, qPrintable(QString::number(fontAtPreferred)));

    const PodLayout out = layoutDigitalPod(in);
    QVERIFY(out.lowered);
    QVERIFY(out.clearOfLabels);
    QCOMPARE(out.valueFontPx, 21);
    QVERIFY(out.pod.y >= labelBottom + m - 1e-9);
    QVERIFY(clearOf(out.pod, in.labelRects, m - 1e-9));
}

// On a tie the preferred place is kept, even if the lowered place is wider
void tst_RadialGaugeLayout::tieKeepsPreferredPlace()
{
    PodLayoutInput in = dial300(50.0, 50.0);
    in.unitWidth = 0.0;
    in.valueWidth = widthOf(2); // fits at 21 px at both places
    const PodLayout out = layoutDigitalPod(in);
    QVERIFY(!out.lowered);
    QCOMPARE(out.valueFontPx, 21);
    QCOMPARE(out.pod.y, 150.0 + 146.0 * 0.28);
}

void tst_RadialGaugeLayout::unitBandChecked()
{
    // A label straddling the centre line in the unit band, above the preferred pod band
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

// #43: the value is never clipped: with no room at either place, the pod holds the value at the
// minimum font anyway
void tst_RadialGaugeLayout::valueHasPrecedenceOverLabels()
{
    PodLayoutInput in = dial300(150.0, 150.0); // labels that leave no room anywhere
    in.valueWidth = widthOf(12);
    const PodLayout out = layoutDigitalPod(in);
    QVERIFY(!out.clearOfLabels);
    QCOMPARE(out.valueFontPx, 11);
    QCOMPARE(out.pod.w, 0.6 * 11 * 12 + 2.0 * 6.0);
    QCOMPARE(out.pod.x + out.pod.w * 0.5, 150.0);
    QVERIFY(out.pod.h > 0.0);
}

void tst_RadialGaugeLayout::podHeightHoldsValue()
{
    PodLayoutInput in = dial300(8.0, 31.0);
    in.minimumFontPx = 40; // 48 px line, taller than 0.28 r = 40.9
    in.preferredFontPx = 40;
    in.valueWidth = widthOf(1);
    QCOMPARE(layoutDigitalPod(in).pod.h, 48.0);
}

void tst_RadialGaugeLayout::nonFiniteInputGivesEmptyPod()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (int field = 0; field < 5; ++field) {
        for (double bad : {nan, inf}) {
            PodLayoutInput in = dial300(8.0, 31.0);
            double *targets[] = {&in.centerX, &in.centerY, &in.radius, &in.unitWidth, &in.padding};
            *targets[field] = bad;
            const PodLayout out = layoutDigitalPod(in);
            QCOMPARE(out.pod.w, 0.0);
            QCOMPARE(out.pod.h, 0.0);
            QVERIFY(!out.clearOfLabels);
        }
    }
    // Non-finite or missing text metrics
    PodLayoutInput in = dial300(8.0, 31.0);
    in.valueWidth = [nan](int) { return nan; };
    QCOMPARE(layoutDigitalPod(in).pod.w, 0.0);
    in = dial300(8.0, 31.0);
    in.valueHeight = nullptr;
    QCOMPARE(layoutDigitalPod(in).pod.w, 0.0);
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

// #50: free space between two boxes along the axis that separates them; negative on overlap
void tst_RadialGaugeLayout::labelGapAlongSeparatingAxis()
{
    const LayoutRect a{0.0, 0.0, 10.0, 5.0};
    QCOMPARE(layoutRectGap(a, LayoutRect{14.0, 0.0, 10.0, 5.0}), 4.0);  // side by side
    QCOMPARE(layoutRectGap(LayoutRect{14.0, 0.0, 10.0, 5.0}, a), 4.0);  // symmetric
    QCOMPARE(layoutRectGap(a, LayoutRect{0.0, 8.0, 10.0, 5.0}), 3.0);   // stacked
    QCOMPARE(layoutRectGap(a, LayoutRect{12.0, 9.0, 10.0, 5.0}), 4.0);  // diagonal: the larger distance
    QCOMPARE(layoutRectGap(a, LayoutRect{10.0, 0.0, 10.0, 5.0}), 0.0);  // touching
    QCOMPARE(layoutRectGap(a, LayoutRect{8.0, 1.0, 10.0, 5.0}), -2.0);  // overlapping
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(layoutRectGap(a, LayoutRect{nan, 0.0, 10.0, 5.0}), -std::numeric_limits<double>::infinity());
}

void tst_RadialGaugeLayout::scaleLabelsKeepMinimumGap_data()
{
    QTest::addColumn<double>("side");
    QTest::addColumn<double>("minimum");
    QTest::addColumn<double>("maximum");

    // The configurations measured in #50, default 270-degree arc
    struct Range { const char *name; double min; double max; };
    const Range ranges[] = {{"0-8000", 0.0, 8000.0}, {"0-100000", 0.0, 100000.0}, {"0-10000000", 0.0, 10000000.0},
                            {"0-100", 0.0, 100.0}, {"-1000-1000", -1000.0, 1000.0}};
    for (double side : {160.0, 200.0, 260.0, 300.0, 400.0}) {
        for (const Range &r : ranges) {
            QTest::addRow("%.0f px, %s", side, r.name) << side << r.min << r.max;
        }
    }
}

// #50: the drawn labels keep scaleLabelMinGap() pairwise, the end labels are drawn, and the step is
// the smallest divisor of majorTicks that achieves it
void tst_RadialGaugeLayout::scaleLabelsKeepMinimumGap()
{
    QFETCH(double, side);
    QFETCH(double, minimum);
    QFETCH(double, maximum);

    const std::vector<LayoutRect> boxes = arcLabels(side, -135.0, 270.0, rangeCharacters(minimum, maximum));
    const double minGap = scaleLabelMinGap(lineHeight(radialScaleGeometry(side).labelFontPx));
    const RadialScaleLabels labels = layoutRadialScaleLabels(boxes, minGap);

    QVERIFY(labels.step >= 1 && 10 % labels.step == 0);
    QCOMPARE(labels.drawn, multiplesOf(labels.step, 10, true));
    QVERIFY2(allPairsKeepGap(boxes, labels.drawn, minGap), qPrintable(QStringLiteral("step %1").arg(labels.step)));
    for (int d = 1; d < labels.step; ++d) {
        if (10 % d == 0) {
            QVERIFY2(!allPairsKeepGap(boxes, multiplesOf(d, 10, true), minGap),
                     qPrintable(QStringLiteral("step %1 chosen, but %2 also keeps the gap").arg(labels.step).arg(d)));
        }
    }
}

// #50: when no smaller divisor fits, only the first and last labels are drawn
void tst_RadialGaugeLayout::primeMajorTicksKeepOnlyEndLabels()
{
    const std::vector<LayoutRect> boxes = arcLabels(200.0, -135.0, 270.0, std::vector<int>(8, 12));
    const RadialScaleLabels labels = layoutRadialScaleLabels(boxes, 4.0);
    QCOMPARE(labels.step, 7);
    QCOMPARE(labels.drawn, (std::vector<int>{0, 7}));

    // Narrow labels: every label
    const RadialScaleLabels narrow = layoutRadialScaleLabels(arcLabels(400.0, -135.0, 270.0, std::vector<int>(8, 1)), 4.0);
    QCOMPARE(narrow.step, 1);
    QCOMPARE(narrow.drawn, multiplesOf(1, 7, true));
}

// #50: on a 360-degree span the last label lies on the first one: it is not drawn and the others
// are chosen in the same way
void tst_RadialGaugeLayout::fullCircleDropsLastLabel()
{
    const RadialScaleLabels narrow = layoutRadialScaleLabels(arcLabels(400.0, 0.0, 360.0, std::vector<int>(11, 1)), 4.0);
    QCOMPARE(narrow.step, 1);
    QCOMPARE(narrow.drawn, multiplesOf(1, 10, false));

    const std::vector<LayoutRect> wideBoxes = arcLabels(300.0, 0.0, 360.0, rangeCharacters(0.0, 10000000.0));
    const double minGap = scaleLabelMinGap(lineHeight(radialScaleGeometry(300.0).labelFontPx));
    const RadialScaleLabels wide = layoutRadialScaleLabels(wideBoxes, minGap);
    QVERIFY(wide.step > 1);
    QCOMPARE(wide.drawn, multiplesOf(wide.step, 10, false));
    QVERIFY(allPairsKeepGap(wideBoxes, wide.drawn, minGap));
}

// #50: every pair of drawn labels is checked, not only neighbouring indices: on a 720-degree span
// labels 0, 2 and 4 of 4 major ticks share a place, so only label 0 remains
void tst_RadialGaugeLayout::fullCircleChecksEveryPair()
{
    const RadialScaleLabels labels = layoutRadialScaleLabels(arcLabels(400.0, 0.0, 720.0, std::vector<int>(5, 1)), 4.0);
    QCOMPARE(labels.step, 4);
    QCOMPARE(labels.drawn, (std::vector<int>{0}));
}

void tst_RadialGaugeLayout::degenerateLabelInput()
{
    const RadialScaleLabels none = layoutRadialScaleLabels({}, 4.0);
    QCOMPARE(none.step, 0);
    QVERIFY(none.drawn.empty());

    const RadialScaleLabels single = layoutRadialScaleLabels({LayoutRect{0.0, 0.0, 10.0, 5.0}}, 4.0);
    QCOMPARE(single.step, 1);
    QCOMPARE(single.drawn, (std::vector<int>{0}));

    // Non-finite minimum gap: treated as 0, so touching labels are all drawn
    const std::vector<LayoutRect> touching{{0.0, 0.0, 10.0, 5.0}, {10.0, 0.0, 10.0, 5.0}, {20.0, 0.0, 10.0, 5.0}};
    const RadialScaleLabels nanGap = layoutRadialScaleLabels(touching, std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(nanGap.drawn, (std::vector<int>{0, 1, 2}));

    // A non-finite box clashes with every other one: only the end labels remain
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<LayoutRect> broken{{0.0, 0.0, 10.0, 5.0}, {inf, 0.0, 10.0, 5.0}, {40.0, 0.0, 10.0, 5.0}};
    QCOMPARE(layoutRadialScaleLabels(broken, 4.0).drawn, (std::vector<int>{0, 2}));
}

QTEST_MAIN(tst_RadialGaugeLayout)
#include "tst_RadialGaugeLayout.moc"
