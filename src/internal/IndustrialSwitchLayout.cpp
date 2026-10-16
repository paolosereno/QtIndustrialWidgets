// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "IndustrialSwitchLayout.h"

#include <QtGui/QFontMetricsF>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

QFont switchTitleFont(const QFont &base)
{
    QFont f = base;
    f.setBold(true);
    f.setPointSizeF(std::max(7.0, base.pointSizeF() * 0.85));
    return f;
}

QFont switchLabelFont(const QFont &base)
{
    QFont f = base;
    f.setBold(true);
    f.setPointSizeF(std::max(6.5, base.pointSizeF() * 0.8));
    return f;
}

double switchScrewRadius(const QSizeF &size)
{
    return std::clamp(std::min(size.width(), size.height()) * 0.04, 3.0, 5.0);
}

namespace {

constexpr double kOpenGuardMinThickness = 3.0;

// Box of a single-line text drawn in rect with the given alignment
QRectF textBox(const QRectF &rect, Qt::Alignment align, const QString &text, const QFont &font)
{
    if (text.isEmpty()) {
        return {};
    }
    const QFontMetricsF fm(font);
    const double w = fm.horizontalAdvance(text);
    const double h = fm.height();
    double x = rect.left();
    if (align & Qt::AlignHCenter) {
        x = rect.center().x() - w * 0.5;
    } else if (align & Qt::AlignRight) {
        x = rect.right() - w;
    }
    return {x, rect.center().y() - h * 0.5, w, h};
}

bool fitsHorizontally(const QRectF &box, const QRectF &rect)
{
    return box.isEmpty() || (box.left() >= rect.left() - 0.01 && box.right() <= rect.right() + 0.01);
}

} // namespace

