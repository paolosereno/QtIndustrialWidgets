#include <QtIndustrialWidgets/QLevelMeter.h>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QLinearGradient>
#include <QtGui/QRadialGradient>
#include <cmath>
#include <algorithm>

QLevelMeter::QLevelMeter(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_channels.resize(m_channelCount);
    for (auto &ch : m_channels) {
        ch.value = m_minimum;
        ch.peakValue = m_minimum;
    }

    m_decayTimer = new QTimer(this);
    m_decayTimer->setInterval(33); // ~30 FPS
    connect(m_decayTimer, &QTimer::timeout, this, &QLevelMeter::updatePeakDecay);
}

QLevelMeter::~QLevelMeter() = default;

double QLevelMeter::value(int channel) const
{
    if (channel >= 0 && channel < m_channels.size()) {
        return m_channels[channel].value;
    }
    return m_minimum;
}

double QLevelMeter::peakValue(int channel) const
{
    if (channel >= 0 && channel < m_channels.size()) {
        return m_channels[channel].peakValue;
    }
    return m_minimum;
}

QVector<double> QLevelMeter::values() const
{
    QVector<double> res;
    res.reserve(m_channels.size());
    for (const auto &ch : m_channels) {
        res.append(ch.value);
    }
    return res;
}

QSize QLevelMeter::sizeHint() const
{
    if (m_orientation == Qt::Vertical) {
        int w = 24 + m_channelCount * 18 + (m_scaleVisible ? 38 : 0);
        return {std::max(w, 65), 220};
    } else {
        int h = 24 + m_channelCount * 18 + (m_scaleVisible ? 32 : 0);
        return {220, std::max(h, 65)};
    }
}

QSize QLevelMeter::minimumSizeHint() const
{
    if (m_orientation == Qt::Vertical) {
        return {45, 100};
    } else {
        return {100, 45};
    }
}

void QLevelMeter::setValue(double value)
{
    setValue(0, value);
}

