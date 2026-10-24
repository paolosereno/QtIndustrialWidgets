// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/StripChart.h>

#include "internal/FontScale.h"
#include "internal/NumberFormat.h"
#include "internal/StripChart_p.h"
#include "internal/M4Decimator.h"
#include "internal/StripChartGeometry.h"
#include "internal/DecimatedTrace.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPixmap>
#include <QtCore/QtMath>
#include <QtCore/QElapsedTimer>
#include <QtCore/QVarLengthArray>
#include <QtWidgets/QApplication>
#include <algorithm>
#include <cmath>
#include <vector>

namespace QtIndustrialWidgets {

namespace {

constexpr double kGlowRadius = 2.5; // glow dot radius on the latest point, logical px

using BlankColumns = QVarLengthArray<QRectF, 16>;

// Collects the device columns (in logical coordinates) of blank buckets - buckets with a
// non-finite sample - that a glow dot centred in bucket kCentre could reach. Every bucket occupies
// exactly one device column; \p columnX returns its logical [left, right). The glow dot is clipped
// so it never draws inside a column left blank for a sensor dropout.
template <typename ColumnX>
void collectBlankColumnsNearGlow(const internal::M4Decimator::IncrementalStream &stream, qint64 kCentre,
                                 qint64 kFirst, qint64 kEnd, double dpr,
                                 const QRectF &plotRect, ColumnX columnX, BlankColumns &out)
{
    const qint64 reach = static_cast<qint64>(std::ceil(kGlowRadius * dpr)) + 1;
    for (qint64 k = std::max(kFirst, kCentre - reach); k <= std::min(kEnd - 1, kCentre + reach); ++k) {
        const internal::M4Bucket *b = stream.bucketAt(k);
        if (!b || !b->hasNonFinite) continue;
        const std::pair<double, double> x = columnX(k);
        out.append(QRectF(x.first, plotRect.top(), x.second - x.first, plotRect.height()));
    }
}

void drawGlowDot(QPainter &painter, const QPointF &centre, const QColor &colour,
                 const QRectF &plotRect, const BlankColumns &blankColumns)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour.lighter(150));
    if (blankColumns.isEmpty()) {
        painter.drawEllipse(centre, kGlowRadius, kGlowRadius);
        return;
    }
    QPainterPath keep;
    keep.addRect(plotRect);
    for (const QRectF &r : blankColumns) {
        QPainterPath blank;
        blank.addRect(r);
        keep = keep.subtracted(blank);
    }
    painter.save();
    painter.setClipPath(keep, Qt::IntersectClip);
    painter.drawEllipse(centre, kGlowRadius, kGlowRadius);
    painter.restore();
}

} // namespace

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
        // ensureAutoScale() is called from const getters through the const_cast pattern
        // so that programmatic queries reflect recent data immediately without waiting for paintEvent.
        const_cast<StripChart*>(this)->ensureAutoScale();
    }
    return d->m_yMinimum;
}

