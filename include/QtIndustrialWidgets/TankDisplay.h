/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtIndustrialWidgets/ValueState.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <memory>

namespace QtIndustrialWidgets {

class TankDisplayPrivate;
namespace internal {
class TankDisplayTestAccess;
}

/**
 * \class TankDisplay
 * \brief Level indicator for tanks and silos: the vessel, its fill, a vertical scale and four limits (#152).
 *
 * The vessel is drawn as a vertical cylinder, a horizontal cylinder, a hopper (a cylinder with a conical bottom)
 * or a sphere, filled with a liquid (a flat fill) or a granular solid (a fill with a grain texture). The scale runs
 * from the bottom of the vessel (minimum()) to its top (maximum()).
 *
 * The fill height is linear in the value: the widget shows the value the application passes, on the scale the
 * application sets, and converts nothing. For a horizontal cylinder and a sphere the volume is not linear in the
 * height, so there a volume scale would put the surface at the wrong height: pass the level (e.g. in % or m) and
 * set the scale and unit() of the level.
 *
 * Value state (valueState(), #151): the last input is kept unclamped (rawValue()) and classified against the
 * range; a range change classifies it again. A new TankDisplay has no value: until the first setValue() rawValue()
 * and value() are NaN, valueState() is \c Invalid (the vessel hatched, the readout "---") and limitState() is
 * \c Unknown, so that a level never read is never shown as a level. This differs from the gauges, which start at
 * their minimum as \c Valid.
 * - \c Valid: the fill and the readout show the value.
 * - \c Overrange / \c Underrange (a finite input outside the range, or ±Inf): the fill stops at the end of the
 *   scale and value() is the bound. The readout shows the input itself, or ">max" / "<min" for ±Inf, after a
 *   triangle pointing up or down, painted as a shape so that the state is not told by colour alone.
 * - \c Invalid (NaN): the vessel is empty and hatched with diagonal lines in scaleColor(), the readout shows
 *   "---" and value() is NaN; the last valid value is never shown.
 *
 * The readout text is never below 7 pt: without room the unit goes first, then the number, never the triangle
 * or "---". minimumSizeHint() is computed from the font and the configuration so that the readout states, the
 * end labels of the scale and the limit tags fit.
 *
 * Limits: lowLowLimit() ≤ lowLimit() < highLimit() ≤ highHighLimit(), each disabled by NaN (the default). The
 * order applies only between the enabled limits; setLimits() and the single setters reject a change that breaks
 * it, or a limit of ±Inf, and change nothing: setLimits() returns false, a single setter (and so a property write)
 * emits a qWarning() that names the limit and the reason. limitState() is computed on the input (rawValue()), not on the
 * value shown, so +Inf gives \c HighHigh and -Inf \c LowLow when those limits are enabled:
 * - a high limit is entered when the input is >= the limit, and left when it is < limit − limitHysteresis();
 * - a low limit is entered when the input is <= the limit, and left when it is > limit + limitHysteresis();
 * - the most severe limit entered or not yet left is in force; \c Normal when none is;
 * - \c Unknown while the input is NaN, never \c Normal; the next finite input is evaluated afresh.
 *
 * Each enabled limit inside the range has a marker on the left of the vessel and a dashed line across it. The
 * marker of the limit in force is filled and carries a text tag (\c HH, \c H, \c L, \c LL), so the state is
 * never told by colour alone. A limit outside the range is evaluated all the same; it has no line and no marker,
 * except while it is in force: then its filled and tagged marker stands at that end of the scale, with a small
 * triangle past it pointing away from the scale, to show that the limit lies beyond it. Every limit state other
 * than \c Normal and \c Unknown is therefore visible in the drawing.
 *
 * TankDisplay only shows the limits and reports their crossings (limitStateChanged()). Alarm handling
 * (acknowledgement, latching, sound) stays in the application.
 *
 * Colours: the defaults suit a dark background. contentColor() has no meaning of its own: a liquid or a solid
 * is the same widget in any colour the application picks.
 *
 * \code
 * auto *tank = new TankDisplay(parent);
 * tank->setShape(TankDisplay::TankShape::Hopper);
 * tank->setContent(TankDisplay::Content::Granular);
 * tank->setRange(0.0, 100.0);
 * tank->setUnit(QStringLiteral("%"));
 * tank->setLimits(5.0, 15.0, 85.0, 95.0);
 * tank->setValue(72.5);
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT TankDisplay : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(TankShape shape READ shape WRITE setShape NOTIFY appearanceChanged)
    Q_PROPERTY(Content content READ content WRITE setContent NOTIFY appearanceChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double rawValue READ rawValue)
    Q_PROPERTY(QtIndustrialWidgets::ValueState valueState READ valueState NOTIFY valueStateChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(int majorTicks READ majorTicks WRITE setMajorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(int minorTicks READ minorTicks WRITE setMinorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(bool digitalDisplayVisible READ digitalDisplayVisible WRITE setDigitalDisplayVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool scaleVisible READ scaleVisible WRITE setScaleVisible NOTIFY appearanceChanged)
    Q_PROPERTY(double lowLowLimit READ lowLowLimit WRITE setLowLowLimit NOTIFY limitsChanged)
    Q_PROPERTY(double lowLimit READ lowLimit WRITE setLowLimit NOTIFY limitsChanged)
    Q_PROPERTY(double highLimit READ highLimit WRITE setHighLimit NOTIFY limitsChanged)
    Q_PROPERTY(double highHighLimit READ highHighLimit WRITE setHighHighLimit NOTIFY limitsChanged)
    Q_PROPERTY(double limitHysteresis READ limitHysteresis WRITE setLimitHysteresis NOTIFY limitsChanged)
    Q_PROPERTY(LimitState limitState READ limitState NOTIFY limitStateChanged)
    Q_PROPERTY(QColor contentColor READ contentColor WRITE setContentColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor vesselColor READ vesselColor WRITE setVesselColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor readoutColor READ readoutColor WRITE setReadoutColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor limitColor READ limitColor WRITE setLimitColor NOTIFY appearanceChanged)

public:
    /// Outline of the vessel.
    enum class TankShape {
        VerticalCylinder,   ///< Upright cylinder with dished heads
        HorizontalCylinder, ///< Cylinder lying on its side, with dished ends
        Hopper,             ///< Upright cylinder with a conical bottom ending in an outlet
        Sphere              ///< Spherical tank
    };
    Q_ENUM(TankShape)

    /// What the vessel holds: only the drawing of the fill changes.
    enum class Content {
        Liquid,  ///< A flat fill
        Granular ///< A fill with a grain texture (bulk solids)
    };
    Q_ENUM(Content)

    /// The limit in force (see the class description).
    enum class LimitState {
        Unknown,  ///< The input is NaN, or there was no input yet
        Normal,   ///< No limit in force
        Low,      ///< lowLimit() in force
        LowLow,   ///< lowLowLimit() in force
        High,     ///< highLimit() in force
        HighHigh  ///< highHighLimit() in force
    };
    Q_ENUM(LimitState)

    /**
     * \brief Constructs a TankDisplay: a vertical cylinder of liquid, 0–100 %, no limits enabled.
     * \param parent Optional parent widget.
     */
    explicit TankDisplay(QWidget *parent = nullptr);
    ~TankDisplay() override;

