/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

class QVariantAnimation;

/**
 * \class QIndustrialSwitch
 * \brief Heavy-duty industrial toggle switch and rocker control with safety guard.
 *
 * QIndustrialSwitch emulates rugged panel switches including:
 * - Bat-handle toggle lever or curved rocker switch styles.
 * - 2-position (ON/OFF) or 3-position (ON/OFF/ON or AUTO/OFF/MANUAL).
 * - Optional spring-loaded missile safety guard cover that must be flipped open to throw the switch.
 * - Smooth physics-inspired spring throw animations using QVariantAnimation.
 * - Integrated miniature status LED lamp, metallic faceplate screws, and engraved labels.
 *
 * \code
 * auto *sw = new QIndustrialSwitch(parent);
 * sw->setSwitchType(QIndustrialSwitch::SwitchType::ToggleLever);
 * sw->setHasSafetyGuard(true);
 * sw->setLabel("MAIN POWER");
 * connect(sw, &QIndustrialSwitch::toggled, [](bool on){
 *     qDebug() << "Power switch:" << on;
 * });
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT QIndustrialSwitch : public QWidget
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
     * \brief Constructs a QIndustrialSwitch with default 2-position toggle lever styling.
     * \param parent Optional parent widget.
     */
    explicit QIndustrialSwitch(QWidget *parent = nullptr);
    ~QIndustrialSwitch() override;

    /** \brief Returns the switch mechanical actuator style. */
    [[nodiscard]] SwitchType switchType() const { return m_switchType; }
    /** \brief Returns the number of discrete detent positions (2 or 3). */
    [[nodiscard]] int positionCount() const { return m_positionCount; }
    /** \brief Returns the current indexed switch position (0 to positionCount-1). */
    [[nodiscard]] int position() const { return m_position; }
    /** \brief Returns true if the switch is in the active/ON position. */
    [[nodiscard]] bool isChecked() const { return m_position == (m_positionCount - 1); }
    /** \brief Returns the installation orientation (Vertical or Horizontal). */
    [[nodiscard]] Qt::Orientation orientation() const { return m_orientation; }
    /** \brief Returns true if the flip-up safety guard is equipped. */
    [[nodiscard]] bool hasSafetyGuard() const { return m_hasSafetyGuard; }
    /** \brief Returns true if the flip-up safety guard is open. */
    [[nodiscard]] bool isGuardOpen() const { return m_isGuardOpen; }
    /** \brief Returns true if lever throw transitions are animated. */
    [[nodiscard]] bool isAnimated() const { return m_animated; }
    /** \brief Returns true if the integrated status pilot LED is present. */
    [[nodiscard]] bool hasLed() const { return m_hasLed; }
    /** \brief Returns the primary title caption text. */
    [[nodiscard]] QString label() const { return m_label; }
    /** \brief Returns the label text for the OFF/lower detent. */
    [[nodiscard]] QString labelOff() const { return m_labelOff; }
    /** \brief Returns the label text for the ON/upper detent. */
    [[nodiscard]] QString labelOn() const { return m_labelOn; }
    /** \brief Returns the label text for the center position (in 3-position mode). */
    [[nodiscard]] QString labelCenter() const { return m_labelCenter; }

    /** \brief Returns the mounting faceplate metal color. */
    [[nodiscard]] QColor plateColor() const { return m_plateColor; }
    /** \brief Returns the lever handle or rocker body color. */
    [[nodiscard]] QColor leverColor() const { return m_leverColor; }
    /** \brief Returns the pilot indicator LED lens color. */
    [[nodiscard]] QColor ledColor() const { return m_ledColor; }
    /** \brief Returns the engraved lettering text color. */
    [[nodiscard]] QColor textColor() const { return m_textColor; }
    /** \brief Returns the safety guard cover color. */
    [[nodiscard]] QColor guardColor() const { return m_guardColor; }

    [[nodiscard]] QSize sizeHint() const override;
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
    /** \brief Enables or disables smooth throw animation. */
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

    SwitchType m_switchType = SwitchType::ToggleLever;
    int m_positionCount = 2; // 2 or 3
    int m_position = 0;      // 0, 1 (or 2 if 3-pos)
    double m_currentPos = 0.0; // for animation: 0.0 to 1.0 (or 2.0)
    Qt::Orientation m_orientation = Qt::Vertical;

    bool m_hasSafetyGuard = false;
    bool m_isGuardOpen = false;
    double m_guardOpenFactor = 0.0; // 0.0 = closed, 1.0 = fully open
    bool m_animated = true;
    bool m_hasLed = true;

    QString m_label;
    QString m_labelOff = QStringLiteral("OFF");
    QString m_labelOn = QStringLiteral("ON");
    QString m_labelCenter = QStringLiteral("AUTO");

    QColor m_plateColor = QColor(42, 45, 52);
    QColor m_leverColor = QColor(220, 225, 230);
    QColor m_ledColor = QColor(46, 204, 113);
    QColor m_textColor = QColor(200, 205, 215);
    QColor m_guardColor = QColor(220, 53, 69); // Industrial crimson safety red

    QVariantAnimation *m_switchAnim = nullptr;
    QVariantAnimation *m_guardAnim = nullptr;

    QPixmap m_cachedBackground;
    bool m_cacheValid = false;
    bool m_isDragging = false;
};
