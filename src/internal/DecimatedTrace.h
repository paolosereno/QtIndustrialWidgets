/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
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

#include <QtCore/QPointF>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>

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
 * \brief Draws one non-empty decimated segment with the painter's current pen: the polyline through its vertices
 *        (appendDecimatedVertices()) and its end pixels (drawSegmentEndPixels()).
 *
 * \p poly is a reusable buffer: it is cleared, filled with the drawn vertices and left so (callers record it).
 */
template <typename MapY, typename ToLogicalX>
void drawDecimatedSegment(QPainter &painter, const PlotXMapping &xmap, const std::vector<M4Point> &segment,
                          QPolygonF &poly, MapY mapY, ToLogicalX toLogicalX, qint64 origin = 0)
{
    poly.clear();
    appendDecimatedVertices(poly, xmap, segment, mapY, toLogicalX, origin);
    painter.drawPolyline(poly);
    drawSegmentEndPixels(painter, xmap, segment, poly, toLogicalX, origin);
}

} // namespace internal
} // namespace QtIndustrialWidgets
