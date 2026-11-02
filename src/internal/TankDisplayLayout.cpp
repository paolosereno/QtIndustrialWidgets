// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/TankDisplayLayout.h"

#include <QtGui/QColor>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

namespace {

double finiteOr0(double v)
{
    return std::isfinite(v) && v > 0.0 ? v : 0.0;
}

// Width of the columns left and right of the vessel
double leftColumn(const TankLayoutInput &in)
{
    return in.markersVisible ? finiteOr0(in.text.tagWidth) + kTankTagGap + kTankMarkerSize + kTankMarkerGap : 0.0;
}

double rightColumn(const TankLayoutInput &in)
{
    return in.scaleVisible ? kTankTickGap + kTankMajorTick + kTankLabelGap + finiteOr0(in.text.labelWidth) : 0.0;
}

// Height above the room of the top label
double topBand(const TankLayoutInput &in)
{
    return kTankMargin + (in.displayVisible ? finiteOr0(in.text.podHeight) + kTankPodGap : 0.0);
}

QSizeF vesselSize(TankOutline outline, double aw, double ah)
{
    aw = std::max(0.0, aw);
    ah = std::max(0.0, ah);
    switch (outline) {
    case TankOutline::HorizontalCylinder: {
        const double h = std::min(ah, kTankLyingRatio * aw);
        return QSizeF(std::min(aw, h / kTankLyingRatio), h);
    }
    case TankOutline::Sphere: {
        const double d = std::min(aw, ah);
        return QSizeF(d, d);
    }
    case TankOutline::VerticalCylinder:
    case TankOutline::Hopper:
        break;
    }
    return QSizeF(std::min(aw, kTankUprightRatio * ah), ah);
}

// WCAG 2.x relative luminance
double channelLuminance(int c8)
{
    const double c = c8 / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double luminance(const QColor &c)
{
    return 0.2126 * channelLuminance(c.red()) + 0.7152 * channelLuminance(c.green()) + 0.0722 * channelLuminance(c.blue());
}

} // namespace

double tankContrastRatio(const QColor &a, const QColor &b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor tankHatchColor(const QColor &inside)
{
    const QColor white(255, 255, 255);
    const QColor black(0, 0, 0);
    const QColor target = tankContrastRatio(inside, white) >= tankContrastRatio(inside, black) ? white : black;
    QColor mixed = target;
    for (int step = 1; step <= 20; ++step) {
        const double t = step / 20.0;
        const auto mix = [t](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * t)); };
        mixed = QColor(mix(inside.red(), target.red()), mix(inside.green(), target.green()), mix(inside.blue(), target.blue()));
        if (tankContrastRatio(mixed, inside) >= 4.5) {
            break;
        }
    }
    return mixed;
}

double tankEndRoom(const TankLayoutInput &in)
{
    const double halfLabel = finiteOr0(in.text.labelHeight) * 0.5;
    return in.markersVisible ? std::max(halfLabel, kTankMarkerSize * 0.5 + kTankBeyondGap + kTankBeyondSign) : halfLabel;
}

TankLayout layoutTank(const TankLayoutInput &in)
{
    const double w = finiteOr0(in.width);
    const double h = finiteOr0(in.height);
    const double room = tankEndRoom(in);
    TankLayout out;
    if (in.displayVisible) {
        out.pod = QRectF(kTankMargin, kTankMargin, std::max(0.0, w - 2.0 * kTankMargin), finiteOr0(in.text.podHeight));
    }
    const double top = topBand(in) + room;
    const double bottom = std::max(top, h - kTankMargin - room);
    const double left = leftColumn(in);
    const double right = rightColumn(in);
    const QSizeF v = vesselSize(in.outline, w - 2.0 * kTankMargin - left - right, bottom - top);
    const double x0 = std::max(kTankMargin, (w - (left + v.width() + right)) * 0.5);
    out.vessel = QRectF(x0 + left, top + (bottom - top - v.height()) * 0.5, v.width(), v.height());
    out.tickX = out.vessel.right() + kTankTickGap;
    out.labelX = out.tickX + kTankMajorTick + kTankLabelGap;
    out.markerTipX = out.vessel.left() - kTankMarkerGap;
    out.tagRight = out.markerTipX - kTankMarkerSize - kTankTagGap;
    return out;
}

