/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/StripChart.h>
#include "StripChart_p.h"

namespace QtIndustrialWidgets {
namespace internal {

// Test hook of StripChart: what the last paint drew, and switches the tests need
class StripChartTestAccess {
public:
    static void enableDiagnostics(StripChart &chart, bool enable = true) {
        chart.d_ptr->recordDiagnostics = enable;
    }

    static const FrameDiagnostics &lastFrame(const StripChart &chart) {
        return chart.d_ptr->lastFrame;
    }

    static QRectF plotArea(const StripChart &chart) {
        return chart.plotArea();
    }

    static void setRawPenCosmetic1px(StripChart &chart, bool enable) {
        chart.d_ptr->m_testRawPenCosmetic1px = enable;
    }

    // Number of full rebuilds of the channel's incremental decimation stream so far
    static quint64 streamRebuildCount(const StripChart &chart, int channelId) {
        if (channelId >= 0 && channelId < static_cast<int>(chart.d_ptr->m_channels.size())) {
            return chart.d_ptr->m_channels[static_cast<size_t>(channelId)].streamRebuilds;
        }
        return 0;
    }

    static bool isDecimating(const StripChart &chart, int channelId) {
        if (channelId >= 0 && channelId < static_cast<int>(chart.d_ptr->m_channels.size())) {
            return chart.d_ptr->m_channels[static_cast<size_t>(channelId)].lastDecimating;
        }
        return false;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
