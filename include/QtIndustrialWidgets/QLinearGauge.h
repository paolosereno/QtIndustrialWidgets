#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

class QTINDUSTRIALWIDGETS_EXPORT QLinearGauge : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(bool thermometerMode READ isThermometerMode WRITE setThermometerMode NOTIFY appearanceChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(int majorTicks READ majorTicks WRITE setMajorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(int minorTicks READ minorTicks WRITE setMinorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(bool dynamicLiquidColor READ isDynamicLiquidColor WRITE setDynamicLiquidColor NOTIFY appearanceChanged)
    Q_PROPERTY(bool gradientLiquid READ isGradientLiquid WRITE setGradientLiquid NOTIFY appearanceChanged)
    Q_PROPERTY(bool digitalDisplayVisible READ digitalDisplayVisible WRITE setDigitalDisplayVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool scaleVisible READ scaleVisible WRITE setScaleVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor liquidColor READ liquidColor WRITE setLiquidColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor troughColor READ troughColor WRITE setTroughColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)

public:
    explicit QLinearGauge(QWidget *parent = nullptr);
    ~QLinearGauge() override = default;

    [[nodiscard]] Qt::Orientation orientation() const { return m_orientation; }
    [[nodiscard]] bool isThermometerMode() const { return m_thermometerMode; }
    [[nodiscard]] double minimum() const { return m_minimum; }
    [[nodiscard]] double maximum() const { return m_maximum; }
    [[nodiscard]] double value() const { return m_value; }
    [[nodiscard]] int precision() const { return m_precision; }
    [[nodiscard]] QString unit() const { return m_unit; }
    [[nodiscard]] int majorTicks() const { return m_majorTicks; }
    [[nodiscard]] int minorTicks() const { return m_minorTicks; }
    [[nodiscard]] double warningThreshold() const { return m_warningThreshold; }
    [[nodiscard]] double errorThreshold() const { return m_errorThreshold; }
    [[nodiscard]] bool isDynamicLiquidColor() const { return m_dynamicLiquidColor; }
    [[nodiscard]] bool isGradientLiquid() const { return m_gradientLiquid; }
    [[nodiscard]] bool digitalDisplayVisible() const { return m_digitalDisplayVisible; }
    [[nodiscard]] bool scaleVisible() const { return m_scaleVisible; }

    [[nodiscard]] QColor liquidColor() const { return m_liquidColor; }
    [[nodiscard]] QColor normalColor() const { return m_normalColor; }
    [[nodiscard]] QColor warningColor() const { return m_warningColor; }
    [[nodiscard]] QColor errorColor() const { return m_errorColor; }
    [[nodiscard]] QColor troughColor() const { return m_troughColor; }
    [[nodiscard]] QColor scaleColor() const { return m_scaleColor; }
    [[nodiscard]] QColor textColor() const { return m_textColor; }
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setOrientation(Qt::Orientation orientation);
    void setThermometerMode(bool thermometer);
    void setValue(double value);
    void setMinimum(double min);
    void setMaximum(double max);
    void setRange(double min, double max);
    void setPrecision(int precision);
    void setUnit(const QString &unit);
    void setMajorTicks(int count);
    void setMinorTicks(int count);
    void setWarningThreshold(double threshold);
    void setErrorThreshold(double threshold);
    void setDynamicLiquidColor(bool dynamic);
    void setGradientLiquid(bool gradient);
    void setDigitalDisplayVisible(bool visible);
    void setScaleVisible(bool visible);

    void setLiquidColor(const QColor &color);
    void setNormalColor(const QColor &color);
    void setWarningColor(const QColor &color);
    void setErrorColor(const QColor &color);
    void setTroughColor(const QColor &color);
    void setScaleColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setBezelColor(const QColor &color);

Q_SIGNALS:
    void valueChanged(double value);
    void rangeChanged(double min, double max);
    void thresholdChanged(double warning, double error);
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void invalidateCache();
    void renderStaticScale(const QSize &size);
    [[nodiscard]] QColor determineActiveLiquidColor() const;

    Qt::Orientation m_orientation{Qt::Vertical};
    bool m_thermometerMode{true};

    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{0.0};
    int m_precision{1};
    QString m_unit{QStringLiteral("°C")};

    int m_majorTicks{10};
    int m_minorTicks{4};

    double m_warningThreshold{70.0};
    double m_errorThreshold{90.0};
    bool m_dynamicLiquidColor{true};
    bool m_gradientLiquid{false};
    bool m_digitalDisplayVisible{true};
    bool m_scaleVisible{true};

    // Colors
    QColor m_liquidColor{QColor(235, 59, 90)};        // Default Mercury Crimson / Liquid
    QColor m_normalColor{QColor(46, 204, 113)};       // Emerald Green
    QColor m_warningColor{QColor(241, 196, 15)};      // Warning Amber
    QColor m_errorColor{QColor(231, 76, 60)};         // Danger Red
    QColor m_troughColor{QColor(30, 36, 45)};         // Dark Glass Tube Interior
    QColor m_scaleColor{QColor(200, 208, 218)};       // Scale ticks
    QColor m_textColor{QColor(240, 244, 248)};        // Readout text
    QColor m_bezelColor{QColor(44, 53, 64)};          // Bezel outer

    // Static scale cache
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};

    // Cached geometry computed during scale render
    QRectF m_tubeRect;
    QPointF m_bulbCenter;
    double m_bulbRadius{0.0};
};
