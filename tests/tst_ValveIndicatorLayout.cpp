// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #153: geometry of ValveIndicator, without rendering (src/internal/ValveIndicatorLayout.*).

#include <QtTest/QtTest>

#include "internal/ValveIndicatorLayout.h"

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets::internal;

Q_DECLARE_METATYPE(QtIndustrialWidgets::internal::ValveSymbol)

class tst_ValveIndicatorLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bodyAtItsPoints();
    void marksAtTheirPoints();
    void extentHoldsTheDrawing_data();
    void extentHoldsTheDrawing();
    void verticalIsTurnedClockwise();
    void staysInsideFromTheMinimumUp_data();
    void staysInsideFromTheMinimumUp();
    void minimumIsTight_data();
    void minimumIsTight();
    void sizeAtScaleGivesThatScale_data();
    void sizeAtScaleGivesThatScale();
    void nonFiniteInputGivesFiniteGeometry();
};

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

ValveLayoutInput input(ValveSymbol symbol, bool vertical, bool stubs, bool opening, double w = 0.0, double h = 0.0)
{
    ValveLayoutInput in;
    in.width = w;
    in.height = h;
    in.symbol = symbol;
    in.vertical = vertical;
    in.pipeStubs = stubs;
    in.openingShown = opening;
    in.text.tagHeight = 14.0;
    in.text.tagWidth = 38.0;
    in.text.podHeight = 21.0;
    in.text.podMinWidth = 30.0;
    return in;
}

// Every symbol, orientation, stubs and opening
void addConfigurations()
{
    QTest::addColumn<ValveSymbol>("symbol");
    QTest::addColumn<bool>("vertical");
    QTest::addColumn<bool>("stubs");
    QTest::addColumn<bool>("opening");
    const std::pair<const char *, ValveSymbol> symbols[] = {{"gate", ValveSymbol::Gate},
                                                            {"ball", ValveSymbol::Ball},
                                                            {"butterfly", ValveSymbol::Butterfly},
                                                            {"control", ValveSymbol::Control}};
    for (const auto &s : symbols) {
        for (bool vertical : {false, true}) {
            for (bool stubs : {false, true}) {
                for (bool opening : {false, true}) {
                    QTest::addRow("%s%s%s%s", s.first, vertical ? ", vertical" : "", stubs ? ", stubs" : "",
                                  opening ? ", opening" : "")
                        << s.second << vertical << stubs << opening;
                }
            }
        }
    }
}

// The whole drawing in unit coordinates, horizontal: body, marks and stubs
QRectF drawingBounds(ValveSymbol symbol, bool stubs)
{
    QPainterPath all = valveBodyPath(symbol);
    all.addPath(valveMarkPath(symbol));
    if (stubs) {
        all.addPath(valveStubsPath());
    }
    return all.boundingRect();
}

bool finite(const QRectF &r)
{
    return std::isfinite(r.x()) && std::isfinite(r.y()) && std::isfinite(r.width()) && std::isfinite(r.height())
        && r.width() >= 0.0 && r.height() >= 0.0;
}

// \a outer contains \a inner, up to the rounding of the arithmetic (1e-9 px)
bool holds(const QRectF &outer, const QRectF &inner)
{
    constexpr double e = 1e-9;
    return inner.left() >= outer.left() - e && inner.top() >= outer.top() - e && inner.right() <= outer.right() + e
        && inner.bottom() <= outer.bottom() + e;
}

bool sameRect(const QRectF &a, const QRectF &b)
{
    return std::abs(a.left() - b.left()) < 1e-9 && std::abs(a.top() - b.top()) < 1e-9
        && std::abs(a.right() - b.right()) < 1e-9 && std::abs(a.bottom() - b.bottom()) < 1e-9;
}

QString str(const QRectF &r)
{
    return QStringLiteral("(%1, %2) - (%3, %4)").arg(r.left()).arg(r.top()).arg(r.right()).arg(r.bottom());
}

} // namespace

