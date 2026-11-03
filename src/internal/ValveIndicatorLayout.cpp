// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/ValveIndicatorLayout.h"

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

namespace {

double finiteOr0(double v)
{
    return std::isfinite(v) && v > 0.0 ? v : 0.0;
}

// Height below the symbol: the tag row, and the opening bar and panel
double rowsBelow(const ValveLayoutInput &in)
{
    double h = kValveTagGap + finiteOr0(in.text.tagHeight);
    if (in.openingShown) {
        h += kValveBarGap + kValveBarHeight + kValvePodGap + finiteOr0(in.text.podHeight);
    }
    return h;
}

} // namespace

QRectF valveSymbolExtent(ValveSymbol symbol, bool vertical, bool pipeStubs)
{
    const double halfLength = 1.0 + (pipeStubs ? kValveStubLength : 0.0);
    double top = -kValveBodyHalfHeight;
    double bottom = kValveBodyHalfHeight;
    switch (symbol) {
    case ValveSymbol::Butterfly:
        top = -kValveButterflyHalfLength;
        bottom = kValveButterflyHalfLength;
        break;
    case ValveSymbol::Control:
        top = kValveStemTop - kValveActuatorRadius;
        break;
    case ValveSymbol::Gate:
    case ValveSymbol::Ball:
        break;
    }
    const QRectF horizontal(QPointF(-halfLength, top), QPointF(halfLength, bottom));
    if (!vertical) {
        return horizontal;
    }
    // Turned 90° clockwise: (x, y) -> (-y, x)
    return QRectF(QPointF(-horizontal.bottom(), horizontal.left()), QPointF(-horizontal.top(), horizontal.right()));
}

QPainterPath valveBodyPath(ValveSymbol symbol)
{
    const double h = kValveBodyHalfHeight;
    QPainterPath body;
    body.moveTo(-1.0, -h);
    body.lineTo(0.0, 0.0);
    body.lineTo(-1.0, h);
    body.closeSubpath();
    body.moveTo(1.0, -h);
    body.lineTo(0.0, 0.0);
    body.lineTo(1.0, h);
    body.closeSubpath();
    if (symbol == ValveSymbol::Ball) {
        QPainterPath ball;
        ball.addEllipse(QPointF(0.0, 0.0), kValveBallRadius, kValveBallRadius);
        body = body.united(ball);
    }
    return body;
}

QPainterPath valveUpstreamHalf(ValveSymbol symbol)
{
    QPainterPath half;
    half.addRect(QRectF(QPointF(-2.0, -2.0), QPointF(0.0, 2.0)));
    return valveBodyPath(symbol).intersected(half);
}

QPainterPath valveMarkPath(ValveSymbol symbol)
{
    QPainterPath marks;
    switch (symbol) {
    case ValveSymbol::Butterfly:
        marks.moveTo(0.0, -kValveButterflyHalfLength);
        marks.lineTo(0.0, kValveButterflyHalfLength);
        break;
    case ValveSymbol::Control: {
        marks.moveTo(0.0, 0.0);
        marks.lineTo(0.0, kValveStemTop);
        // The dome: a half circle on a flat base, its top at kValveStemTop - kValveActuatorRadius
        const double r = kValveActuatorRadius;
        marks.moveTo(-r, kValveStemTop);
        marks.arcTo(QRectF(-r, kValveStemTop - r, 2.0 * r, 2.0 * r), 180.0, -180.0);
        marks.closeSubpath();
        break;
    }
    case ValveSymbol::Gate:
    case ValveSymbol::Ball:
        break;
    }
    return marks;
}

QPainterPath valveStubsPath()
{
    QPainterPath stubs;
    stubs.moveTo(-1.0 - kValveStubLength, 0.0);
    stubs.lineTo(-1.0, 0.0);
    stubs.moveTo(1.0, 0.0);
    stubs.lineTo(1.0 + kValveStubLength, 0.0);
    return stubs;
}

ValveLayout layoutValve(const ValveLayoutInput &in)
{
    const double w = finiteOr0(in.width);
    const double h = finiteOr0(in.height);
    const QRectF ext = valveSymbolExtent(in.symbol, in.vertical, in.pipeStubs);
    const double below = rowsBelow(in);
    const double aw = std::max(0.0, w - 2.0 * kValveMargin - 2.0 * kValvePad);
    const double ah = std::max(0.0, h - 2.0 * kValveMargin - 2.0 * kValvePad - below);
    const double s = std::max(0.0, std::min(aw / ext.width(), ah / ext.height()));

    ValveLayout out;
    out.scale = s;
    const QSizeF symbolSize(ext.width() * s, ext.height() * s);
    // The column centred vertically, the symbol centred across
    const double column = symbolSize.height() + 2.0 * kValvePad + below;
    const double top = kValveMargin + std::max(0.0, (h - 2.0 * kValveMargin - column) * 0.5);
    const double left = std::max(kValveMargin + kValvePad, (w - symbolSize.width()) * 0.5);
    out.symbol = QRectF(QPointF(left, top + kValvePad), symbolSize);
    out.transform.translate(out.symbol.left() - ext.left() * s, out.symbol.top() - ext.top() * s);
    if (in.vertical) {
        out.transform.rotate(90.0);
    }
    out.transform.scale(s, s);

    const double rowWidth = std::max(0.0, w - 2.0 * kValveMargin);
    out.tag = QRectF(kValveMargin, out.symbol.bottom() + kValvePad + kValveTagGap, rowWidth, finiteOr0(in.text.tagHeight));
    if (in.openingShown) {
        out.bar = QRectF(kValveMargin, out.tag.bottom() + kValveBarGap, rowWidth, kValveBarHeight);
        out.pod = QRectF(kValveMargin, out.bar.bottom() + kValvePodGap, rowWidth, finiteOr0(in.text.podHeight));
    }
    return out;
}

QSizeF minimumValveSize(const ValveLayoutInput &in)
{
    return valveSizeAtScale(in, kValveMinimumScale);
}

QSizeF valveSizeAtScale(const ValveLayoutInput &in, double scale)
{
    const double s = finiteOr0(scale);
    const QRectF ext = valveSymbolExtent(in.symbol, in.vertical, in.pipeStubs);
    const double symbolWidth = ext.width() * s + 2.0 * kValvePad;
    double rowsWidth = finiteOr0(in.text.tagWidth);
    if (in.openingShown) {
        rowsWidth = std::max(rowsWidth, finiteOr0(in.text.podMinWidth));
    }
    const double width = 2.0 * kValveMargin + std::max(symbolWidth, rowsWidth);
    const double height = 2.0 * kValveMargin + ext.height() * s + 2.0 * kValvePad + rowsBelow(in);
    return QSizeF(std::ceil(width), std::ceil(height));
}

} // namespace internal
} // namespace QtIndustrialWidgets
