// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/StripChart.h>
#include "internal/M4Decimator.h"
#include "internal/StripChartGeometry.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPixmap>
#include <QtCore/QtMath>
#include <QtCore/QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <vector>

namespace QtIndustrialWidgets {

struct ChannelInternal {
    QString name;
    QColor color;
    bool visible{true};
    double penWidth{1.8};
    size_t count{0};
    double latestValue{0.0};
    quint64 totalSamples{0};
    qint64 lastTimestamp{std::numeric_limits<qint64>::min()};
    quint64 rejectedSamples{0};
    double emaIntervalNs{0.0};
    bool hasEma{false};
    bool receivedArrivalClock{false};
    bool receivedExplicitTimestamp{false};
    bool warnedMixedClock{false};
    bool warnedIgnoredTimestamps{false};

    std::vector<qint64> timestamps;
    std::vector<double> values;
    size_t headIndex{0};

    internal::M4Decimator::IncrementalStream m4Stream;
};

class StripChartPrivate {
public:
    void resetChannelData(ChannelInternal &ch) {
        ch.headIndex = 0;
        ch.count = 0;
        ch.totalSamples = 0;
        ch.lastTimestamp = std::numeric_limits<qint64>::min();
        ch.latestValue = 0.0;
        ch.emaIntervalNs = 0.0;
        ch.hasEma = false;
        ch.receivedArrivalClock = false;
        ch.receivedExplicitTimestamp = false;
        ch.warnedMixedClock = false;
        ch.warnedIgnoredTimestamps = false;
        std::fill(ch.values.begin(), ch.values.end(), 0.0);
        std::fill(ch.timestamps.begin(), ch.timestamps.end(), 0);
        ch.m4Stream.reset();
    }

    int m_capacity{300};
    double m_yMinimum{0.0};
    double m_yMaximum{100.0};
    bool m_autoScaleY{false};
    bool m_autoScaleDirty{false};
    bool m_gridVisible{true};
    bool m_legendVisible{true};
    int m_horizontalDivisions{6};
    int m_verticalDivisions{8};

    StripChart::XAxisMode m_xAxisMode{StripChart::XAxisMode::SampleIndex};
    StripChart::DecimationMode m_decimationMode{StripChart::DecimationMode::Auto};
    StripChart::Interpolation m_interpolation{StripChart::Interpolation::Linear};
    StripChart::TimeLabelFormat m_timeLabelFormat{StripChart::TimeLabelFormat::Relative};
    std::chrono::nanoseconds m_timeSpan{std::chrono::seconds(10)};
    std::chrono::nanoseconds m_gapThreshold{0};
    QDateTime m_timeOrigin;
    QElapsedTimer m_arrivalTimer;
    bool m_warnedCapacityFull{false};

    QColor m_gridColor{QColor(42, 54, 70)};
    QColor m_backgroundColor{QColor(14, 18, 25)};
    QColor m_bezelColor{QColor(38, 46, 60)};
    QColor m_textColor{QColor(210, 220, 235)};

    std::vector<ChannelInternal> m_channels;

    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};

    [[nodiscard]] qint64 arrivalTimestampNs() {
        return m_arrivalTimer.nsecsElapsed();
    }

    bool insertSampleInternal(ChannelInternal &ch, int channelId, qint64 t, double value, bool isExplicit) {
        if (isExplicit) {
            ch.receivedExplicitTimestamp = true;
            if (ch.receivedArrivalClock && !ch.warnedMixedClock) {
                qWarning("StripChart: Channel %d received both arrival-clock (addDataPoint) and explicit timestamps (addSample). Arrival-clock timestamps reflect GUI arrival time, not acquisition time.", channelId);
                ch.warnedMixedClock = true;
            }
        } else {
            ch.receivedArrivalClock = true;
            if (ch.receivedExplicitTimestamp && !ch.warnedMixedClock) {
                qWarning("StripChart: Channel %d received both arrival-clock (addDataPoint) and explicit timestamps (addSample). Arrival-clock timestamps reflect GUI arrival time, not acquisition time.", channelId);
                ch.warnedMixedClock = true;
            }
        }

        // Monotonic non-decreasing check
        if (ch.totalSamples > 0 && t < ch.lastTimestamp) {
            ch.rejectedSamples++;
            if (ch.rejectedSamples == 1 || (ch.rejectedSamples % 1000) == 0) {
                qWarning("StripChart: Channel %d rejected out-of-order sample (t = %lld < lastT = %lld, total rejected: %llu)",
                         channelId, static_cast<long long>(t), static_cast<long long>(ch.lastTimestamp),
                         static_cast<unsigned long long>(ch.rejectedSamples));
            }
            return false;
        }

        if (ch.timestamps.size() != static_cast<size_t>(m_capacity)) {
            ch.timestamps.resize(static_cast<size_t>(m_capacity), 0);
            ch.values.resize(static_cast<size_t>(m_capacity), 0.0);
        }

        // Update Exponential Moving Average interval
        if (ch.totalSamples > 0 && t > ch.lastTimestamp) {
            qint64 dt = t - ch.lastTimestamp;
            if (!ch.hasEma) {
                ch.emaIntervalNs = static_cast<double>(dt);
                ch.hasEma = true;
            } else {
                ch.emaIntervalNs = 0.9 * ch.emaIntervalNs + 0.1 * static_cast<double>(dt);
            }
        }

        ch.timestamps[ch.headIndex] = t;
        ch.values[ch.headIndex] = value;
        ch.headIndex = (ch.headIndex + 1) % static_cast<size_t>(m_capacity);
        if (ch.count < static_cast<size_t>(m_capacity)) {
            ch.count++;
        }
        ch.totalSamples++;
        ch.lastTimestamp = t;
        ch.latestValue = value;

        m_autoScaleDirty = true;
        return true;
    }
};

