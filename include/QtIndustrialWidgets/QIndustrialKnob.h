#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

class QTINDUSTRIALWIDGETS_EXPORT QIndustrialKnob : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double step READ step WRITE setStep NOTIFY appearanceChanged)
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
    enum class KnobMode {
        Continuous,
        Discrete
    };
    Q_ENUM(KnobMode)

    explicit QIndustrialKnob(QWidget *parent = nullptr);
    ~QIndustrialKnob() override = default;

    [[nodiscard]] double minimum() const { return m_minimum; }
    [[nodiscard]] double maximum() const { return m_maximum; }
    [[nodiscard]] double value() const { return m_value; }
    [[nodiscard]] double step() const { return m_step; }
    [[nodiscard]] int precision() const { return m_precision; }
    [[nodiscard]] QString unit() const { return m_unit; }
    [[nodiscard]] double startAngle() const { return m_startAngle; }
    [[nodiscard]] double spanAngle() const { return m_spanAngle; }
    [[nodiscard]] int majorTicks() const { return m_majorTicks; }
    [[nodiscard]] int minorTicks() const { return m_minorTicks; }
    [[nodiscard]] KnobMode mode() const { return m_mode; }
    [[nodiscard]] int discreteSteps() const { return m_discreteSteps; }
    [[nodiscard]] bool isTrackVisible() const { return m_trackVisible; }
    [[nodiscard]] bool isValueDisplayVisible() const { return m_valueDisplayVisible; }

    [[nodiscard]] QColor knobColor() const { return m_knobColor; }
    [[nodiscard]] QColor pointerColor() const { return m_pointerColor; }
    [[nodiscard]] QColor scaleColor() const { return m_scaleColor; }
    [[nodiscard]] QColor trackColor() const { return m_trackColor; }
    [[nodiscard]] QColor textColor() const { return m_textColor; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setValue(double value);
    void setMinimum(double min);
    void setMaximum(double max);
    void setRange(double min, double max);
    void setStep(double step);
    void setPrecision(int precision);
    void setUnit(const QString &unit);
    void setStartAngle(double angle);
    void setSpanAngle(double span);
    void setMajorTicks(int count);
    void setMinorTicks(int count);
    void setMode(KnobMode mode);
    void setDiscreteSteps(int steps);
    void setTrackVisible(bool visible);
    void setValueDisplayVisible(bool visible);

    void setKnobColor(const QColor &color);
    void setPointerColor(const QColor &color);
    void setScaleColor(const QColor &color);
    void setTrackColor(const QColor &color);
    void setTextColor(const QColor &color);

Q_SIGNALS:
    void valueChanged(double value);
    void rangeChanged(double min, double max);
    void modeChanged(KnobMode mode);
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

private:
    void invalidateCache();
    void renderStaticScale(const QSize &size);
    [[nodiscard]] double valueToAngle(double val) const;
    [[nodiscard]] double angleToValue(double angle) const;
    void updateValueFromPoint(const QPointF &pos);

    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{25.0};
    double m_step{1.0};
    int m_precision{0};
    QString m_unit{QStringLiteral("%")};

    // Sweep angles: 0° is 12 o'clock, clockwise. Default: -135° to +135° (270° span)
    double m_startAngle{-135.0};
    double m_spanAngle{270.0};
    int m_majorTicks{10};
    int m_minorTicks{3};

    KnobMode m_mode{KnobMode::Continuous};
    int m_discreteSteps{5};
    bool m_trackVisible{true};
    bool m_valueDisplayVisible{true};

    // Colors
    QColor m_knobColor{QColor(42, 48, 60)};           // Machined Gunmetal
    QColor m_pointerColor{QColor(0, 229, 255)};        // Neon Cyan Indicator
    QColor m_scaleColor{QColor(190, 200, 215)};        // Graduated Scale Silver
    QColor m_trackColor{QColor(0, 229, 255)};          // Active Arc Track
    QColor m_textColor{QColor(240, 244, 250)};         // Readout Text

    // Performance Caching
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};

    // Mouse Interaction
    bool m_isDragging{false};
    QPointF m_lastMousePos;
};