// Two triangles tip to tip between x -1 and 1, half height 0.6 at the ends; the ball adds a circle of radius 0.3
void tst_ValveIndicatorLayout::bodyAtItsPoints()
{
    const QPainterPath gate = valveBodyPath(ValveSymbol::Gate);
    const QPainterPath ball = valveBodyPath(ValveSymbol::Ball);
    QCOMPARE(gate.boundingRect(), QRectF(QPointF(-1.0, -0.6), QPointF(1.0, 0.6)));
    for (const QPainterPath &body : {gate, ball}) {
        // Inside each triangle, near its base and near the tip
        for (double x : {-0.95, -0.5, -0.1, 0.1, 0.5, 0.95}) {
            QVERIFY2(body.contains(QPointF(x, 0.0)), qPrintable(QStringLiteral("x %1 not in the body").arg(x)));
            // Half height 0.6 · |x|: just inside and just outside it
            QVERIFY(body.contains(QPointF(x, 0.6 * std::abs(x) - 0.02)));
            QVERIFY(!body.contains(QPointF(x, 0.6 * std::abs(x) + 0.02)) || (body == ball && std::abs(x) < 0.3));
        }
        QVERIFY(!body.contains(QPointF(-1.05, 0.0)) && !body.contains(QPointF(1.05, 0.0)));
    }
    // Above the tips: empty for the gate, the ball for the ball valve
    for (const QPointF &p : {QPointF(0.0, 0.2), QPointF(0.0, -0.2), QPointF(0.2, 0.15)}) {
        QVERIFY(!gate.contains(p));
        QVERIFY(ball.contains(p));
    }
    QVERIFY(!ball.contains(QPointF(0.0, 0.32)));
    // Butterfly and control have the gate body
    QCOMPARE(valveBodyPath(ValveSymbol::Butterfly).boundingRect(), gate.boundingRect());
    QCOMPARE(valveBodyPath(ValveSymbol::Control).boundingRect(), gate.boundingRect());
    // The upstream half: x < 0 only
    for (ValveSymbol s : {ValveSymbol::Gate, ValveSymbol::Ball}) {
        const QPainterPath half = valveUpstreamHalf(s);
        QVERIFY(half.contains(QPointF(-0.5, 0.0)));
        QVERIFY(!half.contains(QPointF(0.5, 0.0)));
        QVERIFY(half.boundingRect().right() <= 1e-9);
        QCOMPARE(half.boundingRect().left(), -1.0);
    }
    QVERIFY(valveUpstreamHalf(ValveSymbol::Ball).contains(QPointF(-0.1, 0.2)));
}

void tst_ValveIndicatorLayout::marksAtTheirPoints()
{
    QVERIFY(valveMarkPath(ValveSymbol::Gate).isEmpty());
    QVERIFY(valveMarkPath(ValveSymbol::Ball).isEmpty());
    // Butterfly: a vertical line through the centre, from y -0.8 to 0.8
    const QPainterPath disc = valveMarkPath(ValveSymbol::Butterfly);
    QCOMPARE(disc.elementCount(), 2);
    QCOMPARE(QPointF(disc.elementAt(0)), QPointF(0.0, -0.8));
    QCOMPARE(QPointF(disc.elementAt(1)), QPointF(0.0, 0.8));
    // Control: the stem from the centre up to y -1.1, and a dome of radius 0.6 on it, top at y -1.7
    const QPainterPath control = valveMarkPath(ValveSymbol::Control);
    QCOMPARE(QPointF(control.elementAt(0)), QPointF(0.0, 0.0));
    QCOMPARE(QPointF(control.elementAt(1)), QPointF(0.0, -1.1));
    const QRectF dome = control.boundingRect();
    QVERIFY2(sameRect(dome, QRectF(QPointF(-0.6, -1.7), QPointF(0.6, 0.0))), qPrintable(str(dome)));
    // The dome is closed by its flat base: the point just above the base, inside the half circle, is filled
    QPainterPath domeOnly;
    domeOnly.addPath(control);
    QVERIFY(domeOnly.contains(QPointF(0.0, -1.2)));
    QVERIFY(domeOnly.contains(QPointF(0.4, -1.15)));
    QVERIFY(!domeOnly.contains(QPointF(0.5, -1.6)));
    // Stubs: from ±1 out to ±1.5 on the axis
    const QPainterPath stubs = valveStubsPath();
    QCOMPARE(stubs.boundingRect(), QRectF(QPointF(-1.5, 0.0), QPointF(1.5, 0.0)));
    QCOMPARE(QPointF(stubs.elementAt(1)), QPointF(-1.0, 0.0));
    QCOMPARE(QPointF(stubs.elementAt(2)), QPointF(1.0, 0.0));
}

