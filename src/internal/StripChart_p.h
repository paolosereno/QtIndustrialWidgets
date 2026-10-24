/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/StripChart.h>
#include "M4Decimator.h"
#include "StripChartGeometry.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QDateTime>
#include <QtGui/QPolygonF>
#include <QtGui/QPixmap>
#include <QtGui/QColor>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {

enum class RenderPath {
    NotDrawn,
    Raw,
    Decimated
};

struct ChannelFrameDiagnostics {
    RenderPath path{RenderPath::NotDrawn};
    std::vector<QPolygonF> segments;
    double penWidth{1.0};
    bool cosmetic{false};
    bool antialiasing{false};
};

struct XTickDiagnostic {
    qint64 tNs{0};
    double xPos{0.0};
    QString label;
};

struct YTickDiagnostic {
    double value{0.0};
    double yPos{0.0};
    QString label;
};

struct FrameDiagnostics {
    // Plot geometry
    QRectF plotRect;
    int widthDev{0};
    double dpr{1.0};

    // Window - Time mode
    qint64 tStart{0};
    qint64 tEnd{0};
    qint64 dtPx{0};          // nominal bucket width (ns)
    qint64 timeKStart{0};
    int timeNumBuckets{0};

    // Window - SampleIndex mode
    qint64 samplesPerBucket{0};
    qint64 indexKStart{0};
    int indexNumBuckets{0};

    // Common window accessors
    qint64 kStart{0};
    int numBuckets{0};
    qint64 gridNum{1};       // bucket k = floor(x * gridNum / gridDen)
    qint64 gridDen{1};
    double plotLeftDev{0.0}; // device x of the plot's left edge used for traces

    // Per channel diagnostics (indexed by channelId)
    std::vector<ChannelFrameDiagnostics> channels;

    // Ticks
    std::vector<XTickDiagnostic> xTicks;
    std::vector<YTickDiagnostic> yTicks;

    // Legend texts as drawn ("<name>: <latest value>"), in channel order, visible channels only
    std::vector<QString> legendLabels;
};

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
    internal::GeometryKey geometryKey;
    bool geometryValid{false};
    quint64 streamRebuilds{0}; // full O(N) rebuilds of m4Stream (diagnostics)
    bool lastDecimating{false};
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
        ch.geometryValid = false;
        ch.lastDecimating = false;
        ch.m4Stream.reset();
    }

    bool m_testRawPenCosmetic1px{false};

    // Diagnostics recording
    bool recordDiagnostics{false};
    FrameDiagnostics lastFrame;

    // Internal autoscale hysteresis constants
    static constexpr double kAutoScaleMargin = 0.10; // 10 % margin on data span
    static constexpr double kAutoScaleShrinkThreshold = 0.50; // shrink when data span < 50 % of current range
    static constexpr int kAutoScaleShrinkHysteresisCount = 30; // 30 consecutive recomputations before shrinking

    int m_capacity{300};
    double m_yMinimum{0.0};
    double m_yMaximum{100.0};
    bool m_autoScaleY{false};
    bool m_autoScaleDirty{false};
    bool m_autoScaleInitialized{false};
    int m_shrinkCounter{0};
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

    std::vector<std::vector<internal::M4Point>> m_reusableSegments;
    QPolygonF m_reusablePoly;

    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};
    double m_cachedDpr{1.0};

    void invalidateGeometry() {
        for (auto &ch : m_channels) {
            ch.geometryValid = false;
        }
    }

    void rebuildChannelStream(ChannelInternal &ch, const internal::GeometryKey &key) {
        ++ch.streamRebuilds;
        ch.m4Stream.setWidth(key.widthDev, key.ringSize);
        ch.m4Stream.reset();
        ch.geometryKey = key;
        ch.geometryValid = true;

        if (ch.count == 0) return;

        size_t start = (ch.count < static_cast<size_t>(m_capacity)) ? 0 : ch.headIndex;
        size_t cap = static_cast<size_t>(m_capacity);

        if (key.axisMode == internal::GeometryAxisMode::Time) {
            size_t span1 = std::min(ch.count, cap - start);
            for (size_t i = 0; i < span1; ++i) {
                ch.m4Stream.addSample(ch.timestamps[start + i], ch.values[start + i], key.grid);
            }
            size_t span2 = ch.count - span1;
            for (size_t i = 0; i < span2; ++i) {
                ch.m4Stream.addSample(ch.timestamps[i], ch.values[i], key.grid);
            }
        } else {
            quint64 startCounter = (ch.totalSamples >= ch.count) ? (ch.totalSamples - ch.count) : 0;
            size_t span1 = std::min(ch.count, cap - start);
            for (size_t i = 0; i < span1; ++i) {
                qint64 sampleIdx = static_cast<qint64>(startCounter + i);
                ch.m4Stream.addSample(sampleIdx, ch.values[start + i], key.grid);
            }
            size_t span2 = ch.count - span1;
            for (size_t i = 0; i < span2; ++i) {
                qint64 sampleIdx = static_cast<qint64>(startCounter + span1 + i);
                ch.m4Stream.addSample(sampleIdx, ch.values[i], key.grid);
            }
        }
    }

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

        if (ch.geometryValid && ch.visible) {
            if (m_xAxisMode == StripChart::XAxisMode::Time) {
                ch.m4Stream.addSample(t, value, ch.geometryKey.grid);
            } else {
                qint64 sampleIdx = static_cast<qint64>(ch.totalSamples - 1);
                ch.m4Stream.addSample(sampleIdx, value, ch.geometryKey.grid);
            }
        } else if (!ch.visible) {
            ch.geometryValid = false;
        }

        m_autoScaleDirty = true;
        return true;
    }
};

} // namespace QtIndustrialWidgets
