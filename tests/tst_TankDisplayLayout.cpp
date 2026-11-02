// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #152: geometry of TankDisplay, without rendering (src/internal/TankDisplayLayout.*).

#include <QtTest/QtTest>

#include "internal/TankDisplayLayout.h"

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets::internal;

Q_DECLARE_METATYPE(QtIndustrialWidgets::internal::TankOutline)

class tst_TankDisplayLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void levelRunsFromTheBottomToTheTop();
    void outlinesAtTheirGeometricPoints_data();
    void outlinesAtTheirGeometricPoints();
    void vesselKeepsItsAspectAndStaysInside_data();
    void vesselKeepsItsAspectAndStaysInside();
    void columnsStayInsideFromTheMinimumUp_data();
    void columnsStayInsideFromTheMinimumUp();
    void minimumGivesTheMinimumVessel_data();
    void minimumGivesTheMinimumVessel();
    void nonFiniteInputGivesFiniteGeometry();
};

namespace {

TankLayoutInput input(TankOutline outline, double w, double h, bool markers = true)
{
    TankLayoutInput in;
    in.width = w;
    in.height = h;
    in.outline = outline;
    in.displayVisible = true;
    in.scaleVisible = true;
    in.markersVisible = markers;
    in.text.podHeight = 22.0;
    in.text.podMinWidth = 30.0;
    in.text.labelHeight = 14.0;
    in.text.labelWidth = 26.0;
    in.text.tagWidth = 17.0;
    return in;
}

void addOutlines()
{
    QTest::addColumn<TankOutline>("outline");
    QTest::newRow("vertical cylinder") << TankOutline::VerticalCylinder;
    QTest::newRow("horizontal cylinder") << TankOutline::HorizontalCylinder;
    QTest::newRow("hopper") << TankOutline::Hopper;
    QTest::newRow("sphere") << TankOutline::Sphere;
}

bool finite(const QRectF &r)
{
    return std::isfinite(r.x()) && std::isfinite(r.y()) && std::isfinite(r.width()) && std::isfinite(r.height())
        && r.width() >= 0.0 && r.height() >= 0.0;
}

} // namespace

void tst_TankDisplayLayout::levelRunsFromTheBottomToTheTop()
{
    const QRectF vessel(20.0, 40.0, 60.0, 200.0);
    QCOMPARE(tankLevelY(vessel, 0.0), 240.0);
    QCOMPARE(tankLevelY(vessel, 1.0), 40.0);
    QCOMPARE(tankLevelY(vessel, 0.25), 190.0);
    // Clamped to the vessel, and NaN at the bottom
    QCOMPARE(tankLevelY(vessel, 1.5), 40.0);
    QCOMPARE(tankLevelY(vessel, -0.5), 240.0);
    QCOMPARE(tankLevelY(vessel, std::numeric_limits<double>::quiet_NaN()), 240.0);
}

void tst_TankDisplayLayout::outlinesAtTheirGeometricPoints_data()
{
    addOutlines();
}

