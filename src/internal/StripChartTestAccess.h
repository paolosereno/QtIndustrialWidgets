/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/StripChart.h>
#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>

namespace QtIndustrialWidgets {
namespace internal {

class QTINDUSTRIALWIDGETS_EXPORT StripChartTestAccess {
public:
    static void setRawPenCosmetic1px(StripChart &chart, bool enable);
    static bool isDecimating(const StripChart &chart, int channelId);
};

} // namespace internal
} // namespace QtIndustrialWidgets
