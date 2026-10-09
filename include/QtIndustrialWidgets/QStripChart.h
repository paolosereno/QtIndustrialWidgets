#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtCore/QVector>
#include <vector>

class QTINDUSTRIALWIDGETS_EXPORT QStripChart : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int capacity READ capacity WRITE setCapacity NOTIFY capacityChanged)
    Q_PROPERTY(double yMinimum READ yMinimum WRITE setYMinimum NOTIFY yRangeChanged)
    Q_PROPERTY(double yMaximum READ yMaximum WRITE setYMaximum NOTIFY yRangeChanged)
    Q_PROPERTY(bool autoScaleY READ isAutoScaleY WRITE setAutoScaleY NOTIFY appearanceChanged)
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool legendVisible READ isLegendVisible WRITE setLegendVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)
    Q_PROPERTY(int horizontalDivisions READ horizontalDivisions WRITE setHorizontalDivisions NOTIFY appearanceChanged)
    Q_PROPERTY(int verticalDivisions READ verticalDivisions WRITE setVerticalDivisions NOTIFY appearanceChanged)

public:
    struct ChannelInfo {
        QString name;
        QColor color;
        bool visible{true};
        double penWidth{1.8};
        std::vector<double> buffer;
        size_t headIndex{0};
        size_t count{0};
        double latestValue{0.0};
    };

    explicit QStripChart(QWidget *parent = nullptr);
    ~QStripChart() override = default;

    [[nodiscard]] int capacity() const { return m_capacity; }
    [[nodiscard]] double yMinimum() const { return m_yMinimum; }
    [[nodiscard]] double yMaximum() const { return m_yMaximum; }
    [[nodiscard]] bool isAutoScaleY() const { return m_autoScaleY; }
    [[nodiscard]] bool isGridVisible() const { return m_gridVisible; }
    [[nodiscard]] bool isLegendVisible() const { return m_legendVisible; }
    [[nodiscard]] QColor gridColor() const { return m_gridColor; }
    [[nodiscard]] QColor backgroundColor() const { return m_backgroundColor; }
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }
    [[nodiscard]] int horizontalDivisions() const { return m_horizontalDivisions; }
    [[nodiscard]] int verticalDivisions() const { return m_verticalDivisions; }

    [[nodiscard]] int channelCount() const { return static_cast<int>(m_channels.size()); }
    [[nodiscard]] const ChannelInfo *channel(int index) const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    int addChannel(const QString &name, const QColor &color, double penWidth = 1.8);
    void addDataPoint(int channelId, double value);
    void addDataPoints(const QVector<double> &values);
    void clear();

    void setCapacity(int count);
    void setYMinimum(double min);
    void setYMaximum(double max);
    void setYRange(double min, double max);
    void setAutoScaleY(bool autoScale);
    void setGridVisible(bool visible);
    void setLegendVisible(bool visible);
    void setGridColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setBezelColor(const QColor &color);
    void setHorizontalDivisions(int divisions);
    void setVerticalDivisions(int divisions);
    void setChannelVisible(int channelId, bool visible);
    void setChannelColor(int channelId, const QColor &color);

Q_SIGNALS:
    void capacityChanged(int capacity);
    void yRangeChanged(double min, double max);
    void dataAdded();
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void invalidateCache();
    void renderStaticGrid(const QSize &size);
    void updateAutoScaling();
    [[nodiscard]] QRectF plotArea() const;

    int m_capacity{300};
    double m_yMinimum{0.0};
    double m_yMaximum{100.0};
    bool m_autoScaleY{false};
    bool m_gridVisible{true};
    bool m_legendVisible{true};
    int m_horizontalDivisions{6};
    int m_verticalDivisions{8};

    // Colors
    QColor m_gridColor{QColor(42, 54, 70)};           // Subdued grid reticle
    QColor m_backgroundColor{QColor(14, 18, 25)};     // Deep oscilloscope dark
    QColor m_bezelColor{QColor(38, 46, 60)};          // Outer metal frame
    QColor m_textColor{QColor(210, 220, 235)};        // Grid text color

    std::vector<ChannelInfo> m_channels;

    // Static Grid Cache
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};
};
