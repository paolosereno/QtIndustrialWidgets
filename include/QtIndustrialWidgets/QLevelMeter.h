#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtCore/QVector>
#include <QtCore/QStringList>

class QTimer;

class QTINDUSTRIALWIDGETS_EXPORT QLevelMeter : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int channelCount READ channelCount WRITE setChannelCount NOTIFY appearanceChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY appearanceChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY appearanceChanged)
    Q_PROPERTY(int segmentCount READ segmentCount WRITE setSegmentCount NOTIFY appearanceChanged)
    Q_PROPERTY(DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY appearanceChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(bool peakHoldEnabled READ isPeakHoldEnabled WRITE setPeakHoldEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(int peakHoldTimeMs READ peakHoldTimeMs WRITE setPeakHoldTimeMs NOTIFY appearanceChanged)
    Q_PROPERTY(double peakDecayRate READ peakDecayRate WRITE setPeakDecayRate NOTIFY appearanceChanged)
    Q_PROPERTY(bool scaleVisible READ isScaleVisible WRITE setScaleVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)

public:
    enum class DisplayMode {
        Segmented,  // Discrete LED segments ladder
        Continuous  // Smooth gradient bar
    };
    Q_ENUM(DisplayMode)

    explicit QLevelMeter(QWidget *parent = nullptr);
    ~QLevelMeter() override;

    [[nodiscard]] int channelCount() const { return m_channelCount; }
    [[nodiscard]] double minimum() const { return m_minimum; }
    [[nodiscard]] double maximum() const { return m_maximum; }
    [[nodiscard]] double warningThreshold() const { return m_warningThreshold; }
    [[nodiscard]] double errorThreshold() const { return m_errorThreshold; }
    [[nodiscard]] int segmentCount() const { return m_segmentCount; }
    [[nodiscard]] DisplayMode displayMode() const { return m_displayMode; }
    [[nodiscard]] Qt::Orientation orientation() const { return m_orientation; }
    [[nodiscard]] bool isPeakHoldEnabled() const { return m_peakHoldEnabled; }
    [[nodiscard]] int peakHoldTimeMs() const { return m_peakHoldTimeMs; }
    [[nodiscard]] double peakDecayRate() const { return m_peakDecayRate; }
    [[nodiscard]] bool isScaleVisible() const { return m_scaleVisible; }
    [[nodiscard]] QString unit() const { return m_unit; }
    [[nodiscard]] QString title() const { return m_title; }

    [[nodiscard]] double value(int channel = 0) const;
    [[nodiscard]] double peakValue(int channel = 0) const;
    [[nodiscard]] QVector<double> values() const;

    [[nodiscard]] QStringList channelLabels() const { return m_channelLabels; }

    [[nodiscard]] QColor normalColor() const { return m_normalColor; }
    [[nodiscard]] QColor warningColor() const { return m_warningColor; }
    [[nodiscard]] QColor errorColor() const { return m_errorColor; }
    [[nodiscard]] QColor peakColor() const { return m_peakColor; }
    [[nodiscard]] QColor backgroundColor() const { return m_backgroundColor; }
    [[nodiscard]] QColor textColor() const { return m_textColor; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    void setValue(double value);
    void setValue(int channel, double value);
    void setValues(const QVector<double> &values);
    void resetPeaks();

    void setChannelCount(int count);
    void setRange(double min, double max);
    void setMinimum(double min);
    void setMaximum(double max);
    void setWarningThreshold(double threshold);
    void setErrorThreshold(double threshold);
    void setSegmentCount(int count);
    void setDisplayMode(DisplayMode mode);
    void setOrientation(Qt::Orientation orientation);
    void setPeakHoldEnabled(bool enabled);
    void setPeakHoldTimeMs(int ms);
    void setPeakDecayRate(double rate);
    void setScaleVisible(bool visible);
    void setUnit(const QString &unit);
    void setTitle(const QString &title);
    void setChannelLabels(const QStringList &labels);
    void setNormalColor(const QColor &color);
    void setWarningColor(const QColor &color);
    void setErrorColor(const QColor &color);
    void setPeakColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setTextColor(const QColor &color);

Q_SIGNALS:
    void valueChanged(int channel, double value);
    void overloadOccurred(int channel);
    void rangeChanged(double min, double max);
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private Q_SLOTS:
    void updatePeakDecay();

private:
    struct ChannelData {
        double value = 0.0;
        double peakValue = 0.0;
        qint64 lastPeakTime = 0;
        bool overloadState = false;
    };

    void renderStaticBackground();
    void drawChannelBar(QPainter &painter, int chIndex, const QRectF &barRect);
    void drawSegmentedBar(QPainter &painter, double val, double peakVal, const QRectF &barRect);
    void drawContinuousBar(QPainter &painter, double val, double peakVal, const QRectF &barRect);
    QColor colorForNormalizedValue(double norm) const;
    QVector<QRectF> calculateBarRects(const QRectF &contentRect) const;

    int m_channelCount = 2; // Default stereo (L / R)
    double m_minimum = -60.0;
    double m_maximum = 6.0;
    double m_warningThreshold = -6.0;
    double m_errorThreshold = 0.0;
    int m_segmentCount = 24;
    DisplayMode m_displayMode = DisplayMode::Segmented;
    Qt::Orientation m_orientation = Qt::Vertical;

    bool m_peakHoldEnabled = true;
    int m_peakHoldTimeMs = 1200; // Hold peak for 1.2s before decay
    double m_peakDecayRate = 25.0; // Units/second decay rate
    bool m_scaleVisible = true;

    QString m_unit = QStringLiteral("dB");
    QString m_title;
    QStringList m_channelLabels = {QStringLiteral("L"), QStringLiteral("R")};

    QColor m_normalColor = QColor(46, 204, 113);   // Green
    QColor m_warningColor = QColor(254, 211, 48);  // Amber Yellow
    QColor m_errorColor = QColor(235, 59, 90);     // Red
    QColor m_peakColor = QColor(255, 255, 255);    // White peak indicator
    QColor m_backgroundColor = QColor(22, 25, 30); // Deep dark chassis
    QColor m_textColor = QColor(180, 185, 195);

    QVector<ChannelData> m_channels;
    QTimer *m_decayTimer = nullptr;

    QPixmap m_cachedBackground;
    bool m_cacheValid = false;
    qint64 m_lastDecayTime = 0;
};