void tst_ValveIndicatorLayout::extentHoldsTheDrawing_data()
{
    addConfigurations();
}

// The extent is the bounding box of what is drawn (no more, no less), and the transform maps it onto the symbol rect
void tst_ValveIndicatorLayout::extentHoldsTheDrawing()
{
    QFETCH(ValveSymbol, symbol);
    QFETCH(bool, vertical);
    QFETCH(bool, stubs);
    QFETCH(bool, opening);
    const QRectF horizontal = drawingBounds(symbol, stubs);
    QVERIFY2(sameRect(valveSymbolExtent(symbol, false, stubs), horizontal),
             qPrintable(str(valveSymbolExtent(symbol, false, stubs)) + QStringLiteral(" vs ") + str(horizontal)));
    const QRectF ext = valveSymbolExtent(symbol, vertical, stubs);
    QCOMPARE(ext.width(), vertical ? horizontal.height() : horizontal.width());
    QCOMPARE(ext.height(), vertical ? horizontal.width() : horizontal.height());
    for (QSizeF size : {QSizeF(300.0, 300.0), QSizeF(120.0, 400.0), QSizeF(500.0, 150.0)}) {
        const ValveLayout l = layoutValve(input(symbol, vertical, stubs, opening, size.width(), size.height()));
        const QRectF mapped = l.transform.mapRect(horizontal);
        QVERIFY2(std::abs(mapped.left() - l.symbol.left()) < 1e-6 && std::abs(mapped.top() - l.symbol.top()) < 1e-6
                     && std::abs(mapped.right() - l.symbol.right()) < 1e-6
                     && std::abs(mapped.bottom() - l.symbol.bottom()) < 1e-6,
                 qPrintable(str(mapped) + QStringLiteral(" vs ") + str(l.symbol)));
        // The proportions kept
        QVERIFY(std::abs(l.symbol.width() / l.symbol.height() - ext.width() / ext.height()) < 1e-9);
        QVERIFY(std::abs(l.symbol.width() - ext.width() * l.scale) < 1e-9);
    }
}

// Vertical: the flow from the top down, the actuator on the right
void tst_ValveIndicatorLayout::verticalIsTurnedClockwise()
{
    for (ValveSymbol s : {ValveSymbol::Gate, ValveSymbol::Control}) {
        const ValveLayout h = layoutValve(input(s, false, true, false, 300.0, 300.0));
        const ValveLayout v = layoutValve(input(s, true, true, false, 300.0, 300.0));
        const QPointF centreH = h.transform.map(QPointF(0.0, 0.0));
        const QPointF centreV = v.transform.map(QPointF(0.0, 0.0));
        // Upstream (x -1): left of a horizontal valve, above a vertical one, on the same line as the centre
        const QPointF upH = h.transform.map(QPointF(-1.0, 0.0));
        const QPointF upV = v.transform.map(QPointF(-1.0, 0.0));
        QVERIFY(upH.x() < centreH.x() && std::abs(upH.y() - centreH.y()) < 1e-9);
        QVERIFY(upV.y() < centreV.y() && std::abs(upV.x() - centreV.x()) < 1e-9);
        // The top of the symbol (the actuator, y < 0): above a horizontal valve, right of a vertical one
        const QPointF topH = h.transform.map(QPointF(0.0, -1.0));
        const QPointF topV = v.transform.map(QPointF(0.0, -1.0));
        QVERIFY(topH.y() < centreH.y() && std::abs(topH.x() - centreH.x()) < 1e-9);
        QVERIFY(topV.x() > centreV.x() && std::abs(topV.y() - centreV.y()) < 1e-9);
    }
}

