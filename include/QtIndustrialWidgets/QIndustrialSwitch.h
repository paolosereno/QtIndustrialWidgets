#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

class QVariantAnimation;

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
    enum class SwitchType {
        ToggleLever,
        Rocker
    };
    Q_ENUM(SwitchType)

    explicit QIndustrialSwitch(QWidget *parent = nullptr);
    ~QIndustrialSwitch() override;

    [[nodiscard]] SwitchType switchType() const { return m_switchType; }
    [[nodiscard]] int positionCount() const { return m_positionCount; }
    [[nodiscard]] int position() const { return m_position; }
    [[nodiscard]] bool isChecked() const { return m_position == (m_positionCount - 1); }
    [[nodiscard]] Qt::Orientation orientation() const { return m_orientation; }
    [[nodiscard]] bool hasSafetyGuard() const { return m_hasSafetyGuard; }
    [[nodiscard]] bool isGuardOpen() const { return m_isGuardOpen; }
    [[nodiscard]] bool isAnimated() const { return m_animated; }
    [[nodiscard]] bool hasLed() const { return m_hasLed; }
    [[nodiscard]] QString label() const { return m_label; }
    [[nodiscard]] QString labelOff() const { return m_labelOff; }
    [[nodiscard]] QString labelOn() const { return m_labelOn; }
    [[nodiscard]] QString labelCenter() const { return m_labelCenter; }

    [[nodiscard]] QColor plateColor() const { return m_plateColor; }
    [[nodiscard]] QColor leverColor() const { return m_leverColor; }
    [[nodiscard]] QColor ledColor() const { return m_ledColor; }
    [[nodiscard]] QColor textColor() const { return m_textColor; }
    [[nodiscard]] QColor guardColor() const { return m_guardColor; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setSwitchType(SwitchType type);
    void setPositionCount(int count);
    void setPosition(int position);
    void setChecked(bool checked);
    void toggle();
    void setOrientation(Qt::Orientation orientation);
    void setHasSafetyGuard(bool guard);
    void setGuardOpen(bool open);
    void setAnimated(bool animated);
    void setHasLed(bool hasLed);
    void setLabel(const QString &label);
    void setLabelOff(const QString &label);
    void setLabelOn(const QString &label);
    void setLabelCenter(const QString &label);
    void setPlateColor(const QColor &color);
    void setLeverColor(const QColor &color);
    void setLedColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setGuardColor(const QColor &color);

Q_SIGNALS:
    void positionChanged(int position);
    void toggled(bool checked);
    void guardToggled(bool isOpen);
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
