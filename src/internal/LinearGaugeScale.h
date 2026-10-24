/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <functional>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \struct ScaleLabelSlot
 * \brief One drawn LinearGauge scale label: its major-tick index and its extent along the scale.
 *
 * Positions are measured along the scale from the minimum end (0) to the maximum end (scale
 * length): left to right for a horizontal gauge, bottom to top for a vertical one.
 */
struct ScaleLabelSlot {
    int index{0};      ///< Major-tick index, 0 .. majorTicks
    double start{0.0}; ///< Start of the label along the scale
    double end{0.0};   ///< End of the label along the scale
};

/**
 * \struct ScaleLabelLayout
 * \brief Result of layoutScaleLabels(): the step between drawn labels and the drawn labels.
 */
struct ScaleLabelLayout {
    int step{0};                        ///< Every step-th major label is drawn (0 when there are none)
    std::vector<ScaleLabelSlot> labels; ///< Drawn labels, in increasing index order
};

/// Minimum free space between two drawn labels: max(4 px, 0.5 x font height).
double scaleLabelMinGap(double fontHeight);

/**
 * \brief Chooses which major labels a LinearGauge scale draws, and where.
 *
 * \a extents holds the size of every major label along the scale (width for a horizontal
 * gauge, height for a vertical one), one per major tick: majorTicks + 1 values. Label i is
 * centred on tick s_i = scaleLength * i / majorTicks, except that label 0 is pushed to start
 * at or after 0 and the last label to end at or before \a scaleLength, so that both stay
 * inside the scale. Interior labels stay centred.
 *
 * The step is the smallest divisor k of majorTicks for which every pair of drawn labels
 * (indices 0, k, 2k, ..., majorTicks) keeps at least \a minGap between them, measured on the
 * pushed positions. If no divisor fits, the step is majorTicks: the first and last labels are
 * drawn anyway. With majorTicks = 0 the single label is drawn with step 1; with no extents
 * nothing is drawn. Non-finite inputs are treated as 0.
 */
ScaleLabelLayout layoutScaleLabels(const std::vector<double> &extents, double scaleLength, double minGap);

/// Label pixel size derived from the gauge geometry, before the size rule (#31): the height times
/// 0.04 (vertical) or 0.12 (horizontal), clamped to [8, 12] px.
int linearGaugeBaseLabelPixelSize(bool vertical, double height);

/**
 * \brief Label pixel size of a LinearGauge scale (#37 point 1, #51 rule R1b).
 *
 * The largest pixel size in [\a minimumPx, \a preferredPx] at which layoutScaleLabels() draws at
 * least every other label (step <= 2), with the extents and minimum gap of that size given by
 * \a extentsAt and \a minGapAt. If no size reaches step <= 2, \a preferredPx (the size would not
 * bring the labels back, so the text keeps its size).
 */
int chooseLabelPixelSize(int preferredPx, int minimumPx, double scaleLength,
                         const std::function<std::vector<double>(int)> &extentsAt,
                         const std::function<double(int)> &minGapAt);

/// Length of the LinearGauge scale (the tube, along its axis): the widget size less the padding at
/// each end (16 px; the bulb of a thermometer at the minimum end; 36 px above a vertical tube or
/// 55 px right of a horizontal one when the digital display is visible), at least 10 px.
double linearGaugeScaleLength(bool vertical, double width, double height, bool thermometer, bool displayVisible);

/// Axis-aligned rectangle (widget coordinates).
struct GaugeRect {
    double x{0.0};
    double y{0.0};
    double w{0.0};
    double h{0.0};
};

/// Rectangle of the LinearGauge digital display pod: right of the tube for a horizontal gauge,
/// on top for a vertical one (shared by the widget and its tests, #37 point 2).
GaugeRect linearGaugeDisplayRect(bool vertical, double width, double height);

} // namespace internal
} // namespace QtIndustrialWidgets