void tst_ValveIndicatorLayout::staysInsideFromTheMinimumUp_data()
{
    addConfigurations();
}

// From the minimum up: the symbol (with its pad) and the rows inside the margins, from the top in order and without
// overlap, the symbol at least at the minimum scale and centred across, the rows as wide as their texts need
void tst_ValveIndicatorLayout::staysInsideFromTheMinimumUp()
{
    QFETCH(ValveSymbol, symbol);
    QFETCH(bool, vertical);
    QFETCH(bool, stubs);
    QFETCH(bool, opening);
    const ValveLayoutInput base = input(symbol, vertical, stubs, opening);
    const QSizeF min = minimumValveSize(base);
    for (double dw : {0.0, 0.5, 7.0, 60.0, 400.0}) {
        for (double dh : {0.0, 0.5, 9.0, 80.0, 400.0}) {
            ValveLayoutInput in = base;
            in.width = min.width() + dw;
            in.height = min.height() + dh;
            const ValveLayout l = layoutValve(in);
            const QString where = QStringLiteral("%1 x %2").arg(in.width).arg(in.height);
            const QRectF inner(kValveMargin, kValveMargin, in.width - 2.0 * kValveMargin, in.height - 2.0 * kValveMargin);
            const QRectF padded = l.symbol.adjusted(-kValvePad, -kValvePad, kValvePad, kValvePad);
            QVERIFY2(l.scale >= kValveMinimumScale - 1e-9, qPrintable(where + QStringLiteral(": scale %1").arg(l.scale)));
            QVERIFY2(holds(inner, padded), qPrintable(where + QStringLiteral(": symbol ") + str(padded)));
            QVERIFY2(std::abs(l.symbol.center().x() - in.width * 0.5) < 1e-9, qPrintable(where));
            QVERIFY2(holds(inner, l.tag) && l.tag.top() >= padded.bottom() + kValveTagGap - 1e-9, qPrintable(where));
            QVERIFY(l.tag.width() >= in.text.tagWidth && l.tag.height() == in.text.tagHeight);
            if (opening) {
                QVERIFY2(holds(inner, l.bar) && l.bar.top() >= l.tag.bottom() + kValveBarGap - 1e-9, qPrintable(where));
                QVERIFY2(holds(inner, l.pod) && l.pod.top() >= l.bar.bottom() + kValvePodGap - 1e-9, qPrintable(where));
                QCOMPARE(l.bar.height(), kValveBarHeight);
                QCOMPARE(l.pod.height(), in.text.podHeight);
                QVERIFY(l.pod.width() >= in.text.podMinWidth);
            } else {
                QVERIFY(l.bar.isEmpty() && l.pod.isEmpty());
            }
        }
    }
}

void tst_ValveIndicatorLayout::minimumIsTight_data()
{
    addConfigurations();
}

// One pixel less in either direction and something does not fit: the symbol falls below the minimum scale, or a row
// gets narrower than its text
void tst_ValveIndicatorLayout::minimumIsTight()
{
    QFETCH(ValveSymbol, symbol);
    QFETCH(bool, vertical);
    QFETCH(bool, stubs);
    QFETCH(bool, opening);
    for (bool wideText : {false, true}) {
        ValveLayoutInput base = input(symbol, vertical, stubs, opening);
        if (wideText) { // rows wider than the symbol: the width comes from the texts
            base.text.tagWidth = 400.0;
        }
        const QSizeF min = minimumValveSize(base);
        QCOMPARE(min.width(), std::ceil(min.width()));
        QCOMPARE(min.height(), std::ceil(min.height()));
        for (QSizeF less : {QSizeF(1.0, 0.0), QSizeF(0.0, 1.0)}) {
            ValveLayoutInput in = base;
            in.width = min.width() - less.width();
            in.height = min.height() - less.height();
            const ValveLayout l = layoutValve(in);
            const bool small = l.scale < kValveMinimumScale;
            const bool narrow = l.tag.width() < in.text.tagWidth || (opening && l.pod.width() < in.text.podMinWidth);
            QVERIFY2(small || narrow, qPrintable(QStringLiteral("%1 x %2 (minimum %3 x %4): scale %5, tag %6 wide")
                                                     .arg(in.width).arg(in.height).arg(min.width()).arg(min.height())
                                                     .arg(l.scale).arg(l.tag.width())));
        }
    }
}

