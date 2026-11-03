/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the tests of the readout ink against the 7 pt floor (#167): the ink of one colour of a widget, and
// the reference ink box of a readout line drawn alone. Shared by the tests of the widgets with a readout panel.

#include "TextInkTestUtils.h"
#include "internal/RadialGaugeLayout.h"

#include <QtCore/QHash>
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>
#include <QtWidgets/QWidget>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ReadoutInk {

// The device pixels drawn in one colour of \a w: those that change when the colour set by \a set goes from
// transparent to pure magenta. \a render returns an image of the widget. The colour is left at magenta.
template <class Widget, class Render>
QVector<QPoint> magentaInk(Widget &w, void (Widget::*set)(const QColor &), Render render)
{
    (w.*set)(QColor(255, 0, 255, 0));
    const QImage without = render(w);
    (w.*set)(QColor(255, 0, 255));
    const QImage with = render(w);
    return TextInk::differingPixels(with, without);
}

// The font of a readout line at the floor, as the panels of TankDisplay and ValveIndicator draw it: the bold widget
// font at internal::minimumReadoutPixelSize() for the widget's DPI, with greyscale antialiasing
inline QFont floorFont(const QWidget &w)
{
    QFont font = w.font();
    font.setBold(true);
    font.setPixelSize(QtIndustrialWidgets::internal::minimumReadoutPixelSize(w.logicalDpiY()));
    font.setStyleStrategy(QFont::StyleStrategy(font.styleStrategy() | QFont::NoSubpixelAntialias));
    return font;
}

// The ink box, in device px, of a readout line drawn alone in \a font: the triangle (when \a triangle; 0.9 x the cap
// height, 0.35 of it before the text, as the widgets draw it) and \a text. It is drawn as the widget draws it: same
// DPI and device pixel ratio as \a w, on the opaque \a background (so with the same antialiasing), and the smallest
// width and height over all the sub-pixel positions are returned: the edges of a shape cover one column more or
// less depending on where it falls (in x and in y together: a faint bottom row hides faint side columns), and the
// raster engine places shapes and glyphs at 1/64 px, so the 64 x 64 offsets give the exact minimum. The reference
// the readout is measured against, without any slack
inline QSize smallestLineInk(const QString &text, bool triangle, const QFont &font, const QWidget &w, qreal dpr,
                             const QColor &background)
{
    static QHash<QString, QSize> cache;
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8")
                            .arg(triangle)
                            .arg(text)
                            .arg(w.logicalDpiX())
                            .arg(w.logicalDpiY())
                            .arg(dpr)
                            .arg(font.toString())
                            .arg(static_cast<int>(font.styleStrategy()))
                            .arg(background.name(QColor::HexArgb));
    if (cache.contains(key)) {
        return cache.value(key);
    }
    const QFontMetricsF fm(font, &w);
    const double triW = triangle ? fm.capHeight() * 0.9 : 0.0;
    const double gap = (triangle && !text.isEmpty()) ? triW * 0.35 : 0.0;
    QSize smallest(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    const QSize area(static_cast<int>(std::ceil(triW + gap + fm.horizontalAdvance(text) + 8.0)),
                     static_cast<int>(std::ceil(fm.height() + 8.0))); // logical px
    for (int step = 0; step < 64 * 64; ++step) {
        const QPointF offset((step % 64) / 64.0 / dpr, (step / 64) / 64.0 / dpr); // logical px
        QImage img(area * dpr, QImage::Format_RGB32);
        img.setDevicePixelRatio(dpr);
        img.setDotsPerMeterX(qRound(w.logicalDpiX() / 0.0254));
        img.setDotsPerMeterY(qRound(w.logicalDpiY() / 0.0254));
        img.fill(background);
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            const QPointF origin = QPointF(4.0, 4.0 + fm.ascent()) + offset; // left end, baseline
            if (triangle) {
                const double midY = origin.y() - fm.capHeight() * 0.5;
                QPolygonF tri;
                tri << QPointF(origin.x(), midY + triW * 0.5) << QPointF(origin.x() + triW, midY + triW * 0.5)
                    << QPointF(origin.x() + triW * 0.5, midY - triW * 0.5);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 0, 255));
                p.drawPolygon(tri);
            }
            p.setFont(font);
            p.setPen(QColor(255, 0, 255));
            p.drawText(origin + QPointF(triW + gap, 0.0), text);
        }
        QRect box;
        for (int y = 0; y < img.height(); ++y) {
            const auto *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                if (row[x] != background.rgb()) {
                    box |= QRect(x, y, 1, 1);
                }
            }
        }
        smallest = smallest.boundedTo(box.size());
    }
    cache.insert(key, smallest);
    return smallest;
}

} // namespace ReadoutInk