StripChart::StripChart(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<StripChartPrivate>())
{
    d_ptr->m_arrivalTimer.start();
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

StripChart::~StripChart() = default;

int StripChart::capacity() const { Q_D(const StripChart); return d->m_capacity; }

double StripChart::yMinimum() const
{
    Q_D(const StripChart);
    if (d->m_autoScaleY && d->m_autoScaleDirty) {
        const_cast<StripChart*>(this)->updateAutoScaling();
    }
    return d->m_yMinimum;
}

double StripChart::yMaximum() const
{
    Q_D(const StripChart);
    if (d->m_autoScaleY && d->m_autoScaleDirty) {
        const_cast<StripChart*>(this)->updateAutoScaling();
    }
    return d->m_yMaximum;
}

bool StripChart::isAutoScaleY() const { Q_D(const StripChart); return d->m_autoScaleY; }
bool StripChart::isGridVisible() const { Q_D(const StripChart); return d->m_gridVisible; }
bool StripChart::isLegendVisible() const { Q_D(const StripChart); return d->m_legendVisible; }
QColor StripChart::gridColor() const { Q_D(const StripChart); return d->m_gridColor; }
QColor StripChart::backgroundColor() const { Q_D(const StripChart); return d->m_backgroundColor; }
QColor StripChart::bezelColor() const { Q_D(const StripChart); return d->m_bezelColor; }
int StripChart::horizontalDivisions() const { Q_D(const StripChart); return d->m_horizontalDivisions; }
int StripChart::verticalDivisions() const { Q_D(const StripChart); return d->m_verticalDivisions; }
int StripChart::channelCount() const { Q_D(const StripChart); return static_cast<int>(d->m_channels.size()); }

QString StripChart::channelName(int channelId) const
{
    Q_D(const StripChart);
    if (channelId >= 0 && channelId < static_cast<int>(d->m_channels.size())) {
        return d->m_channels[channelId].name;
    }
    return {};
}

QColor StripChart::channelColor(int channelId) const
{
    Q_D(const StripChart);
    if (channelId >= 0 && channelId < static_cast<int>(d->m_channels.size())) {
        return d->m_channels[channelId].color;
    }
    return {};
}

bool StripChart::isChannelVisible(int channelId) const
{
    Q_D(const StripChart);
    if (channelId >= 0 && channelId < static_cast<int>(d->m_channels.size())) {
        return d->m_channels[channelId].visible;
    }
    return false;
}

double StripChart::channelPenWidth(int channelId) const
{
    Q_D(const StripChart);
    if (channelId >= 0 && channelId < static_cast<int>(d->m_channels.size())) {
        return d->m_channels[channelId].penWidth;
    }
    return 0.0;
}

qsizetype StripChart::channelSampleCount(int channelId) const
{
    Q_D(const StripChart);
    if (channelId >= 0 && channelId < static_cast<int>(d->m_channels.size())) {
        return static_cast<qsizetype>(d->m_channels[channelId].count);
    }
    return 0;
}

double StripChart::channelLatestValue(int channelId) const
{
    Q_D(const StripChart);
    if (channelId >= 0 && channelId < static_cast<int>(d->m_channels.size())) {
        return d->m_channels[channelId].latestValue;
    }
    return 0.0;
}

StripChart::XAxisMode StripChart::xAxisMode() const
{
    Q_D(const StripChart);
    return d->m_xAxisMode;
}

void StripChart::setXAxisMode(XAxisMode mode)
{
    Q_D(StripChart);
    if (d->m_xAxisMode == mode) return;
    d->m_xAxisMode = mode;
    for (auto &ch : d->m_channels) {
        d->resetChannelData(ch);
    }
    d->m_autoScaleDirty = true;
    invalidateCache();
    Q_EMIT xAxisModeChanged(mode);
    update();
}

std::chrono::nanoseconds StripChart::timeSpan() const
{
    Q_D(const StripChart);
    return d->m_timeSpan;
}

void StripChart::setTimeSpan(std::chrono::nanoseconds span)
{
    Q_D(StripChart);
    if (span <= std::chrono::nanoseconds::zero() || d->m_timeSpan == span) return;
    d->m_timeSpan = span;
    invalidateCache();
    Q_EMIT timeSpanChanged(timeSpanSeconds());
    update();
}

double StripChart::timeSpanSeconds() const
{
    Q_D(const StripChart);
    return std::chrono::duration<double>(d->m_timeSpan).count();
}

void StripChart::setTimeSpanSeconds(double seconds)
{
    if (seconds <= 0.0) return;
    setTimeSpan(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(seconds)));
}

StripChart::DecimationMode StripChart::decimationMode() const
{
    Q_D(const StripChart);
    return d->m_decimationMode;
}

void StripChart::setDecimationMode(DecimationMode mode)
{
    Q_D(StripChart);
    if (d->m_decimationMode == mode) return;
    d->m_decimationMode = mode;
    Q_EMIT decimationModeChanged(mode);
    update();
}

StripChart::Interpolation StripChart::interpolation() const
{
    Q_D(const StripChart);
    return d->m_interpolation;
}

void StripChart::setInterpolation(Interpolation interp)
{
    Q_D(StripChart);
    if (d->m_interpolation == interp) return;
    d->m_interpolation = interp;
    Q_EMIT interpolationChanged(interp);
    update();
}

std::chrono::nanoseconds StripChart::gapThreshold() const
{
    Q_D(const StripChart);
    return d->m_gapThreshold;
}

void StripChart::setGapThreshold(std::chrono::nanoseconds threshold)
{
    Q_D(StripChart);
    d->m_gapThreshold = threshold;
    update();
}

QDateTime StripChart::timeOrigin() const
{
    Q_D(const StripChart);
    return d->m_timeOrigin;
}

void StripChart::setTimeOrigin(const QDateTime &origin)
{
    Q_D(StripChart);
    d->m_timeOrigin = origin;
    invalidateCache();
    update();
}

StripChart::TimeLabelFormat StripChart::timeLabelFormat() const
{
    Q_D(const StripChart);
    return d->m_timeLabelFormat;
}

void StripChart::setTimeLabelFormat(TimeLabelFormat format)
{
    Q_D(StripChart);
    if (d->m_timeLabelFormat == format) return;
    d->m_timeLabelFormat = format;
    invalidateCache();
    Q_EMIT timeLabelFormatChanged(format);
    update();
}

