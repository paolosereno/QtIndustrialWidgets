/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Small pixel helpers shared by the widget tests: printing a pixel, counting pixels in a logical rectangle, the
// bounding box of a set of device pixels.

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QImage>
#include <QtGui/QRgb>

#include <functional>

namespace PixelTest {

inline QString rgb(QRgb p)
{
    return QStringLiteral("rgb(%1, %2, %3)").arg(qRed(p)).arg(qGreen(p)).arg(qBlue(p));
}

// Device pixels of \a img in a logical rectangle that match a predicate
inline int countIn(const QImage &img, const QRectF &logical, const std::function<bool(QRgb)> &match)
{
    const double dpr = img.devicePixelRatio();
    const QRect r = QRectF(logical.topLeft() * dpr, logical.size() * dpr).toAlignedRect().intersected(img.rect());
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            n += match(img.pixel(x, y)) ? 1 : 0;
        }
    }
    return n;
}

// Bounding box of a set of device pixels (empty for no pixel)
inline QRect boundsOf(const QVector<QPoint> &points)
{
    QRect r;
    for (const QPoint &p : points) {
        r |= QRect(p, QSize(1, 1));
    }
    return r;
}

} // namespace PixelTest
