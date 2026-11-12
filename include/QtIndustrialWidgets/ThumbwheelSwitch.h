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

class ThumbwheelSwitchPrivate;
namespace internal {
class ThumbwheelSwitchTestAccess;
class ThumbwheelSwitchAccessible;
}

/**
 * \class ThumbwheelSwitch
 * \brief Push-wheel numeric selector (#154): one digit drum per decade, each with a + and a − button, as the
 * thumbwheel switches of calibration racks and manual setpoint panels.
 *
 * Value: an exact number of digitCount() digits with a fixed decimal point, decimals() digits after it, so the
 * resolution is 10^-decimals(). The value is kept as an integer count of that resolution, so no rounding error builds
 * up between decades. setValue() rounds a finite value to the resolution (to the nearest count, halves away from
 * zero). Every drum shows its digit, leading zeros included. With the sign drum (setSignDrumVisible()) the value can
 * be negative: the digits are the magnitude and the sign drum shows + or −; 0 is always +, there is no −0.
 *
 * Input that is rejected, with a qWarning(), and leaves value() unchanged:
 * - NaN and ±Inf (#182): a control has no invalid state;
 * - a value the drums cannot show (more digits than digitCount(), or a negative value without the sign drum). The
 *   widget then shows an overflow mark that does not rely on colour, a dash on every drum and an "OVF" tag, until
 *   setValue() gives a value the drums can show (isOverflow(), overflowChanged()). A change of digitCount(),
 *   decimals() or of the sign drum after which the drums cannot show value() shows the same mark.
 *
 * Limits: minimum() and maximum() have the semantics of the value limits of the rules for controls (#182,
 * docs/widgets.md): they bind the operator's input, not setValue(). A value outside them that the drums can show (a
 * value read back from a device, for example) is kept and shown as it is, with a filled triangle in the tag column,
 * ▲ above the maximum and ▼ below the minimum, so that it does not rely on colour. The limits are kept as set and
 * read within what the drums can show.
 *
 * Buttons: the + and − buttons of each drum show whether their step would be applied: a filled arrow when it would,
 * an outlined arrow when it would be rejected. A step is rejected when the digit is already 9 (+) or 0 (−), since a
 * digit neither wraps around nor carries into the next decade, and when it would take the value outside the limits,
 * or further out from a value outside them; it is never rounded to the limit, which would change digits the
 * operator did not touch. On the sign drum, + and − select the sign; a change of sign is rejected for 0 and when it
 * would take the value outside the limits. In overflow and in read-only every step is shown as rejected.
 *
 * Operator input, under the rules for controls (#182, docs/widgets.md):
 * - Mouse: a press on a + or − button arms it; the release on the same button makes one step, so a press alone never
 *   changes anything. A press on a drum, or on one of its buttons, makes its decade the current one.
 * - Keys, with focus: Left and Right move the current decade (the sign drum first when shown), Home and End move it
 *   to the first and the last drum (#154: not to the limits, which would change digits the operator did not touch).
 *   Up and Down step the current decade; 0–9 type its digit and move to the next decade; on the sign drum, + and −
 *   (or Up and Down) select the sign. A digit typed or a sign selected is rejected as a step is. PageUp and PageDown
 *   are not used and reach the parent.
 * - Wheel: off by default (wheelEnabled); when on, it acts only with focus, on the decade under the pointer, which
 *   becomes the current one; one step per notch. Outside the drums and their buttons it reaches the parent.
 * - applyMode() OnConfirm (the default): the operator's input changes a pending value, shown by the drums with a
 *   dashed outline, while the applied value stays visible on a line under them ("SET?  now 0125.0 V"). Enter, the
 *   SET button drawn after the tag column, or applyPending() applies it: value() takes it, with valueChanged() and
 *   valueCommitted(). Escape or discardPending() discards it. A loss of focus never applies it. A value is pending
 *   only while it differs from value(). Unlike IndustrialKnob and IndustrialSlider, whose value moves continuously,
 *   a decade selector reaches a value through intermediate values that can be far from the one wanted (1999, 2999,
 *   2000, one decade at a time), and real thumbwheel panels load the value with a separate button.
 * - applyMode() Immediate: each step is applied at once (valueChanged()), and valueCommitted() is emitted once at the
 *   end of each operator action that changed the value: the release of a button, the release of a key (once for a key
 *   held down), each notch of the wheel, or a loss of focus, a hide or setReadOnly(true) during a key action.
 * - readOnly: mouse, keys and wheel input are ignored and reach the parent; a padlock is drawn under the drums. A
 *   pending value is discarded, and a key action running ends.
 * - Overflow: while the overflow mark is shown, every operator input that would change the value is rejected (the
 *   steps, the digits, the sign, the wheel, Enter), until setValue() gives a value the drums can show. Turning the
 *   mark on discards a pending value. Moving the current decade is still possible.
 *
 * setValue() changes the applied value and never emits valueCommitted(); a pending value stays, unless it is now
 * equal to value() or the overflow mark turns on. A change of digitCount(), decimals() or of the sign drum discards a
 * pending value, and so does a change of applyMode().
 *
 * Focus: the switch takes focus with Tab and a click (Qt::StrongFocus), also in read-only, and then draws a solid
 * 2 px ring in textColor() along its edge and a bar under the current decade.
 *
 * Accessibility (#183): the accessible interface has the role QAccessible::SpinBox; its value interface carries
 * value(), minimum(), maximum() and the resolution 10^-decimals() as the step, and read-only is a state. The value
 * text is the value as the drums show it, with the sign, the decimal point and the unit ("0125.0 V"); outside the
 * limits it adds "above maximum" or "below minimum" (the triangle of the tag column); while a value is pending it is
 * "pending 0130.0 V, set 0125.0 V"; in overflow it is "overflow". A value set by assistive technology is operator
 * input: rounded to the resolution and rejected as a typed digit is (outside the limits, further out from a value
 * outside them, a value the drums cannot show, in read-only, in overflow); ApplyMode::Immediate applies it and emits
 * valueCommitted(), ApplyMode::OnConfirm makes it the pending value. While a value is pending, the actions "Apply" and
 * "Discard" do what Enter and Escape do. Every change of value(), of the pending value, of the overflow mark or of the
 * limits sends one QAccessibleValueChangeEvent with value(); setReadOnly() sends a QAccessibleStateChangeEvent. Known
 * limitation: the current decade is not exposed to assistive technology. The switch has no accessible name of its
 * own: set one with setAccessibleName() (or a QLabel buddy).
 *
 * Size: the drums keep the size of their digit font (the widget font scaled as in the other widgets, #51, never
 * below 7 pt) and the group is centred in the widget. minimumSizeHint() is computed from the font, the logical DPI
 * and the configuration (digits, decimal point, sign drum, unit, tag column, the SET button and the line under the
 * drums in ApplyMode::OnConfirm or in read-only, the bar of the current decade), and follows a change of screen
 * (#177). Neither a pending value nor the focus changes it.
 *
 * \code
 * auto *wheel = new ThumbwheelSwitch(parent);
 * wheel->setDigitCount(5);
 * wheel->setDecimals(1);
 * wheel->setRange(0.0, 2500.0);
 * wheel->setUnit(QStringLiteral("V"));
 * wheel->setValue(125.0); // the drums show 0125.0
 * wheel->setAccessibleName(QStringLiteral("Test voltage")); // #183: the switch has no name of its own
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT ThumbwheelSwitch : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int digitCount READ digitCount WRITE setDigitCount NOTIFY appearanceChanged)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals NOTIFY appearanceChanged)
    Q_PROPERTY(bool signDrumVisible READ isSignDrumVisible WRITE setSignDrumVisible NOTIFY appearanceChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool overflow READ isOverflow NOTIFY overflowChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(QColor drumColor READ drumColor WRITE setDrumColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor digitColor READ digitColor WRITE setDigitColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor buttonColor READ buttonColor WRITE setButtonColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor pendingColor READ pendingColor WRITE setPendingColor NOTIFY appearanceChanged)
    Q_PROPERTY(ApplyMode applyMode READ applyMode WRITE setApplyMode NOTIFY appearanceChanged)
    Q_PROPERTY(bool hasPendingValue READ hasPendingValue NOTIFY pendingValueChanged)
    Q_PROPERTY(double pendingValue READ pendingValue NOTIFY pendingValueChanged)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)
    Q_PROPERTY(bool wheelEnabled READ isWheelEnabled WRITE setWheelEnabled NOTIFY appearanceChanged)

public:
    /// When the operator's input is applied (see the class description).
    enum class ApplyMode {
        OnConfirm, ///< The input changes a pending value; Enter or the SET button applies it (default)
        Immediate  ///< Each step is applied at once and committed at the end of the operator action
    };
    Q_ENUM(ApplyMode)

    /**
     * \brief Constructs a ThumbwheelSwitch of 4 digits, no decimals, no sign drum, limits 0 to 9999, value 0.
     * \param parent Optional parent widget.
     */
    explicit ThumbwheelSwitch(QWidget *parent = nullptr);
    ~ThumbwheelSwitch() override;

    /** \brief Returns the number of digit drums, 1 to 9. Default 4. */
    [[nodiscard]] int digitCount() const;
    /** \brief Returns the number of digits after the decimal point, 0 to digitCount() − 1. Default 0. */
    [[nodiscard]] int decimals() const;
    /** \brief Returns whether the sign drum is shown. Default false. */
    [[nodiscard]] bool isSignDrumVisible() const;
    /** \brief Returns the lower limit of the operator's input, as read within what the drums show. Default 0. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the upper limit of the operator's input, as read within what the drums show. Default 9999. */
    [[nodiscard]] double maximum() const;
    /** \brief Returns the value: always finite, a whole number of the resolution, shown by the drums unless isOverflow(). */
    [[nodiscard]] double value() const;
    /** \brief Returns true while the overflow mark is shown (see the class description). */
    [[nodiscard]] bool isOverflow() const;
    /** \brief Returns the unit drawn after the drums. Default empty. */
    [[nodiscard]] QString unit() const;
    /** \brief Returns the colour of the drum faces. */
    [[nodiscard]] QColor drumColor() const;
    /** \brief Returns the colour of the digits, of the sign and of the arrows of the buttons. */
    [[nodiscard]] QColor digitColor() const;
    /** \brief Returns the colour of the button faces. */
    [[nodiscard]] QColor buttonColor() const;
    /** \brief Returns the colour of the decimal point, of the unit and of the marks of the tag column. */
    [[nodiscard]] QColor textColor() const;
    /** \brief Returns the colour of the dashed outline of the drums while a value is pending. */
    [[nodiscard]] QColor pendingColor() const;
    /** \brief Returns when the operator's input is applied. Default ApplyMode::OnConfirm. */
    [[nodiscard]] ApplyMode applyMode() const;
    /** \brief Returns true while a value set by the operator waits to be applied (ApplyMode::OnConfirm only). */
    [[nodiscard]] bool hasPendingValue() const;
    /** \brief Returns the pending value, or value() when none is pending. */
    [[nodiscard]] double pendingValue() const;
    /** \brief Returns whether the operator's input is ignored. Default false. */
    [[nodiscard]] bool isReadOnly() const;
    /** \brief Returns whether the mouse wheel steps the decade under the pointer. Default false. */
    [[nodiscard]] bool isWheelEnabled() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /**
     * \brief Sets the value (see the class description). NaN, ±Inf and values the drums cannot show are rejected
     * with a qWarning(); a value the drums cannot show also turns the overflow mark on. Emits valueChanged() when
     * value() changes.
     */
    void setValue(double value);
    /**
     * \brief Sets the number of digit drums (clamped to 1–9). decimals() is reduced if needed. value() is kept;
     * if the drums can no longer show it, the overflow mark is shown.
     */
    void setDigitCount(int count);
    /**
     * \brief Sets the number of digits after the decimal point (clamped to 0 – digitCount() − 1). value() is rounded
     * to the new resolution, with valueChanged() if it changes.
     */
    void setDecimals(int decimals);
    /** \brief Shows or hides the sign drum. Without it a negative value cannot be shown: the overflow mark is shown. */
    void setSignDrumVisible(bool visible);
    /** \brief Sets the lower limit (see setRange()). */
    void setMinimum(double min);
    /** \brief Sets the upper limit (see setRange()). */
    void setMaximum(double max);
    /**
     * \brief Sets the limits of the operator's input. Non-finite values and min >= max are ignored. value() is not
     * changed. Emits rangeChanged() if the limits as read change.
     */
    void setRange(double min, double max);
    /** \brief Sets the unit drawn after the drums. */
    void setUnit(const QString &unit);
    /** \brief Sets the colour of the drum faces. */
    void setDrumColor(const QColor &color);
    /** \brief Sets the colour of the digits, of the sign and of the arrows of the buttons. */
    void setDigitColor(const QColor &color);
    /** \brief Sets the colour of the button faces. */
    void setButtonColor(const QColor &color);
    /** \brief Sets the colour of the decimal point, of the unit and of the marks of the tag column. */
    void setTextColor(const QColor &color);
    /** \brief Sets the colour of the dashed outline of the drums while a value is pending. */
    void setPendingColor(const QColor &color);
    /** \brief Sets when the operator's input is applied. A pending value is discarded. */
    void setApplyMode(ApplyMode mode);
    /**
     * \brief Applies the pending value, as Enter does: value() takes it, with valueChanged() and valueCommitted().
     * Does nothing when no value is pending.
     */
    void applyPending();
    /** \brief Discards the pending value, as Escape does. Does nothing when no value is pending. */
    void discardPending();
    /**
     * \brief Ignores the operator's input (true) or accepts it. Turning it on ends a key action running (with a commit
     * if it changed the value) and discards a pending value. setValue() still works.
     */
    void setReadOnly(bool readOnly);
    /** \brief Turns the mouse wheel on or off (see the class description). */
    void setWheelEnabled(bool enabled);

Q_SIGNALS:
    /** \brief Emitted when value() changes, whatever the source. */
    void valueChanged(double value);
    /** \brief Emitted when the limits as read change, also when a new number of drums moves them. */
    void rangeChanged(double min, double max);
    /** \brief Emitted when the overflow mark is turned on or off. */
    void overflowChanged(bool overflow);
    /**
     * \brief Emitted once at the end of an operator action that changed value(): Enter, the SET button or
     * applyPending() in ApplyMode::OnConfirm; see the class description for ApplyMode::Immediate. Never by setValue().
     */
    void valueCommitted(double value);
    /** \brief Emitted when pendingValue() or hasPendingValue() changes. */
    void pendingValueChanged(double pendingValue);
    /** \brief Emitted when readOnly changes. */
    void readOnlyChanged(bool readOnly);
    /** \brief Emitted when a visual property changes. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ThumbwheelSwitchPrivate> d_ptr;
    Q_DECLARE_PRIVATE(ThumbwheelSwitch)
    friend class internal::ThumbwheelSwitchTestAccess;
    friend class internal::ThumbwheelSwitchAccessible; // accessible interface (#183); defined in src/internal/, not installed
};

} // namespace QtIndustrialWidgets