quint64 StripChart::rejectedSampleCount(int channelId) const
{
    Q_D(const StripChart);
    if (channelId >= 0 && channelId < static_cast<int>(d->m_channels.size())) {
        return d->m_channels[channelId].rejectedSamples;
    }
    return 0;
}

bool StripChart::isTimeWindowFullyCovered() const
{
    Q_D(const StripChart);
    if (d->m_xAxisMode == XAxisMode::SampleIndex) return true;

    qint64 tLatest = std::numeric_limits<qint64>::min();
    for (const auto &ch : d->m_channels) {
        if (ch.visible && ch.count > 0 && ch.lastTimestamp > tLatest) {
            tLatest = ch.lastTimestamp;
        }
    }

    if (tLatest == std::numeric_limits<qint64>::min()) return true;

    QRectF pRect = plotArea();
    int W_dev = std::max(1, static_cast<int>(std::round(pRect.width() * devicePixelRatioF())));
    auto win = internal::computeTimeWindow(tLatest, d->m_timeSpan.count(), W_dev);
    qint64 tStart = win.tStart;

    for (const auto &ch : d->m_channels) {
        if (!ch.visible || ch.count == 0) continue;
        if (ch.count == static_cast<size_t>(d->m_capacity)) {
            size_t oldestIdx = ch.headIndex % static_cast<size_t>(d->m_capacity);
            qint64 tOldest = ch.timestamps[oldestIdx];
            if (tOldest > tStart) {
                if (!d->m_warnedCapacityFull) {
                    qWarning("StripChart: buffer capacity (%d) is too small to cover the configured timeSpan (%lld ns). Increase setCapacity().",
                             d->m_capacity, static_cast<long long>(d->m_timeSpan.count()));
                    const_cast<StripChartPrivate*>(d)->m_warnedCapacityFull = true;
                }
                return false;
            }
        }
    }
    return true;
}

QSize StripChart::sizeHint() const
{
    return QSize(380, 220);
}

QSize StripChart::minimumSizeHint() const
{
    return QSize(180, 120);
}

int StripChart::addChannel(const QString &name, const QColor &color, double penWidth)
{
    ChannelInternal ch;
    ch.name = name;
    ch.color = color;
    ch.penWidth = penWidth;
    ch.visible = true;
    ch.count = 0;
    ch.latestValue = 0.0;
    ch.timestamps.resize(static_cast<size_t>(d_ptr->m_capacity), 0);
    ch.values.resize(static_cast<size_t>(d_ptr->m_capacity), 0.0);
    ch.headIndex = 0;

    d_ptr->m_channels.push_back(ch);
    invalidateCache();
    update();
    return static_cast<int>(d_ptr->m_channels.size()) - 1;
}

void StripChart::addDataPoint(int channelId, double value)
{
    if (channelId < 0 || channelId >= static_cast<int>(d_ptr->m_channels.size())) {
        return;
    }

    ChannelInternal &ch = d_ptr->m_channels[channelId];
    qint64 t = 0;
    if (d_ptr->m_xAxisMode == XAxisMode::Time) {
        t = d_ptr->arrivalTimestampNs();
    } else {
        t = static_cast<qint64>(ch.totalSamples);
    }

    bool inserted = d_ptr->insertSampleInternal(ch, channelId, t, value, false);
    if (!inserted) return;

    if (d_ptr->m_autoScaleY) {
        updateAutoScaling();
    }

    Q_EMIT dataAdded();
    update();
}

void StripChart::addDataPoints(const QVector<double> &values)
{
    int limit = std::min(static_cast<int>(values.size()), static_cast<int>(d_ptr->m_channels.size()));
    if (limit <= 0) return;

    bool anyInserted = false;
    qint64 arrivalT = (d_ptr->m_xAxisMode == XAxisMode::Time) ? d_ptr->arrivalTimestampNs() : 0;

    for (int i = 0; i < limit; ++i) {
        ChannelInternal &ch = d_ptr->m_channels[i];
        qint64 t = (d_ptr->m_xAxisMode == XAxisMode::Time) ? arrivalT : static_cast<qint64>(ch.totalSamples);
        if (d_ptr->insertSampleInternal(ch, i, t, values[i], false)) {
            anyInserted = true;
        }
    }

    if (anyInserted) {
        if (d_ptr->m_autoScaleY) {
            updateAutoScaling();
        }
        Q_EMIT dataAdded();
        update();
    }
}

void StripChart::addSample(int channelId, std::chrono::nanoseconds t, double value)
{
    if (channelId < 0 || channelId >= static_cast<int>(d_ptr->m_channels.size())) {
        return;
    }

    ChannelInternal &ch = d_ptr->m_channels[channelId];
    bool isSampleIndex = (d_ptr->m_xAxisMode == XAxisMode::SampleIndex);
    if (isSampleIndex && !ch.warnedIgnoredTimestamps) {
        qWarning("StripChart: Channel %d timestamps ignored in SampleIndex mode.", channelId);
        ch.warnedIgnoredTimestamps = true;
    }
    qint64 sampleT = isSampleIndex ? static_cast<qint64>(ch.totalSamples) : t.count();
    bool inserted = d_ptr->insertSampleInternal(ch, channelId, sampleT, value, !isSampleIndex);
    if (!inserted) return;

    if (d_ptr->m_autoScaleY) {
        updateAutoScaling();
    }

    Q_EMIT dataAdded();
    update();
}

void StripChart::addSamples(int channelId, const std::chrono::nanoseconds *t,
                            const double *values, qsizetype count)
{
    if (channelId < 0 || channelId >= static_cast<int>(d_ptr->m_channels.size())) {
        return;
    }
    if (!t || !values || count <= 0) {
        return;
    }

    ChannelInternal &ch = d_ptr->m_channels[channelId];
    bool isSampleIndex = (d_ptr->m_xAxisMode == XAxisMode::SampleIndex);
    if (isSampleIndex && !ch.warnedIgnoredTimestamps) {
        qWarning("StripChart: Channel %d timestamps ignored in SampleIndex mode.", channelId);
        ch.warnedIgnoredTimestamps = true;
    }

    bool anyInserted = false;
    for (qsizetype i = 0; i < count; ++i) {
        qint64 sampleT = isSampleIndex ? static_cast<qint64>(ch.totalSamples) : t[i].count();
        if (d_ptr->insertSampleInternal(ch, channelId, sampleT, values[i], !isSampleIndex)) {
            anyInserted = true;
        }
    }

    if (anyInserted) {
        if (d_ptr->m_autoScaleY) {
            updateAutoScaling();
        }
        Q_EMIT dataAdded();
        update();
    }
}

