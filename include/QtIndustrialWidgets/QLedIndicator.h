#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtCore/QTimer>

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
    enum class LedShape {
        Circular,
        Rectangular
    };
    Q_ENUM(LedShape)

    explicit QLedIndicator(QWidget *parent = nullptr);
    explicit QLedIndicator(const QColor &onColor, QWidget *parent = nullptr);
    ~QLedIndicator() override = default;

    [[nodiscard]] bool isOn() const { return m_on; }
    [[nodiscard]] bool isBlinking() const { return m_blinking; }
    [[nodiscard]] int blinkRateMs() const { return m_blinkRateMs; }
    [[nodiscard]] QColor onColor() const { return m_onColor; }
    [[nodiscard]] QColor offColor() const { return m_offColor; }
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }
    [[nodiscard]] bool isBezelVisible() const { return m_bezelVisible; }
    [[nodiscard]] bool hasGlowEffect() const { return m_glowEffect; }
    [[nodiscard]] LedShape shape() const { return m_shape; }
    [[nodiscard]] QString labelText() const { return m_labelText; }
    [[nodiscard]] bool isClickable() const { return m_clickable; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setOn(bool on);
    void setOff();
    void toggle();
    void setBlinking(bool blinking);
    void setBlinkRateMs(int rateMs);
    void setOnColor(const QColor &color);
    void setOffColor(const QColor &color);
    void setBezelColor(const QColor &color);
    void setBezelVisible(bool visible);
    void setGlowEffect(bool glow);
    void setShape(LedShape shape);
    void setLabelText(const QString &text);
    void setClickable(bool clickable);

Q_SIGNALS:
    void stateChanged(bool on);
    void blinkingChanged(bool blinking);
    void clicked();
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
