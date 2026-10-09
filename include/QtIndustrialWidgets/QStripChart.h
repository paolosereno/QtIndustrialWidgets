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
#include <QtCore/QVector>
#include <vector>

/**
 * \class QStripChart
 * \brief Real-time multi-channel scrolling oscilloscope and strip chart recorder.
 *
 * QStripChart provides high-performance scrolling telemetry waveform visualization for industrial test benches,
 * SCADA systems, and embedded monitoring.
 *
 * Features include:
 * - O(1) circular ring buffer storage per channel avoiding memory allocations during streaming.
 * - Sub-millisecond rendering with Hi-DPI cached background reticle grid and bezel.
 * - Manual Y-range bounds or dynamic smooth auto-scaling.
 * - Multi-channel legend overlay with live telemetry readouts.
 *
 * \code
 * auto *chart = new QStripChart(parent);
 * chart->setCapacity(300);
 * chart->setYRange(0.0, 100.0);
 * int chRpm = chart->addChannel("RPM %", Qt::cyan, 2.0);
 * int chTemp = chart->addChannel("Temp °C", Qt::red, 2.0);
 *
 * chart->addDataPoint(chRpm, 75.4);
 * chart->addDataPoint(chTemp, 88.2);
 * \endcode
 */
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
    /** \brief Metadata and circular ring buffer for an individual waveform channel. */
    struct ChannelInfo {
        QString name;              ///< Channel name displayed in legend
        QColor color;              ///< Waveform trace stroke color
        bool visible{true};        ///< Channel visibility flag
        double penWidth{1.8};      ///< Trace stroke thickness in pixels
        std::vector<double> buffer;///< Pre-allocated circular ring buffer
        size_t headIndex{0};       ///< Ring buffer insertion head index
        size_t count{0};           ///< Total valid points stored in ring buffer
        double latestValue{0.0};   ///< Most recently streamed telemetry value
    };

    /**
     * \brief Constructs a QStripChart widget with default 300 points capacity.
     * \param parent Optional parent widget.
     */
    explicit QStripChart(QWidget *parent = nullptr);
    ~QStripChart() override = default;

    /** \brief Returns the ring buffer history point capacity per channel. */
    [[nodiscard]] int capacity() const { return m_capacity; }
    /** \brief Returns the minimum vertical Y scale bound. */
    [[nodiscard]] double yMinimum() const { return m_yMinimum; }
    /** \brief Returns the maximum vertical Y scale bound. */
    [[nodiscard]] double yMaximum() const { return m_yMaximum; }
    /** \brief Returns true if vertical auto-scaling is enabled. */
    [[nodiscard]] bool isAutoScaleY() const { return m_autoScaleY; }
    /** \brief Returns true if oscilloscope reticle grid is displayed. */
    [[nodiscard]] bool isGridVisible() const { return m_gridVisible; }
    /** \brief Returns true if top legend overlay is displayed. */
    [[nodiscard]] bool isLegendVisible() const { return m_legendVisible; }
    /** \brief Returns the reticle grid lines color. */
    [[nodiscard]] QColor gridColor() const { return m_gridColor; }
    /** \brief Returns the oscilloscope screen dark background color. */
    [[nodiscard]] QColor backgroundColor() const { return m_backgroundColor; }
    /** \brief Returns the outer chassis bezel frame color. */
    [[nodiscard]] QColor bezelColor() const { return m_bezelColor; }
    /** \brief Returns the number of horizontal grid divisions. */
    [[nodiscard]] int horizontalDivisions() const { return m_horizontalDivisions; }
    /** \brief Returns the number of vertical grid divisions. */
    [[nodiscard]] int verticalDivisions() const { return m_verticalDivisions; }

    /** \brief Returns the total number of registered channels. */
    [[nodiscard]] int channelCount() const { return static_cast<int>(m_channels.size()); }
    /** \brief Returns a pointer to channel info by index, or nullptr if invalid. */
    [[nodiscard]] const ChannelInfo *channel(int index) const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /**
     * \brief Registers a new waveform channel trace.
     * \param name Display label for legend.
     * \param color Waveform pen color.
     * \param penWidth Stroke width in pixels.
     * \return Unique channel ID index.
     */
    int addChannel(const QString &name, const QColor &color, double penWidth = 1.8);
    /** \brief Appends a single telemetry point to a specific channel. */
    void addDataPoint(int channelId, double value);
    /** \brief Appends synchronized telemetry points to multiple channels simultaneously. */
    void addDataPoints(const QVector<double> &values);
    /** \brief Clears all channel ring buffers. */
    void clear();

    /** \brief Sets the ring buffer history depth capacity. */
    void setCapacity(int count);
    /** \brief Sets the minimum vertical Y scale bound. */
    void setYMinimum(double min);
    /** \brief Sets the maximum vertical Y scale bound. */
    void setYMaximum(double max);
    /** \brief Sets both minimum and maximum vertical Y scale bounds. */
    void setYRange(double min, double max);
    /** \brief Enables or disables dynamic Y-axis auto-scaling. */
    void setAutoScaleY(bool autoScale);
    /** \brief Toggles visibility of the reticle grid. */
    void setGridVisible(bool visible);
    /** \brief Toggles visibility of the legend overlay. */
    void setLegendVisible(bool visible);
    /** \brief Sets the reticle grid line color. */
    void setGridColor(const QColor &color);
    /** \brief Sets the background screen color. */
    void setBackgroundColor(const QColor &color);
    /** \brief Sets the outer chassis bezel color. */
    void setBezelColor(const QColor &color);
    /** \brief Sets the horizontal grid line divisions count. */
    void setHorizontalDivisions(int divisions);
    /** \brief Sets the vertical grid line divisions count. */
    void setVerticalDivisions(int divisions);
    /** \brief Toggles visibility for an individual channel. */
    void setChannelVisible(int channelId, bool visible);
    /** \brief Updates stroke color for an individual channel. */
    void setChannelColor(int channelId, const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when the ring buffer capacity changes. */
    void capacityChanged(int capacity);
    /** \brief Emitted when the vertical Y axis range changes. */
    void yRangeChanged(double min, double max);
    /** \brief Emitted whenever new data points are added. */
    void dataAdded();
    /** \brief Emitted when visual appearance styling properties change. */
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
