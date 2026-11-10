/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Test hook of IndustrialSlider (#180): what the last paint drew. Internal: never installed.

#include "internal/IndustrialSlider_p.h"

namespace QtIndustrialWidgets {
namespace internal {

class IndustrialSliderTestAccess
{
public:
    /// Layout of the last paintEvent() (an empty one before the first)
    static SliderLayout lastLayout(const IndustrialSlider &slider) { return slider.d_ptr->m_lastLayout; }
    /// Parts of the setpoint readout drawn by the last paint, joined by spaces ("SP 60.0 %"); empty when not drawn
    static QString lastSetpointReadout(const IndustrialSlider &slider) { return slider.d_ptr->m_lastSetpointReadout; }
    /// The same for the feedback readout; "Δ" and "?" stand for the deviation marks, "▲" / "▼" for the triangles
    static QString lastFeedbackReadout(const IndustrialSlider &slider) { return slider.d_ptr->m_lastFeedbackReadout; }
    /// Pixel size of the setpoint readout drawn by the last paint (0 when not drawn)
    static int lastReadoutPixelSize(const IndustrialSlider &slider) { return slider.d_ptr->m_lastReadoutPx; }
    /// The focus ring drawn by the last paint (the centre line of its 2 px pen); empty when not drawn
    static QRectF lastFocusRing(const IndustrialSlider &slider) { return slider.d_ptr->m_lastFocusRing; }
    /// Pixel size of the feedback readout drawn by the last paint (0 when not drawn)
    static int lastFeedbackReadoutPixelSize(const IndustrialSlider &slider)
    {
        return slider.d_ptr->m_lastFeedbackReadoutPx;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
