// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "IndustrialSliderLayout.h"

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {
namespace internal {

namespace {

double finiteOrZero(double v)
{
    return std::isfinite(v) ? v : 0.0;
}

double nonNegative(double v)
{
    return std::max(0.0, finiteOrZero(v));
}

// Room at each end of a horizontal scale: half the handle, or half the feedback bar with its "beyond" sign
double endRoom()
{
    return std::max(kSliderHandleWidth * 0.5, kSliderBarWidth * 0.5 + 1.0 + kSliderBeyondSign);
}

// Room at each end of a vertical scale: also half a label line, since the end labels are centred on the end ticks
double verticalEndRoom(const SliderTextSizes &t)
{
    return std::max(endRoom(), nonNegative(t.labelHeight) * 0.5);
}

// Size of the scale group across the scale: handle, track, bar overhang, zone band, ticks, labels
double groupAcross(const SliderLayoutInput &in)
{
    const double labels = in.vertical ? nonNegative(in.text.labelWidth) : nonNegative(in.text.labelHeight);
    return kSliderHandleDepth + kSliderTrack + kSliderBarOverhang + kSliderZone + kSliderTickGap + kSliderMajorTick
           + kSliderLabelGap + labels;
}

// Height of the readout panels and of the gap below them (0 without readouts)
double podsBlock(const SliderLayoutInput &in)
{
    if (!in.readoutVisible) {
        return 0.0;
    }
    const double h = nonNegative(in.text.podHeight);
    if (in.vertical && in.feedbackReadoutVisible) {
        return 2.0 * h + kSliderPodGap + kSliderPodGap;
    }
    return h + kSliderPodGap;
}

} // namespace

SliderLayout layoutSlider(const SliderLayoutInput &in)
{
    SliderLayout l;
    l.vertical = in.vertical;
    const double w = nonNegative(in.width);
    const double h = nonNegative(in.height);
    const double m = kSliderMargin;
    const double podH = nonNegative(in.text.podHeight);
    const double inner = std::max(0.0, w - 2.0 * m);

    // Readout panels
    if (in.readoutVisible) {
        if (in.vertical) {
            l.setpointPod = QRectF(m, m, inner, podH);
            if (in.feedbackReadoutVisible) {
                l.feedbackPod = QRectF(m, m + podH + kSliderPodGap, inner, podH);
            }
        } else if (in.feedbackReadoutVisible) {
            const double half = std::max(0.0, (inner - kSliderPodGap) * 0.5);
            l.setpointPod = QRectF(m, m, half, podH);
            l.feedbackPod = QRectF(w - m - half, m, half, podH);
        } else {
            l.setpointPod = QRectF(m, m, inner, podH);
        }
    }
    const double top = m + podsBlock(in);
    const double group = groupAcross(in);

    // Across: the group centred in the space left (horizontal: below the panels; vertical: the width)
    const double acrossStart = in.vertical ? m : top;
    const double acrossEnd = in.vertical ? w - m : h - m;
    const double start = acrossStart + std::max(0.0, (acrossEnd - acrossStart - group) * 0.5);
    l.handleBase = start;
    const double trackNear = start + kSliderHandleDepth;
    l.zoneNear = trackNear + kSliderTrack + kSliderBarOverhang;
    l.tickNear = l.zoneNear + kSliderZone + kSliderTickGap;
    l.labelNear = l.tickNear + kSliderMajorTick + kSliderLabelGap;

    // Along
    if (in.vertical) {
        const double room = verticalEndRoom(in.text);
        l.scaleMax = top + room;
        l.scaleMin = std::max(l.scaleMax, h - m - room);
        l.track = QRectF(trackNear, l.scaleMax, kSliderTrack, l.scaleMin - l.scaleMax);
    } else {
        const double room = endRoom();
        l.scaleMin = m + room;
        l.scaleMax = std::max(l.scaleMin, w - m - room);
        l.track = QRectF(l.scaleMin, trackNear, l.scaleMax - l.scaleMin, kSliderTrack);
    }
    return l;
}

QSizeF minimumSliderSize(const SliderLayoutInput &in)
{
    const double m = kSliderMargin;
    const SliderTextSizes &t = in.text;
    const double podMin = nonNegative(t.podMinWidth);
    const double gap = nonNegative(t.labelMinGap);
    double width = 0.0;
    double height = 0.0;
    if (in.vertical) {
        const double scale = std::max(kSliderMinimumScale, nonNegative(t.labelHeight) + gap);
        width = 2.0 * m + std::max(groupAcross(in), in.readoutVisible ? podMin : 0.0);
        height = 2.0 * m + podsBlock(in) + 2.0 * verticalEndRoom(t) + scale;
    } else {
        const double scale =
            std::max(kSliderMinimumScale, nonNegative(t.firstLabelWidth) + nonNegative(t.lastLabelWidth) + gap);
        double pods = 0.0;
        if (in.readoutVisible) {
            pods = in.feedbackReadoutVisible ? 2.0 * podMin + kSliderPodGap : podMin;
        }
        width = 2.0 * m + std::max(2.0 * endRoom() + scale, pods);
        height = 2.0 * m + podsBlock(in) + groupAcross(in);
    }
    return QSizeF(std::ceil(width), std::ceil(height));
}

QPolygonF sliderHandlePolygon(const SliderLayout &layout, double at)
{
    const double half = kSliderHandleWidth * 0.5;
    const double shoulder = layout.handleBase + kSliderHandleDepth * 0.55;
    const double tip = layout.trackNear();
    QPolygonF p;
    if (layout.vertical) {
        p << QPointF(layout.handleBase, at - half) << QPointF(shoulder, at - half) << QPointF(tip, at)
          << QPointF(shoulder, at + half) << QPointF(layout.handleBase, at + half);
    } else {
        p << QPointF(at - half, layout.handleBase) << QPointF(at + half, layout.handleBase)
          << QPointF(at + half, shoulder) << QPointF(at, tip) << QPointF(at - half, shoulder);
    }
    return p;
}

QRectF sliderFeedbackBar(const SliderLayout &layout, double at)
{
    const double across = kSliderTrack + 2.0 * kSliderBarOverhang;
    const double near = layout.trackNear() - kSliderBarOverhang;
    if (layout.vertical) {
        return QRectF(near, at - kSliderBarWidth * 0.5, across, kSliderBarWidth);
    }
    return QRectF(at - kSliderBarWidth * 0.5, near, kSliderBarWidth, across);
}

} // namespace internal
} // namespace QtIndustrialWidgets
