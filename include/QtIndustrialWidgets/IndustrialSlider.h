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

Q_SIGNALS:
    /** \brief Emitted when setpoint() changes, whatever the source. */
    void setpointChanged(double setpoint);
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

private:
    std::unique_ptr<IndustrialSliderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(IndustrialSlider)
    friend class internal::IndustrialSliderTestAccess;
};

} // namespace QtIndustrialWidgets
