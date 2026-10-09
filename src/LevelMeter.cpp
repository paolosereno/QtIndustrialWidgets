// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/LevelMeter.h>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QLinearGradient>
#include <QtGui/QRadialGradient>
#include <cmath>
#include <algorithm>

namespace QtIndustrialWidgets {


struct ChannelData {
    double value = 0.0;
    double peakValue = 0.0;
    qint64 lastPeakTime = 0;
    bool overloadState = false;
};

class LevelMeterPrivate {
public:
    int m_channelCount = 2; // Default stereo (L / R)
    double m_minimum = -60.0;
    double m_maximum = 6.0;
    double m_warningThreshold = -6.0;
    double m_errorThreshold = 0.0;
    int m_segmentCount = 24;
    LevelMeter::DisplayMode m_displayMode = LevelMeter::DisplayMode::Segmented;
    Qt::Orientation m_orientation = Qt::Vertical;

    bool m_peakHoldEnabled = true;
    int m_peakHoldTimeMs = 1200; // Hold peak for 1.2s before decay
    double m_peakDecayRate = 25.0; // Units/second decay rate
    bool m_scaleVisible = true;

    QString m_unit = QStringLiteral("dB");
    QString m_title;
    QStringList m_channelLabels = {QStringLiteral("L"), QStringLiteral("R")};

    QColor m_normalColor = QColor(46, 204, 113);   // Green
    QColor m_warningColor = QColor(254, 211, 48);  // Amber Yellow
    QColor m_errorColor = QColor(235, 59, 90);     // Red
    QColor m_peakColor = QColor(255, 255, 255);    // White peak indicator
    QColor m_backgroundColor = QColor(22, 25, 30); // Deep dark chassis
    QColor m_textColor = QColor(180, 185, 195);

    QVector<ChannelData> m_channels;
    QTimer *m_decayTimer = nullptr;

    QPixmap m_cachedBackground;
    bool m_cacheValid = false;
    qint64 m_lastDecayTime = 0;
};

LevelMeter::LevelMeter(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<LevelMeterPrivate>())
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    d_ptr->m_channels.resize(d_ptr->m_channelCount);
    for (auto &ch : d_ptr->m_channels) {
        ch.value = d_ptr->m_minimum;
        ch.peakValue = d_ptr->m_minimum;
    }

    d_ptr->m_decayTimer = new QTimer(this);
    d_ptr->m_decayTimer->setInterval(33); // ~30 FPS
    connect(d_ptr->m_decayTimer, &QTimer::timeout, this, &LevelMeter::updatePeakDecay);
}

LevelMeter::~LevelMeter() = default;

int LevelMeter::channelCount() const { Q_D(const LevelMeter); return d->m_channelCount; }
double LevelMeter::minimum() const { Q_D(const LevelMeter); return d->m_minimum; }
double LevelMeter::maximum() const { Q_D(const LevelMeter); return d->m_maximum; }
double LevelMeter::warningThreshold() const { Q_D(const LevelMeter); return d->m_warningThreshold; }
double LevelMeter::errorThreshold() const { Q_D(const LevelMeter); return d->m_errorThreshold; }
int LevelMeter::segmentCount() const { Q_D(const LevelMeter); return d->m_segmentCount; }
LevelMeter::DisplayMode LevelMeter::displayMode() const { Q_D(const LevelMeter); return d->m_displayMode; }
Qt::Orientation LevelMeter::orientation() const { Q_D(const LevelMeter); return d->m_orientation; }
bool LevelMeter::isPeakHoldEnabled() const { Q_D(const LevelMeter); return d->m_peakHoldEnabled; }
int LevelMeter::peakHoldTimeMs() const { Q_D(const LevelMeter); return d->m_peakHoldTimeMs; }
double LevelMeter::peakDecayRate() const { Q_D(const LevelMeter); return d->m_peakDecayRate; }
bool LevelMeter::isScaleVisible() const { Q_D(const LevelMeter); return d->m_scaleVisible; }
QString LevelMeter::unit() const { Q_D(const LevelMeter); return d->m_unit; }
QString LevelMeter::title() const { Q_D(const LevelMeter); return d->m_title; }
QStringList LevelMeter::channelLabels() const { Q_D(const LevelMeter); return d->m_channelLabels; }
QColor LevelMeter::normalColor() const { Q_D(const LevelMeter); return d->m_normalColor; }
QColor LevelMeter::warningColor() const { Q_D(const LevelMeter); return d->m_warningColor; }
QColor LevelMeter::errorColor() const { Q_D(const LevelMeter); return d->m_errorColor; }
QColor LevelMeter::peakColor() const { Q_D(const LevelMeter); return d->m_peakColor; }
QColor LevelMeter::backgroundColor() const { Q_D(const LevelMeter); return d->m_backgroundColor; }
QColor LevelMeter::textColor() const { Q_D(const LevelMeter); return d->m_textColor; }

