// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "internal/ValueStateDrawing.h"

#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPolygonF>

#include <algorithm>

namespace QtIndustrialWidgets {
namespace internal {

void drawStateReadout(QPainter &painter, const QRectF &podRect, QFont font, const QString &text, InputState state,
                      const QColor &color)
{
    const bool outOfRange = (state == InputState::Overrange || state == InputState::Underrange);
    const double available =
        podRect.width() - 2.0 * std::max(2.0, std::min(podRect.width(), podRect.height()) * 0.12);
    double triW = 0.0;
    double gap = 0.0;
    for (int px = font.pixelSize();; --px) {
        font.setPixelSize(px);
        const QFontMetricsF fm(font);
        triW = outOfRange ? fm.capHeight() * 0.9 : 0.0;
        gap = outOfRange ? triW * 0.35 : 0.0;
        if (triW + gap + fm.horizontalAdvance(text) <= available || px <= 6) {
            break;
        }
    }
    const QFontMetricsF fm(font);
    const double textW = fm.horizontalAdvance(text);
    const double left = podRect.center().x() - (triW + gap + textW) * 0.5;
    const QRectF textRect(left + triW + gap, podRect.top(), textW, podRect.height());

    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextDontClip, text);

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
    const double spacing = std::max(4.0, thickness * 0.45);
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

} // namespace internal
} // namespace QtIndustrialWidgets
