/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of LinearGauge (#274). Internal: never installed.

#include <QtIndustrialWidgets/LinearGauge.h>
#include "internal/LinearGauge_p.h"

#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

class LinearGaugeTestAccess
{
public:
    /// The scale labels drawn by the last render of the static scale: major-tick index and extent along the scale in
    /// widget coordinates (x from the left, horizontal; y from the top, vertical; start < end), as laid out
    static std::vector<ScaleLabelSlot> lastScaleLabels(const LinearGauge &gauge)
    {
        return gauge.d_ptr->m_lastScaleLabels;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
