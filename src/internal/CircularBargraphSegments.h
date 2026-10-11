/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/qglobal.h>
#include <vector>

namespace QtIndustrialWidgets {
namespace internal {

/**
 * \enum SegmentState
 * \brief Illumination state of one CircularBargraph LED segment.
 */
enum class SegmentState : quint8 {
    Off,  ///< Unlit (ghost color)
    Lit,  ///< Illuminated with the active zone color
    Peak  ///< Peak-hold indicator (peak color)
};

/**
 * \struct SegmentInput
 * \brief Inputs of the segment-state computation (a snapshot of the widget state).
 */
struct SegmentInput {
    double value{0.0};
    double minimum{0.0};
    double maximum{100.0};
    double peakValue{0.0};
    int segmentCount{24};
    bool dotMode{false};
    bool peakHoldEnabled{true};
};

/**
 * \brief Computes the state of every segment, index 0 being the one nearest the minimum.
 *
 * One mapping is used for everything: a value v lights the first n(v) = round(f * N)
 * segments, f being v normalised to [0, 1] and clamped.
 * - Bar: segments [0, n(value)) are Lit.
 * - Dot: only segment max(n(value) - 1, 0) is Lit.
 * - Peak (peak hold enabled): segment n(peakValue) - 1 is Peak, only if it lies above the
 *   topmost Lit segment; a Lit segment is never turned into Peak.
 *
 * Pure function: no widget, no painter. Returns \c segmentCount entries (none if
 * \c segmentCount < 1); all segments are Off if \c maximum <= \c minimum.
 */
std::vector<SegmentState> computeSegmentStates(const SegmentInput &in);

} // namespace internal
} // namespace QtIndustrialWidgets