// The edge of each outline at a few heights: a point 0.5 px inside it is in the path, one 0.5 px outside is not
void tst_TankDisplayLayout::outlinesAtTheirGeometricPoints()
{
    QFETCH(TankOutline, outline);
    const QRectF r(10.0, 20.0, outline == TankOutline::HorizontalCylinder ? 200.0 : 100.0,
                   outline == TankOutline::HorizontalCylinder ? 100.0 : 200.0);
    const QPainterPath path = tankOutlinePath(outline, r);
    QVERIFY(!path.isEmpty());
    // The outline fills its box: it reaches each side
    QCOMPARE(path.boundingRect(), r);

    const double cx = r.center().x();
    struct Edge {
        double y;
        double halfWidth;
    };
    QVector<Edge> edges;
    switch (outline) {
    case TankOutline::Sphere: {
        const double radius = r.width() * 0.5; // a circle: the box is square in the widget, here only its width counts
        const QPainterPath circle = tankOutlinePath(outline, QRectF(r.x(), r.y(), r.width(), r.width()));
        const double cy = r.y() + radius;
        for (double f : {0.0, 0.3, 0.6, 0.8, -0.6}) {
            const double y = cy + f * radius;
            const double half = radius * std::sqrt(1.0 - f * f);
            QVERIFY2(circle.contains(QPointF(cx + half - 0.5, y)) && !circle.contains(QPointF(cx + half + 0.5, y)),
                     qPrintable(QStringLiteral("sphere at %1 of the radius").arg(f)));
            QVERIFY2(circle.contains(QPointF(cx - half + 0.5, y)) && !circle.contains(QPointF(cx - half - 0.5, y)),
                     qPrintable(QStringLiteral("sphere at %1 of the radius").arg(f)));
        }
        return;
    }
    case TankOutline::Hopper: {
        const double coneY = r.top() + kHopperConeStart * r.height();
        const double outlet = kHopperOutlet * r.width();
        edges << Edge{r.top() + 0.3 * r.height(), r.width() * 0.5} << Edge{coneY - 1.0, r.width() * 0.5};
        for (double f : {0.25, 0.5, 0.75}) {
            // Straight cone from the full width at coneY to the outlet at the bottom
            edges << Edge{coneY + f * (r.bottom() - coneY), r.width() * 0.5 + f * (outlet - r.width() * 0.5)};
        }
        // The outlet: narrower than the vessel, at the bottom
        QVERIFY(path.contains(QPointF(cx + outlet - 0.5, r.bottom() - 0.5)));
        QVERIFY(!path.contains(QPointF(cx + outlet + 1.0, r.bottom() - 0.5)));
        break;
    }
    case TankOutline::VerticalCylinder:
        for (double f : {0.2, 0.5, 0.8}) {
            edges << Edge{r.top() + f * r.height(), r.width() * 0.5};
        }
        break;
    case TankOutline::HorizontalCylinder: {
        // Straight top and bottom between the ends, the ends rounded: narrower near the top and bottom than at mid
        // height
        const double e = std::min(kTankHeadDepth * r.width(), 0.25 * r.height());
        QVERIFY(path.contains(QPointF(cx, r.top() + 0.5)) && !path.contains(QPointF(cx, r.top() - 0.5)));
        QVERIFY(path.contains(QPointF(cx, r.bottom() - 0.5)) && !path.contains(QPointF(cx, r.bottom() + 0.5)));
        edges << Edge{r.center().y(), r.width() * 0.5};
        for (double f : {0.3, 0.8}) {
            // An ellipse of semi-axes e and h / 2 at each end
            const double dy = f * r.height() * 0.5;
            const double dx = e * std::sqrt(1.0 - (dy / (r.height() * 0.5)) * (dy / (r.height() * 0.5)));
            edges << Edge{r.center().y() + dy, r.width() * 0.5 - e + dx};
        }
        break;
    }
    }
    for (const Edge &e : edges) {
        const QString where = QStringLiteral("y %1, half width %2").arg(e.y).arg(e.halfWidth);
        QVERIFY2(path.contains(QPointF(cx + e.halfWidth - 0.5, e.y)), qPrintable(where));
        QVERIFY2(!path.contains(QPointF(cx + e.halfWidth + 0.5, e.y)), qPrintable(where));
        QVERIFY2(path.contains(QPointF(cx - e.halfWidth + 0.5, e.y)), qPrintable(where));
        QVERIFY2(!path.contains(QPointF(cx - e.halfWidth - 0.5, e.y)), qPrintable(where));
    }
}

void tst_TankDisplayLayout::vesselKeepsItsAspectAndStaysInside_data()
{
    addOutlines();
}

void tst_TankDisplayLayout::vesselKeepsItsAspectAndStaysInside()
{
    QFETCH(TankOutline, outline);
    for (const QSizeF size : {QSizeF(160, 260), QSizeF(400, 260), QSizeF(160, 600), QSizeF(260, 200)}) {
        const TankLayoutInput in = input(outline, size.width(), size.height());
        const TankLayout l = layoutTank(in);
        const QString where = QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
        const QRectF v = l.vessel;
        QVERIFY2(v.width() > 0.0 && v.height() > 0.0, qPrintable(where));
        switch (outline) {
        case TankOutline::Sphere:
            QVERIFY2(qFuzzyCompare(v.width(), v.height()), qPrintable(where));
            break;
        case TankOutline::HorizontalCylinder:
            QVERIFY2(v.height() <= kTankLyingRatio * v.width() + 1e-9, qPrintable(where));
            break;
        case TankOutline::VerticalCylinder:
        case TankOutline::Hopper:
            QVERIFY2(v.width() <= kTankUprightRatio * v.height() + 1e-9, qPrintable(where));
            break;
        }
        // Below the readout panel with a gap and half a label line, above the bottom by half a label line
        const double halfLabel = in.text.labelHeight * 0.5;
        QVERIFY2(v.top() >= l.pod.bottom() + kTankPodGap + halfLabel - 1e-9, qPrintable(where));
        QVERIFY2(v.bottom() <= size.height() - kTankMargin - halfLabel + 1e-9, qPrintable(where));
        // The columns around it, in order
        QCOMPARE(l.tickX, v.right() + kTankTickGap);
        QCOMPARE(l.labelX, l.tickX + kTankMajorTick + kTankLabelGap);
        QCOMPARE(l.markerTipX, v.left() - kTankMarkerGap);
        QCOMPARE(l.tagRight, l.markerTipX - kTankMarkerSize - kTankTagGap);
        // The panel across the width
        QCOMPARE(l.pod, QRectF(kTankMargin, kTankMargin, size.width() - 2.0 * kTankMargin, in.text.podHeight));
    }
}

void tst_TankDisplayLayout::columnsStayInsideFromTheMinimumUp_data()
{
    addOutlines();
}

