/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/IndustrialKnob.h>
#include "IndustrialKnob_p.h"

#include <functional>
#include <vector>
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
    // #226: the scale and the pod as the knob draws them at its size, in logical px
    struct Layout {
        KnobScaleGeometry geometry;
        std::vector<QString> texts;     // Every label, drawn or not
        std::vector<double> angles;     // The angle of each label, in degrees (0 = 12 o'clock, clockwise)
        std::vector<LayoutRect> boxes;  // The box of each label (Qt metrics), centred on a device pixel
        std::vector<LayoutRect> ink;    // Where each label draws (measured on a raster)
        std::vector<int> drawn;         // The labels drawn
        bool podVisible{false};
        QRectF pod;                     // The value pod
        int readoutPixelSize{0};        // The pixel size of the readout font
    };
    static Layout layout(IndustrialKnob &knob) {
        IndustrialKnobPrivate &d = *knob.d_ptr;
        // The layout of the last paint; a paint first where it is not that of the current size
        if (!d.m_scaleValid || d.m_scaleSize != knob.size() || d.m_scaleDpr != knob.devicePixelRatioF()) {
            knob.grab();
        }
        const IndustrialKnobPrivate::ScaleLayout &s = d.m_scale;
        Layout out;
        out.geometry = s.geometry;
        out.texts = s.texts;
        out.boxes = s.boxes;
        out.ink = s.ink;
        out.drawn = s.labels.drawn;
        out.angles = s.angles;
        out.podVisible = d.m_valueDisplayVisible;
        out.pod = s.pod;
        out.readoutPixelSize = s.readoutPixelSize;
        return out;
    }

    // Label texts formatted in the last computation of minimumSizeHint() (by labelTexts())
    static quint64 lastMinimumLabelFormats(const IndustrialKnob &knob) {
        return knob.d_ptr->m_lastMinimumLabelFormats;
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