void StripChart::addUniformSamples(int channelId, std::chrono::nanoseconds t0,
                                   std::chrono::nanoseconds dt,
                                   const double *values, qsizetype count)
{
    if (channelId < 0 || channelId >= static_cast<int>(d_ptr->m_channels.size())) {
        return;
    }
    if (!values || count <= 0 || dt.count() <= 0) {
        return;
    }

    ChannelInternal &ch = d_ptr->m_channels[channelId];
    bool isSampleIndex = (d_ptr->m_xAxisMode == XAxisMode::SampleIndex);
    if (isSampleIndex && !ch.warnedIgnoredTimestamps) {
        qWarning("StripChart: Channel %d timestamps ignored in SampleIndex mode.", channelId);
        ch.warnedIgnoredTimestamps = true;
    }

    bool anyInserted = false;
    qint64 curT = t0.count();
    qint64 step = dt.count();

    for (qsizetype i = 0; i < count; ++i) {
        qint64 sampleT = isSampleIndex ? static_cast<qint64>(ch.totalSamples) : curT;
        if (d_ptr->insertSampleInternal(ch, channelId, sampleT, values[i], !isSampleIndex)) {
            anyInserted = true;
        }
        curT += step;
    }

    if (anyInserted) {
        if (d_ptr->m_autoScaleY) {
            updateAutoScaling();
        }
        Q_EMIT dataAdded();
        update();
    }
}

void StripChart::addSynchronousSamples(std::chrono::nanoseconds t,
                                       const QVector<double> &values)
{
    int limit = std::min(static_cast<int>(values.size()), static_cast<int>(d_ptr->m_channels.size()));
    if (limit <= 0) return;

    bool isSampleIndex = (d_ptr->m_xAxisMode == XAxisMode::SampleIndex);
    bool anyInserted = false;
    for (int i = 0; i < limit; ++i) {
        ChannelInternal &ch = d_ptr->m_channels[i];
        if (isSampleIndex && !ch.warnedIgnoredTimestamps) {
            qWarning("StripChart: Channel %d timestamps ignored in SampleIndex mode.", i);
            ch.warnedIgnoredTimestamps = true;
        }
        qint64 sampleT = isSampleIndex ? static_cast<qint64>(ch.totalSamples) : t.count();
        if (d_ptr->insertSampleInternal(ch, i, sampleT, values[i], !isSampleIndex)) {
            anyInserted = true;
        }
    }

    if (anyInserted) {
        if (d_ptr->m_autoScaleY) {
            updateAutoScaling();
        }
        Q_EMIT dataAdded();
        update();
    }
}

void StripChart::clear()
{
    for (auto &ch : d_ptr->m_channels) {
        d_ptr->resetChannelData(ch);
    }
    d_ptr->m_autoScaleDirty = true;
    update();
}

void StripChart::setCapacity(int count)
{
    // Capacity clamped to [10, 2^24 = 16,777,216] samples per channel (16 bytes/sample/channel)
    int c = std::clamp(count, 10, 16777216);
    if (d_ptr->m_capacity == c) return;
    d_ptr->m_capacity = c;

    for (auto &ch : d_ptr->m_channels) {
        ch.timestamps.resize(static_cast<size_t>(d_ptr->m_capacity), 0);
        ch.values.resize(static_cast<size_t>(d_ptr->m_capacity), 0.0);
        ch.headIndex = 0;
        ch.count = 0;
    }

    Q_EMIT capacityChanged(d_ptr->m_capacity);
    update();
}

void StripChart::setYMinimum(double min)
{
    setYRange(min, d_ptr->m_yMaximum);
}

void StripChart::setYMaximum(double max)
{
    setYRange(d_ptr->m_yMinimum, max);
}

void StripChart::setYRange(double min, double max)
{
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max) return;
    if (qFuzzyCompare(min, d_ptr->m_yMinimum) && qFuzzyCompare(max, d_ptr->m_yMaximum)) return;

    d_ptr->m_yMinimum = min;
    d_ptr->m_yMaximum = max;
    invalidateCache();
    Q_EMIT yRangeChanged(d_ptr->m_yMinimum, d_ptr->m_yMaximum);
    update();
}

