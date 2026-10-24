/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/AnalysisChart.h>

#include "AnalysisChartView.h"
#include "AnalysisPyramid.h"

#include <QtCore/QPointF>
#include <QtGui/QFont>
#include <QtGui/QPolygonF>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QColor>

#include <limits>
#include <vector>

namespace QtIndustrialWidgets {

class AnalysisChartPrivate
{
public:
    struct Channel {
        int id{0};
        QString name;
        QColor color;
        double penWidth{1.8};
        bool visible{true};
        std::vector<qint64> t;     // non-decreasing timestamps (ns)
        std::vector<double> v;     // values, possibly non-finite
        qint64 autoGap{0};         // 4 x EMA of the sampling interval (internal::gapThresholdFor)
        double meanInterval{0.0};  // (t.back() - t.front()) / (n - 1); 0 below two samples
        double finiteMin{std::numeric_limits<double>::infinity()};
        double finiteMax{-std::numeric_limits<double>::infinity()};
        internal::AnalysisPyramid pyramid; // over v, rebuilt by setChannelData()
    };

    // What the last paint drew, for the tests (internal::AnalysisChartTestAccess)
    struct ChannelDiagnostics {
        int id{0};
        std::vector<std::vector<qint64>> segments; // timestamps of the samples (or M4 vertices) of each drawn polyline
        std::vector<QPolygonF> polylines;           // the drawn polylines, logical coordinates
        bool decimated{false};
    };
    // One row of the readout band: its cells (columns: label, A, B, delta) and where each was drawn
    struct BandRow {
        QStringList cells;
        std::vector<QRectF> cellRects;
        QColor swatch; // the channel colour drawn before the name, invalid if none
    };
    struct FrameDiagnostics {
        QRectF plotRect;
        internal::XRange view;
        int widthDev{0};      // device columns of the plot, one bucket each
        double leftDev{0.0};  // device x of the plot's left edge (snapped)
        double devScale{1.0}; // device px per logical px
        std::vector<ChannelDiagnostics> channels;
        std::vector<internal::AxisTick> xTicks;
        std::vector<internal::AxisTick> yTicks;
        double cursorX[2]{-1.0, -1.0}; // logical x of the drawn cursors, -1 if not drawn
        QRectF bandRect;
        QFont bandFont;
        std::vector<BandRow> band;
        QRectF boxRect; // the box-zoom rectangle being dragged, empty if none
    };

    Channel *channel(int id)
    {
        for (Channel &c : channels) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }
    const Channel *channel(int id) const
    {
        for (const Channel &c : channels) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }

    std::vector<Channel> channels;
    int nextId{0};
    qint64 gapThreshold{0};

    internal::XRange view;
    double yMinimum{0.0};
    double yMaximum{1.0};
    bool viewFromData{false}; // the view has been set from data at least once

    bool gridVisible{true};
    bool legendVisible{true};
    // Same defaults as StripChart
    QColor gridColor{QColor(42, 54, 70)};
    QColor backgroundColor{QColor(14, 18, 25)};
    QColor bezelColor{QColor(38, 46, 60)};
    QColor textColor{QColor(210, 220, 235)};
    AnalysisChart::TimeLabelFormat timeLabelFormat{AnalysisChart::TimeLabelFormat::Relative};
    AnalysisChart::DecimationMode decimationMode{AnalysisChart::DecimationMode::Auto};
    QDateTime timeOrigin;

    // Cursors (index 0 = A, 1 = B) and the readout band
    bool cursorVisible[2]{false, false};
    qint64 cursorPos[2]{0, 0};
    bool readoutVisible{true};
    QColor cursorColor{QColor(255, 196, 0)};

    // Mouse interaction in progress
    enum class Drag { None, Pan, Box, CursorA, CursorB };
    Drag drag{Drag::None};
    QPointF pressPos;
    QPointF currentPos;
    internal::XRange pressView;
    double pressYMinimum{0.0};
    double pressYMaximum{1.0};

    // Decimated drawing buffers, reused by every channel and frame (#97)
    std::vector<internal::M4Bucket> reusableBuckets;
    std::vector<std::vector<internal::M4Point>> reusableSegments;
    QPolygonF reusablePoly;

    bool recordDiagnostics{false};
    FrameDiagnostics lastFrame;
};

} // namespace QtIndustrialWidgets
