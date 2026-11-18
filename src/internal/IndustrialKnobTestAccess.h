/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/IndustrialKnob.h>
#include "IndustrialKnob_p.h"

#include <functional>
#include <utility>

namespace QtIndustrialWidgets {
namespace internal {

// Test hook of IndustrialKnob (#177)
class IndustrialKnobTestAccess {
public:
    // Number of label ink measures (measureCenteredTextInk()) so far
    static quint64 labelInkMeasureCount(const IndustrialKnob &knob) {
        return knob.d_ptr->m_labelInkMeasures;
    }

    // From now on, minimumSizeHint() measures every label each time it lays out a size, as before the table of
    // #184: the reference the table is checked against. Takes effect at the next computation
    static void setMinimumWithoutTable(IndustrialKnob &knob, bool without) {
        knob.d_ptr->m_minimumWithoutTable = without;
        knob.d_ptr->minimumSizeChanged(&knob);
    }

    // The real text measures of the last computation of minimumSizeHint(); empty without the table
    static std::vector<TextMetricsTable::Measure> lastMinimumMeasures(const IndustrialKnob &knob) {
        return knob.d_ptr->m_lastMinimumMeasures;
    }

    // #182, #243: where the padlock of read-only is drawn, in logical px, at the scale of the last paint
    static QRectF padlockRect(const IndustrialKnob &knob) {
        return IndustrialKnobPrivate::stateMarkPadlockRect(knob.d_ptr->m_scale.geometry);
    }
    // #243: where the ⊘ of disabled is drawn, in logical px, at the scale of the last paint
    static QRectF disabledMarkRect(const IndustrialKnob &knob) {
        return IndustrialKnobPrivate::disabledMarkRect(knob.d_ptr->m_scale.geometry);
    }
    // #243: the disc of knobColor() under the mark of the state: its centre and radius, in logical px
    static std::pair<QPointF, double> stateMarkDisc(const IndustrialKnob &knob) {
        const internal::KnobScaleGeometry &g = knob.d_ptr->m_scale.geometry;
        return {QPointF(g.centerX, g.centerY), IndustrialKnobPrivate::stateMarkDiscRadius(g)};
    }

    // #183: \a observer sees every accessibility event the knob sends, before QAccessible::updateAccessibility();
    // an empty function removes it
    static void setAccessibleObserver(IndustrialKnob &knob, std::function<void(QAccessibleEvent *)> observer) {
        knob.d_ptr->m_accessibleObserver = std::move(observer);
    }
    // Label texts formatted in the last computation of minimumSizeHint() (labelText() calls)
    static quint64 lastMinimumLabelFormats(const IndustrialKnob &knob) {
        return knob.d_ptr->m_lastMinimumLabelFormats;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