// From minimumTankSize() up, the tags, the labels, the "beyond" signs and the vessel stay inside the widget margin
void tst_TankDisplayLayout::columnsStayInsideFromTheMinimumUp()
{
    QFETCH(TankOutline, outline);
    for (bool markers : {false, true}) {
        const QSizeF min = minimumTankSize(input(outline, 0.0, 0.0, markers));
        for (double dw = 0.0; dw <= 120.0; dw += 7.0) {
            for (double dh = 0.0; dh <= 120.0; dh += 11.0) {
                const TankLayoutInput in = input(outline, min.width() + dw, min.height() + dh, markers);
                const TankLayout l = layoutTank(in);
                const QString where = QStringLiteral("%1x%2, markers %3").arg(in.width).arg(in.height).arg(markers);
                const double left = markers ? l.tagRight - in.text.tagWidth : l.vessel.left();
                QVERIFY2(left >= kTankMargin - 1e-9, qPrintable(where));
                QVERIFY2(l.labelX + in.text.labelWidth <= in.width - kTankMargin + 1e-9, qPrintable(where));
                // Half a label line past each end of the scale for the end labels and tags; with the markers, the
                // room of a marker at an end and of its "beyond" sign (more than half a label line here)
                const double sign = kTankMarkerSize * 0.5 + kTankBeyondGap + kTankBeyondSign;
                const double room = markers ? std::max(in.text.labelHeight * 0.5, sign) : in.text.labelHeight * 0.5;
                QVERIFY2(l.vessel.top() - room >= l.pod.bottom() + kTankPodGap - 1e-9, qPrintable(where));
                QVERIFY2(l.vessel.bottom() + room <= in.height - kTankMargin + 1e-9, qPrintable(where));
                QVERIFY2(l.vessel.width() >= kTankMinimumVesselWidth - 1e-9, qPrintable(where));
                QVERIFY2(l.vessel.height() >= kTankMinimumVesselHeight - 1e-9, qPrintable(where));
            }
        }
    }
}

void tst_TankDisplayLayout::minimumGivesTheMinimumVessel_data()
{
    addOutlines();
}

// At minimumTankSize() the vessel has its minimum width and height, and the panel its minimum width; one pixel
// less in either direction loses one of them (the minimum is not larger than needed)
void tst_TankDisplayLayout::minimumGivesTheMinimumVessel()
{
    QFETCH(TankOutline, outline);
    for (bool markers : {false, true}) {
        const TankLayoutInput zero = input(outline, 0.0, 0.0, markers);
        const QSizeF min = minimumTankSize(zero);
        const auto ok = [&](double w, double h) {
            const TankLayoutInput in = input(outline, w, h, markers);
            const TankLayout l = layoutTank(in);
            const double needHeight = std::max(kTankMinimumVesselHeight, 2.0 * in.text.labelHeight + std::max(4.0, 0.5 * in.text.labelHeight));
            return l.vessel.width() >= kTankMinimumVesselWidth - 1e-9 && l.vessel.height() >= needHeight - 1e-9
                && l.pod.width() >= in.text.podMinWidth - 1e-9;
        };
        const QString where = QStringLiteral("minimum %1x%2, markers %3").arg(min.width()).arg(min.height()).arg(markers);
        QVERIFY2(ok(min.width(), min.height()), qPrintable(where));
        QVERIFY2(!ok(min.width() - 1.0, min.height()), qPrintable(where + QStringLiteral(": one pixel narrower is enough")));
        QVERIFY2(!ok(min.width(), min.height() - 1.0), qPrintable(where + QStringLiteral(": one pixel lower is enough")));
    }
    // The tag column widens the minimum
    QVERIFY(minimumTankSize(input(outline, 0.0, 0.0, true)).width() > minimumTankSize(input(outline, 0.0, 0.0, false)).width());
}

void tst_TankDisplayLayout::nonFiniteInputGivesFiniteGeometry()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (TankOutline outline : {TankOutline::VerticalCylinder, TankOutline::HorizontalCylinder, TankOutline::Hopper,
                                TankOutline::Sphere}) {
        for (double bad : {nan, inf, -inf, -50.0}) {
            TankLayoutInput in = input(outline, bad, bad);
            in.text.labelHeight = bad;
            in.text.labelWidth = bad;
            in.text.podHeight = bad;
            const TankLayout l = layoutTank(in);
            QVERIFY(finite(l.vessel));
            QVERIFY(finite(l.pod));
            QVERIFY(std::isfinite(l.tickX) && std::isfinite(l.labelX) && std::isfinite(l.markerTipX)
                    && std::isfinite(l.tagRight));
            const QSizeF min = minimumTankSize(in);
            QVERIFY(std::isfinite(min.width()) && std::isfinite(min.height()));
            QVERIFY(tankOutlinePath(outline, QRectF(0.0, 0.0, bad, bad)).isEmpty());
            QVERIFY(tankOutlinePath(outline, QRectF(bad, 0.0, 50.0, 80.0)).isEmpty() || std::isfinite(bad));
        }
    }
}

QTEST_MAIN(tst_TankDisplayLayout)
#include "tst_TankDisplayLayout.moc"
