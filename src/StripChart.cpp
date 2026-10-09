// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/StripChart.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtCore/QtMath>
#include <algorithm>

namespace QtIndustrialWidgets {


StripChart::StripChart(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

const StripChart::ChannelInfo *StripChart::channel(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_channels.size())) {
        return &m_channels[index];
    }
    return nullptr;
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
    ChannelInfo ch;
    ch.name = name;
    ch.color = color;
    ch.penWidth = penWidth;
    ch.buffer.resize(m_capacity, 0.0);
    ch.headIndex = 0;
    ch.count = 0;
    ch.latestValue = 0.0;

    m_channels.push_back(ch);
    invalidateCache();
    update();
    return static_cast<int>(m_channels.size()) - 1;
}

void StripChart::addDataPoint(int channelId, double value)
{
    if (channelId < 0 || channelId >= static_cast<int>(m_channels.size())) {
        return;
    }

    ChannelInfo &ch = m_channels[channelId];
    if (ch.buffer.size() != static_cast<size_t>(m_capacity)) {
        ch.buffer.resize(m_capacity, 0.0);
    }

    ch.buffer[ch.headIndex] = value;
    ch.headIndex = (ch.headIndex + 1) % m_capacity;
    if (ch.count < static_cast<size_t>(m_capacity)) {
        ch.count++;
    }
    ch.latestValue = value;

    if (m_autoScaleY) {
        updateAutoScaling();
    }

    Q_EMIT dataAdded();
    update();
}

void StripChart::addDataPoints(const QVector<double> &values)
{
    int limit = std::min(static_cast<int>(values.size()), static_cast<int>(m_channels.size()));
    for (int i = 0; i < limit; ++i) {
        ChannelInfo &ch = m_channels[i];
        if (ch.buffer.size() != static_cast<size_t>(m_capacity)) {
            ch.buffer.resize(m_capacity, 0.0);
        }
        double val = values[i];
        ch.buffer[ch.headIndex] = val;
        ch.headIndex = (ch.headIndex + 1) % m_capacity;
        if (ch.count < static_cast<size_t>(m_capacity)) {
            ch.count++;
        }
        ch.latestValue = val;
    }

    if (m_autoScaleY) {
        updateAutoScaling();
    }

    Q_EMIT dataAdded();
    update();
}

