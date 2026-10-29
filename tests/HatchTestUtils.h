/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the tests of the hatch of an invalid (NaN) input in a bar (#151): the test sets the colour of the
// hatch to pure green, and no other pixel of the area is green.

#include <QtCore/QPoint>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QImage>

namespace HatchTest {

inline bool isHatchPixel(QRgb p)
{
    return qGreen(p) > 150 && qGreen(p) - qRed(p) > 80 && qGreen(p) - qBlue(p) > 80;
}

// Empty when the hatch pixels of \a image cover \a area as a pattern of lines: hatch ink in at least 80 % of the
// rows and of the columns of the area, on 10 % to 60 % of its pixels (neither a fill nor a few strokes)
inline QString hatchError(const QImage &image, const QVector<QPoint> &area)
{
    if (area.isEmpty()) {
        return QStringLiteral("empty area");
    }
    QSet<int> rows;
    QSet<int> columns;
    QSet<int> inkRows;
    QSet<int> inkColumns;
    int ink = 0;
    for (const QPoint &p : area) {
        rows.insert(p.y());
        columns.insert(p.x());
        if (isHatchPixel(image.pixel(p))) {
            ++ink;
            inkRows.insert(p.y());
            inkColumns.insert(p.x());
        }
    }
    const double rowCover = double(inkRows.size()) / rows.size();
    const double columnCover = double(inkColumns.size()) / columns.size();
    const double density = double(ink) / area.size();
    if (rowCover < 0.8 || columnCover < 0.8 || density < 0.1 || density > 0.6) {
        return QStringLiteral("hatch on %1 % of the rows, %2 % of the columns, %3 % of the %4 area pixels")
            .arg(rowCover * 100.0, 0, 'f', 1)
            .arg(columnCover * 100.0, 0, 'f', 1)
            .arg(density * 100.0, 0, 'f', 1)
            .arg(area.size());
    }
    return QString();
}

} // namespace HatchTest
