/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers to isolate text ink in widget renders (#46): the pixels that change when only a text
// colour changes.

#include <QtCore/QPoint>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <algorithm>

namespace TextInk {

/// Device pixels that differ between two renders of the same size.
inline QVector<QPoint> differingPixels(const QImage &a, const QImage &b)
{
    QVector<QPoint> out;
    const QImage ca = a.convertToFormat(QImage::Format_ARGB32);
    const QImage cb = b.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < ca.height(); ++y) {
        const auto *la = reinterpret_cast<const QRgb *>(ca.constScanLine(y));
        const auto *lb = reinterpret_cast<const QRgb *>(cb.constScanLine(y));
        for (int x = 0; x < ca.width(); ++x) {
            if (la[x] != lb[x]) {
                out.push_back(QPoint(x, y));
            }
        }
    }
    return out;
}

/// Sorts pixels so that two sets with the same pixels compare equal.
inline QVector<QPoint> sorted(QVector<QPoint> pixels)
{
    std::sort(pixels.begin(), pixels.end(), [](const QPoint &a, const QPoint &b) {
        return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
    });
    return pixels;
}

} // namespace TextInk