double StripChart::yMaximum() const
{
    Q_D(const StripChart);
    if (d->m_autoScaleY && d->m_autoScaleDirty) {
        // ensureAutoScale() is called from const getters through the const_cast pattern
        // so that programmatic queries reflect recent data immediately without waiting for paintEvent.
        const_cast<StripChart*>(this)->ensureAutoScale();
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
    d->invalidateGeometry();
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
    d->invalidateGeometry();
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
    d_ptr->m_autoScaleInitialized = false;
    d_ptr->m_shrinkCounter = 0;
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
    d_ptr->invalidateGeometry();
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
        d_ptr->m_autoScaleDirty = true;
        d_ptr->m_autoScaleInitialized = false;
        d_ptr->m_shrinkCounter = 0;
        ensureAutoScale();
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
            if (visible) {
                d_ptr->m_channels[channelId].geometryValid = false;
            }
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

void StripChart::ensureAutoScale()
{
    if (!d_ptr->m_autoScaleY || !d_ptr->m_autoScaleDirty) {
        return;
    }
    d_ptr->m_autoScaleDirty = false;

    bool hasData = false;
    double minVal = std::numeric_limits<double>::infinity();
    double maxVal = -std::numeric_limits<double>::infinity();

    QRectF pRect = plotArea();
    int W_dev = std::max(1, static_cast<int>(std::round(pRect.width() * devicePixelRatioF())));

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

        auto win = internal::computeTimeWindow(tLatest, d_ptr->m_timeSpan.count(), W_dev);
        qint64 tStart = win.tStart;
        const internal::BucketGrid &grid = win.grid;

        for (auto &ch : d_ptr->m_channels) {
            if (!ch.visible || ch.count == 0) continue;

            size_t start = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;

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
                firstIdx--;
            }

            size_t visibleSamples = ch.count - firstIdx;
            if (visibleSamples == 0) continue;

            bool doDecimate = false;
            if (d_ptr->m_decimationMode == DecimationMode::Always) {
                doDecimate = true;
            } else if (d_ptr->m_decimationMode == DecimationMode::Auto) {
                doDecimate = (visibleSamples > static_cast<size_t>(W_dev));
            }

            if (doDecimate) {
                internal::GeometryKey targetKey;
                targetKey.axisMode = internal::GeometryAxisMode::Time;
                targetKey.grid = win.grid;
                targetKey.widthDev = W_dev;
                targetKey.ringSize = static_cast<size_t>(std::max(win.numBuckets + 4, W_dev + 4));

                if (!ch.geometryValid || ch.geometryKey != targetKey) {
                    d_ptr->rebuildChannelStream(ch, targetKey);
                }

                qint64 kStartQuery = win.kStart;
                int numBucketsQuery = win.numBuckets;

                if (ch.totalSamples > ch.count && ch.count > 0) {
                    size_t oldestIdx = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;
                    qint64 tOldest = ch.timestamps[oldestIdx];
                    qint64 kOldest = grid.bucketOf(tOldest);

                    if (kStartQuery < kOldest) {
                        qint64 diff = kOldest - kStartQuery;
                        kStartQuery = kOldest;
                        numBucketsQuery = std::max(0, numBucketsQuery - static_cast<int>(diff));
                    }

                    internal::M4Bucket freshBucket;
                    size_t cap = static_cast<size_t>(d_ptr->m_capacity);
                    for (size_t j = 0; j < ch.count; ++j) {
                        size_t idx = (oldestIdx + j) % cap;
                        qint64 tSample = ch.timestamps[idx];
                        if (grid.bucketOf(tSample) != kOldest) {
                            break;
                        }
                        freshBucket.addSample(tSample, ch.values[idx]);
                    }
                    ch.m4Stream.setBucket(kOldest, freshBucket);
                }

                for (int i = 0; i < numBucketsQuery; ++i) {
                    qint64 k = kStartQuery + i;
                    const internal::M4Bucket *b = ch.m4Stream.bucketAt(k);
                    if (b && b->isValid()) {
                        hasData = true;
                        if (b->yMin < minVal) minVal = b->yMin;
                        if (b->yMax > maxVal) maxVal = b->yMax;
                    }
                }
            } else {
                size_t cap = static_cast<size_t>(d_ptr->m_capacity);
                for (size_t i = firstIdx; i < ch.count; ++i) {
                    double v = ch.values[(start + i) % cap];
                    if (!std::isfinite(v)) continue;
                    hasData = true;
                    if (v < minVal) minVal = v;
                    if (v > maxVal) maxVal = v;
                }
            }
        }
    } else {
        // SampleIndex Mode
        for (auto &ch : d_ptr->m_channels) {
            if (!ch.visible || ch.count == 0) continue;

            size_t start = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;

            bool doDecimate = false;
            if (d_ptr->m_decimationMode == DecimationMode::Always) {
                doDecimate = true;
            } else if (d_ptr->m_decimationMode == DecimationMode::Auto) {
                doDecimate = (ch.count > static_cast<size_t>(W_dev));
            }

            if (doDecimate) {
                auto ib = internal::computeIndexBuckets(ch.totalSamples, ch.count, d_ptr->m_capacity, W_dev);

                internal::GeometryKey targetKey;
                targetKey.axisMode = internal::GeometryAxisMode::SampleIndex;
                targetKey.grid = ib.grid;
                targetKey.widthDev = W_dev;
                targetKey.ringSize = static_cast<size_t>(std::max(ib.numBuckets + 4, W_dev + 4));

                if (!ch.geometryValid || ch.geometryKey != targetKey) {
                    d_ptr->rebuildChannelStream(ch, targetKey);
                }

                qint64 kStartQuery = ib.kStart;
                int numBucketsQuery = ib.numBuckets;

                quint64 startCounter = (ch.totalSamples >= ch.count) ? (ch.totalSamples - ch.count) : 0;
                if (ch.totalSamples > ch.count && ch.count > 0) {
                    qint64 firstAbsoluteIndex = static_cast<qint64>(startCounter);
                    qint64 kOldest = ib.grid.bucketOf(firstAbsoluteIndex);

                    if (kStartQuery < kOldest) {
                        qint64 diff = kOldest - kStartQuery;
                        kStartQuery = kOldest;
                        numBucketsQuery = std::max(0, numBucketsQuery - static_cast<int>(diff));
                    }

                    internal::M4Bucket freshBucket;
                    size_t cap = static_cast<size_t>(d_ptr->m_capacity);
                    size_t oldestIdx = ch.headIndex;
                    for (size_t j = 0; j < ch.count; ++j) {
                        qint64 sIdx = static_cast<qint64>(startCounter + j);
                        if (ib.grid.bucketOf(sIdx) != kOldest) {
                            break;
                        }
                        freshBucket.addSample(sIdx, ch.values[(oldestIdx + j) % cap]);
                    }
                    ch.m4Stream.setBucket(kOldest, freshBucket);
                }

                for (int i = 0; i < numBucketsQuery; ++i) {
                    qint64 k = kStartQuery + i;
                    const internal::M4Bucket *b = ch.m4Stream.bucketAt(k);
                    if (b && b->isValid()) {
                        hasData = true;
                        if (b->yMin < minVal) minVal = b->yMin;
                        if (b->yMax > maxVal) maxVal = b->yMax;
                    }
                }
            } else {
                size_t cap = static_cast<size_t>(d_ptr->m_capacity);
                for (size_t i = 0; i < ch.count; ++i) {
                    double v = ch.values[(start + i) % cap];
                    if (!std::isfinite(v)) continue;
                    hasData = true;
                    if (v < minVal) minVal = v;
                    if (v > maxVal) maxVal = v;
                }
            }
        }
    }

    if (hasData) {
        double targetMin = minVal;
        double targetMax = maxVal;
        double dataSpan = 0.0;
        if (qFuzzyCompare(minVal, maxVal)) {
            targetMin -= 1.0;
            targetMax += 1.0;
            dataSpan = 2.0;
        } else {
            dataSpan = maxVal - minVal;
            double margin = dataSpan * StripChartPrivate::kAutoScaleMargin;
            targetMin -= margin;
            targetMax += margin;
        }

        if (!d_ptr->m_autoScaleInitialized) {
            d_ptr->m_autoScaleInitialized = true;
            d_ptr->m_shrinkCounter = 0;
            setYRange(targetMin, targetMax);
        } else {
            double currentMin = d_ptr->m_yMinimum;
            double currentMax = d_ptr->m_yMaximum;
            double currentSpan = currentMax - currentMin;

            if (minVal < currentMin || maxVal > currentMax) {
                // Grow immediately when data leaves current range
                d_ptr->m_shrinkCounter = 0;
                setYRange(targetMin, targetMax);
            } else if (currentSpan > 0.0 && dataSpan < StripChartPrivate::kAutoScaleShrinkThreshold * currentSpan) {
                // Shrink only when data span is below 50 % of current range for 30 consecutive recomputations
                d_ptr->m_shrinkCounter++;
                if (d_ptr->m_shrinkCounter >= StripChartPrivate::kAutoScaleShrinkHysteresisCount) {
                    d_ptr->m_shrinkCounter = 0;
                    setYRange(targetMin, targetMax);
                }
            } else {
                d_ptr->m_shrinkCounter = 0;
            }
        }
    }
}