    /** \brief Returns the outline of the vessel. Default TankShape::VerticalCylinder. */
    [[nodiscard]] TankShape shape() const;
    /** \brief Returns what the vessel holds. Default Content::Liquid. */
    [[nodiscard]] Content content() const;
    /** \brief Returns the value at the bottom of the vessel. Default 0. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the value at the top of the vessel. Default 100. */
    [[nodiscard]] double maximum() const;
    /**
     * \brief Returns the indicated value: the last input clamped to [minimum, maximum] (±Inf gives the bound),
     * or NaN when the input is NaN (valueState() is then ValueState::Invalid).
     */
    [[nodiscard]] double value() const;
    /**
     * \brief Returns the last input as it was given, unclamped (#151); NaN before the first setValue().
     */
    [[nodiscard]] double rawValue() const;
    /** \brief Returns the state of the last input against the range (#151); \c Invalid before the first setValue(). */
    [[nodiscard]] QtIndustrialWidgets::ValueState valueState() const;
    /** \brief Returns the decimals of the readout and of the scale labels. Default 1. */
    [[nodiscard]] int precision() const;
    /** \brief Returns the unit shown after the readout value. Default "%". */
    [[nodiscard]] QString unit() const;
    /** \brief Returns the number of major scale intervals. Default 5. */
    [[nodiscard]] int majorTicks() const;
    /** \brief Returns the number of minor ticks between two major ticks. Default 4. */
    [[nodiscard]] int minorTicks() const;
    /** \brief Returns true if the readout above the vessel is shown. Default true. */
    [[nodiscard]] bool digitalDisplayVisible() const;
    /** \brief Returns true if the scale (ticks and labels) is shown. Default true. */
    [[nodiscard]] bool scaleVisible() const;