void QLevelMeter::setValue(int channel, double value)
{
    if (channel < 0 || channel >= m_channels.size()) return;

    double clamped = std::clamp(value, m_minimum, m_maximum);
    ChannelData &ch = m_channels[channel];
    if (std::abs(ch.value - clamped) < 1e-4) return;

    ch.value = clamped;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (ch.value >= ch.peakValue) {
        ch.peakValue = ch.value;
        ch.lastPeakTime = now;
        if (m_peakHoldEnabled && !m_decayTimer->isActive()) {
            m_lastDecayTime = now;
            m_decayTimer->start();
        }
    }

    if (ch.value >= m_errorThreshold) {
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

void QLevelMeter::setValues(const QVector<double> &values)
{
    int count = std::min(static_cast<int>(values.size()), static_cast<int>(m_channels.size()));
    for (int i = 0; i < count; ++i) {
        setValue(i, values[i]);
    }
}

void QLevelMeter::resetPeaks()
{
    for (auto &ch : m_channels) {
        ch.peakValue = ch.value;
        ch.lastPeakTime = QDateTime::currentMSecsSinceEpoch();
    }
    update();
}

void QLevelMeter::updatePeakDecay()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double dt = (m_lastDecayTime > 0) ? (now - m_lastDecayTime) / 1000.0 : 0.033;
    m_lastDecayTime = now;

    bool anyActive = false;
    for (auto &ch : m_channels) {
        if (ch.peakValue > ch.value) {
            if (now - ch.lastPeakTime >= m_peakHoldTimeMs) {
                ch.peakValue = std::max(ch.value, ch.peakValue - m_peakDecayRate * dt);
            }
            anyActive = true;
        } else {
            ch.peakValue = ch.value;
        }
    }

    if (!anyActive) {
        m_decayTimer->stop();
    }
    update();
}

void QLevelMeter::setChannelCount(int count)
{
    count = std::max(1, count);
    if (m_channelCount == count) return;
    m_channelCount = count;

    m_channels.resize(m_channelCount);
    for (auto &ch : m_channels) {
        ch.value = std::clamp(ch.value, m_minimum, m_maximum);
        ch.peakValue = std::clamp(ch.peakValue, m_minimum, m_maximum);
    }

    m_cacheValid = false;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setRange(double min, double max)
{
    if (min >= max) return;
    m_minimum = min;
    m_maximum = max;

    for (auto &ch : m_channels) {
        ch.value = std::clamp(ch.value, m_minimum, m_maximum);
        ch.peakValue = std::clamp(ch.peakValue, m_minimum, m_maximum);
    }

    m_cacheValid = false;
    Q_EMIT rangeChanged(m_minimum, m_maximum);
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setMinimum(double min)
{
    setRange(min, m_maximum);
}

void QLevelMeter::setMaximum(double max)
{
    setRange(m_minimum, max);
}

void QLevelMeter::setWarningThreshold(double threshold)
{
    if (std::abs(m_warningThreshold - threshold) < 1e-4) return;
    m_warningThreshold = threshold;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setErrorThreshold(double threshold)
{
    if (std::abs(m_errorThreshold - threshold) < 1e-4) return;
    m_errorThreshold = threshold;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setSegmentCount(int count)
{
    count = std::clamp(count, 5, 120);
    if (m_segmentCount == count) return;
    m_segmentCount = count;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setDisplayMode(DisplayMode mode)
{
    if (m_displayMode == mode) return;
    m_displayMode = mode;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) return;
    m_orientation = orientation;
    m_cacheValid = false;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setPeakHoldEnabled(bool enabled)
{
    if (m_peakHoldEnabled == enabled) return;
    m_peakHoldEnabled = enabled;
    if (!m_peakHoldEnabled) {
        m_decayTimer->stop();
        resetPeaks();
    }
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setPeakHoldTimeMs(int ms)
{
    if (m_peakHoldTimeMs == ms) return;
    m_peakHoldTimeMs = std::max(0, ms);
    Q_EMIT appearanceChanged();
}

void QLevelMeter::setPeakDecayRate(double rate)
{
    if (std::abs(m_peakDecayRate - rate) < 1e-4) return;
    m_peakDecayRate = std::max(0.1, rate);
    Q_EMIT appearanceChanged();
}

void QLevelMeter::setScaleVisible(bool visible)
{
    if (m_scaleVisible == visible) return;
    m_scaleVisible = visible;
    m_cacheValid = false;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setUnit(const QString &unit)
{
    if (m_unit == unit) return;
    m_unit = unit;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setTitle(const QString &title)
{
    if (m_title == title) return;
    m_title = title;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setChannelLabels(const QStringList &labels)
{
    m_channelLabels = labels;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setNormalColor(const QColor &color)
{
    if (m_normalColor == color) return;
    m_normalColor = color;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setWarningColor(const QColor &color)
{
    if (m_warningColor == color) return;
    m_warningColor = color;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setErrorColor(const QColor &color)
{
    if (m_errorColor == color) return;
    m_errorColor = color;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setPeakColor(const QColor &color)
{
    if (m_peakColor == color) return;
    m_peakColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor == color) return;
    m_backgroundColor = color;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::setTextColor(const QColor &color)
{
    if (m_textColor == color) return;
    m_textColor = color;
    m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void QLevelMeter::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_cacheValid = false;
}

QColor QLevelMeter::colorForNormalizedValue(double norm) const
{
    double val = m_minimum + norm * (m_maximum - m_minimum);
    if (val >= m_errorThreshold) {
        return m_errorColor;
    }
    if (val >= m_warningThreshold) {
        return m_warningColor;
    }
    return m_normalColor;
}

QVector<QRectF> QLevelMeter::calculateBarRects(const QRectF &contentRect) const
{
    QVector<QRectF> rects;
    if (m_channelCount <= 0) return rects;

    double scaleMargin = m_scaleVisible ? 36.0 : 0.0;
    double labelMargin = !m_channelLabels.isEmpty() ? 16.0 : 0.0;
    double titleMargin = !m_title.isEmpty() ? 18.0 : 0.0;

    if (m_orientation == Qt::Vertical) {
        QRectF area = contentRect;
        area.setTop(area.top() + titleMargin);
        area.setBottom(area.bottom() - labelMargin);
        area.setRight(area.right() - scaleMargin);

        double totalW = area.width();
        double gap = 4.0;
        double barW = std::max(6.0, (totalW - gap * (m_channelCount - 1)) / m_channelCount);

        double curX = area.left() + (totalW - (barW * m_channelCount + gap * (m_channelCount - 1))) * 0.5;
        for (int i = 0; i < m_channelCount; ++i) {
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
        double barH = std::max(6.0, (totalH - gap * (m_channelCount - 1)) / m_channelCount);

        double curY = area.top() + (totalH - (barH * m_channelCount + gap * (m_channelCount - 1))) * 0.5;
        for (int i = 0; i < m_channelCount; ++i) {
            rects.append(QRectF(area.left(), curY, area.width(), barH));
            curY += barH + gap;
        }
    }

    return rects;
}

void QLevelMeter::renderStaticBackground()
{
    qreal dpr = devicePixelRatioF();
    QSize pixSize = size() * dpr;
    if (pixSize.isEmpty()) return;

    m_cachedBackground = QPixmap(pixSize);
    m_cachedBackground.setDevicePixelRatio(dpr);
    m_cachedBackground.fill(Qt::transparent);

    QPainter painter(&m_cachedBackground);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF chassisRect = rect().adjusted(3, 3, -3, -3);

    // Drop shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(12, 14, 18, 160));
    painter.drawRoundedRect(chassisRect.translated(1.5, 2.0), 6.0, 6.0);

    // Chassis body
    QLinearGradient bodyGrad(chassisRect.topLeft(), chassisRect.bottomRight());
    bodyGrad.setColorAt(0.0, m_backgroundColor.lighter(120));
    bodyGrad.setColorAt(0.5, m_backgroundColor);
    bodyGrad.setColorAt(1.0, m_backgroundColor.darker(130));
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
    if (!m_title.isEmpty()) {
        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(std::max(6.5, font().pointSizeF() * 0.8));
        painter.setFont(titleFont);
        painter.setPen(m_textColor.lighter(120));
        QRectF titleRect(contentRect.left(), contentRect.top(), contentRect.width(), 14.0);
        painter.drawText(titleRect, Qt::AlignCenter, m_title);
    }

    auto barRects = calculateBarRects(contentRect);

    // Channel Labels below/left of each bar
    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPointSizeF(std::max(6.5, font().pointSizeF() * 0.75));
    painter.setFont(labelFont);
    painter.setPen(m_textColor);

    for (int i = 0; i < barRects.size(); ++i) {
        QString lbl = (i < m_channelLabels.size()) ? m_channelLabels[i] : QString::number(i + 1);
        const QRectF &br = barRects[i];
        if (m_orientation == Qt::Vertical) {
            QRectF lblRect(br.left() - 4, br.bottom() + 2, br.width() + 8, 14);
            painter.drawText(lblRect, Qt::AlignCenter, lbl);
        } else {
            QRectF lblRect(contentRect.left(), br.top(), br.left() - contentRect.left() - 3, br.height());
            painter.drawText(lblRect, Qt::AlignRight | Qt::AlignVCenter, lbl);
        }
    }

    // Scale graduation ticks & labels
    if (m_scaleVisible && !barRects.isEmpty()) {
        const QRectF &refBar = barRects.last();
        QFont scaleFont = font();
        scaleFont.setPointSizeF(std::max(6.0, font().pointSizeF() * 0.7));
        painter.setFont(scaleFont);

        // Determine nice tick values
        QVector<double> ticks;
        if (m_minimum < 0 && m_maximum > 0 && m_unit == QStringLiteral("dB")) {
            // Standard logarithmic dB ladder marks
            QVector<double> stdDb = {-60, -40, -30, -20, -12, -6, -3, 0, 3, 6};
            for (double t : stdDb) {
                if (t >= m_minimum && t <= m_maximum) ticks.append(t);
            }
        } else {
            // Linear division (5 to 7 ticks)
            int numDivs = 6;
            double step = (m_maximum - m_minimum) / numDivs;
            for (int i = 0; i <= numDivs; ++i) {
                ticks.append(m_minimum + i * step);
            }
        }

        for (double t : ticks) {
            double norm = (t - m_minimum) / (m_maximum - m_minimum);
            norm = std::clamp(norm, 0.0, 1.0);

            QColor tColor = colorForNormalizedValue(norm);
            painter.setPen(QPen(tColor, (t == 0.0 || t == m_errorThreshold) ? 1.5 : 1.0));

            QString tickStr;
            if (m_unit == QStringLiteral("dB")) {
                tickStr = (t > 0 ? QStringLiteral("+") : QString()) + QString::number(static_cast<int>(t));
            } else {
                tickStr = QString::number(t, 'f', (std::abs(m_maximum - m_minimum) < 10) ? 1 : 0);
            }

            if (m_orientation == Qt::Vertical) {
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
        if (!m_unit.isEmpty()) {
            painter.setPen(m_textColor.darker(110));
            if (m_orientation == Qt::Vertical) {
                QRectF unitR(refBar.right() + 4.0, refBar.top() - 15.0, 30.0, 14.0);
                painter.drawText(unitR, Qt::AlignLeft | Qt::AlignVCenter, m_unit);
            } else {
                QRectF unitR(refBar.right() + 4.0, refBar.top(), 24.0, refBar.height());
                painter.drawText(unitR, Qt::AlignLeft | Qt::AlignVCenter, m_unit);
            }
        }
    }

    // Unlit Background LED Segments or Recessed Track
    for (const auto &br : barRects) {
        // Dark recessed slot behind bar
        painter.setPen(QPen(QColor(15, 17, 20), 1.0));
        painter.setBrush(QColor(12, 14, 16));
        painter.drawRoundedRect(br.adjusted(-1, -1, 1, 1), 2.0, 2.0);

        if (m_displayMode == DisplayMode::Segmented) {
            double segGap = 1.5;
            if (m_orientation == Qt::Vertical) {
                double totalH = br.height();
                double segH = (totalH - segGap * (m_segmentCount - 1)) / m_segmentCount;
                for (int s = 0; s < m_segmentCount; ++s) {
                    double norm = static_cast<double>(s) / std::max(1, m_segmentCount - 1);
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
                double segW = (totalW - segGap * (m_segmentCount - 1)) / m_segmentCount;
                for (int s = 0; s < m_segmentCount; ++s) {
                    double norm = static_cast<double>(s) / std::max(1, m_segmentCount - 1);
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

    m_cacheValid = true;
}

void QLevelMeter::drawSegmentedBar(QPainter &painter, double val, double peakVal, const QRectF &barRect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    double span = m_maximum - m_minimum;
    double normVal = (span > 0) ? std::clamp((val - m_minimum) / span, 0.0, 1.0) : 0.0;
    double normPeak = (span > 0) ? std::clamp((peakVal - m_minimum) / span, 0.0, 1.0) : 0.0;

    int litCount = static_cast<int>(std::round(normVal * m_segmentCount));
    int peakIndex = (m_peakHoldEnabled && normPeak > 0.0)
                        ? std::clamp(static_cast<int>(std::round(normPeak * (m_segmentCount - 1))), 0, m_segmentCount - 1)
                        : -1;

    double segGap = 1.5;

    if (m_orientation == Qt::Vertical) {
        double segH = (barRect.height() - segGap * (m_segmentCount - 1)) / m_segmentCount;
        for (int s = 0; s < m_segmentCount; ++s) {
            double norm = static_cast<double>(s) / std::max(1, m_segmentCount - 1);
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
                QColor peakCol = (normPeak * span + m_minimum >= m_errorThreshold) ? m_errorColor : m_peakColor;
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
        double segW = (barRect.width() - segGap * (m_segmentCount - 1)) / m_segmentCount;
        for (int s = 0; s < m_segmentCount; ++s) {
            double norm = static_cast<double>(s) / std::max(1, m_segmentCount - 1);
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
                QColor peakCol = (normPeak * span + m_minimum >= m_errorThreshold) ? m_errorColor : m_peakColor;
                painter.setPen(QPen(QColor(255, 255, 255, 200), 0.6));
                painter.setBrush(peakCol);
                painter.drawRoundedRect(segRect, 1.0, 1.0);
            }
        }
    }

    painter.restore();
}

void QLevelMeter::drawContinuousBar(QPainter &painter, double val, double peakVal, const QRectF &barRect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    double span = m_maximum - m_minimum;
    double normVal = (span > 0) ? std::clamp((val - m_minimum) / span, 0.0, 1.0) : 0.0;
    double normPeak = (span > 0) ? std::clamp((peakVal - m_minimum) / span, 0.0, 1.0) : 0.0;

    if (m_orientation == Qt::Vertical) {
        double filledH = normVal * barRect.height();
        if (filledH > 0.5) {
            QRectF fillR(barRect.left(), barRect.bottom() - filledH, barRect.width(), filledH);

            QLinearGradient grad(barRect.bottomLeft(), barRect.topLeft());
            grad.setColorAt(0.0, m_normalColor);
            double warnNorm = (span > 0) ? std::clamp((m_warningThreshold - m_minimum) / span, 0.0, 1.0) : 0.7;
            double errNorm = (span > 0) ? std::clamp((m_errorThreshold - m_minimum) / span, 0.0, 1.0) : 0.9;
            grad.setColorAt(warnNorm, m_warningColor);
            grad.setColorAt(errNorm, m_errorColor);

            painter.setPen(Qt::NoPen);
            painter.setBrush(grad);
            painter.drawRoundedRect(fillR, 1.5, 1.5);
        }

        // Floating peak indicator line
        if (m_peakHoldEnabled && normPeak > 0.0) {
            double peakY = barRect.bottom() - normPeak * barRect.height();
            QColor peakCol = (normPeak * span + m_minimum >= m_errorThreshold) ? m_errorColor : m_peakColor;
            painter.setPen(QPen(peakCol, 2.0));
            painter.drawLine(QPointF(barRect.left(), peakY), QPointF(barRect.right(), peakY));
        }
    } else {
        double filledW = normVal * barRect.width();
        if (filledW > 0.5) {
            QRectF fillR(barRect.left(), barRect.top(), filledW, barRect.height());

            QLinearGradient grad(barRect.topLeft(), barRect.topRight());
            grad.setColorAt(0.0, m_normalColor);
            double warnNorm = (span > 0) ? std::clamp((m_warningThreshold - m_minimum) / span, 0.0, 1.0) : 0.7;
            double errNorm = (span > 0) ? std::clamp((m_errorThreshold - m_minimum) / span, 0.0, 1.0) : 0.9;
            grad.setColorAt(warnNorm, m_warningColor);
            grad.setColorAt(errNorm, m_errorColor);

            painter.setPen(Qt::NoPen);
            painter.setBrush(grad);
            painter.drawRoundedRect(fillR, 1.5, 1.5);
        }

        // Floating peak indicator line
        if (m_peakHoldEnabled && normPeak > 0.0) {
            double peakX = barRect.left() + normPeak * barRect.width();
            QColor peakCol = (normPeak * span + m_minimum >= m_errorThreshold) ? m_errorColor : m_peakColor;
            painter.setPen(QPen(peakCol, 2.0));
            painter.drawLine(QPointF(peakX, barRect.top()), QPointF(peakX, barRect.bottom()));
        }
    }

    painter.restore();
}

void QLevelMeter::paintEvent(QPaintEvent * /*event*/)
{
    if (!m_cacheValid || m_cachedBackground.size() != size() * devicePixelRatioF()) {
        renderStaticBackground();
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_cachedBackground);

    QRectF chassisRect = rect().adjusted(3, 3, -3, -3);
    QRectF contentRect = chassisRect.adjusted(8, 8, -8, -8);
    auto barRects = calculateBarRects(contentRect);

    int count = std::min(static_cast<int>(barRects.size()), static_cast<int>(m_channels.size()));
    for (int i = 0; i < count; ++i) {
        const ChannelData &ch = m_channels[i];
        const QRectF &br = barRects[i];
        if (m_displayMode == DisplayMode::Segmented) {
            drawSegmentedBar(painter, ch.value, ch.peakValue, br);
        } else {
            drawContinuousBar(painter, ch.value, ch.peakValue, br);
        }
    }
}
