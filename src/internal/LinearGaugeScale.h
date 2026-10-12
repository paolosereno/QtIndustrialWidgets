/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

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

} // namespace internal
} // namespace QtIndustrialWidgets
