/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>

#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <chrono>
#include <memory>
#include <optional>

namespace QtIndustrialWidgets {

class AnalysisChartPrivate;
namespace internal {
class AnalysisChartTestAccess;
}

/**
 * \class AnalysisChart
 * \brief Offline review of recorded multi-channel data (#25).
 *
 * Each channel holds its own copy of timestamp + value arrays: absolute std::chrono::nanoseconds
 * timestamps, non-decreasing, with values as double. Channels may have different or irregular
 * sample rates. All channels share one Y axis.
 *
 * Dropouts and gaps follow exactly the rules of StripChart, so the same data never looks
 * different in the two charts: a non-finite value breaks the trace, and so does a time gap between
 * two samples larger than gapThreshold() (0, the default, means 4 times an exponential moving
 * average of the sampling interval, computed in sample order as StripChart does).
 *
 * Time axis labels: Relative shows the time elapsed since dataStart(); Absolute shows
 * timeOrigin() + t as wall-clock time (HH:mm:ss.zzz), or t itself when timeOrigin() is invalid.
 *
 * Fonts: the texts use the widget font at sizes derived from the widget geometry and scaled by the
 * ratio between the effective pixel sizes of the widget font and of the application font, as in
 * StripChart; the room kept for the axis labels and the legend grows with it.
 *
 * \code
 * auto *chart = new AnalysisChart(parent);
 * const int pressure = chart->addChannel("Pressure", Qt::cyan);
 * chart->setChannelData(pressure, timestamps.data(), values.data(), values.size());
 * chart->resetView();
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT AnalysisChart : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(double yMinimum READ yMinimum NOTIFY viewChanged)
    Q_PROPERTY(double yMaximum READ yMaximum NOTIFY viewChanged)
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool legendVisible READ isLegendVisible WRITE setLegendVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(TimeLabelFormat timeLabelFormat READ timeLabelFormat WRITE setTimeLabelFormat NOTIFY appearanceChanged)
    Q_PROPERTY(QDateTime timeOrigin READ timeOrigin WRITE setTimeOrigin NOTIFY appearanceChanged)

public:
    /** \brief Time axis labels. */
    enum class TimeLabelFormat {
        Relative, ///< Time elapsed since dataStart() (default)
        Absolute  ///< timeOrigin() + t as wall-clock time (HH:mm:ss.zzz); t itself if timeOrigin() is invalid
    };
    Q_ENUM(TimeLabelFormat)

    /** \brief One stored sample. */
    struct Sample {
        std::chrono::nanoseconds t{0};
        double value{0.0}; ///< May be non-finite (a dropout)
    };

    explicit AnalysisChart(QWidget *parent = nullptr);
    ~AnalysisChart() override;

    // --- Channels: ids are stable, never reused, and unaffected by removing other channels ---

    /** \brief Adds an empty channel and returns its id. */
    int addChannel(const QString &name, const QColor &color, double penWidth = 1.8);
    /** \brief Removes a channel and its data; false if the id is unknown. */
    bool removeChannel(int channelId);
    /** \brief Removes every channel and its data. */
    void removeAllChannels();
    /** \brief Ids of the channels, in the order they were added. */
    [[nodiscard]] QList<int> channelIds() const;
    [[nodiscard]] int channelCount() const;
    /** \brief Name of a channel; an empty string if the id is unknown. */
    [[nodiscard]] QString channelName(int channelId) const;
    void setChannelName(int channelId, const QString &name);
    /** \brief Colour of a channel; an invalid QColor if the id is unknown. */
    [[nodiscard]] QColor channelColor(int channelId) const;
    void setChannelColor(int channelId, const QColor &color);
    [[nodiscard]] bool isChannelVisible(int channelId) const;
    void setChannelVisible(int channelId, bool visible);
    [[nodiscard]] double channelPenWidth(int channelId) const;
    void setChannelPenWidth(int channelId, double width);

    // --- Data: copied, so the caller's buffers may be freed after the call ---

    /**
     * \brief Replaces the data of a channel.
     *
     * \return false, with a warning and the previous data kept, if \p channelId is unknown,
     *         \p count is negative, a pointer is null while \p count > 0, or the timestamps decrease.
     *         A \p count of 0 clears the channel and returns true.
     */
    bool setChannelData(int channelId, const std::chrono::nanoseconds *t, const double *values, qsizetype count);
    [[nodiscard]] qsizetype channelSampleCount(int channelId) const;
    /** \brief True if any channel holds at least one sample. */
    [[nodiscard]] bool hasData() const;
    /** \brief Earliest timestamp over all channels (0 without data). */
    [[nodiscard]] std::chrono::nanoseconds dataStart() const;
    /** \brief Latest timestamp over all channels (0 without data). */
    [[nodiscard]] std::chrono::nanoseconds dataEnd() const;
    /**
     * \brief Nearest sample in time to \p t (on a tie, the earlier one); nullopt if \p t lies outside
     *        the channel's data or the id is unknown. The value may be non-finite.
     */
    [[nodiscard]] std::optional<Sample> nearestSample(int channelId, std::chrono::nanoseconds t) const;

    /** \brief Time gap that breaks a trace; 0 (default) means 4 x the EMA of the sampling interval. */
    [[nodiscard]] std::chrono::nanoseconds gapThreshold() const;
    void setGapThreshold(std::chrono::nanoseconds threshold);

    // --- View ---

    [[nodiscard]] std::chrono::nanoseconds xStart() const;
    [[nodiscard]] std::chrono::nanoseconds xEnd() const;
    /**
     * \brief Sets the visible time range, clamped to the limits: at least a few samples of the
     *        densest visible channel (never below 1 ns), at most the full data extent, and never more
     *        than one view width outside the data.
     */
    void setXRange(std::chrono::nanoseconds start, std::chrono::nanoseconds end);
    [[nodiscard]] double yMinimum() const;
    [[nodiscard]] double yMaximum() const;
    /** \brief Sets the visible value range; ignored unless both are finite and minimum < maximum. */
    void setYRange(double minimum, double maximum);

    // --- Appearance ---

    [[nodiscard]] bool isGridVisible() const;
    [[nodiscard]] bool isLegendVisible() const;
    [[nodiscard]] QColor gridColor() const;
    [[nodiscard]] QColor backgroundColor() const;
    [[nodiscard]] QColor bezelColor() const;
    [[nodiscard]] QColor textColor() const;
    [[nodiscard]] TimeLabelFormat timeLabelFormat() const;
    [[nodiscard]] QDateTime timeOrigin() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Removes the data of every channel; the channels stay (as StripChart::clear()). */
    void clear();
    /** \brief Shows the full data extent, with Y autoscaled on the visible finite data. */
    void resetView();
    void setGridVisible(bool visible);
    void setLegendVisible(bool visible);
    void setGridColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setBezelColor(const QColor &color);
    void setTextColor(const QColor &color);
    void setTimeLabelFormat(TimeLabelFormat format);
    void setTimeOrigin(const QDateTime &origin);

Q_SIGNALS:
    /** \brief The X or Y range changed (setters, reset, new data). */
    void viewChanged();
    /** \brief A channel was added or removed, or its data or metadata changed. */
    void channelsChanged();
    /** \brief An appearance property changed. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    [[nodiscard]] QRectF plotArea() const;
    void clampViewToData();

    std::unique_ptr<AnalysisChartPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AnalysisChart)
    friend class internal::AnalysisChartTestAccess;
};

} // namespace QtIndustrialWidgets
