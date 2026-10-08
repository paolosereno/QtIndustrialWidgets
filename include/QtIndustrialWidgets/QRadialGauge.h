#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

class QTINDUSTRIALWIDGETS_EXPORT QRadialGauge : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(double startAngle READ startAngle WRITE setStartAngle NOTIFY appearanceChanged)
    Q_PROPERTY(double spanAngle READ spanAngle WRITE setSpanAngle NOTIFY appearanceChanged)
    Q_PROPERTY(int majorTicks READ majorTicks WRITE setMajorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(int minorTicks READ minorTicks WRITE setMinorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(bool thresholdBandsVisible READ thresholdBandsVisible WRITE setThresholdBandsVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool digitalDisplayVisible READ digitalDisplayVisible WRITE setDigitalDisplayVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor needleColor READ needleColor WRITE setNeedleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor dialColor READ dialColor WRITE setDialColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)

public:
    explicit QRadialGauge(QWidget *parent = nullptr);
    ~QRadialGauge() override = default;

    [[nodiscard]] double minimum() const { return m_minimum; }
    [[nodiscard]] double maximum() const { return m_maximum; }
    [[nodiscard]] double value() const { return m_value; }
    [[nodiscard]] int precision() const { return m_precision; }
    [[nodiscard]] QString unit() const { return m_unit; }
    [[nodiscard]] double startAngle() const { return m_startAngle; }
    [[nodiscard]] double spanAngle() const { return m_spanAngle; }
    [[nodiscard]] int majorTicks() const { return m_majorTicks; }
    [[nodiscard]] int minorTicks() const { return m_minorTicks; }
    [[nodiscard]] double warningThreshold() const { return m_warningThreshold; }
    [[nodiscard]] double errorThreshold() const { return m_errorThreshold; }
    [[nodiscard]] bool thresholdBandsVisible() const { return m_thresholdBandsVisible; }
    [[nodiscard]] bool digitalDisplayVisible() const { return m_digitalDisplayVisible; }

    [[nodiscard]] QColor needleColor() const { return m_needleColor; }
    [[nodiscard]] QColor normalColor() const { return m_normalColor; }
    [[nodiscard]] QColor warningColor() const { return m_warningColor; }
    [[nodiscard]] QColor errorColor() const { return m_errorColor; }
    [[nodiscard]] QColor dialColor() const { return m_dialColor; }
    [[nodiscard]] QColor scaleColor() const { return m_scaleColor; }
    [[nodiscard]] QColor textColor() const { return m_textColor; }
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setValue(double value);
    void setMinimum(double min);
    void setMaximum(double max);
    void setRange(double min, double max);
    void setPrecision(int precision);
    void setUnit(const QString &unit);
    void setStartAngle(double angle);
    void setSpanAngle(double span);
    void setMajorTicks(int count);
    void setMinorTicks(int count);
    void setWarningThreshold(double threshold);
    void setErrorThreshold(double threshold);
    void setThresholdBandsVisible(bool visible);
    void setDigitalDisplayVisible(bool visible);

    void setNeedleColor(const QColor &color);
    void setNormalColor(const QColor &color);
    void setWarningColor(const QColor &color);
    void setErrorColor(const QColor &color);
    void setDialColor(const QColor &color);
    void setScaleColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setBezelColor(const QColor &color);

Q_SIGNALS:
    void valueChanged(double value);
    void rangeChanged(double min, double max);
    void thresholdChanged(double warning, double error);
    void warningExceeded(bool exceeded);
    void errorExceeded(bool exceeded);
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void invalidateCache();
    void renderStaticScale(const QSize &size);
    double valueToAngle(double val) const;

    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{0.0};
    int m_precision{1};
    QString m_unit{QStringLiteral("bar")};

    // Angles: 0° is 12 o'clock, clockwise. Default: -135° to +135° (270° span)
    double m_startAngle{-135.0};
    double m_spanAngle{270.0};
    int m_majorTicks{10};
    int m_minorTicks{4};

    double m_warningThreshold{70.0};
    double m_errorThreshold{85.0};
    bool m_thresholdBandsVisible{true};
    bool m_digitalDisplayVisible{true};

    // Colors
    QColor m_needleColor{QColor(235, 59, 90)};       // Industrial Crimson
    QColor m_normalColor{QColor(38, 222, 129)};       // Neon Emerald Green
    QColor m_warningColor{QColor(254, 211, 48)};      // Amber Gold
    QColor m_errorColor{QColor(235, 59, 90)};         // Danger Red
    QColor m_dialColor{QColor(24, 28, 36)};           // Slate Black
    QColor m_scaleColor{QColor(210, 218, 226)};       // Light Silver
    QColor m_textColor{QColor(245, 246, 250)};        // Crisp White
    QColor m_bezelColor{QColor(53, 59, 72)};          // Gunmetal Gray

    // High performance static scale cache
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};

    // Threshold state tracking
    bool m_wasWarning{false};
    bool m_wasError{false};
};
