/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Geometry of IndustrialSlider (#180), shared by the widget and its tests. Internal: never installed.

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtGui/QPolygonF>

namespace QtIndustrialWidgets {
namespace internal {

inline constexpr double kSliderMargin = 4.0;         ///< Free space between the drawing and the widget edge
inline constexpr double kSliderPodGap = 4.0;         ///< Between two readout panels, and below them
inline constexpr double kSliderHandleDepth = 12.0;   ///< Size of the setpoint handle across the scale
inline constexpr double kSliderHandleWidth = 12.0;   ///< Size of the setpoint handle along the scale
inline constexpr double kSliderTrack = 6.0;          ///< Thickness of the track
inline constexpr double kSliderBarOverhang = 3.0;    ///< The feedback bar passes each side of the track by this
inline constexpr double kSliderBarWidth = 3.0;       ///< Size of the feedback bar along the scale
inline constexpr double kSliderBeyondSign = 4.0;     ///< Length of the sign past a feedback bar at an end of the scale
inline constexpr double kSliderZone = 3.0;           ///< Thickness of the zone band
inline constexpr double kSliderTickGap = 2.0;        ///< Between the zone band and the ticks
inline constexpr double kSliderMajorTick = 7.0;      ///< Length of a major tick
inline constexpr double kSliderMinorTick = 4.0;      ///< Length of a minor tick
inline constexpr double kSliderLabelGap = 3.0;       ///< Between the ticks and the scale labels
inline constexpr double kSliderMinimumScale = 60.0;  ///< Shortest scale of minimumSliderSize()

/// Sizes of the texts of an IndustrialSlider, measured by the widget with its fonts.
struct SliderTextSizes {
    double podHeight{0.0};      ///< Height of a readout panel (0 when the readouts are hidden)
    double podMinWidth{0.0};    ///< Width a readout panel needs for what it never leaves out, at the smallest font
    double labelHeight{0.0};    ///< Line height of the scale labels
    double labelWidth{0.0};     ///< Width of the widest scale label
    double firstLabelWidth{0.0}; ///< Width of the label of the minimum
    double lastLabelWidth{0.0};  ///< Width of the label of the maximum
    double labelMinGap{0.0};    ///< Free space kept between two drawn labels (scaleLabelMinGap())
};

/// Input of layoutSlider().
struct SliderLayoutInput {
    double width{0.0};
    double height{0.0};
    bool vertical{false};
    bool readoutVisible{true};          ///< The setpoint readout
    bool feedbackReadoutVisible{true};  ///< A place for the feedback readout, shown or not
    SliderTextSizes text;
    /// > 0: the long sides of the track at the centres of device pixels, so that its 1 px outline covers a whole pixel
    /// (in the dark theme the track is almost the background, and the outline is what shows it). The scale group moves
    /// by less than a device pixel towards the readouts, and the track is a whole number of device pixels thick, never
    /// thicker than kSliderTrack. 0: no snapping
    double devicePixelRatio{0.0};
};

/**
 * \brief Geometry of an IndustrialSlider.
 *
 * "Along" is the axis of the scale, "across" the other one. Horizontal: the minimum on the left; vertical: the
 * minimum at the bottom. Across the scale, in order from the readouts: the handle, the track, the zone band, the
 * ticks and the labels (horizontal: top to bottom; vertical: left to right).
 */
struct SliderLayout {
    bool vertical{false};
    QRectF setpointPod;   ///< Readout panel of the setpoint (empty when hidden)
    QRectF feedbackPod;   ///< Place of the feedback readout panel (empty when hidden)
    QRectF track;         ///< The track, from the minimum to the maximum of the scale
    double scaleMin{0.0}; ///< Along coordinate of the minimum (x horizontal, y vertical)
    double scaleMax{0.0}; ///< Along coordinate of the maximum
    double handleBase{0.0}; ///< Across coordinate of the back of the handle (its tip is at the track)
    double zoneNear{0.0};   ///< Across coordinate of the zone band, on the side of the track
    double tickNear{0.0};   ///< Across coordinate where the ticks start
    double labelNear{0.0};  ///< Across coordinate where the labels start (top horizontal, left vertical)

    /// Along coordinate of a fraction of the scale (0 = minimum, 1 = maximum).
    [[nodiscard]] double along(double fraction) const { return scaleMin + (scaleMax - scaleMin) * fraction; }
    /// Length of the scale.
    [[nodiscard]] double scaleLength() const { return vertical ? scaleMin - scaleMax : scaleMax - scaleMin; }
    /// Across coordinate of the side of the track towards the handle.
    [[nodiscard]] double trackNear() const { return vertical ? track.left() : track.top(); }
    /// Across coordinate of the side of the track away from the handle.
    [[nodiscard]] double trackFar() const { return vertical ? track.right() : track.bottom(); }
};

/**
 * \brief Lays out an IndustrialSlider of the given size.
 *
 * Horizontal: the readout panels in a row at the top, the setpoint on the left and the place of the feedback on the
 * right (the setpoint alone across the width when that place is off), then the scale group, centred in the height
 * left. Vertical: the panels stacked at the top across the width, then the scale group, centred in the width.
 * The scale leaves room at each end for half the handle; a vertical scale also leaves half a label line, since
 * its end labels are centred on the end ticks. A size below minimumSliderSize() gives a shorter scale; no value is
 * ever non-finite, and no rectangle has a negative size.
 */
SliderLayout layoutSlider(const SliderLayoutInput &in);

/**
 * \brief Smallest widget size at which nothing of layoutSlider() is clipped or overlapping (rounded up to whole
 * pixels): the panels at podMinWidth, a scale of kSliderMinimumScale at least and long enough for the labels of
 * both ends with labelMinGap between them, and the whole scale group across.
 */
QSizeF minimumSliderSize(const SliderLayoutInput &in);

/// Outline of the setpoint handle at along coordinate \a at: a pentagon whose tip touches the track.
QPolygonF sliderHandlePolygon(const SliderLayout &layout, double at);

/// The feedback bar at along coordinate \a at, across the track and kSliderBarOverhang past each side.
QRectF sliderFeedbackBar(const SliderLayout &layout, double at);

} // namespace internal
} // namespace QtIndustrialWidgets
