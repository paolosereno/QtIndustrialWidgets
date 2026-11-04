/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of RadialGauge (#184). Internal: never installed.

#include <QtIndustrialWidgets/RadialGauge.h>
#include "internal/RadialGauge_p.h"

namespace QtIndustrialWidgets {
namespace internal {

class RadialGaugeTestAccess
{
public:
    /// From now on, minimumSizeHint() measures every text each time it lays out a side, as before the tables of
    /// #184: the reference the tables are checked against. Takes effect at the next computation.
    static void setMinimumWithoutTable(RadialGauge &gauge, bool without)
    {
        gauge.d_ptr->m_minimumWithoutTable = without;
        gauge.d_ptr->minimumSizeChanged(&gauge);
    }

    /// The real text measures of the last computation of minimumSizeHint(), one log per table (bold, plain); empty
    /// without the tables
    static std::vector<std::vector<TextMetricsTable::Measure>> lastMinimumMeasures(const RadialGauge &gauge)
    {
        return gauge.d_ptr->m_lastMinimumMeasures;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
