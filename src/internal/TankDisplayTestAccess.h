/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of TankDisplay (#152): the geometry of the last paint. Internal: never installed.

#include "internal/TankDisplay_p.h"

namespace QtIndustrialWidgets {
namespace internal {

class TankDisplayTestAccess
{
public:
    /// Layout of the last paintEvent() (an empty one before the first)
    static TankLayout lastLayout(const TankDisplay &tank) { return tank.d_ptr->m_lastLayout; }
};

} // namespace internal
} // namespace QtIndustrialWidgets