void tst_ValveIndicatorLayout::sizeAtScaleGivesThatScale_data()
{
    addConfigurations();
}

void tst_ValveIndicatorLayout::sizeAtScaleGivesThatScale()
{
    QFETCH(ValveSymbol, symbol);
    QFETCH(bool, vertical);
    QFETCH(bool, stubs);
    QFETCH(bool, opening);
    ValveLayoutInput in = input(symbol, vertical, stubs, opening);
    for (double s : {kValveMinimumScale, 23.7, kValvePreferredScale, 80.0}) {
        const QSizeF size = valveSizeAtScale(in, s);
        in.width = size.width();
        in.height = size.height();
        const ValveLayout l = layoutValve(in);
        // At least that scale, and less than one pixel more of symbol (the size is rounded up to whole pixels)
        QVERIFY2(l.scale >= s - 1e-9, qPrintable(QStringLiteral("scale %1 for %2").arg(l.scale).arg(s)));
        const QRectF ext = valveSymbolExtent(symbol, vertical, stubs);
        QVERIFY2(std::min(ext.width() * (l.scale - s), ext.height() * (l.scale - s)) < 1.0 + 1e-9,
                 qPrintable(QStringLiteral("scale %1 for %2").arg(l.scale).arg(s)));
    }
    QVERIFY(valveSizeAtScale(in, kValvePreferredScale).width() > minimumValveSize(in).width());
    QVERIFY(valveSizeAtScale(in, kValvePreferredScale).height() > minimumValveSize(in).height());
}

void tst_ValveIndicatorLayout::nonFiniteInputGivesFiniteGeometry()
{
    for (double bad : {kNaN, kInf, -kInf, -5.0, 0.0}) {
        for (bool opening : {false, true}) {
            ValveLayoutInput in = input(ValveSymbol::Control, false, true, opening, bad, bad);
            in.text.tagHeight = bad;
            in.text.tagWidth = bad;
            in.text.podHeight = bad;
            in.text.podMinWidth = bad;
            const ValveLayout l = layoutValve(in);
            QVERIFY(std::isfinite(l.scale) && l.scale >= 0.0);
            QVERIFY(finite(l.symbol) && finite(l.tag) && finite(l.bar) && finite(l.pod));
            QVERIFY(l.transform.isAffine());
            const QPointF p = l.transform.map(QPointF(1.0, 1.0));
            QVERIFY(std::isfinite(p.x()) && std::isfinite(p.y()));
            const QSizeF min = minimumValveSize(in);
            QVERIFY(std::isfinite(min.width()) && std::isfinite(min.height()) && min.width() > 0.0 && min.height() > 0.0);
            const QSizeF at = valveSizeAtScale(in, bad);
            QVERIFY(std::isfinite(at.width()) && std::isfinite(at.height()) && at.width() >= 0.0 && at.height() >= 0.0);
        }
    }
    // Real widths with a bad height and the other way round
    for (QSizeF size : {QSizeF(200.0, kNaN), QSizeF(kNaN, 200.0), QSizeF(kInf, 200.0), QSizeF(200.0, kInf)}) {
        const ValveLayout l = layoutValve(input(ValveSymbol::Gate, true, false, true, size.width(), size.height()));
        QVERIFY(std::isfinite(l.scale) && finite(l.symbol) && finite(l.tag) && finite(l.bar) && finite(l.pod));
    }
}

QTEST_MAIN(tst_ValveIndicatorLayout)
#include "tst_ValveIndicatorLayout.moc"
