/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/StripChart.h>
#include "internal/StripChartTestAccess.h"
#include "internal/StripChartGeometry.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtCore/QStringList>
#include <QtCore/QLoggingCategory>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

namespace QtIndustrialWidgets {

inline std::unique_ptr<StripChart> makeChart(int w = 800, int h = 600, double yMin = 0.0, double yMax = 100.0)
{
    auto chart = std::make_unique<StripChart>();
    chart->resize(w, h);
    chart->setYRange(yMin, yMax);
    internal::StripChartTestAccess::enableDiagnostics(*chart, true);
    return chart;
}

inline QImage renderToImage(StripChart &chart)
{
    qreal dpr = chart.devicePixelRatioF();
    QSize imgSize = (QSizeF(chart.size()) * dpr).toSize();
    QImage img(imgSize, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::black);
    QPainter painter(&img);
    chart.render(&painter);
    return img;
}

inline std::vector<int> columnsWithColour(const QImage &img, const QColor &colour,
                                         int xMin = -1, int xMax = -1,
                                         int yMin = -1, int yMax = -1,
                                         int tolerance = 30)
{
    if (xMin < 0) xMin = 0;
    if (xMax < 0 || xMax >= img.width()) xMax = img.width() - 1;
    if (yMin < 0) yMin = 0;
    if (yMax < 0 || yMax >= img.height()) yMax = img.height() - 1;

    std::vector<int> cols;
    for (int x = xMin; x <= xMax; ++x) {
        bool found = false;
        for (int y = yMin; y <= yMax; ++y) {
            QColor c = img.pixelColor(x, y);
            if (std::abs(c.red() - colour.red()) <= tolerance &&
                std::abs(c.green() - colour.green()) <= tolerance &&
                std::abs(c.blue() - colour.blue()) <= tolerance) {
                found = true;
                break;
            }
        }
        if (found) {
            cols.push_back(x);
        }
    }
    return cols;
}

inline std::vector<int> traceYAtColumn(const QImage &img, const QColor &colour, int x,
                                      int yMin = -1, int yMax = -1, int tolerance = 30)
{
    if (yMin < 0) yMin = 0;
    if (yMax < 0 || yMax >= img.height()) yMax = img.height() - 1;

    std::vector<int> yCoords;
    if (x < 0 || x >= img.width()) return yCoords;

    for (int y = yMin; y <= yMax; ++y) {
        QColor c = img.pixelColor(x, y);
        if (std::abs(c.red() - colour.red()) <= tolerance &&
            std::abs(c.green() - colour.green()) <= tolerance &&
            std::abs(c.blue() - colour.blue()) <= tolerance) {
            yCoords.push_back(y);
        }
    }
    return yCoords;
}

inline double mapY(const StripChart &chart, double value)
{
    QRectF plotRect = internal::StripChartTestAccess::plotArea(chart);
    double yRange = chart.yMaximum() - chart.yMinimum();
    if (std::abs(yRange) < 1e-9) yRange = 1.0;
    double yNorm = (value - chart.yMinimum()) / yRange;
    return plotRect.bottom() - yNorm * plotRect.height();
}

// x mapping of the last rendered frame (see internal::PlotXMapping), for images rendered with
// renderToImage() (device origin at the widget origin).
inline internal::PlotXMapping frameXMapping(const StripChart &chart)
{
    const auto &f = internal::StripChartTestAccess::lastFrame(chart);
    return internal::makePlotXMapping(internal::BucketGrid(f.gridNum, f.gridDen), f.kStart, f.plotLeftDev);
}

// Logical x of raw sample i (SampleIndex mode; i is the absolute sample index, equal to the buffer
// index while the buffer has not wrapped) in the last rendered frame.
inline double mapIndex(const StripChart &chart, size_t i)
{
    const auto &f = internal::StripChartTestAccess::lastFrame(chart);
    return frameXMapping(chart).positionDev(static_cast<qint64>(i)) / f.dpr;
}

// Logical x of a raw sample at time t in the last rendered frame.
inline double mapT(const StripChart &chart, qint64 t)
{
    const auto &f = internal::StripChartTestAccess::lastFrame(chart);
    return frameXMapping(chart).positionDev(t) / f.dpr;
}

class WarningGuard {
public:
    WarningGuard() {
        s_instance = this;
        s_oldHandler = qInstallMessageHandler(messageHandler);
    }
    ~WarningGuard() {
        qInstallMessageHandler(s_oldHandler);
        s_instance = nullptr;
    }
    [[nodiscard]] const QStringList &warnings() const { return m_warnings; }
    [[nodiscard]] bool hasWarnings() const { return !m_warnings.isEmpty(); }
    void clear() { m_warnings.clear(); }

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        if (type == QtWarningMsg && s_instance) {
            s_instance->m_warnings.append(msg);
        }
        if (s_oldHandler) {
            s_oldHandler(type, context, msg);
        }
    }

    static inline WarningGuard *s_instance{nullptr};
    static inline QtMessageHandler s_oldHandler{nullptr};
    QStringList m_warnings;
};

} // namespace QtIndustrialWidgets
