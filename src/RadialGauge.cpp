// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/RadialGauge.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtCore/QtMath>
#include <algorithm>

namespace QtIndustrialWidgets {


RadialGauge::RadialGauge(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize RadialGauge::sizeHint() const
{
    return QSize(220, 220);
}

QSize RadialGauge::minimumSizeHint() const
{
    return QSize(90, 90);
}

void RadialGauge::setValue(double val)
{
    double clamped = std::clamp(val, m_minimum, m_maximum);
    if (qFuzzyCompare(clamped, m_value)) {
        return;
    }

    m_value = clamped;
    Q_EMIT valueChanged(m_value);

    bool isWarning = (m_value >= m_warningThreshold && m_value < m_errorThreshold);
    bool isError = (m_value >= m_errorThreshold);

    if (isWarning != m_wasWarning) {
        m_wasWarning = isWarning;
        Q_EMIT warningExceeded(isWarning);
    }
    if (isError != m_wasError) {
        m_wasError = isError;
        Q_EMIT errorExceeded(isError);
    }

    update();
}

void RadialGauge::setMinimum(double min)
{
    setRange(min, m_maximum);
}

void RadialGauge::setMaximum(double max)
{
    setRange(m_minimum, max);
}

void RadialGauge::setRange(double min, double max)
{
    if (min >= max) {
        return;
    }
    if (qFuzzyCompare(min, m_minimum) && qFuzzyCompare(max, m_maximum)) {
        return;
    }

    m_minimum = min;
    m_maximum = max;
    m_value = std::clamp(m_value, m_minimum, m_maximum);

    invalidateCache();
    Q_EMIT rangeChanged(m_minimum, m_maximum);
    Q_EMIT valueChanged(m_value);
    update();
}

void RadialGauge::setPrecision(int precision)
{
    if (m_precision == precision) return;
    m_precision = std::max(0, precision);
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setUnit(const QString &unit)
{
    if (m_unit == unit) return;
    m_unit = unit;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setStartAngle(double angle)
{
    if (qFuzzyCompare(m_startAngle, angle)) return;
    m_startAngle = angle;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setSpanAngle(double span)
{
    if (qFuzzyCompare(m_spanAngle, span) || span <= 0.0) return;
    m_spanAngle = span;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setMajorTicks(int count)
{
    if (m_majorTicks == count || count < 1) return;
    m_majorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setMinorTicks(int count)
{
    if (m_minorTicks == count || count < 0) return;
    m_minorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setWarningThreshold(double threshold)
{
    if (qFuzzyCompare(m_warningThreshold, threshold)) return;
    m_warningThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(m_warningThreshold, m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setErrorThreshold(double threshold)
{
    if (qFuzzyCompare(m_errorThreshold, threshold)) return;
    m_errorThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(m_warningThreshold, m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setThresholdBandsVisible(bool visible)
{
    if (m_thresholdBandsVisible == visible) return;
    m_thresholdBandsVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setDigitalDisplayVisible(bool visible)
{
    if (m_digitalDisplayVisible == visible) return;
    m_digitalDisplayVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setNeedleColor(const QColor &color)
{
    if (m_needleColor == color) return;
    m_needleColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setNormalColor(const QColor &color)
{
    if (m_normalColor == color) return;
    m_normalColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setWarningColor(const QColor &color)
{
    if (m_warningColor == color) return;
    m_warningColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setErrorColor(const QColor &color)
{
    if (m_errorColor == color) return;
    m_errorColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setDialColor(const QColor &color)
{
    if (m_dialColor == color) return;
    m_dialColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setScaleColor(const QColor &color)
{
    if (m_scaleColor == color) return;
    m_scaleColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setTextColor(const QColor &color)
{
    if (m_textColor == color) return;
    m_textColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setBezelColor(const QColor &color)
{
    if (m_bezelColor == color) return;
    m_bezelColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::invalidateCache()
{
    m_cacheDirty = true;
}

void RadialGauge::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void RadialGauge::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        invalidateCache();
        update();
    }
    QWidget::changeEvent(event);
}

double RadialGauge::valueToAngle(double val) const
{
    if (m_maximum <= m_minimum) {
        return m_startAngle;
    }
    double factor = (val - m_minimum) / (m_maximum - m_minimum);
    factor = std::clamp(factor, 0.0, 1.0);
    return m_startAngle + factor * m_spanAngle;
}

void RadialGauge::renderStaticScale(const QSize &targetSize)
{
    qreal dpr = devicePixelRatioF();
    QSize pixmapSize = (targetSize * dpr);
    if (pixmapSize.isEmpty()) {
        return;
    }

    m_cachePixmap = QPixmap(pixmapSize);
    m_cachePixmap.setDevicePixelRatio(dpr);
    m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = targetSize.width();
    const double h = targetSize.height();
    const double side = std::min(w, h);
    const QPointF center(w * 0.5, h * 0.5);
    const double radius = (side * 0.5) - 4.0;

    if (radius <= 5.0) {
        m_cacheDirty = false;
        return;
    }

    // 1. Outer Bezel Ring (Machined metal / industrial casing effect)
    QRadialGradient bezelGrad(center, radius);
    bezelGrad.setColorAt(0.0, m_dialColor.lighter(110));
    bezelGrad.setColorAt(0.85, m_dialColor);
    bezelGrad.setColorAt(0.96, m_bezelColor.lighter(130));
    bezelGrad.setColorAt(1.0, m_bezelColor.darker(140));

    painter.setPen(QPen(m_bezelColor.darker(160), 1.5));
    painter.setBrush(bezelGrad);
    painter.drawEllipse(center, radius, radius);

    // Subtle inner shadow ring
    const double innerRadius = radius * 0.95;
    QRadialGradient dialGrad(center, innerRadius);
    dialGrad.setColorAt(0.0, m_dialColor.lighter(105));
    dialGrad.setColorAt(0.7, m_dialColor);
    dialGrad.setColorAt(1.0, m_dialColor.darker(125));

    painter.setPen(QPen(m_dialColor.darker(150), 1.0));
    painter.setBrush(dialGrad);
    painter.drawEllipse(center, innerRadius, innerRadius);

    const double arcRadius = radius * 0.82;
    const double arcWidth = std::max(3.0, radius * 0.055);

    // Helper lambda to draw smooth arc path between two values
    auto drawArcSegment = [&](double vStart, double vEnd, const QColor &color) {
        if (vStart >= vEnd) return;
        double a1 = valueToAngle(vStart);
        double a2 = valueToAngle(vEnd);
        int steps = std::max(6, static_cast<int>(std::abs(a2 - a1) * 0.5));

        QPainterPath path;
        for (int i = 0; i <= steps; ++i) {
            double angle = a1 + (a2 - a1) * (static_cast<double>(i) / steps);
            double rad = (angle - 90.0) * M_PI / 180.0;
            double px = center.x() + arcRadius * std::cos(rad);
            double py = center.y() + arcRadius * std::sin(rad);
            if (i == 0) {
                path.moveTo(px, py);
            } else {
                path.lineTo(px, py);
            }
        }
        QPen arcPen(color, arcWidth, Qt::SolidLine, Qt::FlatCap);
        painter.strokePath(path, arcPen);
    };

    // 2. Colored Threshold Bands
    if (m_thresholdBandsVisible) {
        double vNormEnd = std::min(m_warningThreshold, m_maximum);
        double vWarnEnd = std::min(m_errorThreshold, m_maximum);

        if (m_warningThreshold > m_minimum) {
            drawArcSegment(m_minimum, vNormEnd, m_normalColor);
        }
        if (m_errorThreshold > m_warningThreshold && m_warningThreshold < m_maximum) {
            drawArcSegment(m_warningThreshold, vWarnEnd, m_warningColor);
        }
        if (m_errorThreshold < m_maximum) {
            drawArcSegment(m_errorThreshold, m_maximum, m_errorColor);
        }
    }

    // 3. Ticks and Labels
    const double tickOuter = arcRadius - (arcWidth * 0.6);
    const double majorTickInner = tickOuter - (radius * 0.09);
    const double minorTickInner = tickOuter - (radius * 0.045);
    const double labelRadius = majorTickInner - (radius * 0.12);

    int totalMinorDivisions = m_majorTicks * (m_minorTicks + 1);
    double valueRange = m_maximum - m_minimum;

    // Minor ticks
    if (m_minorTicks > 0) {
        QPen minorPen(m_scaleColor.darker(120), std::max(1.0, radius * 0.012), Qt::SolidLine, Qt::RoundCap);
        painter.setPen(minorPen);

        for (int i = 0; i <= totalMinorDivisions; ++i) {
            if (i % (m_minorTicks + 1) == 0) continue; // Skip major ticks
            double frac = static_cast<double>(i) / totalMinorDivisions;
            double val = m_minimum + frac * valueRange;
            double angle = valueToAngle(val);
            double rad = (angle - 90.0) * M_PI / 180.0;
            double cosR = std::cos(rad);
            double sinR = std::sin(rad);

            QPointF p1(center.x() + minorTickInner * cosR, center.y() + minorTickInner * sinR);
            QPointF p2(center.x() + tickOuter * cosR, center.y() + tickOuter * sinR);
            painter.drawLine(p1, p2);
        }
    }

    // Major ticks and labels font setup
    int labelFontSize = std::max(7, static_cast<int>(radius * 0.082));
    QFont font = painter.font();
    font.setPixelSize(labelFontSize);
    font.setBold(true);
    painter.setFont(font);

    QPen majorPen(m_scaleColor, std::max(1.8, radius * 0.022), Qt::SolidLine, Qt::RoundCap);

    for (int i = 0; i <= m_majorTicks; ++i) {
        double frac = static_cast<double>(i) / m_majorTicks;
        double val = m_minimum + frac * valueRange;
        double angle = valueToAngle(val);
        double rad = (angle - 90.0) * M_PI / 180.0;
        double cosR = std::cos(rad);
        double sinR = std::sin(rad);

        // Major tick mark
        painter.setPen(majorPen);
        QPointF p1(center.x() + majorTickInner * cosR, center.y() + majorTickInner * sinR);
        QPointF p2(center.x() + tickOuter * cosR, center.y() + tickOuter * sinR);
        painter.drawLine(p1, p2);

        // Label text
        QString labelStr = (m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                              : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);

        QFontMetricsF fm(font);
        QRectF textRect = fm.boundingRect(labelStr);
        QPointF labelCenter(center.x() + labelRadius * cosR, center.y() + labelRadius * sinR);
        QRectF drawRect(labelCenter.x() - textRect.width() * 0.5,
                       labelCenter.y() - textRect.height() * 0.5,
                       textRect.width(), textRect.height());

        painter.setPen(m_textColor);
        painter.drawText(drawRect, Qt::AlignCenter, labelStr);
    }

    // 4. Digital Display Background & Unit (Recessed pod below pivot)
    if (m_digitalDisplayVisible) {
        double podWidth = radius * 0.68;
        double podHeight = radius * 0.28;
        QRectF podRect(center.x() - podWidth * 0.5,
                       center.y() + radius * 0.28,
                       podWidth, podHeight);

        // Subtle recessed LCD box
        QLinearGradient podGrad(podRect.topLeft(), podRect.bottomLeft());
        podGrad.setColorAt(0.0, QColor(10, 12, 16, 220));
        podGrad.setColorAt(1.0, QColor(22, 26, 34, 220));

        painter.setPen(QPen(m_bezelColor.darker(120), 1.2));
        painter.setBrush(podGrad);
        painter.drawRoundedRect(podRect, 4.0, 4.0);

        // Static unit text on the dial
        if (!m_unit.isEmpty()) {
            int unitFontSize = std::max(7, static_cast<int>(radius * 0.07));
            QFont unitFont = font;
            unitFont.setPixelSize(unitFontSize);
            unitFont.setBold(false);
            painter.setFont(unitFont);

            QRectF unitRect(podRect.left(), podRect.top() - radius * 0.14, podWidth, radius * 0.14);
            painter.setPen(m_scaleColor.darker(110));
            painter.drawText(unitRect, Qt::AlignCenter, m_unit);
        }
    }

    m_cacheDirty = false;
}

void RadialGauge::paintEvent(QPaintEvent *)
{
    if (m_cacheDirty || m_cachePixmap.size() != (size() * devicePixelRatioF())) {
        renderStaticScale(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast blit of cached static dial face
    painter.drawPixmap(0, 0, m_cachePixmap);

    const double w = width();
    const double h = height();
    const double side = std::min(w, h);
    const QPointF center(w * 0.5, h * 0.5);
    const double radius = (side * 0.5) - 4.0;

    if (radius <= 5.0) return;

    // 2. Dynamic Digital Readout
    if (m_digitalDisplayVisible) {
        double podWidth = radius * 0.68;
        double podHeight = radius * 0.28;
        QRectF podRect(center.x() - podWidth * 0.5,
                       center.y() + radius * 0.28,
                       podWidth, podHeight);

        QString valStr = QString::number(m_value, 'f', m_precision);
        int valFontSize = std::max(8, static_cast<int>(radius * 0.15));
        QFont valFont = font();
        valFont.setPixelSize(valFontSize);
        valFont.setBold(true);
        painter.setFont(valFont);

        QColor displayColor = m_textColor;
        if (m_value >= m_errorThreshold) {
            displayColor = m_errorColor;
        } else if (m_value >= m_warningThreshold) {
            displayColor = m_warningColor;
        }

        painter.setPen(displayColor);
        painter.drawText(podRect, Qt::AlignCenter, valStr);
    }

    // 3. Dynamic Vector Needle
    double needleAngle = valueToAngle(m_value);
    const double needleLen = radius * 0.76;
    const double needleTail = radius * 0.18;
    const double needleBaseW = std::max(2.5, radius * 0.042);

    painter.save();
    painter.translate(center);
    painter.rotate(needleAngle);

    // Left half (light metallic facet)
    QPolygonF leftPolygon;
    leftPolygon << QPointF(0.0, -needleLen)
                << QPointF(-needleBaseW, 0.0)
                << QPointF(-needleBaseW * 0.5, needleTail)
                << QPointF(0.0, needleTail)
                << QPointF(0.0, -needleLen);

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_needleColor.lighter(125));
    painter.drawPolygon(leftPolygon);

    // Right half (shaded metallic facet)
    QPolygonF rightPolygon;
    rightPolygon << QPointF(0.0, -needleLen)
                 << QPointF(0.0, needleTail)
                 << QPointF(needleBaseW * 0.5, needleTail)
                 << QPointF(needleBaseW, 0.0)
                 << QPointF(0.0, -needleLen);

    painter.setBrush(m_needleColor.darker(120));
    painter.drawPolygon(rightPolygon);

    // Sharp central ridge line for 3D needle look
    painter.setPen(QPen(QColor(255, 255, 255, 160), 1.0));
    painter.drawLine(QPointF(0.0, -needleLen + 4.0), QPointF(0.0, needleTail - 2.0));

    painter.restore();

    // 4. Center Pivot Cap (multi-stage chrome / industrial hub)
    const double pivotRadius = std::max(4.0, radius * 0.11);

    // Outer rim
    QRadialGradient pivotGrad(center, pivotRadius);
    pivotGrad.setColorAt(0.0, m_bezelColor.lighter(150));
    pivotGrad.setColorAt(0.7, m_bezelColor.darker(110));
    pivotGrad.setColorAt(1.0, m_bezelColor.darker(180));

    painter.setPen(QPen(m_dialColor.darker(180), 1.2));
    painter.setBrush(pivotGrad);
    painter.drawEllipse(center, pivotRadius, pivotRadius);

    // Inner cap
    const double innerPivot = pivotRadius * 0.65;
    QRadialGradient capGrad(center, innerPivot);
    capGrad.setColorAt(0.0, QColor(220, 225, 230));
    capGrad.setColorAt(0.5, m_dialColor.lighter(130));
    capGrad.setColorAt(1.0, m_dialColor);

    painter.setPen(Qt::NoPen);
    painter.setBrush(capGrad);
    painter.drawEllipse(center, innerPivot, innerPivot);

    // Subtle specular reflection dot on cap
    painter.setBrush(QColor(255, 255, 255, 180));
    painter.drawEllipse(QPointF(center.x() - innerPivot * 0.3, center.y() - innerPivot * 0.3),
                       innerPivot * 0.25, innerPivot * 0.25);
}

} // namespace QtIndustrialWidgets
