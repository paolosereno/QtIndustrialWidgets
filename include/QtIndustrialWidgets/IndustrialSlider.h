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

class IndustrialSliderPrivate;
namespace internal {
class IndustrialSliderTestAccess;
class IndustrialSliderAccessible;
}

/**
 * \class IndustrialSlider
 * \brief Linear setpoint control with position feedback (#180): the commanded value and the actual position of an
 * actuator (a valve, a damper, a speed reference) on one scale.
 *
 * The setpoint is drawn as a handle with its tip on the track, the feedback as a bar across the track, on the
 * other side of the handle. Below the track (right of it when vertical) are the zone band, the ticks and the labels
 * of the scale. The readouts are above: the setpoint, tagged "SP", and the feedback, tagged "POS".
 *
 * Setpoint: setSetpoint() takes a finite value and clamps it to the range; NaN and ±Inf are rejected with a
 * qWarning() and the setpoint stays unchanged. setpointChanged() is emitted at every change of the setpoint,
 * whatever its source. A range change clamps the setpoint again.
 *
 * Feedback: a new IndustrialSlider has no feedback (hasFeedback() is false), and then shows no bar, no feedback
 * readout and no deviation. After setFeedback() it has one, until clearFeedback(). The input is kept unclamped
 * (rawFeedback()) and classified against the range (feedbackState(), #151):
 * - \c Valid: the bar and the readout show the value.
 * - \c Overrange / \c Underrange (a finite input outside the range, or ±Inf): the bar stops at that end of the
 *   scale, with a filled triangle past it pointing away from the scale; the readout shows the input itself, or
 *   ">max" / "<min" for ±Inf, after a triangle pointing up or down, so that the state is not told by colour alone.
 * - \c Invalid (NaN): no bar, the track is hatched with diagonal lines and the readout shows "---"; the last valid
 *   position is never shown.
 *
 * Deviation: with a feedback and deviationThreshold() above 0, deviationState() compares the setpoint with the
 * feedback input:
 * - \c Off: no feedback, or no threshold (the default). Nothing is drawn.
 * - \c Unknown: the feedback is NaN or ±Inf. It is never \c Normal: the readout of the feedback carries a "?" mark,
 *   and the track is hatched (NaN) or the bar stands at an end with its triangle (±Inf).
 * - \c Deviating: |setpoint − feedback| > deviationThreshold(), with a finite feedback outside the range taken as it
 *   is, not clamped. The track between the handle and the bar is cross-hatched, and the readout of the feedback
 *   carries a Δ mark, so that the deviation is not told by colour alone.
 * - \c Normal otherwise.
 *
 * The feedback lags the setpoint: an actuator takes time to follow a new setpoint, so every change of the setpoint
 * larger than the threshold shows \c Deviating until the actuator reaches it. IndustrialSlider does not filter
 * this. An application that uses deviationStateChanged() as an alarm must filter it itself, for example with a
 * delay that fits its actuator.
 *
 * Readouts: on an opaque dark panel in readoutColor() (#159). The text is never below 7 pt: without room the unit
 * goes first, then the number; the tag, the Δ or "?" mark, the triangle and "---" are never left out.
 * minimumSizeHint() is computed from the font and the configuration, so that those parts and the end labels of the
 * scale fit.
 *
 * Input (#182, the rules for controls in docs/widgets.md):
 * - A press never changes the setpoint. A left press on the handle as it is drawn starts a drag; a press anywhere
 *   else (the track, the scale, the readouts) starts none. The drag is relative: the setpoint moves by the distance
 *   the pointer moves along the scale from the press, so the handle never jumps to the pointer. Past an end (or a
 *   setpoint limit) the setpoint stays there until the pointer comes back.
 * - Keys, with focus: Left/Down lower the setpoint by step(), Right/Up raise it; PageDown/PageUp by pageStep();
 *   Home/End go to setpointLimitLow() / setpointLimitHigh().
 * - setpointChanged() is emitted at every change of the setpoint: during a drag, at each key step (auto-repeat
 *   too), from setSetpoint() and when setRange() clamps it. setpointCommitted() is emitted once at the end of an
 *   operator action that changed the setpoint: the release of a drag, or the release of a key that is not an
 *   auto-repeat; a drag or a held key also ends when the slider loses focus, is hidden or is made read-only.
 *   setSetpoint() never emits setpointCommitted(), so an application that writes committed setpoints to a device
 *   gets no write loop.
 * - Mouse wheel: off by default (setWheelEnabled()); while it is off, wheel events are ignored and reach the parent,
 *   so a scroll area around the slider scrolls. When it is on, it acts only while the slider has focus: each notch
 *   (120 units of QWheelEvent::angleDelta(), summed over the events of a high-resolution wheel or a touchpad) is one
 *   step() and commits at once. The part of a notch not yet reached is dropped when the slider loses focus.
 * - Read-only (setReadOnly()): mouse, keyboard and wheel input are ignored and reach the parent; the slider can
 *   still take focus and setSetpoint() still works. The slider is not greyed out: a padlock is drawn in the setpoint
 *   readout, before "SP", in readoutColor(), and it is never left out for lack of room.
 * - Setpoint limits (setSetpointLimits()): they bind the operator's input only. A drag, the keys and the wheel never
 *   take the setpoint outside [setpointLimitLow(), setpointLimitHigh()], and Home/End go to the limits; from a
 *   setpoint outside the limits, input towards them is applied and input that would take it further out is
 *   ignored. setSetpoint() is bound by the range only. On the scale the band of the ticks is hatched outside the
 *   limits, and each limit inside the range has a stop mark across the zone band and the ticks.
 *
 * Focus: the slider takes focus with Tab, Shift+Tab and a click (Qt::StrongFocus), also in read-only, and while it
 * has focus draws a solid 2 px ring in textColor() along the edge of the widget, in the margin kept free of the
 * drawing.
 *
 * Accessibility (#183): the accessible interface has the role QAccessible::Slider; its value interface carries the
 * setpoint, minimum(), maximum() and step(), and read-only is a state. The value text tells the setpoint, the
 * position and the deviation state in words, also when the readouts are hidden: "60.0 %, position 15.0 %,
 * deviation"; "position invalid" for a NaN feedback, "position over range 120.0 %" / "position under range" outside
 * the range, "deviation unknown"; no position without a feedback, no deviation when it is Off or Normal. Every change
 * of the setpoint, and of the range, sends a QAccessibleValueChangeEvent with the setpoint; setReadOnly() sends a
 * QAccessibleStateChangeEvent; a change of the feedback sends none. A setpoint set by assistive technology is
 * operator input: bound by the setpoint limits, ignored in read-only, committed with setpointCommitted(). The slider
 * has no accessible name of its own: set one with setAccessibleName() (or a QLabel buddy).
 *
 * Text size: the sizes of the labels and of the readouts follow the size of the widget font against the
 * application font (#51, rule (b)), as in LinearGauge: the layout makes room for them.
 *
 * Zones: warningThreshold() and errorThreshold() colour a band along the scale (normalColor() up to the first
 * threshold, warningColor() and errorColor() above). They describe the scale and change no state. NaN disables a
 * threshold, and both are disabled by default, so a new slider has no band.
 *
 * \code
 * auto *valve = new IndustrialSlider(parent);
 * valve->setRange(0.0, 100.0);
 * valve->setUnit(QStringLiteral("%"));
 * valve->setDeviationThreshold(5.0);
 * valve->setSetpoint(60.0);
 * valve->setFeedback(15.0); // the position read back from the actuator
 * valve->setAccessibleName(QStringLiteral("Damper position")); // #183: the slider has no name of its own
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT IndustrialSlider : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double setpoint READ setpoint WRITE setSetpoint NOTIFY setpointChanged)
    Q_PROPERTY(double feedback READ feedback WRITE setFeedback NOTIFY feedbackChanged)
    Q_PROPERTY(double rawFeedback READ rawFeedback)
    Q_PROPERTY(QtIndustrialWidgets::ValueState feedbackState READ feedbackState NOTIFY feedbackStateChanged)
    Q_PROPERTY(double deviationThreshold READ deviationThreshold WRITE setDeviationThreshold NOTIFY deviationThresholdChanged)
    Q_PROPERTY(DeviationState deviationState READ deviationState NOTIFY deviationStateChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(int majorTicks READ majorTicks WRITE setMajorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(int minorTicks READ minorTicks WRITE setMinorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(bool readoutVisible READ isReadoutVisible WRITE setReadoutVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool feedbackReadoutVisible READ isFeedbackReadoutVisible WRITE setFeedbackReadoutVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor handleColor READ handleColor WRITE setHandleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor feedbackColor READ feedbackColor WRITE setFeedbackColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor readoutColor READ readoutColor WRITE setReadoutColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(double step READ step WRITE setStep NOTIFY appearanceChanged)
    Q_PROPERTY(double pageStep READ pageStep WRITE setPageStep RESET resetPageStep NOTIFY appearanceChanged)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(bool wheelEnabled READ isWheelEnabled WRITE setWheelEnabled NOTIFY appearanceChanged)

public:
    /// Comparison of the setpoint with the feedback (see the class description).
    enum class DeviationState {
        Off,      ///< No feedback, or no deviation threshold: nothing is drawn
        Unknown,  ///< The feedback is NaN or ±Inf
        Normal,   ///< |setpoint − feedback| ≤ deviationThreshold()
        Deviating ///< |setpoint − feedback| > deviationThreshold()
    };
    Q_ENUM(DeviationState)

    /**
     * \brief Constructs a horizontal IndustrialSlider: 0–100 %, setpoint 0, no feedback, no deviation threshold.
     * \param parent Optional parent widget.
     */
    explicit IndustrialSlider(QWidget *parent = nullptr);
    ~IndustrialSlider() override;

    /** \brief Returns the orientation of the scale. Default Qt::Horizontal (vertical: the maximum at the top). */
    [[nodiscard]] Qt::Orientation orientation() const;
    /** \brief Returns the minimum of the scale. Default 0. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the maximum of the scale. Default 100. */
    [[nodiscard]] double maximum() const;
    /** \brief Returns the setpoint, always finite and within [minimum, maximum]. Default 0. */
    [[nodiscard]] double setpoint() const;
    /**
     * \brief Returns the feedback shown: the input clamped to [minimum, maximum] (±Inf gives the bound), or NaN
     * when the input is NaN or when there is no feedback.
     */
    [[nodiscard]] double feedback() const;
    /** \brief Returns the last feedback input as it was given, unclamped (#151); NaN when there is no feedback. */
    [[nodiscard]] double rawFeedback() const;
    /** \brief Returns the state of the feedback input against the range (#151); \c Invalid when there is none. */
    [[nodiscard]] QtIndustrialWidgets::ValueState feedbackState() const;
    /** \brief Returns true after setFeedback(), false before it and after clearFeedback(). */
    [[nodiscard]] bool hasFeedback() const;
    /** \brief Returns the deviation threshold, in the units of the scale; 0 (the default) turns the comparison off. */
    [[nodiscard]] double deviationThreshold() const;
    /** \brief Returns the comparison of the setpoint with the feedback (see the class description). */
    [[nodiscard]] DeviationState deviationState() const;
    /** \brief Returns the decimals of the readouts and of the scale labels. Default 1. */
    [[nodiscard]] int precision() const;
    /** \brief Returns the unit shown after the readout values. Default "%". */
    [[nodiscard]] QString unit() const;
    /** \brief Returns the number of major scale intervals. Default 5. */
    [[nodiscard]] int majorTicks() const;
    /** \brief Returns the number of minor ticks between two major ticks. Default 4. */
    [[nodiscard]] int minorTicks() const;
    /** \brief Returns the start of the warning zone; NaN when disabled (the default). */
    [[nodiscard]] double warningThreshold() const;
    /** \brief Returns the start of the error zone; NaN when disabled (the default). */
    [[nodiscard]] double errorThreshold() const;
    /** \brief Returns true if the readout panels are shown. Default true. */
    [[nodiscard]] bool isReadoutVisible() const;
    /**
     * \brief Returns true if the readout of the feedback has its panel next to (vertical: below) the setpoint.
     * Default true. The panel is drawn only while there is a feedback, but its place is kept, so the layout does not
     * move when a feedback comes or goes.
     */
    [[nodiscard]] bool isFeedbackReadoutVisible() const;

    /** \brief Returns the colour of the setpoint handle. Default QColor(0, 188, 212). */
    [[nodiscard]] QColor handleColor() const;
    /** \brief Returns the colour of the feedback bar. Default QColor(255, 159, 67). */
    [[nodiscard]] QColor feedbackColor() const;
    /**
     * \brief Returns the colour of the track. The hatching on the track (an invalid feedback, a deviation) is white or
     * black mixed into this colour up to a contrast of 4.5:1. Default QColor(30, 36, 45).
     */
    [[nodiscard]] QColor trackColor() const;
    /** \brief Returns the colour of the scale ticks. Default QColor(200, 208, 218). */
    [[nodiscard]] QColor scaleColor() const;
    /**
     * \brief Returns the colour of the scale labels. Default QColor(240, 244, 248).
     */
    [[nodiscard]] QColor textColor() const;
    /**
     * \brief Returns the colour of the readout texts and marks (#159). The readout panels are opaque and dark in
     * every theme, so this colour does not follow textColor(). Default QColor(240, 244, 248).
     */
    [[nodiscard]] QColor readoutColor() const;
    /** \brief Returns the colour of the zone band below the first threshold. Default QColor(46, 204, 113). */
    [[nodiscard]] QColor normalColor() const;
    /** \brief Returns the colour of the warning zone. Default QColor(241, 196, 15). */
    [[nodiscard]] QColor warningColor() const;
    /** \brief Returns the colour of the error zone. Default QColor(231, 76, 60). */
    [[nodiscard]] QColor errorColor() const;

    /** \brief Returns the step of the arrow keys and of a wheel notch. Default 1. */
    [[nodiscard]] double step() const;
    /** \brief Returns the step of PageUp / PageDown: the one set, or 5 × step() until one is set. */
    [[nodiscard]] double pageStep() const;
    /** \brief Returns true if operator input is ignored (see the class description). Default false. */
    [[nodiscard]] bool isReadOnly() const;
    /** \brief Returns true if the mouse wheel steps the setpoint while the slider has focus. Default false. */
    [[nodiscard]] bool isWheelEnabled() const;
    /** \brief Returns the low setpoint limit as read: the one set, within the range; without limits, minimum(). */
    [[nodiscard]] double setpointLimitLow() const;
    /** \brief Returns the high setpoint limit as read: the one set, within the range; without limits, maximum(). */
    [[nodiscard]] double setpointLimitHigh() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the orientation of the scale. */
    void setOrientation(Qt::Orientation orientation);
    /** \brief Sets the minimum, as setRange(min, maximum()). */
    void setMinimum(double min);
    /** \brief Sets the maximum, as setRange(minimum(), max). */
    void setMaximum(double max);
    /**
     * \brief Sets the range. Non-finite bounds and min >= max are ignored. The setpoint is clamped to the new range
     * (setpointChanged() when it moves), and the feedback input is classified again (#151).
     */
    void setRange(double min, double max);
    /**
     * \brief Sets the setpoint, clamped to [minimum, maximum]. NaN and ±Inf are rejected: the setpoint stays
     * unchanged and a qWarning() is printed. setpointChanged() is emitted when the setpoint changes.
     */
    void setSetpoint(double setpoint);
    /**
     * \brief Sets the feedback input: hasFeedback() becomes true. feedback() is the input clamped to the range,
     * and NaN for a NaN input; rawFeedback() keeps the input and feedbackState() classifies it (#151).
     * feedbackChanged() is emitted when feedback() changes, then feedbackStateChanged() when the state changes,
     * then deviationStateChanged() when the deviation state changes.
     */
    void setFeedback(double feedback);
    /**
     * \brief Removes the feedback: hasFeedback() becomes false, rawFeedback() and feedback() NaN, feedbackState()
     * \c Invalid and deviationState() \c Off. Nothing about the feedback is drawn.
     */
    void clearFeedback();
    /**
     * \brief Sets the deviation threshold. A value above 0 turns the comparison on; 0 or NaN turns it off (the
     * threshold is then 0). Negative values and ±Inf are ignored.
     */
    void setDeviationThreshold(double threshold);
    /** \brief Sets the decimals of the readouts and of the scale labels (negative values give 0). */
    void setPrecision(int precision);
    /** \brief Sets the unit shown after the readout values. */
    void setUnit(const QString &unit);
    /** \brief Sets the number of major scale intervals (at least 1). */
    void setMajorTicks(int count);
    /** \brief Sets the number of minor ticks between two major ticks (at least 0). */
    void setMinorTicks(int count);
    /** \brief Sets the start of the warning zone; NaN disables it, ±Inf is ignored. */
    void setWarningThreshold(double threshold);
    /** \brief Sets the start of the error zone; NaN disables it, ±Inf is ignored. */
    void setErrorThreshold(double threshold);
    /** \brief Shows or hides the readout panels. */
    void setReadoutVisible(bool visible);
    /** \brief Gives or takes away the place of the feedback readout. */
    void setFeedbackReadoutVisible(bool visible);

    /** \brief Sets the colour of the setpoint handle. */
    void setHandleColor(const QColor &color);
    /** \brief Sets the colour of the feedback bar. */
    void setFeedbackColor(const QColor &color);
    /** \brief Sets the colour of the track. */
    void setTrackColor(const QColor &color);
    /** \brief Sets the colour of the scale ticks. */
    void setScaleColor(const QColor &color);
    /** \brief Sets the colour of the scale labels. */
    void setTextColor(const QColor &color);
    /** \brief Sets the colour of the readout texts and marks (#159). */
    void setReadoutColor(const QColor &color);
    /** \brief Sets the colour of the zone band below the first threshold. */
    void setNormalColor(const QColor &color);
    /** \brief Sets the colour of the warning zone. */
    void setWarningColor(const QColor &color);
    /** \brief Sets the colour of the error zone. */
    void setErrorColor(const QColor &color);

    /** \brief Sets the step of the arrow keys and of a wheel notch. Non-finite values and values <= 0 are ignored. */
    void setStep(double step);
    /** \brief Sets the step of PageUp / PageDown. Non-finite values and values <= 0 are ignored. */
    void setPageStep(double step);
    /** \brief Makes pageStep() 5 × step() again. */
    void resetPageStep();
    /**
     * \brief Turns read-only on or off (see the class description). A drag or a held key running when read-only is
     * turned on ends, with setpointCommitted() if it changed the setpoint. Emits readOnlyChanged() if the state
     * changes.
     */
    void setReadOnly(bool readOnly);
    /** \brief Turns the mouse wheel on or off (see the class description). */
    void setWheelEnabled(bool enabled);
    /**
     * \brief Sets the setpoint limits, which bind the operator's input only (see the class description). Non-finite
     * values and low >= high are ignored. The limits are kept as set and read within the range; the setpoint is not
     * changed. Emits setpointLimitsChanged() if the limits read change.
     */
    void setSetpointLimits(double low, double high);
    /** \brief Removes the setpoint limits: they are the range again. Emits setpointLimitsChanged() if they change. */
    void clearSetpointLimits();