void StripChart::updateAutoScaling()
{
    ensureAutoScale();
}

QRectF StripChart::plotArea() const
{
    // #51 rule (b): the room for the labels and the legend grows with the widget font
    const double scale = internal::fontScale(font(), QApplication::font());
    double leftMargin = 42.0 * scale; // Room for Y-axis labels
    double rightMargin = 12.0;
    double topMargin = (d_ptr->m_legendVisible ? 30.0 : 12.0) * scale;
    double bottomMargin = ((d_ptr->m_xAxisMode == XAxisMode::Time) ? 26.0 : 16.0) * scale;

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
    d_ptr->invalidateGeometry();
}

void StripChart::changeEvent(QEvent *event)
{
    // #51: the cached text uses the widget font, so a font change invalidates it too
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        invalidateCache();
        update();
    }
    if (event->type() == QEvent::ScreenChangeInternal) {
        invalidateCache();
        d_ptr->invalidateGeometry();
        update();
    }
    QWidget::changeEvent(event);
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

        // #51 rule (b): the label font follows the widget font size
        const int fontSize = internal::scalePixelSize(9, internal::fontScale(this->font(), QApplication::font()));
        QFont font = this->font(); // #51: the widget font, not the application font of the pixmap painter
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
            // 1-2-5 Tick Selection for Relative Time, over the effective window length (a span
            // shorter than W_dev ns is shown as a W_dev ns window)
            const int widthDev = std::max(1, static_cast<int>(std::round(plotRect.width() * dpr)));
            const qint64 spanNs = internal::effectiveTimeSpan(d_ptr->m_timeSpan.count(), widthDev);
            auto tickResult = internal::computeTimeTicks(0, spanNs, plotRect.width(), 60.0 * internal::fontScale(this->font(), QApplication::font()));

            for (const auto &tick : tickResult.ticks) {
                double frac = static_cast<double>(tick.tNs) / static_cast<double>(spanNs);
                double x = plotRect.left() + frac * plotRect.width();

                painter.setPen(fineGridPen);
                painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));

                QRectF labelRect(x - 30.0, plotRect.bottom() + 3.0, 60.0, fontSize * 1.4);
                painter.setPen(d_ptr->m_textColor);
                painter.drawText(labelRect, Qt::AlignCenter, tick.label);
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
            QString labelStr = internal::formatFixed(val, (std::abs(val) >= 100.0) ? 0 : 1);
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
        ensureAutoScale();
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
    qreal dpr = devicePixelRatioF();
    int W_dev = std::max(1, static_cast<int>(std::round(plotRect.width() * dpr)));

    // Device-space placement of traces and time ticks. The painter's device transform gives the
    // device x of widget coordinates, so the plot's left edge is snapped to a device pixel boundary
    // even when the widget sits at a fractional device position; every decimation bucket then covers
    // exactly one device column. Rotated or sheared painters fall back to the widget's own scale.
    const QTransform deviceTransform = painter.deviceTransform();
    const bool deviceAxisAligned = deviceTransform.type() <= QTransform::TxScale
                                && std::abs(deviceTransform.m11() - dpr) < 1e-9;
    const double devScale = deviceAxisAligned ? deviceTransform.m11() : dpr;
    const double devOffsetX = deviceAxisAligned ? deviceTransform.dx() : 0.0;
    auto toLogicalX = [devScale, devOffsetX](double xDev) { return (xDev - devOffsetX) / devScale; };
    const double plotLeftDevExact = plotRect.left() * devScale + devOffsetX;
    const double plotLeftDev = std::round(plotLeftDevExact);
    // Traces are clipped to whole device columns [plotLeftDev, plotLeftDev + W_dev)
    const QRectF traceClipRect(toLogicalX(plotLeftDev), plotRect.top(), W_dev / devScale, plotRect.height());

    if (d_ptr->recordDiagnostics) {
        d_ptr->lastFrame = FrameDiagnostics();
        d_ptr->lastFrame.plotRect = plotRect;
        d_ptr->lastFrame.widthDev = W_dev;
        d_ptr->lastFrame.dpr = dpr;
    }

    if (plotRect.width() <= 10.0 || plotRect.height() <= 10.0) {
        if (d_ptr->recordDiagnostics) {
            d_ptr->lastFrame.channels.resize(d_ptr->m_channels.size());
        }
        return;
    }

    if (!qFuzzyCompare(dpr, d_ptr->m_cachedDpr)) {
        d_ptr->m_cachedDpr = dpr;
        d_ptr->invalidateGeometry();
    }

    double yRange = d_ptr->m_yMaximum - d_ptr->m_yMinimum;
    if (qFuzzyIsNull(yRange)) yRange = 1.0;

    // Diagnostics: record xTicks and yTicks
    if (d_ptr->recordDiagnostics) {
        if (d_ptr->m_xAxisMode == XAxisMode::Time) {
            qint64 spanNs = d_ptr->m_timeSpan.count();
            if (d_ptr->m_timeLabelFormat == TimeLabelFormat::Relative) {
                // Same ticks as the static grid: effective window length
                const qint64 windowNs = internal::effectiveTimeSpan(spanNs, W_dev);
                auto tickResult = internal::computeTimeTicks(0, windowNs, plotRect.width(), 60.0 * internal::fontScale(this->font(), QApplication::font()));
                d_ptr->lastFrame.xTicks.reserve(tickResult.ticks.size());
                for (const auto &tick : tickResult.ticks) {
                    double frac = static_cast<double>(tick.tNs) / static_cast<double>(windowNs);
                    double x = plotRect.left() + frac * plotRect.width();
                    d_ptr->lastFrame.xTicks.push_back({tick.tNs, x, tick.label});
                }
            } else {
                qint64 tLatest = std::numeric_limits<qint64>::min();
                for (const auto &ch : d_ptr->m_channels) {
                    if (ch.visible && ch.count > 0 && ch.lastTimestamp > tLatest) {
                        tLatest = ch.lastTimestamp;
                    }
                }
                if (tLatest == std::numeric_limits<qint64>::min()) {
                    tLatest = d_ptr->arrivalTimestampNs();
                }
                auto win = internal::computeTimeWindow(tLatest, spanNs, W_dev);
                const internal::PlotXMapping tickMap = internal::makePlotXMapping(win.grid, win.kStart, plotLeftDevExact);
                auto tickResult = internal::computeTimeTicks(win.tStart, win.tEnd, plotRect.width(), 60.0 * internal::fontScale(this->font(), QApplication::font()));
                d_ptr->lastFrame.xTicks.reserve(tickResult.ticks.size());
                for (const auto &tick : tickResult.ticks) {
                    double x = toLogicalX(tickMap.positionDev(tick.tNs));
                    QString labelStr;
                    if (d_ptr->m_timeOrigin.isValid()) {
                        QDateTime dt = d_ptr->m_timeOrigin.addMSecs(tick.tNs / 1000000);
                        labelStr = dt.toString(spanNs < 10000000000LL ? QStringLiteral("HH:mm:ss.zzz") : QStringLiteral("HH:mm:ss"));
                    } else {
                        labelStr = internal::formatFixed(static_cast<double>(tick.tNs) / 1e9, 1) + QStringLiteral(" s");
                    }
                    d_ptr->lastFrame.xTicks.push_back({tick.tNs, x, labelStr});
                }
            }
        } else {
            d_ptr->lastFrame.xTicks.reserve(d_ptr->m_verticalDivisions + 1);
            for (int i = 0; i <= d_ptr->m_verticalDivisions; ++i) {
                double frac = static_cast<double>(i) / d_ptr->m_verticalDivisions;
                double x = plotRect.left() + frac * plotRect.width();
                qint64 sIdx = static_cast<qint64>(std::round(frac * (d_ptr->m_capacity - 1)));
                d_ptr->lastFrame.xTicks.push_back({sIdx, x, QString::number(sIdx)});
            }
        }

        d_ptr->lastFrame.yTicks.reserve(d_ptr->m_horizontalDivisions + 1);
        for (int i = 0; i <= d_ptr->m_horizontalDivisions; ++i) {
            double frac = static_cast<double>(i) / d_ptr->m_horizontalDivisions;
            double y = plotRect.bottom() - frac * plotRect.height();
            double val = d_ptr->m_yMinimum + frac * (d_ptr->m_yMaximum - d_ptr->m_yMinimum);
            QString labelStr = internal::formatFixed(val, (std::abs(val) >= 100.0) ? 0 : 1);
            d_ptr->lastFrame.yTicks.push_back({val, y, labelStr});
        }
    }

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
        const internal::PlotXMapping tickMap = internal::makePlotXMapping(win.grid, win.kStart, plotLeftDevExact);
        qint64 tEnd = win.tEnd;
        qint64 tStart = win.tStart;

        auto tickResult = internal::computeTimeTicks(tStart, tEnd, plotRect.width(), 60.0 * internal::fontScale(this->font(), QApplication::font()));

        QPen fineGridPen(d_ptr->m_gridColor, 1.0, Qt::DotLine);
        // #51 rule (b): the label font follows the widget font size
        const int fontSize = internal::scalePixelSize(9, internal::fontScale(this->font(), QApplication::font()));
        QFont font = painter.font();
        font.setPixelSize(fontSize);
        painter.setFont(font);

        for (const auto &tick : tickResult.ticks) {
            double x = toLogicalX(tickMap.positionDev(tick.tNs));

            painter.setPen(fineGridPen);
            painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));

            QString labelStr;
            if (d_ptr->m_timeOrigin.isValid()) {
                QDateTime dt = d_ptr->m_timeOrigin.addMSecs(tick.tNs / 1000000);
                labelStr = dt.toString(spanNs < 10000000000LL ? QStringLiteral("HH:mm:ss.zzz") : QStringLiteral("HH:mm:ss"));
            } else {
                labelStr = internal::formatFixed(static_cast<double>(tick.tNs) / 1e9, 1) + QStringLiteral(" s");
            }

            QRectF labelRect(x - 40.0, plotRect.bottom() + 3.0, 80.0, fontSize * 1.4);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(labelRect, Qt::AlignCenter, labelStr);
        }
    }

    // 2. Plot Real-Time Channel Waveforms
    painter.save();
    painter.setClipRect(traceClipRect);

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
        const internal::BucketGrid &grid = win.grid;
        const internal::PlotXMapping xmap = internal::makePlotXMapping(grid, win.kStart, plotLeftDevExact);

        if (d_ptr->recordDiagnostics) {
            d_ptr->lastFrame.tStart = win.tStart;
            d_ptr->lastFrame.tEnd = win.tEnd;
            d_ptr->lastFrame.dtPx = win.dtPx;
            d_ptr->lastFrame.timeKStart = win.kStart;
            d_ptr->lastFrame.timeNumBuckets = win.numBuckets;
            d_ptr->lastFrame.kStart = win.kStart;
            d_ptr->lastFrame.numBuckets = win.numBuckets;
            d_ptr->lastFrame.gridNum = win.grid.num();
            d_ptr->lastFrame.gridDen = win.grid.den();
            d_ptr->lastFrame.plotLeftDev = plotLeftDev;
        }

        for (const auto &ch : d_ptr->m_channels) {
            ChannelFrameDiagnostics chDiag;
            if (!ch.visible || ch.count < 1) {
                if (d_ptr->recordDiagnostics) {
                    d_ptr->lastFrame.channels.push_back(chDiag);
                }
                continue;
            }

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
            if (visibleSamples == 0) {
                if (d_ptr->recordDiagnostics) {
                    d_ptr->lastFrame.channels.push_back(chDiag);
                }
                continue;
            }

            bool doDecimate = false;
            if (d_ptr->m_decimationMode == DecimationMode::Always) {
                doDecimate = true;
            } else if (d_ptr->m_decimationMode == DecimationMode::Auto) {
                doDecimate = (visibleSamples > static_cast<size_t>(W_dev));
            }
            const_cast<ChannelInternal&>(ch).lastDecimating = doDecimate;

            qint64 gapThresh = d_ptr->m_gapThreshold.count();
            if (gapThresh == 0 && ch.hasEma && ch.emaIntervalNs > 0.0) {
                gapThresh = static_cast<qint64>(4.0 * ch.emaIntervalNs);
            }

            auto mapY = [&](double val) {
                double yNorm = (val - d_ptr->m_yMinimum) / yRange;
                return plotRect.bottom() - yNorm * plotRect.height();
            };
            // Raw samples: continuous position in the window
            auto mapPoint = [&](qint64 t, double val) -> QPointF {
                return QPointF(toLogicalX(xmap.positionDev(t)), mapY(val));
            };

            QPen tracePen;
            if (doDecimate || d_ptr->m_testRawPenCosmetic1px) {
                tracePen = QPen(ch.color, 0.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
                tracePen.setCosmetic(true);
                painter.setRenderHint(QPainter::Antialiasing, false);
            } else {
                tracePen = QPen(ch.color, ch.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter.setRenderHint(QPainter::Antialiasing, true);
            }
            painter.setPen(tracePen);

            if (d_ptr->recordDiagnostics) {
                chDiag.path = doDecimate ? RenderPath::Decimated : RenderPath::Raw;
                chDiag.penWidth = tracePen.widthF();
                chDiag.cosmetic = tracePen.isCosmetic();
                chDiag.antialiasing = (!doDecimate && !d_ptr->m_testRawPenCosmetic1px);
            }

            QPointF lastDrawnPt;
            bool hasDrawnPt = false;
            bool suppressGlow = false;   // the latest bucket is blank (dropout): no glow dot
            BlankColumns glowBlankColumns;

            if (doDecimate) {
                internal::GeometryKey targetKey;
                targetKey.axisMode = internal::GeometryAxisMode::Time;
                targetKey.grid = win.grid;
                targetKey.widthDev = W_dev;
                targetKey.ringSize = static_cast<size_t>(std::max(win.numBuckets + 4, W_dev + 4));

                if (!ch.geometryValid || ch.geometryKey != targetKey) {
                    d_ptr->rebuildChannelStream(const_cast<ChannelInternal&>(ch), targetKey);
                }

                qint64 kStartQuery = win.kStart;
                int numBucketsQuery = win.numBuckets;

                if (ch.totalSamples > ch.count && ch.count > 0) {
                    size_t oldestIdx = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;
                    qint64 tOldest = ch.timestamps[oldestIdx];
                    qint64 kOldest = grid.bucketOf(tOldest);

                    if (kStartQuery < kOldest) {
                        qint64 diff = kOldest - kStartQuery;
                        kStartQuery = kOldest;
                        numBucketsQuery = std::max(0, numBucketsQuery - static_cast<int>(diff));
                    }

                    internal::M4Bucket freshBucket;
                    size_t cap = static_cast<size_t>(d_ptr->m_capacity);
                    for (size_t j = 0; j < ch.count; ++j) {
                        size_t idx = (oldestIdx + j) % cap;
                        qint64 tSample = ch.timestamps[idx];
                        if (grid.bucketOf(tSample) != kOldest) {
                            break;
                        }
                        freshBucket.addSample(tSample, ch.values[idx]);
                    }
                    const_cast<ChannelInternal&>(ch).m4Stream.setBucket(kOldest, freshBucket);
                }

                // Segments [0, segmentCount); the buffers after them are kept for the next channel or frame (#8)
                const std::size_t segmentCount =
                    ch.m4Stream.extractVisibleSegments(d_ptr->m_reusableSegments, kStartQuery, numBucketsQuery, gapThresh);
                const std::vector<internal::M4Point> *lastSegment =
                    segmentCount > 0 ? &d_ptr->m_reusableSegments[segmentCount - 1] : nullptr;

                // The glow dot must never draw inside a column left blank for a dropout
                {
                    const internal::M4Bucket *latest = ch.m4Stream.bucketAt(grid.bucketOf(ch.lastTimestamp));
                    suppressGlow = latest && latest->hasNonFinite;
                    if (!suppressGlow && lastSegment && !lastSegment->empty()) {
                        const qint64 kLastDrawn = grid.bucketOf(lastSegment->back().t);
                        collectBlankColumnsNearGlow(ch.m4Stream, kLastDrawn, kStartQuery, kStartQuery + numBucketsQuery,
                                                    dpr, plotRect,
                                                    [&](qint64 k) {
                                                        return std::make_pair(toLogicalX(xmap.columnLeftDev(k)),
                                                                              toLogicalX(xmap.columnLeftDev(k) + 1.0));
                                                    },
                                                    glowBlankColumns);
                    }
                }

                for (std::size_t si = 0; si < segmentCount; ++si) {
                    const auto &seg = d_ptr->m_reusableSegments[si];
                    if (seg.empty()) continue;
                    // Vertices at the centre of their column, one vertical line per column, end pixels (#90, #73)
                    internal::drawDecimatedSegment(painter, xmap, seg, d_ptr->m_reusablePoly, mapY, toLogicalX);
                    if (d_ptr->recordDiagnostics) {
                        chDiag.segments.push_back(d_ptr->m_reusablePoly);
                    }
                    lastDrawnPt = d_ptr->m_reusablePoly.last();
                    hasDrawnPt = true;
                }
            } else {
                // Raw drawing - iterate in place without visT/visY allocations!
                d_ptr->m_reusablePoly.clear();
                qint64 prevT = std::numeric_limits<qint64>::min();
                size_t cap = static_cast<size_t>(d_ptr->m_capacity);

                for (size_t j = 0; j < visibleSamples; ++j) {
                    size_t idx = (start + firstIdx + j) % cap;
                    double v = ch.values[idx];
                    qint64 t = ch.timestamps[idx];

                    if (!std::isfinite(v)) {
                        if (!d_ptr->m_reusablePoly.isEmpty()) {
                            painter.drawPolyline(d_ptr->m_reusablePoly);
                            if (d_ptr->recordDiagnostics) {
                                chDiag.segments.push_back(d_ptr->m_reusablePoly);
                            }
                            lastDrawnPt = d_ptr->m_reusablePoly.last();
                            hasDrawnPt = true;
                            d_ptr->m_reusablePoly.clear();
                        }
                        prevT = std::numeric_limits<qint64>::min();
                        continue;
                    }

                    if (prevT != std::numeric_limits<qint64>::min() && gapThresh > 0 && (t - prevT > gapThresh)) {
                        if (!d_ptr->m_reusablePoly.isEmpty()) {
                            painter.drawPolyline(d_ptr->m_reusablePoly);
                            if (d_ptr->recordDiagnostics) {
                                chDiag.segments.push_back(d_ptr->m_reusablePoly);
                            }
                            lastDrawnPt = d_ptr->m_reusablePoly.last();
                            hasDrawnPt = true;
                            d_ptr->m_reusablePoly.clear();
                        }
                    }

                    QPointF pt = mapPoint(t, v);
                    if (d_ptr->m_interpolation == Interpolation::Step && !d_ptr->m_reusablePoly.isEmpty()) {
                        d_ptr->m_reusablePoly << QPointF(pt.x(), d_ptr->m_reusablePoly.last().y());
                    }
                    d_ptr->m_reusablePoly << pt;
                    prevT = t;
                }

                if (!d_ptr->m_reusablePoly.isEmpty()) {
                    painter.drawPolyline(d_ptr->m_reusablePoly);
                    if (d_ptr->recordDiagnostics) {
                        chDiag.segments.push_back(d_ptr->m_reusablePoly);
                    }
                    lastDrawnPt = d_ptr->m_reusablePoly.last();
                    hasDrawnPt = true;
                }
            }

            // Restore antialiasing for downstream elements (glow dot, legend)
            painter.setRenderHint(QPainter::Antialiasing, true);

            // Glow dot on latest point if finite, never inside a blank dropout column
            if (hasDrawnPt && std::isfinite(ch.latestValue) && !suppressGlow) {
                drawGlowDot(painter, lastDrawnPt, ch.color, plotRect, glowBlankColumns);
            }

            if (d_ptr->recordDiagnostics) {
                d_ptr->lastFrame.channels.push_back(chDiag);
            }
        }
    } else {
        // SampleIndex Mode (Legacy compatibility)
        if (d_ptr->recordDiagnostics) {
            quint64 maxTotal = 0;
            size_t maxCount = 0;
            for (const auto &ch : d_ptr->m_channels) {
                if (ch.visible) {
                    maxTotal = std::max(maxTotal, ch.totalSamples);
                    maxCount = std::max(maxCount, ch.count);
                }
            }
            auto ib = internal::computeIndexBuckets(maxTotal, maxCount, d_ptr->m_capacity, W_dev);
            d_ptr->lastFrame.samplesPerBucket = ib.samplesPerBucket;
            d_ptr->lastFrame.indexKStart = ib.kStart;
            d_ptr->lastFrame.indexNumBuckets = ib.numBuckets;
            d_ptr->lastFrame.kStart = ib.kStart;
            d_ptr->lastFrame.numBuckets = ib.numBuckets;
            d_ptr->lastFrame.gridNum = ib.grid.num();
            d_ptr->lastFrame.gridDen = ib.grid.den();
            d_ptr->lastFrame.plotLeftDev = plotLeftDev;
        }

        for (const auto &ch : d_ptr->m_channels) {
            ChannelFrameDiagnostics chDiag;
            if (!ch.visible || ch.count < 1) {
                if (d_ptr->recordDiagnostics) {
                    d_ptr->lastFrame.channels.push_back(chDiag);
                }
                continue;
            }

            size_t start = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;

            bool doDecimate = false;
            if (d_ptr->m_decimationMode == DecimationMode::Always) {
                doDecimate = true;
            } else if (d_ptr->m_decimationMode == DecimationMode::Auto) {
                doDecimate = (ch.count > static_cast<size_t>(W_dev));
            }
            const_cast<ChannelInternal&>(ch).lastDecimating = doDecimate;

            QPen tracePen;
            if (doDecimate || d_ptr->m_testRawPenCosmetic1px) {
                tracePen = QPen(ch.color, 0.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
                tracePen.setCosmetic(true);
                painter.setRenderHint(QPainter::Antialiasing, false);
            } else {
                tracePen = QPen(ch.color, ch.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                painter.setRenderHint(QPainter::Antialiasing, true);
            }
            painter.setPen(tracePen);

            if (d_ptr->recordDiagnostics) {
                chDiag.path = doDecimate ? RenderPath::Decimated : RenderPath::Raw;
                chDiag.penWidth = tracePen.widthF();
                chDiag.cosmetic = tracePen.isCosmetic();
                chDiag.antialiasing = (!doDecimate && !d_ptr->m_testRawPenCosmetic1px);
            }

            QPointF lastDrawnPt;
            bool hasDrawnPt = false;
            bool suppressGlow = false;   // the latest bucket is blank (dropout): no glow dot
            BlankColumns glowBlankColumns;

            // Window and x mapping shared by the raw and decimated paths
            const auto ib = internal::computeIndexBuckets(ch.totalSamples, ch.count, d_ptr->m_capacity, W_dev);
            const internal::PlotXMapping xmap = internal::makePlotXMapping(ib.grid, ib.kStart, plotLeftDevExact);
            const quint64 firstRetainedIndex = (ch.totalSamples >= ch.count) ? (ch.totalSamples - ch.count) : 0;

            if (doDecimate) {

                internal::GeometryKey targetKey;
                targetKey.axisMode = internal::GeometryAxisMode::SampleIndex;
                targetKey.grid = ib.grid;
                targetKey.widthDev = W_dev;
                targetKey.ringSize = static_cast<size_t>(std::max(ib.numBuckets + 4, W_dev + 4));

                if (!ch.geometryValid || ch.geometryKey != targetKey) {
                    d_ptr->rebuildChannelStream(const_cast<ChannelInternal&>(ch), targetKey);
                }

                qint64 kStartQuery = ib.kStart;
                int numBucketsQuery = ib.numBuckets;

                quint64 startCounter = (ch.totalSamples >= ch.count) ? (ch.totalSamples - ch.count) : 0;
                if (ch.totalSamples > ch.count && ch.count > 0) {
                    qint64 firstAbsoluteIndex = static_cast<qint64>(startCounter);
                    qint64 kOldest = ib.grid.bucketOf(firstAbsoluteIndex);

                    if (kStartQuery < kOldest) {
                        qint64 diff = kOldest - kStartQuery;
                        kStartQuery = kOldest;
                        numBucketsQuery = std::max(0, numBucketsQuery - static_cast<int>(diff));
                    }

                    internal::M4Bucket freshBucket;
                    size_t cap = static_cast<size_t>(d_ptr->m_capacity);
                    size_t oldestIdx = ch.headIndex;
                    for (size_t j = 0; j < ch.count; ++j) {
                        qint64 sIdx = static_cast<qint64>(startCounter + j);
                        if (ib.grid.bucketOf(sIdx) != kOldest) {
                            break;
                        }
                        size_t idx = (oldestIdx + j) % cap;
                        freshBucket.addSample(sIdx, ch.values[idx]);
                    }
                    const_cast<ChannelInternal&>(ch).m4Stream.setBucket(kOldest, freshBucket);
                }

                // Segments [0, segmentCount); the buffers after them are kept for the next channel or frame (#8)
                const std::size_t segmentCount =
                    ch.m4Stream.extractVisibleSegments(d_ptr->m_reusableSegments, kStartQuery, numBucketsQuery, 0);
                const std::vector<internal::M4Point> *lastSegment =
                    segmentCount > 0 ? &d_ptr->m_reusableSegments[segmentCount - 1] : nullptr;

                // The glow dot must never draw inside a column left blank for a dropout
                {
                    const internal::M4Bucket *latest = ch.m4Stream.bucketAt(
                        ib.grid.bucketOf(static_cast<qint64>(ch.totalSamples) - 1));
                    suppressGlow = latest && latest->hasNonFinite;
                    if (!suppressGlow && lastSegment && !lastSegment->empty()) {
                        const qint64 kLastDrawn = ib.grid.bucketOf(lastSegment->back().t);
                        collectBlankColumnsNearGlow(ch.m4Stream, kLastDrawn, kStartQuery, kStartQuery + numBucketsQuery,
                                                    dpr, plotRect,
                                                    [&](qint64 k) {
                                                        return std::make_pair(toLogicalX(xmap.columnLeftDev(k)),
                                                                              toLogicalX(xmap.columnLeftDev(k) + 1.0));
                                                    },
                                                    glowBlankColumns);
                    }
                }

                auto mapY = [&](double val) {
                    double yNorm = (val - d_ptr->m_yMinimum) / yRange;
                    return plotRect.bottom() - yNorm * plotRect.height();
                };
                for (std::size_t si = 0; si < segmentCount; ++si) {
                    const auto &seg = d_ptr->m_reusableSegments[si];
                    if (seg.empty()) continue;
                    // Vertices at the centre of their column, one vertical line per column, end pixels (#90, #73)
                    internal::drawDecimatedSegment(painter, xmap, seg, d_ptr->m_reusablePoly, mapY, toLogicalX);
                    if (d_ptr->recordDiagnostics) {
                        chDiag.segments.push_back(d_ptr->m_reusablePoly);
                    }
                    lastDrawnPt = d_ptr->m_reusablePoly.last();
                    hasDrawnPt = true;
                }
            } else {
                d_ptr->m_reusablePoly.clear();
                for (size_t i = 0; i < ch.count; ++i) {
                    size_t bufIdx = (start + i) % static_cast<size_t>(d_ptr->m_capacity);
                    double val = ch.values[bufIdx];

                    if (!std::isfinite(val)) {
                        if (!d_ptr->m_reusablePoly.isEmpty()) {
                            painter.drawPolyline(d_ptr->m_reusablePoly);
                            if (d_ptr->recordDiagnostics) {
                                chDiag.segments.push_back(d_ptr->m_reusablePoly);
                            }
                            lastDrawnPt = d_ptr->m_reusablePoly.last();
                            hasDrawnPt = true;
                            d_ptr->m_reusablePoly.clear();
                        }
                        continue;
                    }

                    // Raw samples: continuous position of the absolute sample index in the window
                    double x = toLogicalX(xmap.positionDev(static_cast<qint64>(firstRetainedIndex + i)));
                    double yNorm = (val - d_ptr->m_yMinimum) / yRange;
                    double y = plotRect.bottom() - yNorm * plotRect.height();
                    y = std::clamp(y, plotRect.top(), plotRect.bottom());
                    QPointF pt(x, y);

                    if (d_ptr->m_interpolation == Interpolation::Step && !d_ptr->m_reusablePoly.isEmpty()) {
                        d_ptr->m_reusablePoly << QPointF(pt.x(), d_ptr->m_reusablePoly.last().y());
                    }
                    d_ptr->m_reusablePoly << pt;
                }

                if (!d_ptr->m_reusablePoly.isEmpty()) {
                    painter.drawPolyline(d_ptr->m_reusablePoly);
                    if (d_ptr->recordDiagnostics) {
                        chDiag.segments.push_back(d_ptr->m_reusablePoly);
                    }
                    lastDrawnPt = d_ptr->m_reusablePoly.last();
                    hasDrawnPt = true;
                }
            }

            // Restore antialiasing for downstream elements (glow dot, legend)
            painter.setRenderHint(QPainter::Antialiasing, true);

            // Glow dot on latest point if finite, never inside a blank dropout column
            if (hasDrawnPt && std::isfinite(ch.latestValue) && !suppressGlow) {
                drawGlowDot(painter, lastDrawnPt, ch.color, plotRect, glowBlankColumns);
            }

            if (d_ptr->recordDiagnostics) {
                d_ptr->lastFrame.channels.push_back(chDiag);
            }
        }
    }

    painter.restore();

    // 3. Channel Legend and Real-Time Readouts (Top Bar)
    if (d_ptr->m_legendVisible && !d_ptr->m_channels.empty()) {
        double curX = plotRect.left() + 4.0;
        const int fontSize = internal::scalePixelSize(10, internal::fontScale(this->font(), QApplication::font()));
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
            QString valStr = std::isfinite(ch.latestValue) ? internal::formatFixed(ch.latestValue, 1) : QStringLiteral("---");
            QString txt = QStringLiteral("%1: %2").arg(ch.name, valStr);
            QRectF txtRect = fm.boundingRect(txt);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(QPointF(curX, 19.0), txt);
            if (d_ptr->recordDiagnostics) {
                d_ptr->lastFrame.legendLabels.push_back(txt);
            }

            curX += txtRect.width() + 18.0;
            if (curX > plotRect.right() - 20.0) break; // Don't overflow
        }
    }
}

} // namespace QtIndustrialWidgets
