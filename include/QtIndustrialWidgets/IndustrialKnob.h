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

class IndustrialKnobPrivate;
namespace internal {
class IndustrialKnobTestAccess;
}

/**
 * \class IndustrialKnob
 * \brief Rotary knob for a continuous value or discrete steps.
 *
 * IndustrialKnob provides an interactive rotary control with a metallic face, a knurled grip ring,
 * continuous or discrete indexed stepping, mouse dragging, mouse wheel, and keyboard arrow controls.
 *
 * The background ticks and scale are cached in a pixmap at the device pixel ratio, so a rotation redraws only the
 * moving parts.
 *
 * Input (#182, the rules for controls in docs/widgets.md):
 * - A press never changes the value. A left press on the dial as it is drawn (within the radius of its scale
 *   geometry), on the scale, starts a drag; a press in the gap between the ends of the scale or outside the dial
 *   starts none (#175). The drag is relative: the value moves by the angle the pointer moves around the centre of
 *   the dial from the press. Past an end the value stays there until the pointer comes back on that end, the way
 *   it came or after a full turn (#175).
 * - Keys: the arrows move by step(), PageUp/PageDown by pageStep(), Home/End go to minimum() / maximum().
 * - valueChanged() is emitted at every change of the value: during a drag, at each key step (auto-repeat too),
 *   from setValue() and when setRange() clamps the value. valueCommitted() is emitted once at the end of an
 *   operator action that changed the value: the release of a drag, or the release of a key (for a held key, once,
 *   at the release that is not an auto-repeat); a drag or a held key also ends when the knob loses focus or is
 *   hidden. setValue() never emits valueCommitted(), so an application that writes committed values to a device
 *   and shows the value read back with setValue() gets no write loop.
 *
 * Non-finite input (#151, #182): the knob is an input control, so it has no value state (see
 * QtIndustrialWidgets::ValueState for the indicators). setValue() rejects NaN and +Inf / -Inf: the value stays
 * unchanged and a warning is printed (qWarning()). setRange() ignores non-finite bounds.
 *
 * Fonts: the texts use the family, weight and style of the widget font (setFont(), style sheets);
 * their sizes come from the widget geometry and do not follow the size of the widget font, because
 * the layout does not yet make room for larger text.
 *
 * \code
 * auto *knob = new IndustrialKnob(parent);
 * knob->setRange(0.0, 100.0);
 * knob->setValue(50.0);
 * knob->setStep(1.0);
 * knob->setUnit("%");
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT IndustrialKnob : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double step READ step WRITE setStep NOTIFY appearanceChanged)
    Q_PROPERTY(double pageStep READ pageStep WRITE setPageStep RESET resetPageStep NOTIFY appearanceChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(double startAngle READ startAngle WRITE setStartAngle NOTIFY appearanceChanged)
    Q_PROPERTY(double spanAngle READ spanAngle WRITE setSpanAngle NOTIFY appearanceChanged)
    Q_PROPERTY(int majorTicks READ majorTicks WRITE setMajorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(int minorTicks READ minorTicks WRITE setMinorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(KnobMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(int discreteSteps READ discreteSteps WRITE setDiscreteSteps NOTIFY appearanceChanged)
    Q_PROPERTY(bool trackVisible READ isTrackVisible WRITE setTrackVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool valueDisplayVisible READ isValueDisplayVisible WRITE setValueDisplayVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor knobColor READ knobColor WRITE setKnobColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor pointerColor READ pointerColor WRITE setPointerColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)

public:
    /** \brief Operating mode for the knob adjustment behavior. */
    enum class KnobMode {
        Continuous, ///< Continuous value, as a potentiometer
        Discrete    ///< Stepped indexed selector switch
    };
    Q_ENUM(KnobMode)

    /**
     * \brief Constructs a IndustrialKnob widget with default [0, 100] range.
     * \param parent Optional parent widget.
     */
    explicit IndustrialKnob(QWidget *parent = nullptr);
    ~IndustrialKnob() override;

    /** \brief Returns the minimum dial value. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the maximum dial value. */
    [[nodiscard]] double maximum() const;
    /** \brief Returns the current rotary value. */
    [[nodiscard]] double value() const;
    /** \brief Returns the increment step size. */
    [[nodiscard]] double step() const;
    /**
     * \brief Returns the large step of PageUp and PageDown (#182).
     *
     * 5 x step() until setPageStep() is called, and again after resetPageStep().
     */
    [[nodiscard]] double pageStep() const;
    /** \brief Returns the decimal display precision. */
    [[nodiscard]] int precision() const;
    /** \brief Returns the measurement unit label. */
    [[nodiscard]] QString unit() const;
    /** \brief Returns the start angle in degrees (default: -135°). */
    [[nodiscard]] double startAngle() const;
    /** \brief Returns the sweep span in degrees (default: 270°). */
    [[nodiscard]] double spanAngle() const;
    /** \brief Returns the major ticks count. */
    [[nodiscard]] int majorTicks() const;
    /** \brief Returns the minor subdivisions count. */
    [[nodiscard]] int minorTicks() const;
    /** \brief Returns the operating mode (Continuous or Discrete). */
    [[nodiscard]] KnobMode mode() const;
    /** \brief Returns the number of discrete detent positions. */
    [[nodiscard]] int discreteSteps() const;
    /** \brief Returns true if the active value arc track is visible. */
    [[nodiscard]] bool isTrackVisible() const;
    /** \brief Returns true if the center numeric readout is displayed. */
    [[nodiscard]] bool isValueDisplayVisible() const;

    /** \brief Returns the knob cap metal color. */
    [[nodiscard]] QColor knobColor() const;
    /** \brief Returns the pointer dot/line indicator color. */
    [[nodiscard]] QColor pointerColor() const;
    /** \brief Returns the scale ticks color. */
    [[nodiscard]] QColor scaleColor() const;
    /** \brief Returns the active progress arc track color. */
    [[nodiscard]] QColor trackColor() const;
    /** \brief Returns the label and numeric text color. */
    [[nodiscard]] QColor textColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    /**
     * \brief Returns the smallest supported size.
     *
     * It depends on the configuration: it is computed from the widget font, the range, the precision, the major
     * ticks, the discrete steps, the mode and the start and span angles, and it changes with them (updateGeometry()
     * is called). It also depends on the device pixel ratio, since the labels are measured on the pixels they draw:
     * when the window moves to a monitor of another ratio, updateGeometry() is called at its next repaint.
     * From this size up, the scale labels that are drawn stay inside the widget and the first and last
     * ones are drawn, except that where they lie on each other even at sizeHint() only the first one is. It is never
     * below 80 x 90 and never above sizeHint(): when the longest labels cannot keep both there, the result is
     * sizeHint(), in the same proportions at every size.
     *
     * Below this size the labels that are drawn still keep their gap, but the last one can be left out or a label can
     * reach the edge of the widget.
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /**
     * \brief Sets the value, clamped to the range (and to the nearest step in Discrete mode).
     *
     * Emits valueChanged() if the value changes, never valueCommitted(). NaN and +Inf / -Inf are rejected: the value
     * stays unchanged and a warning is printed (#182).
     */
    void setValue(double value);
    /** \brief Sets the minimum value. */
    void setMinimum(double min);
    /** \brief Sets the maximum value. */
    void setMaximum(double max);
    /** \brief Sets both minimum and maximum scale bounds. */
    void setRange(double min, double max);
    /** \brief Sets the stepping increment for wheel and arrow keys. */
    void setStep(double step);
    /** \brief Sets the large step of PageUp and PageDown; non-finite and non-positive values are ignored (#182). */
    void setPageStep(double step);
    /** \brief Makes pageStep() 5 x step() again (#182). */
    void resetPageStep();
    /** \brief Sets the decimal display precision. */
    void setPrecision(int precision);
    /** \brief Sets the measurement unit label. */
    void setUnit(const QString &unit);
    /** \brief Sets the start angle in degrees. */
    void setStartAngle(double angle);
    /** \brief Sets the sweep span angle in degrees. */
    void setSpanAngle(double span);
    /** \brief Sets the number of major graduation intervals. */
    void setMajorTicks(int count);
    /** \brief Sets the number of minor subdivisions between major intervals. */
    void setMinorTicks(int count);
    /** \brief Sets continuous or discrete operation mode. */
    void setMode(KnobMode mode);
    /** \brief Sets the number of discrete steps (when mode is Discrete). */
    void setDiscreteSteps(int steps);
    /** \brief Toggles visibility of the progress arc track. */
    void setTrackVisible(bool visible);
    /** \brief Toggles visibility of the center digital readout. */
    void setValueDisplayVisible(bool visible);

    /** \brief Sets the rotary knob body color. */
    void setKnobColor(const QColor &color);
    /** \brief Sets the pointer dot/line indicator color. */
    void setPointerColor(const QColor &color);
    /** \brief Sets the scale tick marks color. */
    void setScaleColor(const QColor &color);
    /** \brief Sets the progress arc track color. */
    void setTrackColor(const QColor &color);
    /** \brief Sets the text labels color. */
    void setTextColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted at every change of the value, from the operator or from the application. */
    void valueChanged(double value);
    /**
     * \brief Emitted once at the end of an operator action that changed the value: the release of a drag or of a
     * key, or the loss of focus or a hide during one (#182). Never emitted by setValue().
     */
    void valueCommitted(double value);
    /** \brief Emitted when the scale range changes. */
    void rangeChanged(double min, double max);
    /** \brief Emitted when the knob mode changes. */
    void modeChanged(KnobMode mode);
    /** \brief Emitted when visual styling properties change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void invalidateCache();
    void renderStaticScale(const QSize &size);
    [[nodiscard]] double valueToAngle(double val) const;
    [[nodiscard]] double angleToValue(double angle) const;
    void updateValueFromPoint(const QPointF &pos);

    std::unique_ptr<IndustrialKnobPrivate> d_ptr;
    Q_DECLARE_PRIVATE(IndustrialKnob)
    friend class internal::IndustrialKnobTestAccess; // test-only access; defined in src/internal/, not installed
};

} // namespace QtIndustrialWidgets
