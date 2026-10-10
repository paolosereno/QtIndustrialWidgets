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
#include <cmath>

namespace QtIndustrialWidgets {

class RadialGaugePrivate {
public:
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{0.0};
    int m_precision{1};
    QString m_unit{QStringLiteral("bar")};

    // Angles: 0° is 12 o'clock, clockwise. Default: -135° to +135° (270° span)
    double m_startAngle{-135.0};
    double m_spanAngle{270.0};
    int m_majorTicks{10};
    int m_minorTicks{4};

    double m_warningThreshold{70.0};
    double m_errorThreshold{85.0};
    bool m_thresholdBandsVisible{true};
    bool m_digitalDisplayVisible{true};

    // Colors
    QColor m_needleColor{QColor(235, 59, 90)};       // Industrial Crimson
    QColor m_normalColor{QColor(38, 222, 129)};       // Neon Emerald Green
    QColor m_warningColor{QColor(254, 211, 48)};      // Amber Gold
    QColor m_errorColor{QColor(235, 59, 90)};         // Danger Red
    QColor m_dialColor{QColor(24, 28, 36)};           // Slate Black
    QColor m_scaleColor{QColor(210, 218, 226)};       // Light Silver
    QColor m_textColor{QColor(245, 246, 250)};        // Crisp White
    QColor m_bezelColor{QColor(53, 59, 72)};          // Gunmetal Gray

    // High performance static scale cache
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};

    // Threshold state tracking
    bool m_wasWarning{false};
    bool m_wasError{false};
};



RadialGauge::RadialGauge(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<RadialGaugePrivate>())
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

RadialGauge::~RadialGauge() = default;

double RadialGauge::minimum() const { Q_D(const RadialGauge); return d->m_minimum; }
double RadialGauge::maximum() const { Q_D(const RadialGauge); return d->m_maximum; }
double RadialGauge::value() const { Q_D(const RadialGauge); return d->m_value; }
int RadialGauge::precision() const { Q_D(const RadialGauge); return d->m_precision; }
QString RadialGauge::unit() const { Q_D(const RadialGauge); return d->m_unit; }
double RadialGauge::startAngle() const { Q_D(const RadialGauge); return d->m_startAngle; }
double RadialGauge::spanAngle() const { Q_D(const RadialGauge); return d->m_spanAngle; }
int RadialGauge::majorTicks() const { Q_D(const RadialGauge); return d->m_majorTicks; }
int RadialGauge::minorTicks() const { Q_D(const RadialGauge); return d->m_minorTicks; }
double RadialGauge::warningThreshold() const { Q_D(const RadialGauge); return d->m_warningThreshold; }
double RadialGauge::errorThreshold() const { Q_D(const RadialGauge); return d->m_errorThreshold; }
bool RadialGauge::thresholdBandsVisible() const { Q_D(const RadialGauge); return d->m_thresholdBandsVisible; }
bool RadialGauge::digitalDisplayVisible() const { Q_D(const RadialGauge); return d->m_digitalDisplayVisible; }

QColor RadialGauge::needleColor() const { Q_D(const RadialGauge); return d->m_needleColor; }
QColor RadialGauge::normalColor() const { Q_D(const RadialGauge); return d->m_normalColor; }
QColor RadialGauge::warningColor() const { Q_D(const RadialGauge); return d->m_warningColor; }
QColor RadialGauge::errorColor() const { Q_D(const RadialGauge); return d->m_errorColor; }
QColor RadialGauge::dialColor() const { Q_D(const RadialGauge); return d->m_dialColor; }
QColor RadialGauge::scaleColor() const { Q_D(const RadialGauge); return d->m_scaleColor; }
QColor RadialGauge::textColor() const { Q_D(const RadialGauge); return d->m_textColor; }
QColor RadialGauge::bezelColor() const { Q_D(const RadialGauge); return d->m_bezelColor; }



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
    if (std::isnan(val)) {
        return;
    }
    if (std::isinf(val)) {
        val = (val > 0.0) ? d_ptr->m_maximum : d_ptr->m_minimum;
    }

    double clamped = std::clamp(val, d_ptr->m_minimum, d_ptr->m_maximum);
    if (qFuzzyCompare(clamped, d_ptr->m_value)) {
        return;
    }

    d_ptr->m_value = clamped;
    Q_EMIT valueChanged(d_ptr->m_value);

    bool isWarning = (d_ptr->m_value >= d_ptr->m_warningThreshold && d_ptr->m_value < d_ptr->m_errorThreshold);
    bool isError = (d_ptr->m_value >= d_ptr->m_errorThreshold);

    if (isWarning != d_ptr->m_wasWarning) {
        d_ptr->m_wasWarning = isWarning;
        Q_EMIT warningExceeded(isWarning);
    }
    if (isError != d_ptr->m_wasError) {
        d_ptr->m_wasError = isError;
        Q_EMIT errorExceeded(isError);
    }

    update();
}