    /** \brief Returns the low-low limit; NaN when disabled (the default). */
    [[nodiscard]] double lowLowLimit() const;
    /** \brief Returns the low limit; NaN when disabled (the default). */
    [[nodiscard]] double lowLimit() const;
    /** \brief Returns the high limit; NaN when disabled (the default). */
    [[nodiscard]] double highLimit() const;
    /** \brief Returns the high-high limit; NaN when disabled (the default). */
    [[nodiscard]] double highHighLimit() const;
    /** \brief Returns the hysteresis applied when leaving a limit, in the units of the value. Default 0. */
    [[nodiscard]] double limitHysteresis() const;
    /** \brief Returns the limit in force (see the class description); \c Unknown before the first setValue(). */
    [[nodiscard]] LimitState limitState() const;

    /** \brief Returns the colour of the fill. Default QColor(52, 152, 219). */
    [[nodiscard]] QColor contentColor() const;
    /** \brief Returns the colour of the vessel wall; the empty inside is a dark shade of it. Default QColor(120, 134, 150). */
    [[nodiscard]] QColor vesselColor() const;
    /** \brief Returns the colour of the scale ticks and of the hatch of an invalid input. Default QColor(200, 208, 218). */
    [[nodiscard]] QColor scaleColor() const;
    /** \brief Returns the colour of the scale labels and of the limit tags. Default QColor(240, 244, 248). */
    [[nodiscard]] QColor textColor() const;
    /**
     * \brief Returns the colour of the readout text (#159). The readout panel is opaque and dark in every theme,
     * so this colour does not follow textColor(). Default QColor(240, 244, 248).
     */
    [[nodiscard]] QColor readoutColor() const;
    /** \brief Returns the colour of the limit markers and lines. Default QColor(231, 76, 60). */
    [[nodiscard]] QColor limitColor() const;

