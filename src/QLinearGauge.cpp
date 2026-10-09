// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/QLinearGauge.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QLinearGradient>
#include <QtCore/QtMath>
#include <algorithm>

QLinearGauge::QLinearGauge(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize QLinearGauge::sizeHint() const
{
    return (m_orientation == Qt::Vertical) ? QSize(100, 280) : QSize(280, 100);
}

QSize QLinearGauge::minimumSizeHint() const
{
    return (m_orientation == Qt::Vertical) ? QSize(50, 120) : QSize(120, 50);
}

void QLinearGauge::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) return;
    m_orientation = orientation;
    invalidateCache();
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setThermometerMode(bool thermometer)
{
    if (m_thermometerMode == thermometer) return;
    m_thermometerMode = thermometer;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setValue(double val)
{
    double clamped = std::clamp(val, m_minimum, m_maximum);
    if (qFuzzyCompare(clamped, m_value)) {
        return;
    }

    m_value = clamped;
    Q_EMIT valueChanged(m_value);
    update();
}

void QLinearGauge::setMinimum(double min)
{
    setRange(min, m_maximum);
}

void QLinearGauge::setMaximum(double max)
{
    setRange(m_minimum, max);
}

void QLinearGauge::setRange(double min, double max)
{
    if (min >= max) return;
    if (qFuzzyCompare(min, m_minimum) && qFuzzyCompare(max, m_maximum)) return;

    m_minimum = min;
    m_maximum = max;
    m_value = std::clamp(m_value, m_minimum, m_maximum);

    invalidateCache();
    Q_EMIT rangeChanged(m_minimum, m_maximum);
    Q_EMIT valueChanged(m_value);
    update();
}

void QLinearGauge::setPrecision(int precision)
{
    if (m_precision == precision) return;
    m_precision = std::max(0, precision);
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setUnit(const QString &unit)
{
    if (m_unit == unit) return;
    m_unit = unit;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setMajorTicks(int count)
{
    if (m_majorTicks == count || count < 1) return;
    m_majorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setMinorTicks(int count)
{
    if (m_minorTicks == count || count < 0) return;
    m_minorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setWarningThreshold(double threshold)
{
    if (qFuzzyCompare(m_warningThreshold, threshold)) return;
    m_warningThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(m_warningThreshold, m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setErrorThreshold(double threshold)
{
    if (qFuzzyCompare(m_errorThreshold, threshold)) return;
    m_errorThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(m_warningThreshold, m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setDynamicLiquidColor(bool dynamic)
{
    if (m_dynamicLiquidColor == dynamic) return;
    m_dynamicLiquidColor = dynamic;
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setGradientLiquid(bool gradient)
{
    if (m_gradientLiquid == gradient) return;
    m_gradientLiquid = gradient;
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setDigitalDisplayVisible(bool visible)
{
    if (m_digitalDisplayVisible == visible) return;
    m_digitalDisplayVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setScaleVisible(bool visible)
{
    if (m_scaleVisible == visible) return;
    m_scaleVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setLiquidColor(const QColor &color)
{
    if (m_liquidColor == color) return;
    m_liquidColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setNormalColor(const QColor &color)
{
    if (m_normalColor == color) return;
    m_normalColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setWarningColor(const QColor &color)
{
    if (m_warningColor == color) return;
    m_warningColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setErrorColor(const QColor &color)
{
    if (m_errorColor == color) return;
    m_errorColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setTroughColor(const QColor &color)
{
    if (m_troughColor == color) return;
    m_troughColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setScaleColor(const QColor &color)
{
    if (m_scaleColor == color) return;
    m_scaleColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setTextColor(const QColor &color)
{
    if (m_textColor == color) return;
    m_textColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::setBezelColor(const QColor &color)
{
    if (m_bezelColor == color) return;
    m_bezelColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QLinearGauge::invalidateCache()
{
    m_cacheDirty = true;
}

void QLinearGauge::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void QLinearGauge::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        invalidateCache();
        update();
    }
    QWidget::changeEvent(event);
}

QColor QLinearGauge::determineActiveLiquidColor() const
{
    if (!m_dynamicLiquidColor) {
        return m_liquidColor;
    }
    if (m_value >= m_errorThreshold) {
        return m_errorColor;
    }
    if (m_value >= m_warningThreshold) {
        return m_warningColor;
    }
    return m_normalColor;
}

void QLinearGauge::renderStaticScale(const QSize &targetSize)
{
    qreal dpr = devicePixelRatioF();
    QSize pixmapSize = targetSize * dpr;
    if (pixmapSize.isEmpty()) return;

    m_cachePixmap = QPixmap(pixmapSize);
    m_cachePixmap.setDevicePixelRatio(dpr);
    m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = targetSize.width();
    const double h = targetSize.height();

    // Draw background casing
    QRectF casingRect(1.0, 1.0, w - 2.0, h - 2.0);
    QLinearGradient casingGrad(casingRect.topLeft(), casingRect.bottomRight());
    casingGrad.setColorAt(0.0, m_bezelColor.lighter(110));
    casingGrad.setColorAt(1.0, m_bezelColor.darker(130));
    painter.setPen(QPen(m_bezelColor.darker(160), 1.5));
    painter.setBrush(casingGrad);
    painter.drawRoundedRect(casingRect, 6.0, 6.0);

    // Compute layout based on orientation and thermometer mode
    if (m_orientation == Qt::Vertical) {
        // Top area: optional digital display
        double topPadding = m_digitalDisplayVisible ? 36.0 : 16.0;
        double bottomPadding = 16.0;
        double tubeWidth = std::clamp(w * 0.16, 10.0, 26.0);

        if (m_thermometerMode) {
            m_bulbRadius = tubeWidth * 1.35;
            bottomPadding = m_bulbRadius * 2.0 + 12.0;
        } else {
            m_bulbRadius = 0.0;
        }

        double tubeX = m_scaleVisible ? (w * 0.32 - tubeWidth * 0.5) : (w * 0.5 - tubeWidth * 0.5);
        double tubeTop = topPadding;
        double tubeBottom = h - bottomPadding;
        double tubeHeight = std::max(10.0, tubeBottom - tubeTop);

        m_tubeRect = QRectF(tubeX, tubeTop, tubeWidth, tubeHeight);
        m_bulbCenter = QPointF(tubeX + tubeWidth * 0.5, h - m_bulbRadius - 8.0);

        // Draw empty glass tube trough
        QLinearGradient troughGrad(m_tubeRect.topLeft(), m_tubeRect.topRight());
        troughGrad.setColorAt(0.0, m_troughColor.darker(150));
        troughGrad.setColorAt(0.3, m_troughColor.darker(110));
        troughGrad.setColorAt(0.7, m_troughColor);
        troughGrad.setColorAt(1.0, m_troughColor.darker(140));

        painter.setPen(QPen(m_bezelColor.darker(180), 1.2));
        painter.setBrush(troughGrad);

        if (m_thermometerMode) {
            // Merge tube and bulb into single path
            QPainterPath troughPath;
            troughPath.addRoundedRect(m_tubeRect, tubeWidth * 0.5, tubeWidth * 0.5);
            troughPath.addEllipse(m_bulbCenter, m_bulbRadius, m_bulbRadius);
            painter.drawPath(troughPath.simplified());
        } else {
            painter.drawRoundedRect(m_tubeRect, 4.0, 4.0);
        }

        // Draw Scale (Ticks and numeric labels)
        if (m_scaleVisible) {
            const double tickStartX = m_tubeRect.right() + 6.0;
            const double majorTickLen = std::max(6.0, w * 0.12);
            const double minorTickLen = majorTickLen * 0.55;
            const double labelX = tickStartX + majorTickLen + 5.0;

            int totalMinorDivisions = m_majorTicks * (m_minorTicks + 1);
            double valRange = m_maximum - m_minimum;

            // Minor ticks
            if (m_minorTicks > 0) {
                painter.setPen(QPen(m_scaleColor.darker(130), 1.0));
                for (int i = 0; i <= totalMinorDivisions; ++i) {
                    if (i % (m_minorTicks + 1) == 0) continue;
                    double frac = static_cast<double>(i) / totalMinorDivisions;
                    double y = m_tubeRect.bottom() - frac * m_tubeRect.height();
                    painter.drawLine(QPointF(tickStartX, y), QPointF(tickStartX + minorTickLen, y));
                }
            }

            // Major ticks and labels
            int fontSize = std::clamp(static_cast<int>(h * 0.04), 8, 12);
            QFont f = painter.font();
            f.setPixelSize(fontSize);
            f.setBold(true);
            painter.setFont(f);

            painter.setPen(QPen(m_scaleColor, 1.6));

            for (int i = 0; i <= m_majorTicks; ++i) {
                double frac = static_cast<double>(i) / m_majorTicks;
                double val = m_minimum + frac * valRange;
                double y = m_tubeRect.bottom() - frac * m_tubeRect.height();

                painter.setPen(QPen(m_scaleColor, 1.6));
                painter.drawLine(QPointF(tickStartX, y), QPointF(tickStartX + majorTickLen, y));

                QString s = (m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                               : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);

                QFontMetricsF fm(f);
                QRectF textR = fm.boundingRect(s);
                QRectF drawR(labelX, y - textR.height() * 0.5, w - labelX - 4.0, textR.height());
                painter.setPen(m_textColor);
                painter.drawText(drawR, Qt::AlignLeft | Qt::AlignVCenter, s);
            }
        }

        // Digital display static pod (top)
        if (m_digitalDisplayVisible) {
            QRectF podRect(6.0, 6.0, w - 12.0, 24.0);
            painter.setPen(QPen(m_bezelColor.darker(140), 1.0));
            painter.setBrush(QColor(16, 20, 26, 220));
            painter.drawRoundedRect(podRect, 3.0, 3.0);
        }
    } else {
        // Horizontal orientation
        double leftPadding = 16.0;
        double rightPadding = m_digitalDisplayVisible ? 55.0 : 16.0;
        double tubeHeight = std::clamp(h * 0.16, 10.0, 26.0);

        if (m_thermometerMode) {
            m_bulbRadius = tubeHeight * 1.35;
            leftPadding = m_bulbRadius * 2.0 + 12.0;
        } else {
            m_bulbRadius = 0.0;
        }

        double tubeY = m_scaleVisible ? (h * 0.40 - tubeHeight * 0.5) : (h * 0.5 - tubeHeight * 0.5);
        double tubeLeft = leftPadding;
        double tubeRight = w - rightPadding;
        double tubeWidth = std::max(10.0, tubeRight - tubeLeft);

        m_tubeRect = QRectF(tubeLeft, tubeY, tubeWidth, tubeHeight);
        m_bulbCenter = QPointF(m_bulbRadius + 8.0, tubeY + tubeHeight * 0.5);

        // Glass trough
        QLinearGradient troughGrad(m_tubeRect.topLeft(), m_tubeRect.bottomLeft());
        troughGrad.setColorAt(0.0, m_troughColor.darker(150));
        troughGrad.setColorAt(0.3, m_troughColor.darker(110));
        troughGrad.setColorAt(0.7, m_troughColor);
        troughGrad.setColorAt(1.0, m_troughColor.darker(140));

        painter.setPen(QPen(m_bezelColor.darker(180), 1.2));
        painter.setBrush(troughGrad);

        if (m_thermometerMode) {
            QPainterPath troughPath;
            troughPath.addRoundedRect(m_tubeRect, tubeHeight * 0.5, tubeHeight * 0.5);
            troughPath.addEllipse(m_bulbCenter, m_bulbRadius, m_bulbRadius);
            painter.drawPath(troughPath.simplified());
        } else {
            painter.drawRoundedRect(m_tubeRect, 4.0, 4.0);
        }

        // Scale
        if (m_scaleVisible) {
            const double tickStartY = m_tubeRect.bottom() + 6.0;
            const double majorTickLen = std::max(6.0, h * 0.12);
            const double minorTickLen = majorTickLen * 0.55;
            const double labelY = tickStartY + majorTickLen + 2.0;

            int totalMinorDivisions = m_majorTicks * (m_minorTicks + 1);
            double valRange = m_maximum - m_minimum;

            if (m_minorTicks > 0) {
                painter.setPen(QPen(m_scaleColor.darker(130), 1.0));
                for (int i = 0; i <= totalMinorDivisions; ++i) {
                    if (i % (m_minorTicks + 1) == 0) continue;
                    double frac = static_cast<double>(i) / totalMinorDivisions;
                    double x = m_tubeRect.left() + frac * m_tubeRect.width();
                    painter.drawLine(QPointF(x, tickStartY), QPointF(x, tickStartY + minorTickLen));
                }
            }

            int fontSize = std::clamp(static_cast<int>(h * 0.12), 8, 12);
            QFont f = painter.font();
            f.setPixelSize(fontSize);
            f.setBold(true);
            painter.setFont(f);

            for (int i = 0; i <= m_majorTicks; ++i) {
                double frac = static_cast<double>(i) / m_majorTicks;
                double val = m_minimum + frac * valRange;
                double x = m_tubeRect.left() + frac * m_tubeRect.width();

                painter.setPen(QPen(m_scaleColor, 1.6));
                painter.drawLine(QPointF(x, tickStartY), QPointF(x, tickStartY + majorTickLen));

                QString s = (m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                               : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);

                QFontMetricsF fm(f);
                QRectF textR = fm.boundingRect(s);
                QRectF drawR(x - textR.width() * 0.5, labelY, textR.width(), textR.height());
                painter.setPen(m_textColor);
                painter.drawText(drawR, Qt::AlignCenter, s);
            }
        }

        // Digital display static pod (right)
        if (m_digitalDisplayVisible) {
            QRectF podRect(w - rightPadding + 4.0, 6.0, rightPadding - 10.0, h - 12.0);
            painter.setPen(QPen(m_bezelColor.darker(140), 1.0));
            painter.setBrush(QColor(16, 20, 26, 220));
            painter.drawRoundedRect(podRect, 3.0, 3.0);
        }
    }

    m_cacheDirty = false;
}

void QLinearGauge::paintEvent(QPaintEvent *)
{
    if (m_cacheDirty || m_cachePixmap.size() != (size() * devicePixelRatioF())) {
        renderStaticScale(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast blit of cached background & scale
    painter.drawPixmap(0, 0, m_cachePixmap);

    // 2. Liquid Column & Bulb Dynamic Rendering
    QColor activeColor = determineActiveLiquidColor();
    double factor = (m_maximum > m_minimum) ? (m_value - m_minimum) / (m_maximum - m_minimum) : 0.0;
    factor = std::clamp(factor, 0.0, 1.0);

    if (m_orientation == Qt::Vertical) {
        double fillHeight = m_tubeRect.height() * factor;
        QRectF fillRect(m_tubeRect.left() + 1.0,
                        m_tubeRect.bottom() - fillHeight,
                        m_tubeRect.width() - 2.0,
                        fillHeight);

        // Fill bulb first if in thermometer mode
        if (m_thermometerMode) {
            QRadialGradient bulbGrad(m_bulbCenter, m_bulbRadius);
            bulbGrad.setColorAt(0.0, activeColor.lighter(130));
            bulbGrad.setColorAt(0.7, activeColor);
            bulbGrad.setColorAt(1.0, activeColor.darker(130));

            painter.setPen(Qt::NoPen);
            painter.setBrush(bulbGrad);
            painter.drawEllipse(m_bulbCenter, m_bulbRadius - 1.0, m_bulbRadius - 1.0);
        }

        // Fill liquid column
        if (fillHeight > 0.0) {
            QLinearGradient liquidGrad(fillRect.topLeft(), fillRect.topRight());
            if (m_gradientLiquid) {
                liquidGrad = QLinearGradient(fillRect.bottomLeft(), fillRect.topLeft());
                liquidGrad.setColorAt(0.0, m_normalColor);
                liquidGrad.setColorAt(0.7, m_warningColor);
                liquidGrad.setColorAt(1.0, m_errorColor);
            } else {
                liquidGrad.setColorAt(0.0, activeColor.lighter(125));
                liquidGrad.setColorAt(0.4, activeColor);
                liquidGrad.setColorAt(1.0, activeColor.darker(120));
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(liquidGrad);
            painter.drawRoundedRect(fillRect, 2.0, 2.0);
        }

        // Glass highlight reflection line along the tube
        painter.setPen(QPen(QColor(255, 255, 255, 90), 1.5));
        painter.drawLine(QPointF(m_tubeRect.left() + 2.5, m_tubeRect.top() + 4.0),
                         QPointF(m_tubeRect.left() + 2.5, m_tubeRect.bottom() - 2.0));

        // Digital display readout
        if (m_digitalDisplayVisible) {
            QRectF podRect(6.0, 6.0, width() - 12.0, 24.0);
            QString txt = QStringLiteral("%1 %2").arg(QString::number(m_value, 'f', m_precision), m_unit);
            QFont f = font();
            f.setPixelSize(12);
            f.setBold(true);
            painter.setFont(f);
            painter.setPen(activeColor);
            painter.drawText(podRect, Qt::AlignCenter, txt);
        }
    } else {
        // Horizontal orientation
        double fillWidth = m_tubeRect.width() * factor;
        QRectF fillRect(m_tubeRect.left(),
                        m_tubeRect.top() + 1.0,
                        fillWidth,
                        m_tubeRect.height() - 2.0);

        if (m_thermometerMode) {
            QRadialGradient bulbGrad(m_bulbCenter, m_bulbRadius);
            bulbGrad.setColorAt(0.0, activeColor.lighter(130));
            bulbGrad.setColorAt(0.7, activeColor);
            bulbGrad.setColorAt(1.0, activeColor.darker(130));

            painter.setPen(Qt::NoPen);
            painter.setBrush(bulbGrad);
            painter.drawEllipse(m_bulbCenter, m_bulbRadius - 1.0, m_bulbRadius - 1.0);
        }

        if (fillWidth > 0.0) {
            QLinearGradient liquidGrad(fillRect.topLeft(), fillRect.bottomLeft());
            if (m_gradientLiquid) {
                liquidGrad = QLinearGradient(fillRect.topLeft(), fillRect.topRight());
                liquidGrad.setColorAt(0.0, m_normalColor);
                liquidGrad.setColorAt(0.7, m_warningColor);
                liquidGrad.setColorAt(1.0, m_errorColor);
            } else {
                liquidGrad.setColorAt(0.0, activeColor.lighter(125));
                liquidGrad.setColorAt(0.4, activeColor);
                liquidGrad.setColorAt(1.0, activeColor.darker(120));
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(liquidGrad);
            painter.drawRoundedRect(fillRect, 2.0, 2.0);
        }

        // Glass highlight reflection line
        painter.setPen(QPen(QColor(255, 255, 255, 90), 1.5));
        painter.drawLine(QPointF(m_tubeRect.left() + 2.0, m_tubeRect.top() + 2.5),
                         QPointF(m_tubeRect.right() - 2.0, m_tubeRect.top() + 2.5));

        // Digital display readout
        if (m_digitalDisplayVisible) {
            double rightPadding = 55.0;
            QRectF podRect(width() - rightPadding + 4.0, 6.0, rightPadding - 10.0, height() - 12.0);
            QString txt = QString::number(m_value, 'f', m_precision);
            QFont f = font();
            f.setPixelSize(12);
            f.setBold(true);
            painter.setFont(f);
            painter.setPen(activeColor);
            painter.drawText(podRect, Qt::AlignCenter, txt);
        }
    }
}