QSizeF minimumTankSize(const TankLayoutInput &in)
{
    const double labelHeight = finiteOr0(in.text.labelHeight);
    // Two end labels and the gap of the scale (scaleLabelMinGap()) between them
    const double vesselHeight = std::max(kTankMinimumVesselHeight, 2.0 * labelHeight + std::max(4.0, 0.5 * labelHeight));
    const double vesselWidth = kTankMinimumVesselWidth;
    double aw = vesselWidth;
    double ah = vesselHeight;
    switch (in.outline) {
    case TankOutline::HorizontalCylinder:
        aw = std::max(vesselWidth, vesselHeight / kTankLyingRatio);
        break;
    case TankOutline::Sphere:
        aw = std::max(vesselWidth, vesselHeight);
        break;
    case TankOutline::VerticalCylinder:
    case TankOutline::Hopper:
        ah = std::max(vesselHeight, vesselWidth / kTankUprightRatio);
        break;
    }
    const double width = std::max(2.0 * kTankMargin + leftColumn(in) + aw + rightColumn(in),
                                  in.displayVisible ? 2.0 * kTankMargin + finiteOr0(in.text.podMinWidth) : 0.0);
    const double height = topBand(in) + 2.0 * tankEndRoom(in) + ah + kTankMargin;
    return QSizeF(std::ceil(width), std::ceil(height));
}

double tankLevelY(const QRectF &vessel, double fraction)
{
    const double f = std::isfinite(fraction) ? std::clamp(fraction, 0.0, 1.0) : 0.0;
    return vessel.bottom() - f * vessel.height();
}

QPainterPath tankOutlinePath(TankOutline outline, const QRectF &r)
{
    QPainterPath path;
    if (!(r.width() > 0.0) || !(r.height() > 0.0)
        || !std::isfinite(r.left()) || !std::isfinite(r.top()) || !std::isfinite(r.right()) || !std::isfinite(r.bottom())) {
        return path;
    }
    switch (outline) {
    case TankOutline::Sphere:
        path.addEllipse(r);
        break;
    case TankOutline::HorizontalCylinder: {
        const double e = std::min(kTankHeadDepth * r.width(), 0.25 * r.height());
        path.moveTo(r.left() + e, r.top());
        path.lineTo(r.right() - e, r.top());
        path.arcTo(QRectF(r.right() - 2.0 * e, r.top(), 2.0 * e, r.height()), 90.0, -180.0);
        path.lineTo(r.left() + e, r.bottom());
        path.arcTo(QRectF(r.left(), r.top(), 2.0 * e, r.height()), 270.0, -180.0);
        path.closeSubpath();
        break;
    }
    case TankOutline::Hopper: {
        const double e = std::min(kTankHeadDepth * r.width(), 0.1 * r.height());
        const double coneY = r.top() + kHopperConeStart * r.height();
        const double outlet = kHopperOutlet * r.width();
        path.moveTo(r.left(), r.top() + e);
        path.arcTo(QRectF(r.left(), r.top(), r.width(), 2.0 * e), 180.0, -180.0);
        path.lineTo(r.right(), coneY);
        path.lineTo(r.center().x() + outlet, r.bottom());
        path.lineTo(r.center().x() - outlet, r.bottom());
        path.lineTo(r.left(), coneY);
        path.closeSubpath();
        break;
    }
    case TankOutline::VerticalCylinder: {
        const double e = std::min(kTankHeadDepth * r.width(), 0.25 * r.height());
        path.moveTo(r.left(), r.top() + e);
        path.arcTo(QRectF(r.left(), r.top(), r.width(), 2.0 * e), 180.0, -180.0);
        path.lineTo(r.right(), r.bottom() - e);
        path.arcTo(QRectF(r.left(), r.bottom() - 2.0 * e, r.width(), 2.0 * e), 0.0, -180.0);
        path.closeSubpath();
        break;
    }
    }
    return path;
}

} // namespace internal
} // namespace QtIndustrialWidgets
