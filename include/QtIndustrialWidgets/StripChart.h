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
#include <QtCore/QDateTime>
#include <chrono>
#include <memory>

namespace QtIndustrialWidgets {

class StripChartPrivate;
namespace internal {
class StripChartTestAccess;
}

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
 * - Real-time axis (std::chrono::nanoseconds) and high-speed M4 min/max decimation at 60 FPS.
 *
 * \code
 * auto *chart = new StripChart(parent);
 * chart->setCapacity(1000000);
 * chart->setXAxisMode(StripChart::XAxisMode::Time);
 * chart->setTimeSpan(std::chrono::seconds(10));
 * int ch1 = chart->addChannel("Torque", Qt::cyan);
 * chart->addSample(ch1, std::chrono::nanoseconds(1000000), 45.2);
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
    Q_PROPERTY(XAxisMode xAxisMode READ xAxisMode WRITE setXAxisMode NOTIFY xAxisModeChanged)
    Q_PROPERTY(DecimationMode decimationMode READ decimationMode WRITE setDecimationMode NOTIFY decimationModeChanged)
    Q_PROPERTY(Interpolation interpolation READ interpolation WRITE setInterpolation NOTIFY interpolationChanged)
    Q_PROPERTY(TimeLabelFormat timeLabelFormat READ timeLabelFormat WRITE setTimeLabelFormat NOTIFY timeLabelFormatChanged)
    Q_PROPERTY(double timeSpanSeconds READ timeSpanSeconds WRITE setTimeSpanSeconds NOTIFY timeSpanChanged)

public:
    /** \brief Horizontal X-axis progression mode. */
    enum class XAxisMode {
        SampleIndex, ///< Legacy mode: X coordinate mapped to ring buffer sample slot index (default)
        Time         ///< Real-time mode: X coordinate mapped to nanosecond timestamps
    };
    Q_ENUM(XAxisMode)

    /** \brief Waveform decimation mode. */
    enum class DecimationMode {
        Off,    ///< Always draw raw samples
        Auto,   ///< Automatically enable M4 decimation when visible samples exceed plot width in device pixels (default)
        Always  ///< Always run M4 decimation
    };
    Q_ENUM(DecimationMode)

    /** \brief Waveform trace line interpolation style. */
    enum class Interpolation {
        Linear, ///< Linear interpolation between consecutive points (default)
        Step    ///< Sample-and-hold step interpolation
    };
    Q_ENUM(Interpolation)

    /** \brief Time axis label formatting convention. */
    enum class TimeLabelFormat {
        Relative, ///< Relative negative elapsed time labels (e.g. -10s ... 0s) (default)
        Absolute  ///< Wall-clock absolute timestamps using timeOrigin (e.g. HH:mm:ss.zzz)
    };
    Q_ENUM(TimeLabelFormat)

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

    /** \brief Returns the active horizontal X-axis mode. */
    [[nodiscard]] XAxisMode xAxisMode() const;
    /**
     * \brief Sets the horizontal X-axis mode.
     * \details When the mode changes, all buffered sample data (rings, counts, latest values,
     *          timestamps, and decimation states) are cleared across all channels to avoid
     *          mixing incompatible timebases. Channel metadata (names, colors, pen widths,
     *          visibility) and the diagnostic rejectedSampleCount() are preserved.
     * \param mode Desired X-axis mode (SampleIndex or Time).
     */
    void setXAxisMode(XAxisMode mode);

    /** \brief Returns the scrolling time window span. Default is 10 seconds. */
    [[nodiscard]] std::chrono::nanoseconds timeSpan() const;
    /** \brief Sets the scrolling time window span. */
    void setTimeSpan(std::chrono::nanoseconds span);

    /** \brief Returns the scrolling time window span in seconds. */
    [[nodiscard]] double timeSpanSeconds() const;
    /** \brief Sets the scrolling time window span in seconds. */
    void setTimeSpanSeconds(double seconds);

    /** \brief Returns the active decimation mode. Default is Auto. */
    [[nodiscard]] DecimationMode decimationMode() const;
    /** \brief Sets the decimation mode.
     *
     * When decimation is active, traces are rendered using a cosmetic 1-device-pixel pen
     * without antialiasing to guarantee bounded frame time (O(W)). Custom channel pen
     * widths and antialiasing apply when decimation is inactive.
     */
    void setDecimationMode(DecimationMode mode);

    /** \brief Returns the active waveform interpolation style. Default is Linear. */
    [[nodiscard]] Interpolation interpolation() const;
    /** \brief Sets the waveform interpolation style. */
    void setInterpolation(Interpolation interp);