double LevelMeter::value(int channel) const
{
    Q_D(const LevelMeter);
    if (channel >= 0 && channel < d->m_channels.size()) {
        return d->m_channels[channel].value;
    }
    return d->m_minimum;
}

double LevelMeter::peakValue(int channel) const
{
    Q_D(const LevelMeter);
    if (channel >= 0 && channel < d->m_channels.size()) {
        return d->m_channels[channel].peakValue;
    }
    return d->m_minimum;
}

QVector<double> LevelMeter::values() const
{
    Q_D(const LevelMeter);
    QVector<double> res;
    res.reserve(d->m_channels.size());
    for (const auto &ch : d->m_channels) {
        res.append(ch.value);
    }
    return res;
}


QSize LevelMeter::sizeHint() const
{
    if (d_ptr->m_orientation == Qt::Vertical) {
        int w = 24 + d_ptr->m_channelCount * 18 + (d_ptr->m_scaleVisible ? 38 : 0);
        return {std::max(w, 65), 220};
    } else {
        int h = 24 + d_ptr->m_channelCount * 18 + (d_ptr->m_scaleVisible ? 32 : 0);
        return {220, std::max(h, 65)};
    }
}

QSize LevelMeter::minimumSizeHint() const
{
    if (d_ptr->m_orientation == Qt::Vertical) {
        return {45, 100};
    } else {
        return {100, 45};
    }
}

void LevelMeter::setValue(double value)
{
    setValue(0, value);
}

void LevelMeter::setValue(int channel, double value)
{
    if (channel < 0 || channel >= d_ptr->m_channels.size()) return;

    double clamped = std::clamp(value, d_ptr->m_minimum, d_ptr->m_maximum);
    ChannelData &ch = d_ptr->m_channels[channel];
    if (std::abs(ch.value - clamped) < 1e-4) return;

    ch.value = clamped;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (ch.value >= ch.peakValue) {
        ch.peakValue = ch.value;
        ch.lastPeakTime = now;
        if (d_ptr->m_peakHoldEnabled && !d_ptr->m_decayTimer->isActive()) {
            d_ptr->m_lastDecayTime = now;
            d_ptr->m_decayTimer->start();
        }
    }

    if (ch.value >= d_ptr->m_errorThreshold) {
        if (!ch.overloadState) {
            ch.overloadState = true;
            Q_EMIT overloadOccurred(channel);
        }
    } else {
        ch.overloadState = false;
    }

    Q_EMIT valueChanged(channel, ch.value);
    update();
}

void LevelMeter::setValues(const QVector<double> &values)
{
    int count = std::min(static_cast<int>(values.size()), static_cast<int>(d_ptr->m_channels.size()));
    for (int i = 0; i < count; ++i) {
        setValue(i, values[i]);
    }
}

void LevelMeter::resetPeaks()
{
    for (auto &ch : d_ptr->m_channels) {
        ch.peakValue = ch.value;
        ch.lastPeakTime = QDateTime::currentMSecsSinceEpoch();
    }
    update();
}

