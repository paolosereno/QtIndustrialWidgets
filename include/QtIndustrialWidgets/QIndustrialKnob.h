#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

/**
 * \class QIndustrialKnob
 * \brief Precision industrial rotary control potentiometer and selector knob.
 *
 * QIndustrialKnob provides an interactive rotary control with brushed metal texture, notched grips,
 * continuous or discrete indexed stepping, mouse dragging, mouse wheel, and keyboard arrow controls.
 *
 * It features high-DPI caching for background ticks and scales to ensure smooth 60 FPS rotation interaction.
 *
 * \code
 * auto *knob = new QIndustrialKnob(parent);
 * knob->setRange(0.0, 100.0);
 * knob->setValue(50.0);
 * knob->setStep(1.0);
 * knob->setUnit("%");
 * \endcode
 */
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
    /** \brief Operating mode for the knob adjustment behavior. */
    enum class KnobMode {
        Continuous, ///< Smooth continuous analog potentiometer
        Discrete    ///< Stepped indexed selector switch
    };
    Q_ENUM(KnobMode)

    /**
     * \brief Constructs a QIndustrialKnob widget with default [0, 100] range.
     * \param parent Optional parent widget.
     */
    explicit QIndustrialKnob(QWidget *parent = nullptr);
    ~QIndustrialKnob() override = default;

    /** \brief Returns the minimum dial value. */
    [[nodiscard]] double minimum() const { return m_minimum; }
    /** \brief Returns the maximum dial value. */
    [[nodiscard]] double maximum() const { return m_maximum; }
    /** \brief Returns the current rotary value. */
    [[nodiscard]] double value() const { return m_value; }
    /** \brief Returns the increment step size. */
    [[nodiscard]] double step() const { return m_step; }
    /** \brief Returns the decimal display precision. */
    [[nodiscard]] int precision() const { return m_precision; }
    /** \brief Returns the measurement unit label. */
    [[nodiscard]] QString unit() const { return m_unit; }
    /** \brief Returns the start angle in degrees (default: -135°). */
    [[nodiscard]] double startAngle() const { return m_startAngle; }
    /** \brief Returns the sweep span in degrees (default: 270°). */
    [[nodiscard]] double spanAngle() const { return m_spanAngle; }
    /** \brief Returns the major ticks count. */
    [[nodiscard]] int majorTicks() const { return m_majorTicks; }
    /** \brief Returns the minor subdivisions count. */
    [[nodiscard]] int minorTicks() const { return m_minorTicks; }
    /** \brief Returns the operating mode (Continuous or Discrete). */
    [[nodiscard]] KnobMode mode() const { return m_mode; }
    /** \brief Returns the number of discrete detent positions. */
    [[nodiscard]] int discreteSteps() const { return m_discreteSteps; }
    /** \brief Returns true if the active value arc track is visible. */
    [[nodiscard]] bool isTrackVisible() const { return m_trackVisible; }
    /** \brief Returns true if the center numeric readout is displayed. */
    [[nodiscard]] bool isValueDisplayVisible() const { return m_valueDisplayVisible; }

    /** \brief Returns the knob cap metal color. */
    [[nodiscard]] QColor knobColor() const { return m_knobColor; }
    /** \brief Returns the pointer dot/line indicator color. */
    [[nodiscard]] QColor pointerColor() const { return m_pointerColor; }
    /** \brief Returns the scale ticks color. */
    [[nodiscard]] QColor scaleColor() const { return m_scaleColor; }
    /** \brief Returns the active progress arc track color. */
    [[nodiscard]] QColor trackColor() const { return m_trackColor; }
    /** \brief Returns the label and numeric text color. */
    [[nodiscard]] QColor textColor() const { return m_textColor; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the current rotary position value. */
    void setValue(double value);
    /** \brief Sets the minimum value. */
    void setMinimum(double min);
    /** \brief Sets the maximum value. */
    void setMaximum(double max);
    /** \brief Sets both minimum and maximum scale bounds. */
    void setRange(double min, double max);
    /** \brief Sets the stepping increment for wheel and arrow keys. */
    void setStep(double step);
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
    /** \brief Emitted when the knob value changes. */
    void valueChanged(double value);
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