SwitchLayout layoutSwitch(const SwitchLayoutInput &in)
{
    const QFont labelFont = switchLabelFont(in.font);
    const QFont titleFont = switchTitleFont(in.font);
    const bool vertical = in.orientation == Qt::Vertical;
    const QRectF all(QPointF(0, 0), in.size);

    // Natural switch rectangle (as before #44)
    const QRectF r = all.adjusted(10, 10, -10, -10);
    const double w = r.width();
    const double h = r.height();
    QRectF natural;
    if (vertical) {
        const double sh = h * 0.55;
        const double sw = std::min(w * 0.7, sh * 0.65);
        natural = QRectF(r.left() + (w - sw) * 0.5, r.top() + (h - sh) * 0.52, sw, sh);
    } else {
        const double sw = w * 0.55;
        const double sh = std::min(h * 0.7, sw * 0.65);
        natural = QRectF(r.left() + (w - sw) * 0.52, r.top() + (h - sh) * 0.5, sw, sh);
    }

    SwitchLayout L;
    L.plate = all.adjusted(3, 3, -3, -3);
    const QRectF &P = L.plate;
    L.titleRect = vertical ? QRectF(P.left(), P.top() + 6, P.width(), 16) : QRectF(P.left() + 6, P.top() + 6, P.width() - 12, 14);
    L.titleBox = textBox(L.titleRect, Qt::AlignCenter, in.title, titleFont);
    const double titleBottom = L.titleBox.isEmpty() ? P.top() + 2.0 : L.titleBox.bottom();
    // The title does not depend on the switch size: scaling the switch cannot make it fit. It must
    // stay clear of the corner screws (centred 2.2 radii from the plate corner).
    const double screwClearance = switchScrewRadius(in.size) * 3.2 + 1.0;
    const bool titleFits = L.titleBox.isEmpty()
        || (fitsHorizontally(L.titleBox, L.titleRect) && L.titleBox.left() >= P.left() + screwClearance
            && L.titleBox.right() <= P.right() - screwClearance);

    for (int step = 0; step <= 70; ++step) {
        const double k = 1.0 - step * 0.01;
        const QPointF c = natural.center();
        L.scale = k;
        L.switchRect = QRectF(c.x() - natural.width() * k * 0.5, c.y() - natural.height() * k * 0.5, natural.width() * k, natural.height() * k);
        const QRectF &S = L.switchRect;
        L.guard = in.guard ? S.adjusted(-8, -12, 8, 12) : S;
        // drop shadow (+2, +3), lift tab (4 px below), hinge brackets (2 px beside)
        L.guardOuter = in.guard ? L.guard.adjusted(-4, -2, 4, 6) : S;
        const QRectF &O = L.guardOuter;

        if (vertical) {
            L.onRect = QRectF(P.left(), O.top() - 16, P.width(), 14);
            L.offRect = QRectF(P.left(), O.bottom() + 3, P.width(), 14);
            L.centerRect = QRectF(O.right() + 4, S.center().y() - 7, P.right() - O.right() - 6, 14);
            L.onAlign = L.offAlign = Qt::AlignCenter;
            L.centerAlign = Qt::AlignLeft | Qt::AlignVCenter;
        } else {
            L.offRect = QRectF(P.left() + 4, S.top(), O.left() - P.left() - 6, S.height());
            L.onRect = QRectF(O.right() + 6, S.top(), P.right() - O.right() - 8, S.height());
            const double half = std::min(S.center().x() - P.left(), P.right() - S.center().x());
            L.centerRect = QRectF(S.center().x() - half, O.bottom() + 2, 2 * half, 14);
            L.offAlign = Qt::AlignRight | Qt::AlignVCenter;
            L.onAlign = Qt::AlignLeft | Qt::AlignVCenter;
            L.centerAlign = Qt::AlignCenter;
        }
        L.onBox = textBox(L.onRect, L.onAlign, in.on, labelFont);
        L.offBox = textBox(L.offRect, L.offAlign, in.off, labelFont);
        L.centerBox = in.positions == 3 ? textBox(L.centerRect, L.centerAlign, in.center, labelFont) : QRectF();

        if (vertical) {
            L.led = L.onBox.isEmpty() ? L.onRect.center() : QPointF(L.onBox.left() - 2.0 - kSwitchLedGlow, L.onRect.center().y());
        } else {
            L.led = L.onBox.isEmpty() ? QPointF(L.onRect.left() + kSwitchLedGlow, L.onRect.center().y())
                                      : QPointF(L.onBox.center().x(), L.onBox.top() - 2.0 - kSwitchLedGlow);
        }

        if (vertical) {
            const double space = L.guard.left() - (P.left() + 2.0);
            const double t = std::max(kOpenGuardMinThickness, std::min(0.17 * L.guard.width(), space));
            L.openGuard = QRectF(L.guard.left() - t, L.guard.top(), t, L.guard.height());
        } else {
            double top = L.guard.bottom();
            if (!L.centerBox.isEmpty()) {
                top = std::max(top, L.centerBox.bottom() + 2.0);
            }
            const double space = P.bottom() - 2.0 - top;
            const double t = std::max(kOpenGuardMinThickness, std::min(0.17 * L.guard.height(), space));
            L.openGuard = QRectF(L.guard.left(), top, L.guard.width(), t);
        }

        bool ok = fitsHorizontally(L.onBox, L.onRect) && fitsHorizontally(L.offBox, L.offRect) && fitsHorizontally(L.centerBox, L.centerRect);
        ok = ok && L.led.y() - kSwitchLedGlow >= titleBottom + 1.0;
        ok = ok && L.led.x() - kSwitchLedGlow >= P.left() + 1.0 && L.led.x() + kSwitchLedGlow <= P.right() - 1.0;
        ok = ok && (L.onBox.isEmpty() || L.onBox.top() >= titleBottom + 1.0) && O.top() >= titleBottom + 1.0;
        ok = ok && L.openGuard.left() >= P.left() + 1.0 && L.openGuard.bottom() <= P.bottom() - 1.0;
        ok = ok && (vertical ? L.offRect.bottom() <= P.bottom() - 2.0 : (L.centerBox.isEmpty() || L.centerBox.bottom() <= P.bottom() - 2.0));
        L.fits = ok && titleFits;
        if (ok) {
            break;
        }
    }
    return L;
}

QSize switchSizeHint(SwitchLayoutInput in, const QSize &base)
{
    for (int step = 0; step <= 60; ++step) {
        const double s = 1.0 + step * 0.05;
        const QSize size(static_cast<int>(std::ceil(base.width() * s)), static_cast<int>(std::ceil(base.height() * s)));
        in.size = size;
        const SwitchLayout L = layoutSwitch(in);
        if (L.fits && L.scale >= 1.0) {
            return size;
        }
    }
    return base;
}

} // namespace internal
} // namespace QtIndustrialWidgets
