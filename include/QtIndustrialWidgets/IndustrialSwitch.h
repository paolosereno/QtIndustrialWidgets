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

class IndustrialSwitchPrivate;

/**
 * \class IndustrialSwitch
 * \brief Toggle lever or rocker switch with an optional safety guard.
 *
 * IndustrialSwitch draws panel switches:
 * - Bat-handle toggle lever or curved rocker switch styles.
 * - 2 positions (labels OFF / ON by default) or 3 positions (OFF / AUTO / ON by default); the labels are set
 *   with setLabelOff(), setLabelCenter() and setLabelOn().
 * - Optional safety guard cover: while it is closed, a mouse click, Space or Return opens it instead of moving
 *   the switch, and no other key, drag or wheel step moves it; a click on the open guard closes it. Calls from
 *   the application (setPosition(), setChecked(), toggle()) are not blocked by the guard.
 * - Animated movement with QVariantAnimation (130 ms, QEasingCurve::OutBack; the guard 180 ms, OutCubic).
 * - Optional status LED, faceplate screws, and labels.
 *
 * \code
 * auto *sw = new IndustrialSwitch(parent);
 * sw->setSwitchType(IndustrialSwitch::SwitchType::ToggleLever);
 * sw->setHasSafetyGuard(true);
 * sw->setLabel("MAIN POWER");
 * connect(sw, &IndustrialSwitch::toggled, [](bool on){
 *     qDebug() << "Power switch:" << on;
 * });
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT IndustrialSwitch : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(SwitchType switchType READ switchType WRITE setSwitchType NOTIFY appearanceChanged)
    Q_PROPERTY(int positionCount READ positionCount WRITE setPositionCount NOTIFY appearanceChanged)
    Q_PROPERTY(int position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(bool hasSafetyGuard READ hasSafetyGuard WRITE setHasSafetyGuard NOTIFY appearanceChanged)
    Q_PROPERTY(bool isGuardOpen READ isGuardOpen WRITE setGuardOpen NOTIFY guardToggled)
    Q_PROPERTY(bool animated READ isAnimated WRITE setAnimated NOTIFY appearanceChanged)
    Q_PROPERTY(bool hasLed READ hasLed WRITE setHasLed NOTIFY appearanceChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY appearanceChanged)
    Q_PROPERTY(QString labelOff READ labelOff WRITE setLabelOff NOTIFY appearanceChanged)
    Q_PROPERTY(QString labelOn READ labelOn WRITE setLabelOn NOTIFY appearanceChanged)
    Q_PROPERTY(QString labelCenter READ labelCenter WRITE setLabelCenter NOTIFY appearanceChanged)
    Q_PROPERTY(QColor plateColor READ plateColor WRITE setPlateColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor leverColor READ leverColor WRITE setLeverColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor ledColor READ ledColor WRITE setLedColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor guardColor READ guardColor WRITE setGuardColor NOTIFY appearanceChanged)

public:
    /** \brief Mechanical actuator mechanism type. */
    enum class SwitchType {
        ToggleLever, ///< Industrial metal bat handle toggle lever
        Rocker       ///< Curved dual-throw rocker switch
    };
    Q_ENUM(SwitchType)

    /**
     * \brief Constructs a IndustrialSwitch with default 2-position toggle lever styling.
     * \param parent Optional parent widget.
     */
    explicit IndustrialSwitch(QWidget *parent = nullptr);
    ~IndustrialSwitch() override;

    /** \brief Returns the switch mechanical actuator style. */
    [[nodiscard]] SwitchType switchType() const;
    /** \brief Returns the number of discrete detent positions (2 or 3). */
    [[nodiscard]] int positionCount() const;
    /** \brief Returns the current indexed switch position (0 to positionCount-1). */
    [[nodiscard]] int position() const;
    /** \brief Returns true if the switch is in the active/ON position. */
    [[nodiscard]] bool isChecked() const;
    /** \brief Returns the installation orientation (Vertical or Horizontal). */
    [[nodiscard]] Qt::Orientation orientation() const;
    /** \brief Returns true if the flip-up safety guard is equipped. */
    [[nodiscard]] bool hasSafetyGuard() const;
    /** \brief Returns true if the flip-up safety guard is open. */
    [[nodiscard]] bool isGuardOpen() const;
    /** \brief Returns true if lever throw transitions are animated. */
    [[nodiscard]] bool isAnimated() const;
    /** \brief Returns true if the integrated status pilot LED is present. */
    [[nodiscard]] bool hasLed() const;
    /** \brief Returns the primary title caption text. */
    [[nodiscard]] QString label() const;
    /** \brief Returns the label text for the OFF/lower detent. */
    [[nodiscard]] QString labelOff() const;
    /** \brief Returns the label text for the ON/upper detent. */
    [[nodiscard]] QString labelOn() const;
    /** \brief Returns the label text for the center position (in 3-position mode). */
    [[nodiscard]] QString labelCenter() const;

    /** \brief Returns the mounting faceplate metal color. */
    [[nodiscard]] QColor plateColor() const;
    /** \brief Returns the lever handle or rocker body color. */
    [[nodiscard]] QColor leverColor() const;
    /** \brief Returns the pilot indicator LED lens color. */
    [[nodiscard]] QColor ledColor() const;
    /** \brief Returns the engraved lettering text color. */
    [[nodiscard]] QColor textColor() const;
    /** \brief Returns the safety guard cover color. */
    [[nodiscard]] QColor guardColor() const;

    /**
     * \brief Preferred size: 75x125 (vertical) or 125x75 (horizontal), never smaller than
     * minimumSizeHint(). With a safety guard and 3 positions, the smallest size with that aspect
     * at which the switch is drawn at its natural size and every text fits.
     */
    [[nodiscard]] QSize sizeHint() const override;
    /**
     * \brief Smallest size at which every label is drawn whole inside the plate and nothing covers
     * it, computed from the real metrics of the widget font, the safety guard, the number of
     * positions and the texts. At this size or larger, no label is clipped or covered (the switch
     * may be drawn smaller to make room). Below it, the LED and the guard still never cover a
     * label, but labels may be clipped or extend past the plate.
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the actuator mechanism style. */
    void setSwitchType(SwitchType type);
    /** \brief Sets the number of detent positions (2 or 3). */
    void setPositionCount(int count);
    /** \brief Sets the active detent position index. */
    void setPosition(int position);
    /** \brief Sets checked ON state (position = positionCount - 1). */
    void setChecked(bool checked);
    /** \brief Inverts or cycles the current switch position. */
    void toggle();
    /** \brief Sets orientation (Qt::Vertical or Qt::Horizontal). */
    void setOrientation(Qt::Orientation orientation);
    /** \brief Enables or disables the flip-up safety guard cover. */
    void setHasSafetyGuard(bool guard);
    /** \brief Opens or closes the flip-up safety guard. */
    void setGuardOpen(bool open);
    /** \brief Enables or disables the animation of the lever or rocker movement. */
    void setAnimated(bool animated);
    /** \brief Toggles the integrated status pilot LED. */
    void setHasLed(bool hasLed);
    /** \brief Sets the main panel header label text. */
    void setLabel(const QString &label);
    /** \brief Sets the label text for the OFF position. */
    void setLabelOff(const QString &label);
    /** \brief Sets the label text for the ON position. */
    void setLabelOn(const QString &label);
    /** \brief Sets the label text for the center neutral position. */
    void setLabelCenter(const QString &label);
    /** \brief Sets the faceplate metallic background color. */
    void setPlateColor(const QColor &color);
    /** \brief Sets the lever handle / rocker surface color. */
    void setLeverColor(const QColor &color);
    /** \brief Sets the pilot status LED color. */
    void setLedColor(const QColor &color);
    /** \brief Sets the engraved font lettering color. */
    void setTextColor(const QColor &color);
    /** \brief Sets the safety flip-cover color. */
    void setGuardColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when the switch detent position changes. */
    void positionChanged(int position);
    /** \brief Emitted when the binary checked state changes. */
    void toggled(bool checked);
    /** \brief Emitted when the safety guard is opened or closed. */
    void guardToggled(bool isOpen);
    /** \brief Emitted when visual styling properties change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void renderStaticBackground();
    void drawToggleLever(QPainter &painter, const QRectF &switchArea, double currentPos);
    void drawRocker(QPainter &painter, const QRectF &switchArea, double currentPos);
    void drawSafetyGuard(QPainter &painter, const QRectF &switchArea);
    void drawLed(QPainter &painter, const QPointF &center, double radius, bool active);
    void drawScrew(QPainter &painter, const QPointF &center, double radius);
    QRectF calculateSwitchRect() const;
    QRectF calculateGuardRect() const;

    std::unique_ptr<IndustrialSwitchPrivate> d_ptr;
    Q_DECLARE_PRIVATE(IndustrialSwitch)
};

} // namespace QtIndustrialWidgets