    /**
     * \brief Sets the four limits at once. NaN disables a limit.
     *
     * The enabled limits must satisfy lowLow ≤ low < high ≤ highHigh, and none may be ±Inf. Otherwise the call is
     * rejected: nothing changes, no signal is emitted, and it returns false. On success limitsChanged() is emitted
     * if a limit changed, the limit state is evaluated again on the last input, and it returns true.
     */
    bool setLimits(double lowLow, double low, double high, double highHigh);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the outline of the vessel. */
    void setShape(TankShape shape);
    /** \brief Sets what the vessel holds. */
    void setContent(Content content);
    /**
     * \brief Sets the input. value() is the input clamped within [minimum, maximum], and NaN for a NaN input;
     * rawValue() keeps the input and valueState() classifies it (#151). valueChanged() is emitted when value()
     * changes, then valueStateChanged() when the state changes, then limitStateChanged() when the limit state
     * changes.
     */
    void setValue(double value);
    /** \brief Sets the minimum, as setRange(min, maximum()). */
    void setMinimum(double min);
    /** \brief Sets the maximum, as setRange(minimum(), max). */
    void setMaximum(double max);
    /**
     * \brief Sets the range. Non-finite bounds and min >= max are ignored. The last input is classified again
     * against the new range (#151); the limit state does not depend on the range.
     */
    void setRange(double min, double max);
    /** \brief Sets the decimals of the readout and of the scale labels (negative values give 0). */
    void setPrecision(int precision);
    /** \brief Sets the unit shown after the readout value. */
    void setUnit(const QString &unit);
    /** \brief Sets the number of major scale intervals (at least 1). */
    void setMajorTicks(int count);
    /** \brief Sets the number of minor ticks between two major ticks (at least 0). */
    void setMinorTicks(int count);
    /** \brief Shows or hides the readout above the vessel. */
    void setDigitalDisplayVisible(bool visible);
    /** \brief Shows or hides the scale. The limit markers stay. */
    void setScaleVisible(bool visible);

    /**
     * \brief Sets the low-low limit, as setLimits() with the other three unchanged. A value that setLimits() would reject
     * changes nothing and emits a qWarning() that names the limit and the reason.
     */
    void setLowLowLimit(double limit);
    /**
     * \brief Sets the low limit, as setLimits() with the other three unchanged. A value that setLimits() would reject
     * changes nothing and emits a qWarning() that names the limit and the reason.
     */
    void setLowLimit(double limit);
    /**
     * \brief Sets the high limit, as setLimits() with the other three unchanged. A value that setLimits() would reject
     * changes nothing and emits a qWarning() that names the limit and the reason.
     */
    void setHighLimit(double limit);
    /**
     * \brief Sets the high-high limit, as setLimits() with the other three unchanged. A value that setLimits() would reject
     * changes nothing and emits a qWarning() that names the limit and the reason.
     */
    void setHighHighLimit(double limit);
    /**
     * \brief Sets the hysteresis applied when leaving a limit. A negative or non-finite value is ignored. The
     * limit state is evaluated again on the last input, keeping the limit in force if it is not left.
     */
    void setLimitHysteresis(double hysteresis);

    /** \brief Sets the colour of the fill. */
    void setContentColor(const QColor &color);
    /** \brief Sets the colour of the vessel wall. */
    void setVesselColor(const QColor &color);
    /** \brief Sets the colour of the scale ticks and of the hatch of an invalid input. */
    void setScaleColor(const QColor &color);
    /** \brief Sets the colour of the scale labels and of the limit tags. */
    void setTextColor(const QColor &color);
    /** \brief Sets the colour of the readout text (#159). */
    void setReadoutColor(const QColor &color);
    /** \brief Sets the colour of the limit markers and lines. */
    void setLimitColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when value() changes. */
    void valueChanged(double value);
    /** \brief Emitted when the range changes. */
    void rangeChanged(double min, double max);
    /** \brief Emitted when valueState() changes, after valueChanged() when both change. */
    void valueStateChanged(QtIndustrialWidgets::ValueState state);
    /** \brief Emitted when a limit or the hysteresis changes. */
    void limitsChanged();
    /** \brief Emitted when limitState() changes, after valueChanged() and valueStateChanged(). */
    void limitStateChanged(QtIndustrialWidgets::TankDisplay::LimitState state);
    /** \brief Emitted when a visual property changes. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<TankDisplayPrivate> d_ptr;
    Q_DECLARE_PRIVATE(TankDisplay)
    friend class internal::TankDisplayTestAccess;
};

} // namespace QtIndustrialWidgets