void RadialGauge::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void RadialGauge::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void RadialGauge::setRange(double min, double max)
{
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max) {
        return;
    }
    if (qFuzzyCompare(min, d_ptr->m_minimum) && qFuzzyCompare(max, d_ptr->m_maximum)) {
        return;
    }

    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;
    d_ptr->m_value = std::clamp(d_ptr->m_value, d_ptr->m_minimum, d_ptr->m_maximum);

    invalidateCache();
    Q_EMIT rangeChanged(d_ptr->m_minimum, d_ptr->m_maximum);
    Q_EMIT valueChanged(d_ptr->m_value);
    update();
}

void RadialGauge::setPrecision(int precision)
{
    if (d_ptr->m_precision == precision) return;
    d_ptr->m_precision = std::max(0, precision);
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setUnit(const QString &unit)
{
    if (d_ptr->m_unit == unit) return;
    d_ptr->m_unit = unit;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setStartAngle(double angle)
{
    if (!std::isfinite(angle) || qFuzzyCompare(d_ptr->m_startAngle, angle)) return;
    d_ptr->m_startAngle = angle;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setSpanAngle(double span)
{
    if (!std::isfinite(span) || span <= 0.0 || qFuzzyCompare(d_ptr->m_spanAngle, span)) return;
    d_ptr->m_spanAngle = span;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setMajorTicks(int count)
{
    if (d_ptr->m_majorTicks == count || count < 1) return;
    d_ptr->m_majorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setMinorTicks(int count)
{
    if (d_ptr->m_minorTicks == count || count < 0) return;
    d_ptr->m_minorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setWarningThreshold(double threshold)
{
    if (!std::isfinite(threshold) || qFuzzyCompare(d_ptr->m_warningThreshold, threshold)) return;
    d_ptr->m_warningThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setErrorThreshold(double threshold)
{
    if (!std::isfinite(threshold) || qFuzzyCompare(d_ptr->m_errorThreshold, threshold)) return;
    d_ptr->m_errorThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setThresholdBandsVisible(bool visible)
{
    if (d_ptr->m_thresholdBandsVisible == visible) return;
    d_ptr->m_thresholdBandsVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setDigitalDisplayVisible(bool visible)
{
    if (d_ptr->m_digitalDisplayVisible == visible) return;
    d_ptr->m_digitalDisplayVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setNeedleColor(const QColor &color)
{
    if (d_ptr->m_needleColor == color) return;
    d_ptr->m_needleColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setNormalColor(const QColor &color)
{
    if (d_ptr->m_normalColor == color) return;
    d_ptr->m_normalColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setWarningColor(const QColor &color)
{
    if (d_ptr->m_warningColor == color) return;
    d_ptr->m_warningColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setErrorColor(const QColor &color)
{
    if (d_ptr->m_errorColor == color) return;
    d_ptr->m_errorColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setDialColor(const QColor &color)
{
    if (d_ptr->m_dialColor == color) return;
    d_ptr->m_dialColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setScaleColor(const QColor &color)
{
    if (d_ptr->m_scaleColor == color) return;
    d_ptr->m_scaleColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor == color) return;
    d_ptr->m_textColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::setBezelColor(const QColor &color)
{
    if (d_ptr->m_bezelColor == color) return;
    d_ptr->m_bezelColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void RadialGauge::invalidateCache()
{
    d_ptr->m_cacheDirty = true;
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
    if (d_ptr->m_maximum <= d_ptr->m_minimum) {
        return d_ptr->m_startAngle;
    }
    double factor = (val - d_ptr->m_minimum) / (d_ptr->m_maximum - d_ptr->m_minimum);
    factor = std::clamp(factor, 0.0, 1.0);
    return d_ptr->m_startAngle + factor * d_ptr->m_spanAngle;
}

void RadialGauge::renderStaticScale(const QSize &targetSize)
{
    qreal dpr = devicePixelRatioF();
    QSize pixmapSize = (targetSize * dpr);
    if (pixmapSize.isEmpty()) {
        return;
    }

    d_ptr->m_cachePixmap = QPixmap(pixmapSize);
    d_ptr->m_cachePixmap.setDevicePixelRatio(dpr);
    d_ptr->m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&d_ptr->m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = targetSize.width();
    const double h = targetSize.height();
    const double side = std::min(w, h);
    const QPointF center(w * 0.5, h * 0.5);
    const double radius = (side * 0.5) - 4.0;

    if (radius <= 5.0) {
        d_ptr->m_cacheDirty = false;
        return;
    }

    // 1. Outer Bezel Ring (Machined metal / industrial casing effect)
    QRadialGradient bezelGrad(center, radius);
    bezelGrad.setColorAt(0.0, d_ptr->m_dialColor.lighter(110));
    bezelGrad.setColorAt(0.85, d_ptr->m_dialColor);
    bezelGrad.setColorAt(0.96, d_ptr->m_bezelColor.lighter(130));
    bezelGrad.setColorAt(1.0, d_ptr->m_bezelColor.darker(140));

    painter.setPen(QPen(d_ptr->m_bezelColor.darker(160), 1.5));
    painter.setBrush(bezelGrad);
    painter.drawEllipse(center, radius, radius);

    // Subtle inner shadow ring
    const double innerRadius = radius * 0.95;
    QRadialGradient dialGrad(center, innerRadius);
    dialGrad.setColorAt(0.0, d_ptr->m_dialColor.lighter(105));
    dialGrad.setColorAt(0.7, d_ptr->m_dialColor);
    dialGrad.setColorAt(1.0, d_ptr->m_dialColor.darker(125));

    painter.setPen(QPen(d_ptr->m_dialColor.darker(150), 1.0));
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
    if (d_ptr->m_thresholdBandsVisible) {
        double vNormEnd = std::min(d_ptr->m_warningThreshold, d_ptr->m_maximum);
        double vWarnEnd = std::min(d_ptr->m_errorThreshold, d_ptr->m_maximum);

        if (d_ptr->m_warningThreshold > d_ptr->m_minimum) {
            drawArcSegment(d_ptr->m_minimum, vNormEnd, d_ptr->m_normalColor);
        }
        if (d_ptr->m_errorThreshold > d_ptr->m_warningThreshold && d_ptr->m_warningThreshold < d_ptr->m_maximum) {
            drawArcSegment(d_ptr->m_warningThreshold, vWarnEnd, d_ptr->m_warningColor);
        }
        if (d_ptr->m_errorThreshold < d_ptr->m_maximum) {
            drawArcSegment(d_ptr->m_errorThreshold, d_ptr->m_maximum, d_ptr->m_errorColor);
        }
    }

    // 3. Ticks and Labels
    const double tickOuter = arcRadius - (arcWidth * 0.6);
    const double majorTickInner = tickOuter - (radius * 0.09);
    const double minorTickInner = tickOuter - (radius * 0.045);
    const double labelRadius = majorTickInner - (radius * 0.12);

    int totalMinorDivisions = d_ptr->m_majorTicks * (d_ptr->m_minorTicks + 1);
    double valueRange = d_ptr->m_maximum - d_ptr->m_minimum;

    // Minor ticks
    if (d_ptr->m_minorTicks > 0) {
        QPen minorPen(d_ptr->m_scaleColor.darker(120), std::max(1.0, radius * 0.012), Qt::SolidLine, Qt::RoundCap);
        painter.setPen(minorPen);

        for (int i = 0; i <= totalMinorDivisions; ++i) {
            if (i % (d_ptr->m_minorTicks + 1) == 0) continue; // Skip major ticks
            double frac = static_cast<double>(i) / totalMinorDivisions;
            double val = d_ptr->m_minimum + frac * valueRange;
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

    QPen majorPen(d_ptr->m_scaleColor, std::max(1.8, radius * 0.022), Qt::SolidLine, Qt::RoundCap);

    for (int i = 0; i <= d_ptr->m_majorTicks; ++i) {
        double frac = static_cast<double>(i) / d_ptr->m_majorTicks;
        double val = d_ptr->m_minimum + frac * valueRange;
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
        QString labelStr = (d_ptr->m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                              : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);

        QFontMetricsF fm(font);
        QRectF textRect = fm.boundingRect(labelStr);
        QPointF labelCenter(center.x() + labelRadius * cosR, center.y() + labelRadius * sinR);
        QRectF drawRect(labelCenter.x() - textRect.width() * 0.5,
                       labelCenter.y() - textRect.height() * 0.5,
                       textRect.width(), textRect.height());

        painter.setPen(d_ptr->m_textColor);
        painter.drawText(drawRect, Qt::AlignCenter, labelStr);
    }

    // 4. Digital Display Background & Unit (Recessed pod below pivot)
    if (d_ptr->m_digitalDisplayVisible) {
        double podWidth = radius * 0.68;
        double podHeight = radius * 0.28;
        QRectF podRect(center.x() - podWidth * 0.5,
                       center.y() + radius * 0.28,
                       podWidth, podHeight);

        // Subtle recessed LCD box
        QLinearGradient podGrad(podRect.topLeft(), podRect.bottomLeft());
        podGrad.setColorAt(0.0, QColor(10, 12, 16, 220));
        podGrad.setColorAt(1.0, QColor(22, 26, 34, 220));

        painter.setPen(QPen(d_ptr->m_bezelColor.darker(120), 1.2));
        painter.setBrush(podGrad);
        painter.drawRoundedRect(podRect, 4.0, 4.0);

        // Static unit text on the dial
        if (!d_ptr->m_unit.isEmpty()) {
            int unitFontSize = std::max(7, static_cast<int>(radius * 0.07));
            QFont unitFont = font;
            unitFont.setPixelSize(unitFontSize);
            unitFont.setBold(false);
            painter.setFont(unitFont);

            QRectF unitRect(podRect.left(), podRect.top() - radius * 0.14, podWidth, radius * 0.14);
            painter.setPen(d_ptr->m_scaleColor.darker(110));
            painter.drawText(unitRect, Qt::AlignCenter, d_ptr->m_unit);
        }
    }

    d_ptr->m_cacheDirty = false;
}

void RadialGauge::paintEvent(QPaintEvent *)
{
    if (d_ptr->m_cacheDirty || d_ptr->m_cachePixmap.size() != (size() * devicePixelRatioF())) {
        renderStaticScale(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast blit of cached static dial face
    painter.drawPixmap(0, 0, d_ptr->m_cachePixmap);

    const double w = width();
    const double h = height();
    const double side = std::min(w, h);
    const QPointF center(w * 0.5, h * 0.5);
    const double radius = (side * 0.5) - 4.0;

    if (radius <= 5.0) return;

    // 2. Dynamic Digital Readout
    if (d_ptr->m_digitalDisplayVisible) {
        double podWidth = radius * 0.68;
        double podHeight = radius * 0.28;
        QRectF podRect(center.x() - podWidth * 0.5,
                       center.y() + radius * 0.28,
                       podWidth, podHeight);

        QString valStr = QString::number(d_ptr->m_value, 'f', d_ptr->m_precision);
        int valFontSize = std::max(8, static_cast<int>(radius * 0.15));
        QFont valFont = font();
        valFont.setPixelSize(valFontSize);
        valFont.setBold(true);
        painter.setFont(valFont);

        QColor displayColor = d_ptr->m_textColor;
        if (d_ptr->m_value >= d_ptr->m_errorThreshold) {
            displayColor = d_ptr->m_errorColor;
        } else if (d_ptr->m_value >= d_ptr->m_warningThreshold) {
            displayColor = d_ptr->m_warningColor;
        }

        painter.setPen(displayColor);
        painter.drawText(podRect, Qt::AlignCenter, valStr);
    }

    // 3. Dynamic Vector Needle
    double needleAngle = valueToAngle(d_ptr->m_value);
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
    painter.setBrush(d_ptr->m_needleColor.lighter(125));
    painter.drawPolygon(leftPolygon);

    // Right half (shaded metallic facet)
    QPolygonF rightPolygon;
    rightPolygon << QPointF(0.0, -needleLen)
                 << QPointF(0.0, needleTail)
                 << QPointF(needleBaseW * 0.5, needleTail)
                 << QPointF(needleBaseW, 0.0)
                 << QPointF(0.0, -needleLen);

    painter.setBrush(d_ptr->m_needleColor.darker(120));
    painter.drawPolygon(rightPolygon);

    // Sharp central ridge line for 3D needle look
    painter.setPen(QPen(QColor(255, 255, 255, 160), 1.0));
    painter.drawLine(QPointF(0.0, -needleLen + 4.0), QPointF(0.0, needleTail - 2.0));

    painter.restore();

    // 4. Center Pivot Cap (multi-stage chrome / industrial hub)
    const double pivotRadius = std::max(4.0, radius * 0.11);

    // Outer rim
    QRadialGradient pivotGrad(center, pivotRadius);
    pivotGrad.setColorAt(0.0, d_ptr->m_bezelColor.lighter(150));
    pivotGrad.setColorAt(0.7, d_ptr->m_bezelColor.darker(110));
    pivotGrad.setColorAt(1.0, d_ptr->m_bezelColor.darker(180));

    painter.setPen(QPen(d_ptr->m_dialColor.darker(180), 1.2));
    painter.setBrush(pivotGrad);
    painter.drawEllipse(center, pivotRadius, pivotRadius);

    // Inner cap
    const double innerPivot = pivotRadius * 0.65;
    QRadialGradient capGrad(center, innerPivot);
    capGrad.setColorAt(0.0, QColor(220, 225, 230));
    capGrad.setColorAt(0.5, d_ptr->m_dialColor.lighter(130));
    capGrad.setColorAt(1.0, d_ptr->m_dialColor);

    painter.setPen(Qt::NoPen);
    painter.setBrush(capGrad);
    painter.drawEllipse(center, innerPivot, innerPivot);

    // Subtle specular reflection dot on cap
    painter.setBrush(QColor(255, 255, 255, 180));
    painter.drawEllipse(QPointF(center.x() - innerPivot * 0.3, center.y() - innerPivot * 0.3),
                       innerPivot * 0.25, innerPivot * 0.25);
}

} // namespace QtIndustrialWidgets
