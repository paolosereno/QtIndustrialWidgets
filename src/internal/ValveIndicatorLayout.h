/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtGui/QPainterPath>
#include <QtGui/QTransform>

namespace QtIndustrialWidgets {
namespace internal {

/// Symbol of a ValveIndicator (the values of ValveIndicator::ValveType).
enum class ValveSymbol { Gate, Ball, Butterfly, Control };

// The symbol in unit coordinates, flow from left to right: the body spans x from -1 to 1 and the centre is (0, 0);
// y grows downwards, as on the screen. A vertical valve is the same symbol turned 90° clockwise (flow from the top
// down, actuator on the right).
inline constexpr double kValveBodyHalfHeight = 0.6;      ///< Half height of the body (two triangles tip to tip)
inline constexpr double kValveBallRadius = 0.3;          ///< Ball: the circle at the centre
inline constexpr double kValveButterflyHalfLength = 0.8; ///< Butterfly: the line through the centre (the disc)
inline constexpr double kValveStemTop = -1.1;            ///< Control: the stem from the centre up to the actuator
inline constexpr double kValveActuatorRadius = 0.6;      ///< Control: the diaphragm actuator, a dome on the stem
inline constexpr double kValveStubLength = 0.5;          ///< Pipe stubs on both sides of the body

// Pixels
inline constexpr double kValveMargin = 4.0;        ///< Free space between the drawing and the widget edge
inline constexpr double kValvePad = 2.0;           ///< Around the symbol: half the widest pen and its antialiasing
inline constexpr double kValveMinimumScale = 16.0; ///< Pixels per unit at the minimum size (a body 32 px long)
inline constexpr double kValveTagGap = 3.0;        ///< Between the symbol and the status tag
inline constexpr double kValveBarGap = 4.0;        ///< Between the status tag and the opening bar
inline constexpr double kValveBarHeight = 6.0;     ///< Height of the opening bar
inline constexpr double kValvePodGap = 3.0;        ///< Between the opening bar and the readout panel

/// Sizes of the texts of a ValveIndicator, measured by the widget with its fonts.
struct ValveTextSizes {
    double tagHeight{0.0};   ///< Line height of the status tag ("?", "FAULT"); its row is always reserved
    double tagWidth{0.0};    ///< Width of the widest status tag
    double podHeight{0.0};   ///< Height of the readout panel of the opening
    double podMinWidth{0.0}; ///< Width the readout panel needs for its state marks at the smallest font
};

/// Input of layoutValve().
struct ValveLayoutInput {
    double width{0.0};
    double height{0.0};
    ValveSymbol symbol{ValveSymbol::Gate};
    bool vertical{false};
    bool pipeStubs{false};
    bool openingShown{false}; ///< The opening bar and its readout
    ValveTextSizes text;
};

/// Geometry of a ValveIndicator, shared by the widget and its tests.
struct ValveLayout {
    QTransform transform; ///< Unit coordinates of the symbol to widget coordinates
    double scale{0.0};    ///< Pixels per unit
    QRectF symbol;        ///< The symbol (stubs, actuator, marks) in widget coordinates, without the pad
    QRectF tag;           ///< Row of the status tag
    QRectF bar;           ///< Opening bar (empty when the opening is not shown)
    QRectF pod;           ///< Readout panel of the opening (empty when the opening is not shown)
};

/// Bounding box of the symbol in unit coordinates, turned for a vertical valve.
QRectF valveSymbolExtent(ValveSymbol symbol, bool vertical, bool pipeStubs);

/// The body (unit coordinates, horizontal): two triangles tip to tip, and the circle of a ball valve.
QPainterPath valveBodyPath(ValveSymbol symbol);

/// The upstream half of the body (x < 0), filled for a valve in transit.
QPainterPath valveUpstreamHalf(ValveSymbol symbol);

/// The lines that tell the type apart (unit coordinates, horizontal), stroked: the disc of a butterfly valve, the
/// stem and the actuator of a control valve. Empty for a gate or a ball valve (the ball is part of the body).
QPainterPath valveMarkPath(ValveSymbol symbol);

/// The pipe stubs (unit coordinates, horizontal), stroked.
QPainterPath valveStubsPath();

/**
 * \brief Lays out a ValveIndicator of the given size.
 *
 * From the top, centred as a column: the symbol, the row of the status tag, then (when the opening is shown) the
 * opening bar and its readout panel, both across the width. The symbol takes the space left, keeping its
 * proportions. A size below minimumValveSize() gives a smaller symbol; no value is ever non-finite or negative.
 */
ValveLayout layoutValve(const ValveLayoutInput &in);

/// Widget size at which the symbol has \a scale pixels per unit and the texts fit (rounded up to whole pixels).
QSizeF valveSizeAtScale(const ValveLayoutInput &in, double scale);

/// Smallest widget size: valveSizeAtScale() at kValveMinimumScale.
QSizeF minimumValveSize(const ValveLayoutInput &in);

inline constexpr double kValvePreferredScale = 30.0; ///< Pixels per unit of sizeHint()

} // namespace internal
} // namespace QtIndustrialWidgets