    /** \brief Returns the inter-sample gap threshold. 0 nanoseconds means automatic (4x EMA). */
    [[nodiscard]] std::chrono::nanoseconds gapThreshold() const;
    /** \brief Sets the inter-sample gap threshold. 0 nanoseconds enables automatic EMA detection. */
    void setGapThreshold(std::chrono::nanoseconds threshold);

    /** \brief Returns the optional absolute wall-clock reference instant for t = 0. */
    [[nodiscard]] QDateTime timeOrigin() const;
    /** \brief Sets the optional absolute wall-clock reference instant for t = 0. */
    void setTimeOrigin(const QDateTime &origin);

    /** \brief Returns the time axis label formatting convention. Default is Relative. */
    [[nodiscard]] TimeLabelFormat timeLabelFormat() const;
    /** \brief Sets the time axis label formatting convention. */
    void setTimeLabelFormat(TimeLabelFormat format);

    /** \brief Returns the count of out-of-order samples rejected for a channel. */
    [[nodiscard]] quint64 rejectedSampleCount(int channelId) const;
    /** \brief Returns true if the ring buffer capacity is sufficient to cover the entire timeSpan. */
    [[nodiscard]] bool isTimeWindowFullyCovered() const;

    /**
     * \brief Appends a single timestamped sample to a channel.
     * \note Must be called from the GUI thread only.
     * \details In \c Time mode, \p t must be >= the channel's last timestamp (out-of-order samples
     *          are rejected and increment \c rejectedSampleCount). In \c SampleIndex mode,
     *          \p t is ignored and the sample is indexed with the running sample index,
     *          issuing a one-time warning per channel.
     * \param channelId Channel ID.
     * \param t Acquisition timestamp relative to run origin (in Time mode).
     * \param value Telemetry sample value. NaN and +/-Inf are stored as invalid markers.
     */
    void addSample(int channelId, std::chrono::nanoseconds t, double value);

    /**
     * \brief Appends a batch of timestamped samples to a channel.
     * \note Must be called from the GUI thread only. Emits dataAdded() once per call.
     * \details In \c Time mode, timestamps must be monotonically non-decreasing. In \c SampleIndex mode,
     *          timestamps in \p t are ignored and samples are indexed sequentially, issuing a
     *          one-time warning per channel.
     * \param channelId Channel ID.
     * \param t Array of timestamps (in Time mode).
     * \param values Array of sample values.
     * \param count Number of samples in the batch.
     */
    void addSamples(int channelId, const std::chrono::nanoseconds *t,
                    const double *values, qsizetype count);

    /**
     * \brief Appends uniformly sampled data points to a channel.
     * \note Must be called from the GUI thread only. \p dt must be > 0.
     * \details In \c Time mode, samples are timestamped starting at \p t0 with step \p dt.
     *          In \c SampleIndex mode, \p t0 and \p dt are ignored (though \p dt <= 0 is still rejected)
     *          and samples are indexed sequentially, issuing a one-time warning per channel.
     * \param channelId Channel ID.
     * \param t0 Starting timestamp (in Time mode).
     * \param dt Time interval between consecutive samples (must be > 0).
     * \param values Array of sample values.
     * \param count Number of samples.
     */
    void addUniformSamples(int channelId, std::chrono::nanoseconds t0,
                           std::chrono::nanoseconds dt,
                           const double *values, qsizetype count);

    /**
     * \brief Appends a single synchronized timestamped sample across all channels.
     * \note Must be called from the GUI thread only. Emits dataAdded() once per call.
     * \details In \c Time mode, \p t must be >= each channel's last timestamp. In \c SampleIndex mode,
     *          \p t is ignored and each channel is indexed with its running sample index,
     *          issuing a one-time warning per channel.
     * \param t Acquisition timestamp (in Time mode).
     * \param values Vector of sample values matching registered channels.
     */
    void addSynchronousSamples(std::chrono::nanoseconds t,
                               const QVector<double> &values);

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
    /** \brief Emitted when the X-axis mode changes. */
    void xAxisModeChanged(XAxisMode mode);
    /** \brief Emitted when the decimation mode changes. */
    void decimationModeChanged(DecimationMode mode);
    /** \brief Emitted when the interpolation style changes. */
    void interpolationChanged(Interpolation interp);
    /** \brief Emitted when the time label format changes. */
    void timeLabelFormatChanged(TimeLabelFormat format);
    /** \brief Emitted when the time span duration changes. */
    void timeSpanChanged(double seconds);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void invalidateCache();
    void renderStaticGrid(const QSize &size);
    void updateAutoScaling();
    void ensureAutoScale();
    [[nodiscard]] QRectF plotArea() const;

    std::unique_ptr<StripChartPrivate> d_ptr;
    Q_DECLARE_PRIVATE(StripChart)
    friend class internal::StripChartTestAccess;
};

} // namespace QtIndustrialWidgets
