// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/ValueStateDrawing.h"

#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtGui/QPolygonF>

#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

void drawStateReadout(QPainter &painter, const QRectF &podRect, QFont font, const QString &text, InputState state,
                      const QColor &color, int minimumPx)
{
    const bool outOfRange = (state == InputState::Overrange || state == InputState::Underrange);
    const double available =
        podRect.width() - 2.0 * std::max(2.0, std::min(podRect.width(), podRect.height()) * 0.12);
    double triW = 0.0;
    double gap = 0.0;
    bool textFits = false;
    for (int px = font.pixelSize();; --px) {
        font.setPixelSize(px);
        const QFontMetricsF fm(font);
        triW = outOfRange ? fm.capHeight() * 0.9 : 0.0;
        gap = outOfRange ? triW * 0.35 : 0.0;
        textFits = triW + gap + fm.horizontalAdvance(text) <= available;
        if (textFits || px <= (minimumPx > 0 ? minimumPx : 6)) {
            break;
        }
    }
    const bool leaveTextOut = minimumPx > 0 && outOfRange && !textFits;
    if (leaveTextOut) {
        gap = 0.0;
    }
    const QFontMetricsF fm(font);
    const double textW = leaveTextOut ? 0.0 : fm.horizontalAdvance(text);
    const double left = podRect.center().x() - (triW + gap + textW) * 0.5;
    const QRectF textRect(left + triW + gap, podRect.top(), textW, podRect.height());

    painter.setFont(font);
    painter.setPen(color);
    if (!leaveTextOut) {
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextDontClip, text);
    }

    if (outOfRange) {
        // Centred on the digits: AlignVCenter centres the line box (ascent + descent)
        const double baseline = podRect.center().y() - fm.height() * 0.5 + fm.ascent();
        const double midY = baseline - fm.capHeight() * 0.5;
        const double halfH = triW * 0.5;
        QPolygonF tri;
        if (state == InputState::Overrange) {
            tri << QPointF(left, midY + halfH) << QPointF(left + triW, midY + halfH)
                << QPointF(left + triW * 0.5, midY - halfH);
        } else {
            tri << QPointF(left, midY - halfH) << QPointF(left + triW, midY - halfH)
                << QPointF(left + triW * 0.5, midY + halfH);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(tri);
    }
}

void drawInvalidHatch(QPainter &painter, const QPainterPath &area, double thickness, const QColor &color)
{
    const QRectF bounds = area.boundingRect();
    if (bounds.isEmpty()) {
        return;
    }
    const double spacing = std::clamp(thickness * 0.45, 4.0, 8.0);
    painter.save();
    painter.setClipPath(area, Qt::IntersectClip);
    painter.setPen(QPen(color, 1.2, Qt::SolidLine, Qt::FlatCap));
    painter.setBrush(Qt::NoBrush);
    // Lines rising to the right at 45°, from x - h to x over the height h of the area
    const double h = bounds.height();
    for (double x = bounds.left(); x < bounds.right() + h; x += spacing) {
        painter.drawLine(QPointF(x - h, bounds.bottom()), QPointF(x, bounds.top()));
    }
    painter.restore();
}

void drawPadlock(QPainter &painter, const QRectF &box, const QColor &color)
{
    const double w = box.width();
    const double h = box.height();
    const double cx = box.center().x();
    const double stroke = w * 0.18;
    const double bodyTop = box.top() + h * 0.42;
    const double arch = w * 0.70 - stroke; // Width of the centre line of the shackle
    const QRectF archRect(cx - arch * 0.5, box.top() + stroke * 0.5, arch, arch);
    QPainterPath shackle;
    shackle.moveTo(cx - arch * 0.5, bodyTop + stroke * 0.5);
    shackle.lineTo(cx - arch * 0.5, archRect.center().y());
    shackle.arcTo(archRect, 180.0, -180.0);
    shackle.lineTo(cx + arch * 0.5, bodyTop + stroke * 0.5);
    painter.save();
    painter.setPen(QPen(color, stroke, Qt::SolidLine, Qt::FlatCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shackle);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(box.left(), bodyTop, w, box.bottom() - bodyTop), w * 0.12, w * 0.12);
    painter.restore();
}

} // namespace internal
} // namespace QtIndustrialWidgets