void LevelMeter::updatePeakDecay()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double dt = (d_ptr->m_lastDecayTime > 0) ? (now - d_ptr->m_lastDecayTime) / 1000.0 : 0.033;
    d_ptr->m_lastDecayTime = now;

    bool anyActive = false;
    for (auto &ch : d_ptr->m_channels) {
        if (ch.peakValue > ch.value) {
            if (now - ch.lastPeakTime >= d_ptr->m_peakHoldTimeMs) {
                ch.peakValue = std::max(ch.value, ch.peakValue - d_ptr->m_peakDecayRate * dt);
            }
            anyActive = true;
        } else {
            ch.peakValue = ch.value;
        }
    }

    if (!anyActive) {
        d_ptr->m_decayTimer->stop();
    }
    update();
}

void LevelMeter::setChannelCount(int count)
{
    count = std::max(1, count);
    if (d_ptr->m_channelCount == count) return;
    d_ptr->m_channelCount = count;

    d_ptr->m_channels.resize(d_ptr->m_channelCount);
    for (auto &ch : d_ptr->m_channels) {
        ch.value = std::clamp(ch.value, d_ptr->m_minimum, d_ptr->m_maximum);
        ch.peakValue = std::clamp(ch.peakValue, d_ptr->m_minimum, d_ptr->m_maximum);
    }

    d_ptr->m_cacheValid = false;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setRange(double min, double max)
{
    if (min >= max) return;
    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;

    for (auto &ch : d_ptr->m_channels) {
        ch.value = std::clamp(ch.value, d_ptr->m_minimum, d_ptr->m_maximum);
        ch.peakValue = std::clamp(ch.peakValue, d_ptr->m_minimum, d_ptr->m_maximum);
    }

    d_ptr->m_cacheValid = false;
    Q_EMIT rangeChanged(d_ptr->m_minimum, d_ptr->m_maximum);
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void LevelMeter::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void LevelMeter::setWarningThreshold(double threshold)
{
    if (std::abs(d_ptr->m_warningThreshold - threshold) < 1e-4) return;
    d_ptr->m_warningThreshold = threshold;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setErrorThreshold(double threshold)
{
    if (std::abs(d_ptr->m_errorThreshold - threshold) < 1e-4) return;
    d_ptr->m_errorThreshold = threshold;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setSegmentCount(int count)
{
    count = std::clamp(count, 5, 120);
    if (d_ptr->m_segmentCount == count) return;
    d_ptr->m_segmentCount = count;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setDisplayMode(DisplayMode mode)
{
    if (d_ptr->m_displayMode == mode) return;
    d_ptr->m_displayMode = mode;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setOrientation(Qt::Orientation orientation)
{
    if (d_ptr->m_orientation == orientation) return;
    d_ptr->m_orientation = orientation;
    d_ptr->m_cacheValid = false;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setPeakHoldEnabled(bool enabled)
{
    if (d_ptr->m_peakHoldEnabled == enabled) return;
    d_ptr->m_peakHoldEnabled = enabled;
    if (!d_ptr->m_peakHoldEnabled) {
        d_ptr->m_decayTimer->stop();
        resetPeaks();
    }
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setPeakHoldTimeMs(int ms)
{
    if (d_ptr->m_peakHoldTimeMs == ms) return;
    d_ptr->m_peakHoldTimeMs = std::max(0, ms);
    Q_EMIT appearanceChanged();
}

void LevelMeter::setPeakDecayRate(double rate)
{
    if (std::abs(d_ptr->m_peakDecayRate - rate) < 1e-4) return;
    d_ptr->m_peakDecayRate = std::max(0.1, rate);
    Q_EMIT appearanceChanged();
}

void LevelMeter::setScaleVisible(bool visible)
{
    if (d_ptr->m_scaleVisible == visible) return;
    d_ptr->m_scaleVisible = visible;
    d_ptr->m_cacheValid = false;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setUnit(const QString &unit)
{
    if (d_ptr->m_unit == unit) return;
    d_ptr->m_unit = unit;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setTitle(const QString &title)
{
    if (d_ptr->m_title == title) return;
    d_ptr->m_title = title;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setChannelLabels(const QStringList &labels)
{
    d_ptr->m_channelLabels = labels;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setNormalColor(const QColor &color)
{
    if (d_ptr->m_normalColor == color) return;
    d_ptr->m_normalColor = color;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setWarningColor(const QColor &color)
{
    if (d_ptr->m_warningColor == color) return;
    d_ptr->m_warningColor = color;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setErrorColor(const QColor &color)
{
    if (d_ptr->m_errorColor == color) return;
    d_ptr->m_errorColor = color;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setPeakColor(const QColor &color)
{
    if (d_ptr->m_peakColor == color) return;
    d_ptr->m_peakColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setBackgroundColor(const QColor &color)
{
    if (d_ptr->m_backgroundColor == color) return;
    d_ptr->m_backgroundColor = color;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor == color) return;
    d_ptr->m_textColor = color;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void LevelMeter::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    d_ptr->m_cacheValid = false;
}

QColor LevelMeter::colorForNormalizedValue(double norm) const
{
    double val = d_ptr->m_minimum + norm * (d_ptr->m_maximum - d_ptr->m_minimum);
    if (val >= d_ptr->m_errorThreshold) {
        return d_ptr->m_errorColor;
    }
    if (val >= d_ptr->m_warningThreshold) {
        return d_ptr->m_warningColor;
    }
    return d_ptr->m_normalColor;
}

QVector<QRectF> LevelMeter::calculateBarRects(const QRectF &contentRect) const
{
    QVector<QRectF> rects;
    if (d_ptr->m_channelCount <= 0) return rects;

    double scaleMargin = d_ptr->m_scaleVisible ? 36.0 : 0.0;
    double labelMargin = !d_ptr->m_channelLabels.isEmpty() ? 16.0 : 0.0;
    double titleMargin = !d_ptr->m_title.isEmpty() ? 18.0 : 0.0;

    if (d_ptr->m_orientation == Qt::Vertical) {
        QRectF area = contentRect;
        area.setTop(area.top() + titleMargin);
        area.setBottom(area.bottom() - labelMargin);
        area.setRight(area.right() - scaleMargin);

        double totalW = area.width();
        double gap = 4.0;
        double barW = std::max(6.0, (totalW - gap * (d_ptr->m_channelCount - 1)) / d_ptr->m_channelCount);

        double curX = area.left() + (totalW - (barW * d_ptr->m_channelCount + gap * (d_ptr->m_channelCount - 1))) * 0.5;
        for (int i = 0; i < d_ptr->m_channelCount; ++i) {
            rects.append(QRectF(curX, area.top(), barW, area.height()));
            curX += barW + gap;
        }
    } else {
        QRectF area = contentRect;
        area.setLeft(area.left() + labelMargin);
        area.setTop(area.top() + titleMargin);
        area.setBottom(area.bottom() - scaleMargin);

        double totalH = area.height();
        double gap = 4.0;
        double barH = std::max(6.0, (totalH - gap * (d_ptr->m_channelCount - 1)) / d_ptr->m_channelCount);

        double curY = area.top() + (totalH - (barH * d_ptr->m_channelCount + gap * (d_ptr->m_channelCount - 1))) * 0.5;
        for (int i = 0; i < d_ptr->m_channelCount; ++i) {
            rects.append(QRectF(area.left(), curY, area.width(), barH));
            curY += barH + gap;
        }
    }

    return rects;
}

void LevelMeter::renderStaticBackground()
{
    qreal dpr = devicePixelRatioF();
    QSize pixSize = size() * dpr;
    if (pixSize.isEmpty()) return;

    d_ptr->m_cachedBackground = QPixmap(pixSize);
    d_ptr->m_cachedBackground.setDevicePixelRatio(dpr);
    d_ptr->m_cachedBackground.fill(Qt::transparent);

    QPainter painter(&d_ptr->m_cachedBackground);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF chassisRect = rect().adjusted(3, 3, -3, -3);

    // Drop shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(12, 14, 18, 160));
    painter.drawRoundedRect(chassisRect.translated(1.5, 2.0), 6.0, 6.0);

    // Chassis body
    QLinearGradient bodyGrad(chassisRect.topLeft(), chassisRect.bottomRight());
    bodyGrad.setColorAt(0.0, d_ptr->m_backgroundColor.lighter(120));
    bodyGrad.setColorAt(0.5, d_ptr->m_backgroundColor);
    bodyGrad.setColorAt(1.0, d_ptr->m_backgroundColor.darker(130));
    painter.setBrush(bodyGrad);

    // Border bevel
    QLinearGradient borderGrad(chassisRect.topLeft(), chassisRect.bottomLeft());
    borderGrad.setColorAt(0.0, QColor(140, 150, 165, 160));
    borderGrad.setColorAt(0.5, QColor(70, 75, 85, 120));
    borderGrad.setColorAt(1.0, QColor(30, 32, 38, 200));
    painter.setPen(QPen(borderGrad, 1.2));
    painter.drawRoundedRect(chassisRect, 6.0, 6.0);

    // Title
    QRectF contentRect = chassisRect.adjusted(8, 8, -8, -8);
    if (!d_ptr->m_title.isEmpty()) {
        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(std::max(6.5, font().pointSizeF() * 0.8));
        painter.setFont(titleFont);
        painter.setPen(d_ptr->m_textColor.lighter(120));
        QRectF titleRect(contentRect.left(), contentRect.top(), contentRect.width(), 14.0);
        painter.drawText(titleRect, Qt::AlignCenter, d_ptr->m_title);
    }

    auto barRects = calculateBarRects(contentRect);

    // Channel Labels below/left of each bar
    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPointSizeF(std::max(6.5, font().pointSizeF() * 0.75));
    painter.setFont(labelFont);
    painter.setPen(d_ptr->m_textColor);

    for (int i = 0; i < barRects.size(); ++i) {
        QString lbl = (i < d_ptr->m_channelLabels.size()) ? d_ptr->m_channelLabels[i] : QString::number(i + 1);
        const QRectF &br = barRects[i];
        if (d_ptr->m_orientation == Qt::Vertical) {
            QRectF lblRect(br.left() - 4, br.bottom() + 2, br.width() + 8, 14);
            painter.drawText(lblRect, Qt::AlignCenter, lbl);
        } else {
            QRectF lblRect(contentRect.left(), br.top(), br.left() - contentRect.left() - 3, br.height());
            painter.drawText(lblRect, Qt::AlignRight | Qt::AlignVCenter, lbl);
        }
    }

    // Scale graduation ticks & labels
    if (d_ptr->m_scaleVisible && !barRects.isEmpty()) {
        const QRectF &refBar = barRects.last();
        QFont scaleFont = font();
        scaleFont.setPointSizeF(std::max(6.0, font().pointSizeF() * 0.7));
        painter.setFont(scaleFont);

        // Determine nice tick values
        QVector<double> ticks;
        if (d_ptr->m_minimum < 0 && d_ptr->m_maximum > 0 && d_ptr->m_unit == QStringLiteral("dB")) {
            // Standard logarithmic dB ladder marks
            QVector<double> stdDb = {-60, -40, -30, -20, -12, -6, -3, 0, 3, 6};
            for (double t : stdDb) {
                if (t >= d_ptr->m_minimum && t <= d_ptr->m_maximum) ticks.append(t);
            }
        } else {
            // Linear division (5 to 7 ticks)
            int numDivs = 6;
            double step = (d_ptr->m_maximum - d_ptr->m_minimum) / numDivs;
            for (int i = 0; i <= numDivs; ++i) {
                ticks.append(d_ptr->m_minimum + i * step);
            }
        }

        for (double t : ticks) {
            double norm = (t - d_ptr->m_minimum) / (d_ptr->m_maximum - d_ptr->m_minimum);
            norm = std::clamp(norm, 0.0, 1.0);

            QColor tColor = colorForNormalizedValue(norm);
            painter.setPen(QPen(tColor, (t == 0.0 || t == d_ptr->m_errorThreshold) ? 1.5 : 1.0));

            QString tickStr;
            if (d_ptr->m_unit == QStringLiteral("dB")) {
                tickStr = (t > 0 ? QStringLiteral("+") : QString()) + QString::number(static_cast<int>(t));
            } else {
                tickStr = QString::number(t, 'f', (std::abs(d_ptr->m_maximum - d_ptr->m_minimum) < 10) ? 1 : 0);
            }

            if (d_ptr->m_orientation == Qt::Vertical) {
                double y = refBar.bottom() - norm * refBar.height();
                double x1 = refBar.right() + 3.0;
                double x2 = x1 + 4.0;
                painter.drawLine(QPointF(x1, y), QPointF(x2, y));

                QRectF textR(x2 + 3.0, y - 7.0, 24.0, 14.0);
                painter.setPen(tColor);
                painter.drawText(textR, Qt::AlignLeft | Qt::AlignVCenter, tickStr);
            } else {
                double x = refBar.left() + norm * refBar.width();
                double y1 = refBar.bottom() + 3.0;
                double y2 = y1 + 4.0;
                painter.drawLine(QPointF(x, y1), QPointF(x, y2));

                QRectF textR(x - 12.0, y2 + 2.0, 24.0, 14.0);
                painter.setPen(tColor);
                painter.drawText(textR, Qt::AlignCenter, tickStr);
            }
        }

        // Unit label
        if (!d_ptr->m_unit.isEmpty()) {
            painter.setPen(d_ptr->m_textColor.darker(110));
            if (d_ptr->m_orientation == Qt::Vertical) {
                QRectF unitR(refBar.right() + 4.0, refBar.top() - 15.0, 30.0, 14.0);
                painter.drawText(unitR, Qt::AlignLeft | Qt::AlignVCenter, d_ptr->m_unit);
            } else {
                QRectF unitR(refBar.right() + 4.0, refBar.top(), 24.0, refBar.height());
                painter.drawText(unitR, Qt::AlignLeft | Qt::AlignVCenter, d_ptr->m_unit);
            }
        }
    }

    // Unlit Background LED Segments or Recessed Track
    for (const auto &br : barRects) {
        // Dark recessed slot behind bar
        painter.setPen(QPen(QColor(15, 17, 20), 1.0));
        painter.setBrush(QColor(12, 14, 16));
        painter.drawRoundedRect(br.adjusted(-1, -1, 1, 1), 2.0, 2.0);

        if (d_ptr->m_displayMode == DisplayMode::Segmented) {
            double segGap = 1.5;
            if (d_ptr->m_orientation == Qt::Vertical) {
                double totalH = br.height();
                double segH = (totalH - segGap * (d_ptr->m_segmentCount - 1)) / d_ptr->m_segmentCount;
                for (int s = 0; s < d_ptr->m_segmentCount; ++s) {
                    double norm = static_cast<double>(s) / std::max(1, d_ptr->m_segmentCount - 1);
                    double y = br.bottom() - (s + 1) * segH - s * segGap;
                    QRectF segRect(br.left(), y, br.width(), segH);

                    QColor unlit = colorForNormalizedValue(norm);
                    unlit.setAlpha(35); // Dim unlit ghost segment
                    painter.setPen(QPen(QColor(25, 27, 30), 0.5));
                    painter.setBrush(unlit);
                    painter.drawRoundedRect(segRect, 1.0, 1.0);
                }
            } else {
                double totalW = br.width();
                double segW = (totalW - segGap * (d_ptr->m_segmentCount - 1)) / d_ptr->m_segmentCount;
                for (int s = 0; s < d_ptr->m_segmentCount; ++s) {
                    double norm = static_cast<double>(s) / std::max(1, d_ptr->m_segmentCount - 1);
                    double x = br.left() + s * segW + s * segGap;
                    QRectF segRect(x, br.top(), segW, br.height());

                    QColor unlit = colorForNormalizedValue(norm);
                    unlit.setAlpha(35);
                    painter.setPen(QPen(QColor(25, 27, 30), 0.5));
                    painter.setBrush(unlit);
                    painter.drawRoundedRect(segRect, 1.0, 1.0);
                }
            }
        }
    }

    d_ptr->m_cacheValid = true;
}

void LevelMeter::drawSegmentedBar(QPainter &painter, double val, double peakVal, const QRectF &barRect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    double span = d_ptr->m_maximum - d_ptr->m_minimum;
    double normVal = (span > 0) ? std::clamp((val - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.0;
    double normPeak = (span > 0) ? std::clamp((peakVal - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.0;

    int litCount = static_cast<int>(std::round(normVal * d_ptr->m_segmentCount));
    int peakIndex = (d_ptr->m_peakHoldEnabled && normPeak > 0.0)
                        ? std::clamp(static_cast<int>(std::round(normPeak * (d_ptr->m_segmentCount - 1))), 0, d_ptr->m_segmentCount - 1)
                        : -1;

    double segGap = 1.5;

    if (d_ptr->m_orientation == Qt::Vertical) {
        double segH = (barRect.height() - segGap * (d_ptr->m_segmentCount - 1)) / d_ptr->m_segmentCount;
        for (int s = 0; s < d_ptr->m_segmentCount; ++s) {
            double norm = static_cast<double>(s) / std::max(1, d_ptr->m_segmentCount - 1);
            double y = barRect.bottom() - (s + 1) * segH - s * segGap;
            QRectF segRect(barRect.left(), y, barRect.width(), segH);

            bool isLit = (s < litCount);
            bool isPeak = (s == peakIndex);

            if (isLit) {
                QColor col = colorForNormalizedValue(norm);

                // LED 3D gradient highlight
                QLinearGradient segGrad(segRect.topLeft(), segRect.bottomLeft());
                segGrad.setColorAt(0.0, col.lighter(135));
                segGrad.setColorAt(0.5, col);
                segGrad.setColorAt(1.0, col.darker(120));

                painter.setPen(QPen(col.darker(140), 0.5));
                painter.setBrush(segGrad);
                painter.drawRoundedRect(segRect, 1.0, 1.0);
            }

            if (isPeak && !isLit) {
                // Floating peak indicator segment
                QColor peakCol = (normPeak * span + d_ptr->m_minimum >= d_ptr->m_errorThreshold) ? d_ptr->m_errorColor : d_ptr->m_peakColor;
                QLinearGradient peakGrad(segRect.topLeft(), segRect.bottomLeft());
                peakGrad.setColorAt(0.0, QColor(255, 255, 255));
                peakGrad.setColorAt(0.4, peakCol);
                peakGrad.setColorAt(1.0, peakCol.darker(120));

                painter.setPen(QPen(QColor(255, 255, 255, 200), 0.6));
                painter.setBrush(peakGrad);
                painter.drawRoundedRect(segRect, 1.0, 1.0);
            }
        }
    } else {
        double segW = (barRect.width() - segGap * (d_ptr->m_segmentCount - 1)) / d_ptr->m_segmentCount;
        for (int s = 0; s < d_ptr->m_segmentCount; ++s) {
            double norm = static_cast<double>(s) / std::max(1, d_ptr->m_segmentCount - 1);
            double x = barRect.left() + s * segW + s * segGap;
            QRectF segRect(x, barRect.top(), segW, barRect.height());

            bool isLit = (s < litCount);
            bool isPeak = (s == peakIndex);

            if (isLit) {
                QColor col = colorForNormalizedValue(norm);
                QLinearGradient segGrad(segRect.topLeft(), segRect.bottomLeft());
                segGrad.setColorAt(0.0, col.lighter(135));
                segGrad.setColorAt(0.5, col);
                segGrad.setColorAt(1.0, col.darker(120));

                painter.setPen(QPen(col.darker(140), 0.5));
                painter.setBrush(segGrad);
                painter.drawRoundedRect(segRect, 1.0, 1.0);
            }

            if (isPeak && !isLit) {
                QColor peakCol = (normPeak * span + d_ptr->m_minimum >= d_ptr->m_errorThreshold) ? d_ptr->m_errorColor : d_ptr->m_peakColor;
                painter.setPen(QPen(QColor(255, 255, 255, 200), 0.6));
                painter.setBrush(peakCol);
                painter.drawRoundedRect(segRect, 1.0, 1.0);
            }
        }
    }

    painter.restore();
}

void LevelMeter::drawContinuousBar(QPainter &painter, double val, double peakVal, const QRectF &barRect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    double span = d_ptr->m_maximum - d_ptr->m_minimum;
    double normVal = (span > 0) ? std::clamp((val - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.0;
    double normPeak = (span > 0) ? std::clamp((peakVal - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.0;

    if (d_ptr->m_orientation == Qt::Vertical) {
        double filledH = normVal * barRect.height();
        if (filledH > 0.5) {
            QRectF fillR(barRect.left(), barRect.bottom() - filledH, barRect.width(), filledH);

            QLinearGradient grad(barRect.bottomLeft(), barRect.topLeft());
            grad.setColorAt(0.0, d_ptr->m_normalColor);
            double warnNorm = (span > 0) ? std::clamp((d_ptr->m_warningThreshold - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.7;
            double errNorm = (span > 0) ? std::clamp((d_ptr->m_errorThreshold - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.9;
            grad.setColorAt(warnNorm, d_ptr->m_warningColor);
            grad.setColorAt(errNorm, d_ptr->m_errorColor);

            painter.setPen(Qt::NoPen);
            painter.setBrush(grad);
            painter.drawRoundedRect(fillR, 1.5, 1.5);
        }

        // Floating peak indicator line
        if (d_ptr->m_peakHoldEnabled && normPeak > 0.0) {
            double peakY = barRect.bottom() - normPeak * barRect.height();
            QColor peakCol = (normPeak * span + d_ptr->m_minimum >= d_ptr->m_errorThreshold) ? d_ptr->m_errorColor : d_ptr->m_peakColor;
            painter.setPen(QPen(peakCol, 2.0));
            painter.drawLine(QPointF(barRect.left(), peakY), QPointF(barRect.right(), peakY));
        }
    } else {
        double filledW = normVal * barRect.width();
        if (filledW > 0.5) {
            QRectF fillR(barRect.left(), barRect.top(), filledW, barRect.height());

            QLinearGradient grad(barRect.topLeft(), barRect.topRight());
            grad.setColorAt(0.0, d_ptr->m_normalColor);
            double warnNorm = (span > 0) ? std::clamp((d_ptr->m_warningThreshold - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.7;
            double errNorm = (span > 0) ? std::clamp((d_ptr->m_errorThreshold - d_ptr->m_minimum) / span, 0.0, 1.0) : 0.9;
            grad.setColorAt(warnNorm, d_ptr->m_warningColor);
            grad.setColorAt(errNorm, d_ptr->m_errorColor);

            painter.setPen(Qt::NoPen);
            painter.setBrush(grad);
            painter.drawRoundedRect(fillR, 1.5, 1.5);
        }

        // Floating peak indicator line
        if (d_ptr->m_peakHoldEnabled && normPeak > 0.0) {
            double peakX = barRect.left() + normPeak * barRect.width();
            QColor peakCol = (normPeak * span + d_ptr->m_minimum >= d_ptr->m_errorThreshold) ? d_ptr->m_errorColor : d_ptr->m_peakColor;
            painter.setPen(QPen(peakCol, 2.0));
            painter.drawLine(QPointF(peakX, barRect.top()), QPointF(peakX, barRect.bottom()));
        }
    }

    painter.restore();
}

void LevelMeter::paintEvent(QPaintEvent * /*event*/)
{
    if (!d_ptr->m_cacheValid || d_ptr->m_cachedBackground.size() != size() * devicePixelRatioF()) {
        renderStaticBackground();
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, d_ptr->m_cachedBackground);

    QRectF chassisRect = rect().adjusted(3, 3, -3, -3);
    QRectF contentRect = chassisRect.adjusted(8, 8, -8, -8);
    auto barRects = calculateBarRects(contentRect);

    int count = std::min(static_cast<int>(barRects.size()), static_cast<int>(d_ptr->m_channels.size()));
    for (int i = 0; i < count; ++i) {
        const ChannelData &ch = d_ptr->m_channels[i];
        const QRectF &br = barRects[i];
        if (d_ptr->m_displayMode == DisplayMode::Segmented) {
            drawSegmentedBar(painter, ch.value, ch.peakValue, br);
        } else {
            drawContinuousBar(painter, ch.value, ch.peakValue, br);
        }
    }
}

} // namespace QtIndustrialWidgets
