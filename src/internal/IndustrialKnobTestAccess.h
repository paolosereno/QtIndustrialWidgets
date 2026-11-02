/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/IndustrialKnob.h>
#include "IndustrialKnob_p.h"

namespace QtIndustrialWidgets {
namespace internal {

// Test hook of IndustrialKnob (#177)
class IndustrialKnobTestAccess {
public:
    // Number of label ink measures (measureCenteredTextInk()) so far
    static quint64 labelInkMeasureCount(const IndustrialKnob &knob) {
        return knob.d_ptr->m_labelInkMeasures;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
