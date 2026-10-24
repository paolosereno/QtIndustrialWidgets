/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Drawing of a decimated (M4) trace segment, shared by StripChart's Time and SampleIndex paths, AnalysisChart (#91)
// and tst_M4Decimator (#90): one source for how decimated data looks.
//
// Buckets: a vertex at time t belongs to bucket xmap.grid.bucketOf(t - origin). StripChart anchors its buckets to
// absolute time or sample index (origin 0); AnalysisChart anchors them to the start of its view (origin = xStart,
// with xmap.kStart 0).

#include "M4Decimator.h"
#include "StripChartGeometry.h"

#include <QtCore/QLineF>
#include <QtCore/QPointF>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>
#include <QtGui/QTransform>

#include <cmath>

#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \brief Appends the vertices of a decimated segment to \p poly: every vertex at the centre of its bucket's device
 *        column (PlotXMapping::vertexDev()), at the height \p mapY gives its value.
 *
 * \p mapY maps a value to a logical y; \p toLogicalX maps a device x to a logical x; \p origin is subtracted from the
 * vertex times before the bucket is computed (see above).
 */
template <typename MapY, typename ToLogicalX>
void appendDecimatedVertices(QPolygonF &poly, const PlotXMapping &xmap, const std::vector<M4Point> &segment,
                             MapY mapY, ToLogicalX toLogicalX, qint64 origin = 0)
{
    for (const auto &pt : segment) {
        poly << QPointF(toLogicalX(xmap.vertexDev(xmap.grid.bucketOf(pt.t - origin))), mapY(pt.value));
    }
}

// An aliased cosmetic line can leave out the first or last pixel of a polyline (for instance when
// a decimated segment starts with a vertical stretch shorter than a pixel, or consists of a single
// bucket). The end pixels of each decimated segment are therefore drawn explicitly; a point at
// 0.25 device px inside a column lights exactly that column.
template <typename ToLogicalX>
void drawSegmentEndPixels(QPainter &painter, const PlotXMapping &xmap, const std::vector<M4Point> &segment,
                          const QPolygonF &poly, ToLogicalX toLogicalX, qint64 origin = 0)
{
    const qint64 kFirst = xmap.grid.bucketOf(segment.front().t - origin);
    const qint64 kLast = xmap.grid.bucketOf(segment.back().t - origin);
    painter.drawPoint(QPointF(toLogicalX(xmap.columnLeftDev(kFirst) + 0.25), poly.first().y()));
    painter.drawPoint(QPointF(toLogicalX(xmap.columnLeftDev(kLast) + 0.25), poly.last().y()));
}

/**
 * \brief Draws one non-empty decimated segment with the painter's current pen (#73): every device column as one
 *        vertical line from its minimum to its maximum vertex, the connection from the last vertex of each column to
 *        the first vertex of the next, the end pixels of these lines and the segment's end pixels
 *        (drawSegmentEndPixels()).
 *
 * Each column's extent is stroked once (a polyline through first, min, max and last strokes it twice). Compared with
 * that polyline a column lights at most one more row at each end of its extent, and no pixel detached from the trace:
 * the difference approved by the maintainer on #73, from Qt's pixel model for aliased drawing (Qt documentation,
 * "Coordinate System", section "Aliased Painting": https://doc.qt.io/qt-6/coordsys.html#aliased-painting).
 * tst_M4Decimator::columnStrokeStaysWithinPolylineExtent() checks it.
 *
 * \p poly is a reusable buffer: it is cleared, filled with the vertices (appendDecimatedVertices()) and left so
 * (callers record it).
 */
template <typename MapY, typename ToLogicalX>
void drawDecimatedSegment(QPainter &painter, const PlotXMapping &xmap, const std::vector<M4Point> &segment,
                          QPolygonF &poly, MapY mapY, ToLogicalX toLogicalX, qint64 origin = 0)
{
    poly.clear();
    appendDecimatedVertices(poly, xmap, segment, mapY, toLogicalX, origin);
    thread_local std::vector<QLineF> lines;
    thread_local std::vector<QPointF> ends;
    lines.clear();
    ends.clear();
    // An aliased line lights the device pixel row that contains its end point, an aliased point the row nearest to it
    // (Qt 6.4.2: y 29.708 lights row 29 as a line end, row 30 as a point): the explicit end pixels are placed 0.25
    // device px inside the row that contains the vertex
    const QTransform toDevice = painter.deviceTransform();
    const QTransform toLogical = toDevice.inverted();
    auto rowOf = [&](qreal y) {
        const qreal yDev = toDevice.map(QPointF(0.0, y)).y();
        return toLogical.map(QPointF(0.0, std::floor(yDev) + 0.25)).y();
    };
    const int n = static_cast<int>(poly.size());
    int i = 0;
    while (i < n) {
        const qint64 k = xmap.grid.bucketOf(segment[static_cast<size_t>(i)].t - origin);
        int j = i;
        qreal yMin = poly[i].y();
        qreal yMax = yMin;
        while (j + 1 < n && xmap.grid.bucketOf(segment[static_cast<size_t>(j + 1)].t - origin) == k) {
            ++j;
            yMin = qMin(yMin, poly[j].y());
            yMax = qMax(yMax, poly[j].y());
        }
        // A zero-length line in drawLines() corrupts the next line in Qt's aliased cosmetic stroker (seen with Qt 6.4.2):
        // a column without vertical extent gets no line; its connections and end pixels light it
        if (yMax > yMin) lines.emplace_back(poly[i].x(), yMin, poly[i].x(), yMax);
        if (j + 1 < n) lines.emplace_back(poly[j], poly[j + 1]);
        // The caller's FlatCap pen leaves out an end pixel of every separate line (a polyline's joins cover it): the end
        // pixels of the column's lines (min, max, first and last vertex) are drawn explicitly, as drawSegmentEndPixels()
        // does, with a point 0.25 device px inside the column
        const qreal xEnd = toLogicalX(xmap.columnLeftDev(k) + 0.25);
        ends.emplace_back(xEnd, rowOf(yMin));
        if (yMax > yMin) ends.emplace_back(xEnd, rowOf(yMax));
        if (poly[i].y() != yMin && poly[i].y() != yMax) ends.emplace_back(xEnd, rowOf(poly[i].y()));
        if (j != i && poly[j].y() != yMin && poly[j].y() != yMax) ends.emplace_back(xEnd, rowOf(poly[j].y()));
        i = j + 1;
    }
    painter.drawLines(lines.data(), static_cast<int>(lines.size()));
    painter.drawPoints(ends.data(), static_cast<int>(ends.size()));
    drawSegmentEndPixels(painter, xmap, segment, poly, toLogicalX, origin);
}

} // namespace internal
} // namespace QtIndustrialWidgets
