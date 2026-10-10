/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtCore/QVector>
#include <memory>

namespace QtIndustrialWidgets {

class StripChartPrivate;

/**
 * \class StripChart
 * \brief Real-time multi-channel scrolling oscilloscope and strip chart recorder.
 *
 * StripChart provides high-performance scrolling telemetry waveform visualization for industrial test benches,
 * SCADA systems, and embedded monitoring.
 *
 * Features include:
 * - O(1) circular ring buffer storage per channel avoiding memory allocations during streaming.
 * - Sub-millisecond rendering with Hi-DPI cached background reticle grid and bezel.
 * - Manual Y-range bounds or dynamic smooth auto-scaling.
 * - Multi-channel legend overlay with live telemetry readouts.
 *
 * \code
 * auto *chart = new StripChart(parent);
 * chart->setCapacity(300);
 * chart->setYRange(0.0, 100.0);
 * int chRpm = chart->addChannel("RPM %", Qt::cyan, 2.0);
 * int chTemp = chart->addChannel("Temp °C", Qt::red, 2.0);
 *
 * chart->addDataPoint(chRpm, 75.4);
 * chart->addDataPoint(chTemp, 88.2);
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT StripChart : public QWidget
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
    /**
     * \brief Constructs a StripChart widget with default 300 points capacity.
     * \param parent Optional parent widget.
     */
    explicit StripChart(QWidget *parent = nullptr);
    ~StripChart() override;

    /** \brief Returns the ring buffer history point capacity per channel. */
    [[nodiscard]] int capacity() const;
    /** \brief Returns the minimum vertical Y scale bound. */
    [[nodiscard]] double yMinimum() const;
    /** \brief Returns the maximum vertical Y scale bound. */
    [[nodiscard]] double yMaximum() const;
    /** \brief Returns true if vertical auto-scaling is enabled. */
    [[nodiscard]] bool isAutoScaleY() const;
    /** \brief Returns true if oscilloscope reticle grid is displayed. */
    [[nodiscard]] bool isGridVisible() const;
    /** \brief Returns true if top legend overlay is displayed. */
    [[nodiscard]] bool isLegendVisible() const;
    /** \brief Returns the reticle grid lines color. */
    [[nodiscard]] QColor gridColor() const;
    /** \brief Returns the oscilloscope screen dark background color. */
    [[nodiscard]] QColor backgroundColor() const;
    /** \brief Returns the outer chassis bezel frame color. */
    [[nodiscard]] QColor bezelColor() const;
    /** \brief Returns the number of horizontal grid divisions. */
    [[nodiscard]] int horizontalDivisions() const;
    /** \brief Returns the number of vertical grid divisions. */
    [[nodiscard]] int verticalDivisions() const;

    /** \brief Returns the total number of registered channels. */
    [[nodiscard]] int channelCount() const;
    /** \brief Returns the display name of a channel, or an empty string if invalid. */
    [[nodiscard]] QString channelName(int channelId) const;
    /** \brief Returns the waveform stroke color of a channel, or an invalid QColor if invalid. */
    [[nodiscard]] QColor channelColor(int channelId) const;
    /** \brief Returns true if the channel waveform is currently visible. */
    [[nodiscard]] bool isChannelVisible(int channelId) const;
    /** \brief Returns the stroke pen width in pixels of a channel. */
    [[nodiscard]] double channelPenWidth(int channelId) const;
    /** \brief Returns the number of valid points currently stored in the channel buffer. */
    [[nodiscard]] qsizetype channelSampleCount(int channelId) const;
    /** \brief Returns the most recently streamed telemetry value for a channel. */
    [[nodiscard]] double channelLatestValue(int channelId) const;

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

    std::unique_ptr<StripChartPrivate> d_ptr;
    Q_DECLARE_PRIVATE(StripChart)
};

} // namespace QtIndustrialWidgets
