// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/StripChart.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPixmap>
#include <QtCore/QtMath>
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
    std::vector<double> buffer;
    size_t headIndex{0};
};

class StripChartPrivate {
public:
    int m_capacity{300};
    double m_yMinimum{0.0};
    double m_yMaximum{100.0};
    bool m_autoScaleY{false};
    bool m_gridVisible{true};
    bool m_legendVisible{true};
    int m_horizontalDivisions{6};
    int m_verticalDivisions{8};

    QColor m_gridColor{QColor(42, 54, 70)};
    QColor m_backgroundColor{QColor(14, 18, 25)};
    QColor m_bezelColor{QColor(38, 46, 60)};
    QColor m_textColor{QColor(210, 220, 235)};

    std::vector<ChannelInternal> m_channels;

    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};
};

StripChart::StripChart(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<StripChartPrivate>())
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

StripChart::~StripChart() = default;

int StripChart::capacity() const { Q_D(const StripChart); return d->m_capacity; }
double StripChart::yMinimum() const { Q_D(const StripChart); return d->m_yMinimum; }
double StripChart::yMaximum() const { Q_D(const StripChart); return d->m_yMaximum; }
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
    ch.buffer.resize(d_ptr->m_capacity, 0.0);
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
    if (ch.buffer.size() != static_cast<size_t>(d_ptr->m_capacity)) {
        ch.buffer.resize(d_ptr->m_capacity, 0.0);
    }

    if (std::isnan(value)) {
        value = (ch.count > 0 && std::isfinite(ch.latestValue)) ? ch.latestValue : 0.0;
    } else if (std::isinf(value)) {
        value = (value > 0.0) ? d_ptr->m_yMaximum : d_ptr->m_yMinimum;
    }

    ch.buffer[ch.headIndex] = value;
    ch.headIndex = (ch.headIndex + 1) % d_ptr->m_capacity;
    if (ch.count < static_cast<size_t>(d_ptr->m_capacity)) {
        ch.count++;
    }
    ch.latestValue = value;

    if (d_ptr->m_autoScaleY) {
        updateAutoScaling();
    }

    Q_EMIT dataAdded();
    update();
}

void StripChart::addDataPoints(const QVector<double> &values)
{
    int limit = std::min(static_cast<int>(values.size()), static_cast<int>(d_ptr->m_channels.size()));
    for (int i = 0; i < limit; ++i) {
        ChannelInternal &ch = d_ptr->m_channels[i];
        if (ch.buffer.size() != static_cast<size_t>(d_ptr->m_capacity)) {
            ch.buffer.resize(d_ptr->m_capacity, 0.0);
        }
        double val = values[i];
        if (std::isnan(val)) {
            val = (ch.count > 0 && std::isfinite(ch.latestValue)) ? ch.latestValue : 0.0;
        } else if (std::isinf(val)) {
            val = (val > 0.0) ? d_ptr->m_yMaximum : d_ptr->m_yMinimum;
        }

        ch.buffer[ch.headIndex] = val;
        ch.headIndex = (ch.headIndex + 1) % d_ptr->m_capacity;
        if (ch.count < static_cast<size_t>(d_ptr->m_capacity)) {
            ch.count++;
        }
        ch.latestValue = val;
    }

    if (d_ptr->m_autoScaleY) {
        updateAutoScaling();
    }

    Q_EMIT dataAdded();
    update();
}

void StripChart::clear()
{
    for (auto &ch : d_ptr->m_channels) {
        ch.headIndex = 0;
        ch.count = 0;
        ch.latestValue = 0.0;
        std::fill(ch.buffer.begin(), ch.buffer.end(), 0.0);
    }
    update();
}

void StripChart::setCapacity(int count)
{
    int c = std::clamp(count, 10, 5000);
    if (d_ptr->m_capacity == c) return;
    d_ptr->m_capacity = c;

    for (auto &ch : d_ptr->m_channels) {
        ch.buffer.resize(d_ptr->m_capacity, 0.0);
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
    double minVal = 1e9;
    double maxVal = -1e9;

    for (const auto &ch : d_ptr->m_channels) {
        if (!ch.visible || ch.count == 0) continue;
        for (size_t i = 0; i < ch.count; ++i) {
            double v = ch.buffer[i];
            if (!std::isfinite(v)) continue;
            hasData = true;
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
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
    } else {
        setYRange(0.0, 100.0);
    }
}

QRectF StripChart::plotArea() const
{
    double leftMargin = 42.0; // Room for Y-axis labels
    double rightMargin = 12.0;
    double topMargin = d_ptr->m_legendVisible ? 30.0 : 12.0;
    double bottomMargin = 16.0;

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

        // Vertical division lines
        for (int i = 0; i <= d_ptr->m_verticalDivisions; ++i) {
            double x = plotRect.left() + (static_cast<double>(i) / d_ptr->m_verticalDivisions) * plotRect.width();
            bool isCenter = (i == d_ptr->m_verticalDivisions / 2);
            painter.setPen(isCenter ? solidGridPen : fineGridPen);
            painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
        }

        // Horizontal division lines & Y-axis labels
        int fontSize = 9;
        QFont font = painter.font();
        font.setPixelSize(fontSize);
        painter.setFont(font);

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

    // 2. Plot Real-Time Channel Waveforms
    painter.save();
    painter.setClipRect(plotRect);

    double yRange = d_ptr->m_yMaximum - d_ptr->m_yMinimum;
    if (qFuzzyIsNull(yRange)) yRange = 1.0;

    for (const auto &ch : d_ptr->m_channels) {
        if (!ch.visible || ch.count < 2) continue;

        QPolygonF polyline;
        polyline.reserve(static_cast<int>(ch.count));

        size_t start = (ch.count < static_cast<size_t>(d_ptr->m_capacity)) ? 0 : ch.headIndex;

        for (size_t i = 0; i < ch.count; ++i) {
            size_t bufIdx = (start + i) % d_ptr->m_capacity;
            double val = ch.buffer[bufIdx];
            if (!std::isfinite(val)) continue;

            double x = plotRect.left() + (static_cast<double>(i) / (d_ptr->m_capacity - 1)) * plotRect.width();
            double yNorm = (val - d_ptr->m_yMinimum) / yRange;
            if (!std::isfinite(yNorm)) continue;
            double y = plotRect.bottom() - yNorm * plotRect.height();
            y = std::clamp(y, plotRect.top(), plotRect.bottom());

            polyline << QPointF(x, y);
        }

        // Draw smooth waveform trace
        QPen tracePen(ch.color, ch.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(tracePen);
        painter.drawPolyline(polyline);

        // Specular glow dot on the latest incoming data point
        if (!polyline.isEmpty()) {
            QPointF latestPt = polyline.last();
            painter.setPen(Qt::NoPen);
            painter.setBrush(ch.color.lighter(150));
            painter.drawEllipse(latestPt, 2.5, 2.5);
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

            // Channel name and latest numeric readout
            QString txt = QStringLiteral("%1: %2").arg(ch.name, QString::number(ch.latestValue, 'f', 1));
            QRectF txtRect = fm.boundingRect(txt);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(QPointF(curX, 19.0), txt);

            curX += txtRect.width() + 18.0;
            if (curX > plotRect.right() - 20.0) break; // Don't overflow
        }
    }
}

} // namespace QtIndustrialWidgets
