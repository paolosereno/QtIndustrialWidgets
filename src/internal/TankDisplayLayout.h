/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtGui/QColor>
#include <QtGui/QPainterPath>

namespace QtIndustrialWidgets {
namespace internal {

/// Outline of a TankDisplay vessel (the values of TankDisplay::TankShape).
enum class TankOutline { VerticalCylinder, HorizontalCylinder, Hopper, Sphere };

inline constexpr double kTankMargin = 4.0;          ///< Free space between the drawing and the widget edge
inline constexpr double kTankPodGap = 4.0;          ///< Between the readout panel and the top label room
inline constexpr double kTankTickGap = 3.0;         ///< Between the vessel and the scale ticks
inline constexpr double kTankMajorTick = 7.0;       ///< Length of a major tick
inline constexpr double kTankMinorTick = 4.0;       ///< Length of a minor tick
inline constexpr double kTankLabelGap = 3.0;        ///< Between the ticks and the scale labels
inline constexpr double kTankMarkerSize = 8.0;      ///< Width and height of a limit marker
inline constexpr double kTankMarkerGap = 2.0;       ///< Between a limit marker tip and the vessel
inline constexpr double kTankTagGap = 3.0;          ///< Between a limit tag and its marker
inline constexpr double kTankMarkerPen = 1.2;       ///< Width of the outline of a limit marker (Qt::BevelJoin)
inline constexpr double kTankBeyondGap = 2.0;       ///< Between a marker at the end of the scale and its "beyond" sign
inline constexpr double kTankBeyondSign = 4.0;      ///< Height of the "beyond" sign (its width is 3/4 of the marker)
inline constexpr double kTankMinimumVesselWidth = 24.0;
inline constexpr double kTankMinimumVesselHeight = 48.0;
inline constexpr double kTankUprightRatio = 0.6;    ///< Largest width / height of a vertical cylinder or a hopper
inline constexpr double kTankLyingRatio = 0.55;     ///< Largest height / width of a horizontal cylinder
inline constexpr double kHopperConeStart = 0.6;     ///< The cone of a hopper starts at this fraction of the height
inline constexpr double kHopperOutlet = 0.12;       ///< Half width of the hopper outlet, as a fraction of the width
inline constexpr double kTankHeadDepth = 0.15;      ///< Depth of a dished head, as a fraction of the vessel width

/// Sizes of the texts of a TankDisplay, measured by the widget with its fonts.
struct TankTextSizes {
    double podHeight{0.0};     ///< Height of the readout panel (0 when it is hidden)
    double podMinWidth{0.0};   ///< Width the readout panel needs for its state marks at the smallest font
    double labelHeight{0.0};   ///< Line height of the scale labels and of the limit tags
    double labelWidth{0.0};    ///< Width of the widest scale label (0 when the scale is hidden)
    double tagWidth{0.0};      ///< Width of the widest limit tag (0 when no limit has a marker)
};

/// Input of layoutTank().
struct TankLayoutInput {
    double width{0.0};
    double height{0.0};
    TankOutline outline{TankOutline::VerticalCylinder};
    bool displayVisible{true};
    bool scaleVisible{true};
    bool markersVisible{false}; ///< At least one limit is enabled
    TankTextSizes text;
};

/// Geometry of a TankDisplay, shared by the widget and its tests.
struct TankLayout {
    QRectF pod;           ///< Readout panel (empty when hidden)
    QRectF vessel;        ///< Bounding box of the vessel outline: the scale runs from its bottom to its top
    double tickX{0.0};    ///< Left end of the scale ticks
    double labelX{0.0};   ///< Left edge of the scale labels
    double markerTipX{0.0}; ///< x of the tip of the limit markers (they point right, at the vessel)
    double tagRight{0.0}; ///< Right edge of the limit tags
};

/**
 * \brief Lays out a TankDisplay of the given size.
 *
 * From the top: the readout panel across the width, then the vessel between a room above and below it: half a
 * label line (the end labels and tags are centred on the ends of the scale), and with the markers at least the
 * room of a marker at an end of the scale and of its "beyond" sign (tankEndRoom()). Across: the limit tags and markers, the
 * vessel, the scale ticks and labels; the group is centred. The vessel takes the space left, within the aspect
 * ratio of its outline, centred vertically. A size below minimumTankSize() gives a smaller vessel; no value is ever
 * non-finite or negative.
 */
TankLayout layoutTank(const TankLayoutInput &in);

/// Smallest widget size at which every part of layoutTank() keeps its minimum size (rounded up to whole pixels).
QSizeF minimumTankSize(const TankLayoutInput &in);

/// WCAG 2.x contrast ratio of two opaque colours.
double tankContrastRatio(const QColor &a, const QColor &b);

/**
 * \brief Colour of a hatch drawn on \a inside (the empty inside of TankDisplay, the track of IndustrialSlider): the
 * colour mixed toward white or black, whichever contrasts more with it, until 4.5:1 against it. White or black is
 * above 4.58:1 against any colour, so the ratio is always reached; the margin over the 3:1 of graphical objects
 * (WCAG 2.x, 1.4.11) covers the antialiasing of the thin hatch lines.
 */
QColor hatchColorOn(const QColor &inside);

/// Room above and below the vessel: see layoutTank().
double tankEndRoom(const TankLayoutInput &in);

/// y of a fraction (0 = minimum, 1 = maximum) of the scale: from the bottom of the vessel to its top.
double tankLevelY(const QRectF &vessel, double fraction);

/// Outline of the vessel in its bounding box.
QPainterPath tankOutlinePath(TankOutline outline, const QRectF &vessel);

} // namespace internal
} // namespace QtIndustrialWidgets
