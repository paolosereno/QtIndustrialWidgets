/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/AnalysisChart.h>
#include "AnalysisChart_p.h"

#include <cstddef>

namespace QtIndustrialWidgets {
namespace internal {

// Test hook of AnalysisChart: what the last paint drew
class AnalysisChartTestAccess {
public:
    static void enableDiagnostics(AnalysisChart &chart, bool enable = true)
    {
        chart.d_ptr->recordDiagnostics = enable;
    }

    static const AnalysisChartPrivate::FrameDiagnostics &lastFrame(const AnalysisChart &chart)
    {
        return chart.d_ptr->lastFrame;
    }

    static QRectF plotArea(const AnalysisChart &chart)
    {
        return chart.plotArea();
    }

    // Bytes held by the min/max pyramid of a channel (0 for an unknown channel)
    static std::size_t pyramidBytes(const AnalysisChart &chart, int channelId)
    {
        const auto *ch = chart.d_ptr->channel(channelId);
        return ch ? ch->pyramid.memoryBytes() : 0;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
