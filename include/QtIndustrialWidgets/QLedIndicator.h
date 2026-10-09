/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtCore/QTimer>

/**
 * \class QLedIndicator
 * \brief Realistic industrial status LED pilot lamp widget.
 *
 * QLedIndicator renders an industrial pilot light or status LED with specular highlight reflections,
 * radial glow aura halos, blinking timer support, circular or rectangular shapes, optional metallic bezels,
 * and optional integrated text labels.
 *
 * \code
 * auto *led = new QLedIndicator(QColor(46, 204, 113), parent);
 * led->setLabelText("PUMP 1 RUNNING");
 * led->setBlinking(true);
 * led->setBlinkRateMs(400);
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT QLedIndicator : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(bool on READ isOn WRITE setOn NOTIFY stateChanged)
    Q_PROPERTY(bool blinking READ isBlinking WRITE setBlinking NOTIFY blinkingChanged)
    Q_PROPERTY(int blinkRateMs READ blinkRateMs WRITE setBlinkRateMs NOTIFY appearanceChanged)
    Q_PROPERTY(QColor onColor READ onColor WRITE setOnColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor offColor READ offColor WRITE setOffColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)
    Q_PROPERTY(bool bezelVisible READ isBezelVisible WRITE setBezelVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool glowEffect READ hasGlowEffect WRITE setGlowEffect NOTIFY appearanceChanged)
    Q_PROPERTY(LedShape shape READ shape WRITE setShape NOTIFY appearanceChanged)
    Q_PROPERTY(QString labelText READ labelText WRITE setLabelText NOTIFY appearanceChanged)
    Q_PROPERTY(bool clickable READ isClickable WRITE setClickable NOTIFY appearanceChanged)

public:
    /** \brief Geometric contour shape for the LED body. */
    enum class LedShape {
        Circular,   ///< Round lens pilot lamp
        Rectangular ///< Rectangular annunciator lamp
    };
    Q_ENUM(LedShape)

    /**
     * \brief Constructs a default green pilot LED.
     * \param parent Optional parent widget.
     */
    explicit QLedIndicator(QWidget *parent = nullptr);
    /**
     * \brief Constructs a pilot LED with a specific illuminated color.
     * \param onColor Color when illuminated.
     * \param parent Optional parent widget.
     */
    explicit QLedIndicator(const QColor &onColor, QWidget *parent = nullptr);
    ~QLedIndicator() override = default;

    /** \brief Returns true if the LED is currently powered ON. */
    [[nodiscard]] bool isOn() const { return m_on; }
    /** \brief Returns true if autonomous periodic blinking is active. */
    [[nodiscard]] bool isBlinking() const { return m_blinking; }
    /** \brief Returns the blinking toggle period in milliseconds. */
    [[nodiscard]] int blinkRateMs() const { return m_blinkRateMs; }
    /** \brief Returns the lit ON state color. */
    [[nodiscard]] QColor onColor() const { return m_onColor; }
    /** \brief Returns the unlit OFF state color. */
    [[nodiscard]] QColor offColor() const { return m_offColor; }
    /** \brief Returns the outer rim bezel color. */
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }
    /** \brief Returns true if the metallic bezel rim is displayed. */
    [[nodiscard]] bool isBezelVisible() const { return m_bezelVisible; }
    /** \brief Returns true if the radial glow aura effect is enabled. */
    [[nodiscard]] bool hasGlowEffect() const { return m_glowEffect; }
    /** \brief Returns the current lens shape. */
    [[nodiscard]] LedShape shape() const { return m_shape; }
    /** \brief Returns the status caption label text. */
    [[nodiscard]] QString labelText() const { return m_labelText; }
    /** \brief Returns true if user mouse clicks emit clicked() and toggle state. */
    [[nodiscard]] bool isClickable() const { return m_clickable; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Powers the LED on (true) or off (false). */
    void setOn(bool on);
    /** \brief Powers the LED off. */
    void setOff();
    /** \brief Inverts the current ON/OFF state. */
    void toggle();
    /** \brief Starts or stops automatic blinking. */
    void setBlinking(bool blinking);
    /** \brief Sets the blinking toggle period in milliseconds. */
    void setBlinkRateMs(int rateMs);
    /** \brief Sets the illuminated ON state color. */
    void setOnColor(const QColor &color);
    /** \brief Sets the unlit OFF state color. */
    void setOffColor(const QColor &color);
    /** \brief Sets the outer rim bezel color. */
    void setBezelColor(const QColor &color);
    /** \brief Toggles visibility of the outer metallic bezel rim. */
    void setBezelVisible(bool visible);
    /** \brief Toggles the radial light diffusion aura glow. */
    void setGlowEffect(bool glow);
    /** \brief Sets the LED contour shape. */
    void setShape(LedShape shape);
    /** \brief Sets the status caption label text. */
    void setLabelText(const QString &text);
    /** \brief Toggles interactive mouse click support. */
    void setClickable(bool clickable);

Q_SIGNALS:
    /** \brief Emitted when the ON/OFF state changes. */
    void stateChanged(bool on);
    /** \brief Emitted when blinking mode is toggled. */
    void blinkingChanged(bool blinking);
    /** \brief Emitted when the user clicks on the indicator (if clickable). */
    void clicked();
    /** \brief Emitted when visual appearance properties change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private Q_SLOTS:
    void onBlinkTimeout();

private:
    [[nodiscard]] QColor calculateDefaultOffColor(const QColor &onCol) const;

    bool m_on{true};
    bool m_blinking{false};
    bool m_blinkState{true};
    int m_blinkRateMs{500};

    QColor m_onColor{QColor(46, 204, 113)};          // Neon Emerald Green
    QColor m_offColor{QColor(15, 60, 35)};           // Dim green-tinted off state
    QColor m_bezelColor{QColor(60, 70, 85)};         // Metallic Bezel
    bool m_bezelVisible{true};
    bool m_glowEffect{true};
    LedShape m_shape{LedShape::Circular};
    QString m_labelText;
    bool m_clickable{false};

    QTimer m_blinkTimer;
};