void StripChart::clear()
{
    for (auto &ch : m_channels) {
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
    if (m_capacity == c) return;
    m_capacity = c;

    for (auto &ch : m_channels) {
        ch.buffer.resize(m_capacity, 0.0);
        ch.headIndex = 0;
        ch.count = 0;
    }

    Q_EMIT capacityChanged(m_capacity);
    update();
}

void StripChart::setYMinimum(double min)
{
    setYRange(min, m_yMaximum);
}

void StripChart::setYMaximum(double max)
{
    setYRange(m_yMinimum, max);
}

void StripChart::setYRange(double min, double max)
{
    if (min >= max) return;
    if (qFuzzyCompare(min, m_yMinimum) && qFuzzyCompare(max, m_yMaximum)) return;

    m_yMinimum = min;
    m_yMaximum = max;
    invalidateCache();
    Q_EMIT yRangeChanged(m_yMinimum, m_yMaximum);
    update();
}

void StripChart::setAutoScaleY(bool autoScale)
{
    if (m_autoScaleY == autoScale) return;
    m_autoScaleY = autoScale;
    if (m_autoScaleY) {
        updateAutoScaling();
    }
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setGridVisible(bool visible)
{
    if (m_gridVisible == visible) return;
    m_gridVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setLegendVisible(bool visible)
{
    if (m_legendVisible == visible) return;
    m_legendVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setGridColor(const QColor &color)
{
    if (m_gridColor == color) return;
    m_gridColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor == color) return;
    m_backgroundColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setBezelColor(const QColor &color)
{
    if (m_bezelColor == color) return;
    m_bezelColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setHorizontalDivisions(int divisions)
{
    int d = std::clamp(divisions, 2, 20);
    if (m_horizontalDivisions == d) return;
    m_horizontalDivisions = d;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setVerticalDivisions(int divisions)
{
    int d = std::clamp(divisions, 2, 30);
    if (m_verticalDivisions == d) return;
    m_verticalDivisions = d;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void StripChart::setChannelVisible(int channelId, bool visible)
{
    if (channelId >= 0 && channelId < static_cast<int>(m_channels.size())) {
        if (m_channels[channelId].visible != visible) {
            m_channels[channelId].visible = visible;
            update();
        }
    }
}

void StripChart::setChannelColor(int channelId, const QColor &color)
{
    if (channelId >= 0 && channelId < static_cast<int>(m_channels.size())) {
        m_channels[channelId].color = color;
        update();
    }
}

void StripChart::updateAutoScaling()
{
    bool hasData = false;
    double minVal = 1e9;
    double maxVal = -1e9;

    for (const auto &ch : m_channels) {
        if (!ch.visible || ch.count == 0) continue;
        hasData = true;
        for (size_t i = 0; i < ch.count; ++i) {
            double v = ch.buffer[i];
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
    }
}

QRectF StripChart::plotArea() const
{
    double leftMargin = 42.0; // Room for Y-axis labels
    double rightMargin = 12.0;
    double topMargin = m_legendVisible ? 30.0 : 12.0;
    double bottomMargin = 16.0;

    double w = std::max(10.0, width() - leftMargin - rightMargin);
    double h = std::max(10.0, height() - topMargin - bottomMargin);

    return QRectF(leftMargin, topMargin, w, h);
}

void StripChart::invalidateCache()
{
    m_cacheDirty = true;
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

    m_cachePixmap = QPixmap(pixmapSize);
    m_cachePixmap.setDevicePixelRatio(dpr);
    m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = targetSize.width();
    const double h = targetSize.height();

    // 1. Outer Bezel & Metal Chassis
    QRectF outerRect(1.0, 1.0, w - 2.0, h - 2.0);
    painter.setPen(QPen(m_bezelColor.darker(170), 1.5));
    painter.setBrush(m_bezelColor);
    painter.drawRoundedRect(outerRect, 6.0, 6.0);

    // 2. Oscilloscope Plot Screen (Dark recessed glass)
    const QRectF plotRect = plotArea();

    QRectF screenRect(plotRect.left() - 4.0, plotRect.top() - 4.0,
                      plotRect.width() + 8.0, plotRect.height() + 8.0);

    painter.setPen(QPen(m_bezelColor.darker(190), 1.2));
    painter.setBrush(m_backgroundColor);
    painter.drawRoundedRect(screenRect, 4.0, 4.0);

    // 3. Grid Reticle
    if (m_gridVisible && plotRect.width() > 10.0 && plotRect.height() > 10.0) {
        QPen fineGridPen(m_gridColor, 1.0, Qt::DotLine);
        QPen solidGridPen(m_gridColor.lighter(130), 1.0, Qt::SolidLine);

        // Vertical division lines
        for (int i = 0; i <= m_verticalDivisions; ++i) {
            double x = plotRect.left() + (static_cast<double>(i) / m_verticalDivisions) * plotRect.width();
            bool isCenter = (i == m_verticalDivisions / 2);
            painter.setPen(isCenter ? solidGridPen : fineGridPen);
            painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
        }

        // Horizontal division lines & Y-axis labels
        int fontSize = 9;
        QFont font = painter.font();
        font.setPixelSize(fontSize);
        painter.setFont(font);

        for (int i = 0; i <= m_horizontalDivisions; ++i) {
            double frac = static_cast<double>(i) / m_horizontalDivisions;
            double y = plotRect.bottom() - frac * plotRect.height();
            double val = m_yMinimum + frac * (m_yMaximum - m_yMinimum);

            bool isZero = qFuzzyIsNull(val);
            painter.setPen(isZero ? QPen(m_gridColor.lighter(170), 1.2) : fineGridPen);
            painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));

            // Y label on the left
            QString labelStr = QString::number(val, 'f', (std::abs(val) >= 100.0) ? 0 : 1);
            QRectF labelRect(0.0, y - fontSize * 0.7, plotRect.left() - 6.0, fontSize * 1.4);
            painter.setPen(m_textColor);
            painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, labelStr);
        }
    }

    m_cacheDirty = false;
}

void StripChart::paintEvent(QPaintEvent *)
{
    if (m_cacheDirty || m_cachePixmap.size() != (QSizeF(size()) * devicePixelRatioF()).toSize()) {
        renderStaticGrid(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast Blit of Cached Grid & Chassis
    painter.drawPixmap(0, 0, m_cachePixmap);

    const QRectF plotRect = plotArea();
    if (plotRect.width() <= 10.0 || plotRect.height() <= 10.0) return;

    // 2. Plot Real-Time Channel Waveforms
    painter.save();
    painter.setClipRect(plotRect);

    double yRange = m_yMaximum - m_yMinimum;
    if (qFuzzyIsNull(yRange)) yRange = 1.0;

    for (const auto &ch : m_channels) {
        if (!ch.visible || ch.count < 2) continue;

        QPolygonF polyline;
        polyline.reserve(static_cast<int>(ch.count));

        size_t start = (ch.count < static_cast<size_t>(m_capacity)) ? 0 : ch.headIndex;

        for (size_t i = 0; i < ch.count; ++i) {
            size_t bufIdx = (start + i) % m_capacity;
            double val = ch.buffer[bufIdx];

            double x = plotRect.left() + (static_cast<double>(i) / (m_capacity - 1)) * plotRect.width();
            double yNorm = (val - m_yMinimum) / yRange;
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
    if (m_legendVisible && !m_channels.empty()) {
        double curX = plotRect.left() + 4.0;
        int fontSize = 10;
        QFont f = font();
        f.setPixelSize(fontSize);
        f.setBold(true);
        painter.setFont(f);
        QFontMetricsF fm(f);

        for (const auto &ch : m_channels) {
            if (!ch.visible) continue;

            // Channel color swatch
            painter.setPen(Qt::NoPen);
            painter.setBrush(ch.color);
            painter.drawRoundedRect(QRectF(curX, 10.0, 10.0, 10.0), 2.0, 2.0);
            curX += 14.0;

            // Channel name and latest numeric readout
            QString txt = QStringLiteral("%1: %2").arg(ch.name, QString::number(ch.latestValue, 'f', 1));
            QRectF txtRect = fm.boundingRect(txt);
            painter.setPen(m_textColor);
            painter.drawText(QPointF(curX, 19.0), txt);

            curX += txtRect.width() + 18.0;
            if (curX > plotRect.right() - 20.0) break; // Don't overflow
        }
    }
}

} // namespace QtIndustrialWidgets