Q_SIGNALS:
    /** \brief Emitted when setpoint() changes, whatever the source. */
    void setpointChanged(double setpoint);
    /**
     * \brief Emitted once at the end of an operator action that changed the setpoint (see the class description);
     * never by setSetpoint().
     */
    void setpointCommitted(double setpoint);
    /** \brief Emitted when read-only is turned on or off. */
    void readOnlyChanged(bool readOnly);
    /** \brief Emitted when the setpoint limits as read change, also when a new range moves them. */
    void setpointLimitsChanged(double low, double high);
    /** \brief Emitted when the range changes. */
    void rangeChanged(double min, double max);
    /** \brief Emitted when feedback() changes. */
    void feedbackChanged(double feedback);
    /** \brief Emitted when feedbackState() changes, after feedbackChanged() when both change. */
    void feedbackStateChanged(QtIndustrialWidgets::ValueState state);
    /** \brief Emitted when deviationThreshold() changes. */
    void deviationThresholdChanged(double threshold);
    /** \brief Emitted when deviationState() changes, after the setpoint and feedback signals of the same change. */
    void deviationStateChanged(QtIndustrialWidgets::IndustrialSlider::DeviationState state);
    /** \brief Emitted when a zone threshold changes. */
    void thresholdChanged(double warning, double error);
    /** \brief Emitted when a visual property changes. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<IndustrialSliderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(IndustrialSlider)
    friend class internal::IndustrialSliderTestAccess;
    friend class internal::IndustrialSliderAccessible; // accessible interface (#183); defined in src/internal/, not installed
};

} // namespace QtIndustrialWidgets
