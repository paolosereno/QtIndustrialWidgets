/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QFont>

namespace QtIndustrialWidgets {
namespace internal {

/// Font of the IndustrialSwitch title: bold, 85 % of the widget font (at least 7 pt).
QFont switchTitleFont(const QFont &base);
/// Font of the IndustrialSwitch position labels: bold, 80 % of the widget font (at least 6.5 pt).
QFont switchLabelFont(const QFont &base);

/// Radius of the four corner screws of the plate: 4 % of the smaller widget side, 3..5 px.
double switchScrewRadius(const QSizeF &size);

constexpr double kSwitchLedRadius = 4.5;
constexpr double kSwitchLedGlow = kSwitchLedRadius * 2.5; ///< Radius of the glow of a lit LED
constexpr double kClosedGuardOpacity = 0.9;

/// Input of layoutSwitch().
struct SwitchLayoutInput {
    QSizeF size;
    Qt::Orientation orientation{Qt::Vertical};
    int positions{2};
    bool guard{false}; ///< hasSafetyGuard(): the layout depends on the presence of the guard, not on its state
    QFont font;        ///< widget font
    QString title;
    QString on;
    QString off;
    QString center;
};

/// Geometry of an IndustrialSwitch (logical pixels), shared by painting, mouse handling and tests.
struct SwitchLayout {
    QRectF plate;      ///< Panel plate
    QRectF switchRect; ///< Lever socket or rocker
    QRectF guard;      ///< Safety guard cover (equal to switchRect without a guard)
    QRectF guardOuter; ///< Guard plus its drop shadow, lift tab and hinge brackets
    QRectF titleRect, onRect, offRect, centerRect; ///< Rectangles the texts are drawn into (aligned side shifted by the ink overhang)
    Qt::Alignment onAlign, offAlign, centerAlign;
    QRectF titleBox, onBox, offBox, centerBox;     ///< Boxes of the texts, advance and glyph ink (empty for an empty text)
    QPointF led;      ///< Centre of the status LED, next to the ON label
    QRectF openGuard; ///< The open guard, seen edge-on in free plate space
    double scale{1.0}; ///< Factor applied to the natural switch size to make room (1 = not scaled down)
    bool fits{false};  ///< All the constraints of layoutSwitch() are met at this scale, title included
};

/**
 * \brief Lays out an IndustrialSwitch so that no text is covered or clipped (#44).
 *
 * The switch starts at its natural size and position (the formulas used before #44). The position
 * labels are placed around the guard (around the switch without a guard): the closed cover, its
 * shadow, lift tab and hinge brackets never overlap a label. The LED sits next to the ON label: to
 * its left (vertical) or above it (horizontal), with its glow clear of every text. The open guard
 * is drawn edge-on in free plate space: left of the switch (vertical) or below it and below the
 * centre label (horizontal), between 3 px and 17 % of the guard thick.
 *
 * If a position label would not fit in its rectangle, the LED glow would reach the title or leave
 * the plate, the guard would reach the title, or the open guard or a label would leave the plate,
 * the switch is scaled down about its centre in steps of 1 %, down to 30 % (then fits is false).
 * The title must also fit in its rectangle, clear of the corner screws, and (vertical layout) the
 * LED glow must stay inside the plate, for fits to be true; scaling the switch does not change
 * those two, so they never make the switch smaller.
 */
SwitchLayout layoutSwitch(const SwitchLayoutInput &in);

/**
 * \brief Smallest size, with the aspect ratio of \a base and not smaller than it, at which the
 * layout fits (the switch may be scaled down); \a base if none up to 4x. Text sizes come from
 * the real metrics of \a in.font, glyph ink included.
 */
QSize switchMinimumSize(SwitchLayoutInput in, const QSize &base);

/**
 * \brief Smallest size, with the aspect ratio of \a base and not smaller than it, at which the
 * switch keeps its natural size (scale 1) and the layout fits; \a base if none up to 4x.
 */
QSize switchSizeHint(SwitchLayoutInput in, const QSize &base);

} // namespace internal
} // namespace QtIndustrialWidgets
