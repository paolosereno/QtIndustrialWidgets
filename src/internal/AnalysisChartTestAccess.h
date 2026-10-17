/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/AnalysisChart.h>
#include "AnalysisChart_p.h"

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
};

} // namespace internal
} // namespace QtIndustrialWidgets
