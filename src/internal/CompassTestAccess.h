/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of Compass (#226). Internal: never installed.

#include <QtIndustrialWidgets/Compass.h>
#include "internal/Compass_p.h"

namespace QtIndustrialWidgets {
namespace internal {

class CompassTestAccess
{
public:
    /// The HDG flag as the last paint laid it out (also while the heading is valid and no flag is drawn)
    static CompassFlag flag(const Compass &compass) { return compass.d_ptr->m_flag; }

    /// The texts of the card as the last drawing of the card laid them out
    static CompassCardLayout cardLayout(const Compass &compass) { return compass.d_ptr->m_cardLayout; }
};

} // namespace internal
} // namespace QtIndustrialWidgets
