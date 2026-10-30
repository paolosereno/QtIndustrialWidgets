/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the pixel tests of the radial scale labels (#50, #60).

#include <QtCore/QHash>
#include <QtCore/QRect>
#include <QtCore/QVector>
#include <QtGui/QFont>
#include <QtGui/QFontMetricsF>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <vector>

namespace LabelInk {

// Device-pixel ink box of a text drawn alone in the given font at the given device pixel ratio
inline QRect inkBoxAlone(const QString &text, const QFont &font, qreal dpr)
{
    const QFontMetricsF fm(font);
    const QSizeF logical(fm.horizontalAdvance(text) + 40.0, fm.height() + 40.0);
    QImage img((logical * dpr).toSize(), QImage::Format_ARGB32);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::black);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(QPointF(20.0, 20.0 + fm.ascent()), text);
    }
    QRect box;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (img.pixel(x, y) != qRgb(0, 0, 0)) {
                box |= QRect(x, y, 1, 1);
            }
        }
    }
    return box;
}

// Bounding boxes of the groups of pixels closer than \a threshold free pixels along the axis that
// separates them (union-find over the ink pixels, neighbours searched on a grid)
inline QVector<QRect> inkClusters(const QVector<QPoint> &ink, double threshold)
{
    const int reach = static_cast<int>(std::ceil(threshold));
    QHash<qint64, int> at;
    auto key = [](int x, int y) { return (static_cast<qint64>(y) << 32) | static_cast<quint32>(x); };
    for (int i = 0; i < ink.size(); ++i) {
        at.insert(key(ink[i].x(), ink[i].y()), i);
    }
    std::vector<int> parent(static_cast<size_t>(ink.size()));
    for (int i = 0; i < ink.size(); ++i) {
        parent[static_cast<size_t>(i)] = i;
    }
    std::function<int(int)> root = [&](int i) {
        while (parent[static_cast<size_t>(i)] != i) {
            parent[static_cast<size_t>(i)] = parent[static_cast<size_t>(parent[static_cast<size_t>(i)])];
            i = parent[static_cast<size_t>(i)];
        }
        return i;
    };
    for (int i = 0; i < ink.size(); ++i) {
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                // free pixels between the two along the separating axis
                const int free = std::max(std::abs(dx), std::abs(dy)) - 1;
                if (free >= threshold) {
                    continue;
                }
                const auto it = at.constFind(key(ink[i].x() + dx, ink[i].y() + dy));
                if (it != at.constEnd()) {
                    parent[static_cast<size_t>(root(i))] = root(it.value());
                }
            }
        }
    }
    QHash<int, QRect> clusters;
    for (int i = 0; i < ink.size(); ++i) {
        clusters[root(i)] |= QRect(ink[i], QSize(1, 1));
    }
    QVector<QRect> out;
    for (const QRect &c : clusters) {
        out.append(c);
    }
    return out;
}

} // namespace LabelInk