void StripChart::setAutoScaleY(bool autoScale)
{
    if (d_ptr->m_autoScaleY == autoScale) return;
    d_ptr->m_autoScaleY = autoScale;
    if (d_ptr->m_autoScaleY) {
        updateAutoScaling();
    }
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setGridVisible(bool visible)
{
    if (d_ptr->m_gridVisible == visible) return;
    d_ptr->m_gridVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setLegendVisible(bool visible)
{
    if (d_ptr->m_legendVisible == visible) return;
    d_ptr->m_legendVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setGridColor(const QColor &color)
{
    if (d_ptr->m_gridColor == color) return;
    d_ptr->m_gridColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setBackgroundColor(const QColor &color)
{
    if (d_ptr->m_backgroundColor == color) return;
    d_ptr->m_backgroundColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setBezelColor(const QColor &color)
{
    if (d_ptr->m_bezelColor == color) return;
    d_ptr->m_bezelColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setHorizontalDivisions(int divisions)
{
    int d = std::clamp(divisions, 2, 20);
    if (d_ptr->m_horizontalDivisions == d) return;
    d_ptr->m_horizontalDivisions = d;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setVerticalDivisions(int divisions)
{
    int d = std::clamp(divisions, 2, 30);
    if (d_ptr->m_verticalDivisions == d) return;
    d_ptr->m_verticalDivisions = d;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setChannelVisible(int channelId, bool visible)
{
    if (channelId >= 0 && channelId < static_cast<int>(d_ptr->m_channels.size())) {
        if (d_ptr->m_channels[channelId].visible != visible) {
            d_ptr->m_channels[channelId].visible = visible;
            update();
        }
    }
}

void StripChart::setChannelColor(int channelId, const QColor &color)
{
    if (channelId >= 0 && channelId < static_cast<int>(d_ptr->m_channels.size())) {
        d_ptr->m_channels[channelId].color = color;
        update();
    }
}

void StripChart::updateAutoScaling()
{
    bool hasData = false;
    double minVal = std::numeric_limits<double>::infinity();
    double maxVal = -std::numeric_limits<double>::infinity();

    qint64 timeStart = 0;
    if (d_ptr->m_xAxisMode == XAxisMode::Time) {
        qint64 tLatest = std::numeric_limits<qint64>::min();
        for (const auto &ch : d_ptr->m_channels) {
            if (ch.visible && ch.count > 0 && ch.lastTimestamp > tLatest) {
                tLatest = ch.lastTimestamp;
            }
        }
        if (tLatest == std::numeric_limits<qint64>::min()) {
            tLatest = d_ptr->arrivalTimestampNs();
        }
        QRectF pRect = plotArea();
        int W_dev = std::max(1, static_cast<int>(std::round(pRect.width() * devicePixelRatioF())));
        auto win = internal::computeTimeWindow(tLatest, d_ptr->m_timeSpan.count(), W_dev);
        timeStart = win.tStart;
    }

    for (const auto &ch : d_ptr->m_channels) {
        if (!ch.visible || ch.count == 0) continue;

        if (d_ptr->m_xAxisMode == XAxisMode::Time) {
            size_t start = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;

            for (size_t i = 0; i < ch.count; ++i) {
                size_t bufIdx = (start + i) % static_cast<size_t>(d_ptr->m_capacity);
                qint64 t = ch.timestamps[bufIdx];
                if (t < timeStart) continue;
                double v = ch.values[bufIdx];
                if (!std::isfinite(v)) continue;
                hasData = true;
                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
            }
        } else {
            for (size_t i = 0; i < ch.count; ++i) {
                double v = ch.values[i];
                if (!std::isfinite(v)) continue;
                hasData = true;
                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
            }
        }
    }

    if (hasData) {
        if (qFuzzyCompare(minVal, maxVal)) {
            minVal -= 1.0;
            maxVal += 1.0;
        } else {
            double margin = (maxVal - minVal) * 0.1;
            minVal -= margin;
            maxVal += margin;
        }
        setYRange(minVal, maxVal);
    }

    d_ptr->m_autoScaleDirty = false;
}

QRectF StripChart::plotArea() const
{
    double leftMargin = 42.0; // Room for Y-axis labels
    double rightMargin = 12.0;
    double topMargin = d_ptr->m_legendVisible ? 30.0 : 12.0;
    double bottomMargin = (d_ptr->m_xAxisMode == XAxisMode::Time) ? 26.0 : 16.0;

    double w = std::max(10.0, width() - leftMargin - rightMargin);
    double h = std::max(10.0, height() - topMargin - bottomMargin);

    return QRectF(leftMargin, topMargin, w, h);
}

void StripChart::invalidateCache()
{
    d_ptr->m_cacheDirty = true;
}

void StripChart::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void StripChart::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        invalidateCache();
        update();
    }
    QWidget::changeEvent(event);
}

static qint64 calculate125TickStep(qint64 spanNs, int targetDivs)
{
    if (spanNs <= 0 || targetDivs <= 0) return 1;
    double rawStep = static_cast<double>(spanNs) / targetDivs;
    double expVal = std::floor(std::log10(rawStep));
    double base = std::pow(10.0, expVal);
    double fraction = rawStep / base;

    double mult = 1.0;
    if (fraction < 1.5) {
        mult = 1.0;
    } else if (fraction < 3.5) {
        mult = 2.0;
    } else if (fraction < 7.5) {
        mult = 5.0;
    } else {
        mult = 10.0;
    }

    qint64 step = static_cast<qint64>(mult * base);
    return std::max(qint64(1), step);
}

void StripChart::renderStaticGrid(const QSize &targetSize)
{
    qreal dpr = devicePixelRatioF();
    QSize pixmapSize = (QSizeF(targetSize) * dpr).toSize();
    if (pixmapSize.isEmpty()) return;

    d_ptr->m_cachePixmap = QPixmap(pixmapSize);
    d_ptr->m_cachePixmap.setDevicePixelRatio(dpr);
    d_ptr->m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&d_ptr->m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = targetSize.width();
    const double h = targetSize.height();

    // 1. Outer Bezel & Metal Chassis
    QRectF outerRect(1.0, 1.0, w - 2.0, h - 2.0);
    painter.setPen(QPen(d_ptr->m_bezelColor.darker(170), 1.5));
    painter.setBrush(d_ptr->m_bezelColor);
    painter.drawRoundedRect(outerRect, 6.0, 6.0);

    // 2. Oscilloscope Plot Screen (Dark recessed glass)
    const QRectF plotRect = plotArea();

    QRectF screenRect(plotRect.left() - 4.0, plotRect.top() - 4.0,
                      plotRect.width() + 8.0, plotRect.height() + 8.0);

    painter.setPen(QPen(d_ptr->m_bezelColor.darker(190), 1.2));
    painter.setBrush(d_ptr->m_backgroundColor);
    painter.drawRoundedRect(screenRect, 4.0, 4.0);

    // 3. Grid Reticle
    if (d_ptr->m_gridVisible && plotRect.width() > 10.0 && plotRect.height() > 10.0) {
        QPen fineGridPen(d_ptr->m_gridColor, 1.0, Qt::DotLine);
        QPen solidGridPen(d_ptr->m_gridColor.lighter(130), 1.0, Qt::SolidLine);

        int fontSize = 9;
        QFont font = painter.font();
        font.setPixelSize(fontSize);
        painter.setFont(font);

        // Vertical division lines
        if (d_ptr->m_xAxisMode == XAxisMode::SampleIndex) {
            for (int i = 0; i <= d_ptr->m_verticalDivisions; ++i) {
                double x = plotRect.left() + (static_cast<double>(i) / d_ptr->m_verticalDivisions) * plotRect.width();
                bool isCenter = (i == d_ptr->m_verticalDivisions / 2);
                painter.setPen(isCenter ? solidGridPen : fineGridPen);
                painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
            }
        } else if (d_ptr->m_xAxisMode == XAxisMode::Time && d_ptr->m_timeLabelFormat == TimeLabelFormat::Relative) {
            // 1-2-5 Tick Selection for Relative Time
            qint64 spanNs = d_ptr->m_timeSpan.count();
            qint64 tickStepNs = calculate125TickStep(spanNs, 8);

            QString unit = QStringLiteral("s");
            double divisor = 1e9;
            if (spanNs < 2000) {
                unit = QStringLiteral("ns");
                divisor = 1.0;
            } else if (spanNs < 2000000) {
                unit = QStringLiteral("µs");
                divisor = 1e3;
            } else if (spanNs < 2000000000LL) {
                unit = QStringLiteral("ms");
                divisor = 1e6;
            } else if (spanNs < 120000000000LL) {
                unit = QStringLiteral("s");
                divisor = 1e9;
            } else {
                unit = QStringLiteral("min");
                divisor = 60e9;
            }

            for (qint64 tOffset = 0; tOffset <= spanNs; tOffset += tickStepNs) {
                double frac = 1.0 - (static_cast<double>(tOffset) / static_cast<double>(spanNs));
                double x = plotRect.left() + frac * plotRect.width();

                painter.setPen(fineGridPen);
                painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));

                double relVal = -static_cast<double>(tOffset) / divisor;
                QString labelStr = QString::number(relVal, 'f', (std::abs(relVal) >= 10.0 || qFuzzyIsNull(std::remainder(relVal, 1.0))) ? 0 : 1)
                                   + QLatin1Char(' ') + unit;

                QRectF labelRect(x - 30.0, plotRect.bottom() + 3.0, 60.0, fontSize * 1.4);
                painter.setPen(d_ptr->m_textColor);
                painter.drawText(labelRect, Qt::AlignCenter, labelStr);
            }
        }

        // Horizontal division lines & Y-axis labels
        for (int i = 0; i <= d_ptr->m_horizontalDivisions; ++i) {
            double frac = static_cast<double>(i) / d_ptr->m_horizontalDivisions;
            double y = plotRect.bottom() - frac * plotRect.height();
            double val = d_ptr->m_yMinimum + frac * (d_ptr->m_yMaximum - d_ptr->m_yMinimum);

            bool isZero = qFuzzyIsNull(val);
            painter.setPen(isZero ? QPen(d_ptr->m_gridColor.lighter(170), 1.2) : fineGridPen);
            painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));

            // Y label on the left
            QString labelStr = QString::number(val, 'f', (std::abs(val) >= 100.0) ? 0 : 1);
            QRectF labelRect(0.0, y - fontSize * 0.7, plotRect.left() - 6.0, fontSize * 1.4);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, labelStr);
        }
    }

    d_ptr->m_cacheDirty = false;
}

void StripChart::paintEvent(QPaintEvent *)
{
    if (d_ptr->m_autoScaleY && d_ptr->m_autoScaleDirty) {
        updateAutoScaling();
    }

    if (d_ptr->m_cacheDirty || d_ptr->m_cachePixmap.size() != (QSizeF(size()) * devicePixelRatioF()).toSize()) {
        renderStaticGrid(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast Blit of Cached Grid & Chassis
    painter.drawPixmap(0, 0, d_ptr->m_cachePixmap);

    const QRectF plotRect = plotArea();
    if (plotRect.width() <= 10.0 || plotRect.height() <= 10.0) return;

    qreal dpr = devicePixelRatioF();
    int W_dev = std::max(1, static_cast<int>(std::round(plotRect.width() * dpr)));

    double yRange = d_ptr->m_yMaximum - d_ptr->m_yMinimum;
    if (qFuzzyIsNull(yRange)) yRange = 1.0;

    // Dynamic layer for Absolute Time grid lines & labels
    if (d_ptr->m_gridVisible && d_ptr->m_xAxisMode == XAxisMode::Time && d_ptr->m_timeLabelFormat == TimeLabelFormat::Absolute) {
        qint64 tLatest = std::numeric_limits<qint64>::min();
        for (const auto &ch : d_ptr->m_channels) {
            if (ch.visible && ch.count > 0 && ch.lastTimestamp > tLatest) {
                tLatest = ch.lastTimestamp;
            }
        }
        if (tLatest == std::numeric_limits<qint64>::min()) {
            tLatest = d_ptr->arrivalTimestampNs();
        }

        qint64 spanNs = d_ptr->m_timeSpan.count();
        auto win = internal::computeTimeWindow(tLatest, spanNs, W_dev);
        qint64 tEnd = win.tEnd;
        qint64 tStart = win.tStart;

        qint64 tickStepNs = calculate125TickStep(spanNs, 8);
        qint64 firstTick = internal::M4Decimator::floorDiv(tStart + tickStepNs - 1, tickStepNs) * tickStepNs;

        QPen fineGridPen(d_ptr->m_gridColor, 1.0, Qt::DotLine);
        int fontSize = 9;
        QFont font = painter.font();
        font.setPixelSize(fontSize);
        painter.setFont(font);

        for (qint64 tick = firstTick; tick <= tEnd; tick += tickStepNs) {
            double frac = static_cast<double>(tick - tStart) / static_cast<double>(spanNs);
            double x = plotRect.left() + frac * plotRect.width();

            painter.setPen(fineGridPen);
            painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));

            QString labelStr;
            if (d_ptr->m_timeOrigin.isValid()) {
                QDateTime dt = d_ptr->m_timeOrigin.addMSecs(tick / 1000000);
                labelStr = dt.toString(spanNs < 10000000000LL ? QStringLiteral("HH:mm:ss.zzz") : QStringLiteral("HH:mm:ss"));
            } else {
                labelStr = QStringLiteral("%1 s").arg(static_cast<double>(tick) / 1e9, 0, 'f', 1);
            }

            QRectF labelRect(x - 40.0, plotRect.bottom() + 3.0, 80.0, fontSize * 1.4);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(labelRect, Qt::AlignCenter, labelStr);
        }
    }

    // 2. Plot Real-Time Channel Waveforms
    painter.save();
    painter.setClipRect(plotRect);

    if (d_ptr->m_xAxisMode == XAxisMode::Time) {
        // Find latest timestamp across visible channels
        qint64 tLatest = std::numeric_limits<qint64>::min();
        for (const auto &ch : d_ptr->m_channels) {
            if (ch.visible && ch.count > 0 && ch.lastTimestamp > tLatest) {
                tLatest = ch.lastTimestamp;
            }
        }
        if (tLatest == std::numeric_limits<qint64>::min()) {
            tLatest = d_ptr->arrivalTimestampNs();
        }

        qint64 timeSpanNs = d_ptr->m_timeSpan.count();

        // TODO: Display-clock-driven scrolling with fixed latency for block-delivered DAQ data
        auto win = internal::computeTimeWindow(tLatest, timeSpanNs, W_dev);
        qint64 tStart = win.tStart;
        qint64 dt_px = win.dtPx;

        for (const auto &ch : d_ptr->m_channels) {
            if (!ch.visible || ch.count < 1) continue;

            size_t start = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;

            // Binary search for first visible sample with t >= tStart
            size_t low = 0;
            size_t high = ch.count - 1;
            size_t firstIdx = ch.count;
            while (low <= high) {
                size_t mid = low + (high - low) / 2;
                qint64 tMid = ch.timestamps[(start + mid) % static_cast<size_t>(d_ptr->m_capacity)];
                if (tMid >= tStart) {
                    firstIdx = mid;
                    if (mid == 0) break;
                    high = mid - 1;
                } else {
                    low = mid + 1;
                }
            }

            if (firstIdx > 0) {
                firstIdx--; // One sample before visible window for smooth trace entry
            }

            size_t visibleSamples = ch.count - firstIdx;
            if (visibleSamples == 0) continue;

            std::vector<qint64> visT(visibleSamples);
            std::vector<double> visY(visibleSamples);
            for (size_t j = 0; j < visibleSamples; ++j) {
                size_t idx = (start + firstIdx + j) % static_cast<size_t>(d_ptr->m_capacity);
                visT[j] = ch.timestamps[idx];
                visY[j] = ch.values[idx];
            }

            bool doDecimate = false;
            if (d_ptr->m_decimationMode == DecimationMode::Always) {
                doDecimate = true;
            } else if (d_ptr->m_decimationMode == DecimationMode::Auto) {
                doDecimate = (visibleSamples > static_cast<size_t>(W_dev));
            }

            qint64 gapThresh = d_ptr->m_gapThreshold.count();
            if (gapThresh == 0 && ch.hasEma && ch.emaIntervalNs > 0.0) {
                gapThresh = static_cast<qint64>(4.0 * ch.emaIntervalNs);
            }

            auto mapPoint = [&](qint64 t, double val) -> QPointF {
                double xFrac = static_cast<double>(t - tStart) / static_cast<double>(timeSpanNs);
                double x = plotRect.left() + xFrac * plotRect.width();
                double yNorm = (val - d_ptr->m_yMinimum) / yRange;
                double y = plotRect.bottom() - yNorm * plotRect.height();
                return QPointF(x, y);
            };

            QPen tracePen;
            if (doDecimate) {
                tracePen = QPen(ch.color, 0.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
                tracePen.setCosmetic(true);
                painter.setRenderHint(QPainter::Antialiasing, false);
            } else {
                tracePen = QPen(ch.color, ch.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter.setRenderHint(QPainter::Antialiasing, true);
            }
            painter.setPen(tracePen);

            QPointF lastDrawnPt;
            bool hasDrawnPt = false;

            if (doDecimate) {
                auto segments = internal::M4Decimator::decimateToSegments(
                    visT.data(), visY.data(), visibleSamples, dt_px, win.kStart, win.numBuckets, gapThresh);

                for (const auto &seg : segments) {
                    if (seg.empty()) continue;
                    QPolygonF poly;
                    poly.reserve(static_cast<int>(seg.size()));
                    for (const auto &pt : seg) {
                        poly << mapPoint(pt.t, pt.value);
                    }
                    painter.drawPolyline(poly);
                    lastDrawnPt = poly.last();
                    hasDrawnPt = true;
                }
            } else {
                // Raw drawing
                QPolygonF currentPoly;
                qint64 prevT = std::numeric_limits<qint64>::min();

                for (size_t j = 0; j < visibleSamples; ++j) {
                    double v = visY[j];
                    qint64 t = visT[j];

                    if (!std::isfinite(v)) {
                        if (!currentPoly.isEmpty()) {
                            painter.drawPolyline(currentPoly);
                            lastDrawnPt = currentPoly.last();
                            hasDrawnPt = true;
                            currentPoly.clear();
                        }
                        prevT = std::numeric_limits<qint64>::min();
                        continue;
                    }

                    if (prevT != std::numeric_limits<qint64>::min() && gapThresh > 0 && (t - prevT > gapThresh)) {
                        if (!currentPoly.isEmpty()) {
                            painter.drawPolyline(currentPoly);
                            lastDrawnPt = currentPoly.last();
                            hasDrawnPt = true;
                            currentPoly.clear();
                        }
                    }

                    QPointF pt = mapPoint(t, v);
                    if (d_ptr->m_interpolation == Interpolation::Step && !currentPoly.isEmpty()) {
                        currentPoly << QPointF(pt.x(), currentPoly.last().y());
                    }
                    currentPoly << pt;
                    prevT = t;
                }

                if (!currentPoly.isEmpty()) {
                    painter.drawPolyline(currentPoly);
                    lastDrawnPt = currentPoly.last();
                    hasDrawnPt = true;
                }
            }

            // Restore antialiasing for downstream elements (glow dot, legend)
            painter.setRenderHint(QPainter::Antialiasing, true);

            // Glow dot on latest point if finite
            if (hasDrawnPt && std::isfinite(ch.latestValue)) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(ch.color.lighter(150));
                painter.drawEllipse(lastDrawnPt, 2.5, 2.5);
            }
        }
    } else {
        // SampleIndex Mode (Legacy compatibility)
        for (const auto &ch : d_ptr->m_channels) {
            if (!ch.visible || ch.count < 1) continue;

            size_t start = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;

            bool doDecimate = false;
            if (d_ptr->m_decimationMode == DecimationMode::Always) {
                doDecimate = true;
            } else if (d_ptr->m_decimationMode == DecimationMode::Auto) {
                doDecimate = (ch.count > static_cast<size_t>(W_dev));
            }

            QPen tracePen;
            if (doDecimate) {
                tracePen = QPen(ch.color, 0.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
                tracePen.setCosmetic(true);
                painter.setRenderHint(QPainter::Antialiasing, false);
            } else {
                tracePen = QPen(ch.color, ch.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter.setRenderHint(QPainter::Antialiasing, true);
            }
            painter.setPen(tracePen);

            QPointF lastDrawnPt;
            bool hasDrawnPt = false;

            if (doDecimate) {
                std::vector<qint64> sampleIndices(ch.count);
                std::vector<double> sampleValues(ch.count);
                quint64 startCounter = ch.totalSamples >= ch.count ? (ch.totalSamples - ch.count) : 0;
                for (size_t i = 0; i < ch.count; ++i) {
                    size_t bufIdx = (start + i) % static_cast<size_t>(d_ptr->m_capacity);
                    sampleIndices[i] = static_cast<qint64>(startCounter + i);
                    sampleValues[i] = ch.values[bufIdx];
                }

                auto ib = internal::computeIndexBuckets(ch.totalSamples, ch.count, d_ptr->m_capacity, W_dev);
                auto segments = internal::M4Decimator::decimateToSegments(
                    sampleIndices.data(), sampleValues.data(), ch.count, ib.samplesPerBucket, ib.kStart, ib.numBuckets, 0);

                for (const auto &seg : segments) {
                    if (seg.empty()) continue;
                    QPolygonF poly;
                    poly.reserve(static_cast<int>(seg.size()));
                    for (const auto &pt : seg) {
                        double frac = (ch.count <= 1) ? 0.0 : static_cast<double>(pt.t - startCounter) / (d_ptr->m_capacity - 1);
                        double x = plotRect.left() + frac * plotRect.width();
                        double yNorm = (pt.value - d_ptr->m_yMinimum) / yRange;
                        double y = plotRect.bottom() - yNorm * plotRect.height();
                        poly << QPointF(x, y);
                    }
                    painter.drawPolyline(poly);
                    lastDrawnPt = poly.last();
                    hasDrawnPt = true;
                }
            } else {
                QPolygonF currentPoly;
                for (size_t i = 0; i < ch.count; ++i) {
                    size_t bufIdx = (start + i) % static_cast<size_t>(d_ptr->m_capacity);
                    double val = ch.values[bufIdx];

                    if (!std::isfinite(val)) {
                        if (!currentPoly.isEmpty()) {
                            painter.drawPolyline(currentPoly);
                            lastDrawnPt = currentPoly.last();
                            hasDrawnPt = true;
                            currentPoly.clear();
                        }
                        continue;
                    }

                    double frac = (d_ptr->m_capacity <= 1) ? 0.0 : (static_cast<double>(i) / (d_ptr->m_capacity - 1));
                    double x = plotRect.left() + frac * plotRect.width();
                    double yNorm = (val - d_ptr->m_yMinimum) / yRange;
                    double y = plotRect.bottom() - yNorm * plotRect.height();
                    y = std::clamp(y, plotRect.top(), plotRect.bottom());
                    QPointF pt(x, y);

                    if (d_ptr->m_interpolation == Interpolation::Step && !currentPoly.isEmpty()) {
                        currentPoly << QPointF(pt.x(), currentPoly.last().y());
                    }
                    currentPoly << pt;
                }

                if (!currentPoly.isEmpty()) {
                    painter.drawPolyline(currentPoly);
                    lastDrawnPt = currentPoly.last();
                    hasDrawnPt = true;
                }
            }

            // Restore antialiasing for downstream elements (glow dot, legend)
            painter.setRenderHint(QPainter::Antialiasing, true);

            if (hasDrawnPt && std::isfinite(ch.latestValue)) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(ch.color.lighter(150));
                painter.drawEllipse(lastDrawnPt, 2.5, 2.5);
            }
        }
    }

    painter.restore();

    // 3. Channel Legend and Real-Time Readouts (Top Bar)
    if (d_ptr->m_legendVisible && !d_ptr->m_channels.empty()) {
        double curX = plotRect.left() + 4.0;
        int fontSize = 10;
        QFont f = font();
        f.setPixelSize(fontSize);
        f.setBold(true);
        painter.setFont(f);
        QFontMetricsF fm(f);

        for (const auto &ch : d_ptr->m_channels) {
            if (!ch.visible) continue;

            // Channel color swatch
            painter.setPen(Qt::NoPen);
            painter.setBrush(ch.color);
            painter.drawRoundedRect(QRectF(curX, 10.0, 10.0, 10.0), 2.0, 2.0);
            curX += 14.0;

            // Channel name and latest numeric readout ("---" if non-finite)
            QString valStr = std::isfinite(ch.latestValue) ? QString::number(ch.latestValue, 'f', 1) : QStringLiteral("---");
            QString txt = QStringLiteral("%1: %2").arg(ch.name, valStr);
            QRectF txtRect = fm.boundingRect(txt);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(QPointF(curX, 19.0), txt);

            curX += txtRect.width() + 18.0;
            if (curX > plotRect.right() - 20.0) break; // Don't overflow
        }
    }
}

} // namespace QtIndustrialWidgets
