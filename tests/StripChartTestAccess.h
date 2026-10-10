/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/StripChart.h>
#include "internal/StripChart_p.h"

namespace QtIndustrialWidgets {

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

    static bool isDecimating(const StripChart &chart, int channelId) {
        if (channelId >= 0 && channelId < static_cast<int>(chart.d_ptr->m_channels.size())) {
            return chart.d_ptr->m_channels[channelId].lastDecimating;
        }
        return false;
    }
};

namespace internal {
class StripChartTestAccess : public QtIndustrialWidgets::StripChartTestAccess {};
}

} // namespace QtIndustrialWidgets
