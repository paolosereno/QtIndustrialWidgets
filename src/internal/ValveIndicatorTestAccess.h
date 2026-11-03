/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of ValveIndicator (#153): the geometry of the last paint. Internal: never installed.

#include "internal/ValveIndicator_p.h"

namespace QtIndustrialWidgets {
namespace internal {

class ValveIndicatorTestAccess
{
public:
    /// Layout of the last paintEvent() (an empty one before the first)
    static ValveLayout lastLayout(const ValveIndicator &valve) { return valve.d_ptr->m_lastLayout; }
};

} // namespace internal
} // namespace QtIndustrialWidgets
