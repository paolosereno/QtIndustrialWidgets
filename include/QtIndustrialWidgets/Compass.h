/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <memory>

namespace QtIndustrialWidgets {

class CompassPrivate;

/**
 * \class Compass
 * \brief Heading indicator: a rotating compass card or a fixed card with a rotating needle.
 *
 * Compass shows a 360° heading, as on marine and aeronautical heading indicators.
 *
 * Features include:
 * - Dual operating modes:
 *   - \b HeadingUp: Rotating compass card where current vessel/aircraft heading is at 12 o'clock (lubber line).
 *   - \b NorthUp: Fixed compass rose with a rotating two-colour needle (by default red north half, grey south
 *     half: needleColor(), needleTailColor()).
 * - Target Heading Bug: Adjustable course/autopilot heading index with interactive mouse drag.
 * - courseDeviation(): signed angular deviation of the heading from the target heading (computed, not drawn).
 * - 360° graduation marks with cardinal (N, E, S, W) and intercardinal (NE, SE, SW, NW) indices.
 * - 12 o'clock lubber reference marker.
 * - Central digital readout pod with 3-digit padded heading angle (e.g. `045°`).
 * - Circular card cached in a pixmap at the device pixel ratio: a heading change redraws only the moving parts.
 *
 * Fonts: the texts use the family, weight and style of the widget font (setFont(), style sheets);
 * their sizes come from the widget geometry and do not follow the size of the widget font, because
 * the layout does not yet make room for larger text.
 *
 * \code
 * auto *compass = new qiw::Compass(parent);
 * compass->setDisplayMode(qiw::Compass::DisplayMode::HeadingUp);
 * compass->setHeading(45.0);        // 045° North-East
 * compass->setTargetHeading(90.0);  // Target East (090°)
 * compass->setHeadingBugVisible(true);
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT Compass : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(double heading READ heading WRITE setHeading NOTIFY headingChanged)
    Q_PROPERTY(double targetHeading READ targetHeading WRITE setTargetHeading NOTIFY targetHeadingChanged)
    Q_PROPERTY(DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(bool headingBugVisible READ isHeadingBugVisible WRITE setHeadingBugVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool headingBugInteractive READ isHeadingBugInteractive WRITE setHeadingBugInteractive NOTIFY appearanceChanged)
    Q_PROPERTY(bool lubberLineVisible READ isLubberLineVisible WRITE setLubberLineVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool digitalReadoutVisible READ isDigitalReadoutVisible WRITE setDigitalReadoutVisible NOTIFY appearanceChanged)
    Q_PROPERTY(double courseDeviation READ courseDeviation NOTIFY headingChanged)

    Q_PROPERTY(QColor dialColor READ dialColor WRITE setDialColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor cardinalColor READ cardinalColor WRITE setCardinalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor needleColor READ needleColor WRITE setNeedleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor needleTailColor READ needleTailColor WRITE setNeedleTailColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bugColor READ bugColor WRITE setBugColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor lubberColor READ lubberColor WRITE setLubberColor NOTIFY appearanceChanged)

public:
    /** \brief Operating mode for the compass dial display. */
    enum class DisplayMode {
        HeadingUp, ///< Dial rotates so the current heading is at 12 o'clock (aircraft/marine compass card)
        NorthUp    ///< North is fixed at 12 o'clock, magnetic needle rotates to indicate heading
    };
    Q_ENUM(DisplayMode)

    /**
     * \brief Constructs a Compass widget with the default appearance.
     * \param parent Optional parent widget.
     */
    explicit Compass(QWidget *parent = nullptr);
    ~Compass() override;

    /** \brief Returns the current vessel/aircraft heading in degrees [0.0, 360.0). */
    [[nodiscard]] double heading() const;
    /** \brief Returns the target / course heading bug in degrees [0.0, 360.0). */
    [[nodiscard]] double targetHeading() const;
    /** \brief Returns the display mode (HeadingUp or NorthUp). */
    [[nodiscard]] DisplayMode displayMode() const;
    /** \brief Returns true if the target heading bug chevron is displayed. */
    [[nodiscard]] bool isHeadingBugVisible() const;
    /** \brief Returns true if mouse clicks/drags can reposition the heading bug. */
    [[nodiscard]] bool isHeadingBugInteractive() const;
    /** \brief Returns true if the 12 o'clock lubber reference marker is visible. */
    [[nodiscard]] bool isLubberLineVisible() const;
    /** \brief Returns true if the central digital heading readout pod is visible. */
    [[nodiscard]] bool isDigitalReadoutVisible() const;

    /**
     * \brief Computes the signed course deviation from the target heading in degrees [-180.0, +180.0].
     * A positive value indicates deviation to starboard (right), negative to port (left).
     */
    [[nodiscard]] double courseDeviation() const;

    /** \brief Normalizes any arbitrary angle in degrees to the [0.0, 360.0) range. */
    [[nodiscard]] static double normalizeDegrees(double deg);

    // Styling colors
    [[nodiscard]] QColor dialColor() const;
    [[nodiscard]] QColor bezelColor() const;
    [[nodiscard]] QColor textColor() const;
    [[nodiscard]] QColor cardinalColor() const;
    [[nodiscard]] QColor needleColor() const;
    [[nodiscard]] QColor needleTailColor() const;
    [[nodiscard]] QColor bugColor() const;
    [[nodiscard]] QColor lubberColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the current heading in degrees (automatically normalized to [0, 360)). */
    void setHeading(double heading);
    /** \brief Sets the target heading bug in degrees (automatically normalized to [0, 360)). */
    void setTargetHeading(double targetHeading);
    /** \brief Sets the compass display mode (HeadingUp or NorthUp). */
    void setDisplayMode(DisplayMode mode);
    /** \brief Toggles visibility of the target heading bug. */
    void setHeadingBugVisible(bool visible);
    /** \brief Toggles interactive adjustment of the heading bug via mouse click/drag. */
    void setHeadingBugInteractive(bool interactive);
    /** \brief Toggles visibility of the 12 o'clock lubber line. */
    void setLubberLineVisible(bool visible);
    /** \brief Toggles visibility of the central digital heading display. */
    void setDigitalReadoutVisible(bool visible);

    void setDialColor(const QColor &color);
    void setBezelColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setCardinalColor(const QColor &color);
    void setNeedleColor(const QColor &color);
    void setNeedleTailColor(const QColor &color);
    void setBugColor(const QColor &color);
    void setLubberColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when the current heading changes. */
    void headingChanged(double heading);
    /** \brief Emitted when the target heading bug changes. */
    void targetHeadingChanged(double targetHeading);
    /** \brief Emitted when the display mode changes. */
    void displayModeChanged(DisplayMode mode);
    /** \brief Emitted when appearance colors or visual configurations change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void invalidateCache();
    void renderCompassCard(const QSize &size);
    [[nodiscard]] double angleFromPoint(const QPointF &pos) const;

    std::unique_ptr<CompassPrivate> d_ptr;
    Q_DECLARE_PRIVATE(Compass)
};

} // namespace QtIndustrialWidgets
