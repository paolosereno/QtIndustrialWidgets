// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/LinearGauge.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QLinearGradient>
#include <QtCore/QtMath>
#include <algorithm>

namespace QtIndustrialWidgets {

class LinearGaugePrivate {
public:
    Qt::Orientation m_orientation{Qt::Vertical};
    bool m_thermometerMode{true};
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{0.0};
    int m_precision{1};
    QString m_unit{QStringLiteral("°C")};
    int m_majorTicks{10};
    int m_minorTicks{4};
    double m_warningThreshold{70.0};
    double m_errorThreshold{90.0};
    bool m_dynamicLiquidColor{true};
    bool m_gradientLiquid{false};
    bool m_digitalDisplayVisible{true};
    bool m_scaleVisible{true};
    QColor m_liquidColor{QColor(235, 59, 90)};
    QColor m_normalColor{QColor(46, 204, 113)};
    QColor m_warningColor{QColor(241, 196, 15)};
    QColor m_errorColor{QColor(231, 76, 60)};
    QColor m_troughColor{QColor(30, 36, 45)};
    QColor m_scaleColor{QColor(200, 208, 218)};
    QColor m_textColor{QColor(240, 244, 248)};
    QColor m_bezelColor{QColor(44, 53, 64)};
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};
    QRectF m_tubeRect;
    QPointF m_bulbCenter;
    double m_bulbRadius{0.0};
    bool m_wasWarning{false};
    bool m_wasError{false};
};



LinearGauge::LinearGauge(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<LinearGaugePrivate>())
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

LinearGauge::~LinearGauge() = default;

Qt::Orientation LinearGauge::orientation() const { Q_D(const LinearGauge); return d->m_orientation; }
bool LinearGauge::isThermometerMode() const { Q_D(const LinearGauge); return d->m_thermometerMode; }
double LinearGauge::minimum() const { Q_D(const LinearGauge); return d->m_minimum; }
double LinearGauge::maximum() const { Q_D(const LinearGauge); return d->m_maximum; }
double LinearGauge::value() const { Q_D(const LinearGauge); return d->m_value; }
int LinearGauge::precision() const { Q_D(const LinearGauge); return d->m_precision; }
QString LinearGauge::unit() const { Q_D(const LinearGauge); return d->m_unit; }
int LinearGauge::majorTicks() const { Q_D(const LinearGauge); return d->m_majorTicks; }
int LinearGauge::minorTicks() const { Q_D(const LinearGauge); return d->m_minorTicks; }
double LinearGauge::warningThreshold() const { Q_D(const LinearGauge); return d->m_warningThreshold; }
double LinearGauge::errorThreshold() const { Q_D(const LinearGauge); return d->m_errorThreshold; }
bool LinearGauge::isDynamicLiquidColor() const { Q_D(const LinearGauge); return d->m_dynamicLiquidColor; }
bool LinearGauge::isGradientLiquid() const { Q_D(const LinearGauge); return d->m_gradientLiquid; }
bool LinearGauge::digitalDisplayVisible() const { Q_D(const LinearGauge); return d->m_digitalDisplayVisible; }
bool LinearGauge::scaleVisible() const { Q_D(const LinearGauge); return d->m_scaleVisible; }
QColor LinearGauge::liquidColor() const { Q_D(const LinearGauge); return d->m_liquidColor; }
QColor LinearGauge::normalColor() const { Q_D(const LinearGauge); return d->m_normalColor; }
QColor LinearGauge::warningColor() const { Q_D(const LinearGauge); return d->m_warningColor; }
QColor LinearGauge::errorColor() const { Q_D(const LinearGauge); return d->m_errorColor; }
QColor LinearGauge::troughColor() const { Q_D(const LinearGauge); return d->m_troughColor; }
QColor LinearGauge::scaleColor() const { Q_D(const LinearGauge); return d->m_scaleColor; }
QColor LinearGauge::textColor() const { Q_D(const LinearGauge); return d->m_textColor; }
QColor LinearGauge::bezelColor() const { Q_D(const LinearGauge); return d->m_bezelColor; }



QSize LinearGauge::sizeHint() const
{
    return (d_ptr->m_orientation == Qt::Vertical) ? QSize(100, 280) : QSize(280, 100);
}

QSize LinearGauge::minimumSizeHint() const
{
    return (d_ptr->m_orientation == Qt::Vertical) ? QSize(50, 120) : QSize(120, 50);
}

void LinearGauge::setOrientation(Qt::Orientation orientation)
{
    if (d_ptr->m_orientation == orientation) return;
    d_ptr->m_orientation = orientation;
    invalidateCache();
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setThermometerMode(bool thermometer)
{
    if (d_ptr->m_thermometerMode == thermometer) return;
    d_ptr->m_thermometerMode = thermometer;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setValue(double val)
{
    double clamped = std::clamp(val, d_ptr->m_minimum, d_ptr->m_maximum);
    if (qFuzzyCompare(clamped, d_ptr->m_value)) {
        return;
    }

    d_ptr->m_value = clamped;
    Q_EMIT valueChanged(d_ptr->m_value);
    update();
}

void LinearGauge::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void LinearGauge::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void LinearGauge::setRange(double min, double max)
{
    if (min >= max) return;
    if (qFuzzyCompare(min, d_ptr->m_minimum) && qFuzzyCompare(max, d_ptr->m_maximum)) return;

    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;
    d_ptr->m_value = std::clamp(d_ptr->m_value, d_ptr->m_minimum, d_ptr->m_maximum);

    invalidateCache();
    Q_EMIT rangeChanged(d_ptr->m_minimum, d_ptr->m_maximum);
    Q_EMIT valueChanged(d_ptr->m_value);
    update();
}

void LinearGauge::setPrecision(int precision)
{
    if (d_ptr->m_precision == precision) return;
    d_ptr->m_precision = std::max(0, precision);
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setUnit(const QString &unit)
{
    if (d_ptr->m_unit == unit) return;
    d_ptr->m_unit = unit;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setMajorTicks(int count)
{
    if (d_ptr->m_majorTicks == count || count < 1) return;
    d_ptr->m_majorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setMinorTicks(int count)
{
    if (d_ptr->m_minorTicks == count || count < 0) return;
    d_ptr->m_minorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setWarningThreshold(double threshold)
{
    if (qFuzzyCompare(d_ptr->m_warningThreshold, threshold)) return;
    d_ptr->m_warningThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setErrorThreshold(double threshold)
{
    if (qFuzzyCompare(d_ptr->m_errorThreshold, threshold)) return;
    d_ptr->m_errorThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setDynamicLiquidColor(bool dynamic)
{
    if (d_ptr->m_dynamicLiquidColor == dynamic) return;
    d_ptr->m_dynamicLiquidColor = dynamic;
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setGradientLiquid(bool gradient)
{
    if (d_ptr->m_gradientLiquid == gradient) return;
    d_ptr->m_gradientLiquid = gradient;
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setDigitalDisplayVisible(bool visible)
{
    if (d_ptr->m_digitalDisplayVisible == visible) return;
    d_ptr->m_digitalDisplayVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setScaleVisible(bool visible)
{
    if (d_ptr->m_scaleVisible == visible) return;
    d_ptr->m_scaleVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setLiquidColor(const QColor &color)
{
    if (d_ptr->m_liquidColor == color) return;
    d_ptr->m_liquidColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setNormalColor(const QColor &color)
{
    if (d_ptr->m_normalColor == color) return;
    d_ptr->m_normalColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setWarningColor(const QColor &color)
{
    if (d_ptr->m_warningColor == color) return;
    d_ptr->m_warningColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setErrorColor(const QColor &color)
{
    if (d_ptr->m_errorColor == color) return;
    d_ptr->m_errorColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setTroughColor(const QColor &color)
{
    if (d_ptr->m_troughColor == color) return;
    d_ptr->m_troughColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setScaleColor(const QColor &color)
{
    if (d_ptr->m_scaleColor == color) return;
    d_ptr->m_scaleColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor == color) return;
    d_ptr->m_textColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setBezelColor(const QColor &color)
{
    if (d_ptr->m_bezelColor == color) return;
    d_ptr->m_bezelColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::invalidateCache()
{
    d_ptr->m_cacheDirty = true;
}

void LinearGauge::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void LinearGauge::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        invalidateCache();
        update();
    }
    QWidget::changeEvent(event);
}

QColor LinearGauge::determineActiveLiquidColor() const
{
    if (!d_ptr->m_dynamicLiquidColor) {
        return d_ptr->m_liquidColor;
    }
    if (d_ptr->m_value >= d_ptr->m_errorThreshold) {
        return d_ptr->m_errorColor;
    }
    if (d_ptr->m_value >= d_ptr->m_warningThreshold) {
        return d_ptr->m_warningColor;
    }
    return d_ptr->m_normalColor;
}

void LinearGauge::renderStaticScale(const QSize &targetSize)
{
    qreal dpr = devicePixelRatioF();
    QSize pixmapSize = targetSize * dpr;
    if (pixmapSize.isEmpty()) return;

    d_ptr->m_cachePixmap = QPixmap(pixmapSize);
    d_ptr->m_cachePixmap.setDevicePixelRatio(dpr);
    d_ptr->m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&d_ptr->m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = targetSize.width();
    const double h = targetSize.height();

    // Draw background casing
    QRectF casingRect(1.0, 1.0, w - 2.0, h - 2.0);
    QLinearGradient casingGrad(casingRect.topLeft(), casingRect.bottomRight());
    casingGrad.setColorAt(0.0, d_ptr->m_bezelColor.lighter(110));
    casingGrad.setColorAt(1.0, d_ptr->m_bezelColor.darker(130));
    painter.setPen(QPen(d_ptr->m_bezelColor.darker(160), 1.5));
    painter.setBrush(casingGrad);
    painter.drawRoundedRect(casingRect, 6.0, 6.0);

    // Compute layout based on orientation and thermometer mode
    if (d_ptr->m_orientation == Qt::Vertical) {
        // Top area: optional digital display
        double topPadding = d_ptr->m_digitalDisplayVisible ? 36.0 : 16.0;
        double bottomPadding = 16.0;
        double tubeWidth = std::clamp(w * 0.16, 10.0, 26.0);

        if (d_ptr->m_thermometerMode) {
            d_ptr->m_bulbRadius = tubeWidth * 1.35;
            bottomPadding = d_ptr->m_bulbRadius * 2.0 + 12.0;
        } else {
            d_ptr->m_bulbRadius = 0.0;
        }

        double tubeX = d_ptr->m_scaleVisible ? (w * 0.32 - tubeWidth * 0.5) : (w * 0.5 - tubeWidth * 0.5);
        double tubeTop = topPadding;
        double tubeBottom = h - bottomPadding;
        double tubeHeight = std::max(10.0, tubeBottom - tubeTop);

        d_ptr->m_tubeRect = QRectF(tubeX, tubeTop, tubeWidth, tubeHeight);
        d_ptr->m_bulbCenter = QPointF(tubeX + tubeWidth * 0.5, h - d_ptr->m_bulbRadius - 8.0);

        // Draw empty glass tube trough
        QLinearGradient troughGrad(d_ptr->m_tubeRect.topLeft(), d_ptr->m_tubeRect.topRight());
        troughGrad.setColorAt(0.0, d_ptr->m_troughColor.darker(150));
        troughGrad.setColorAt(0.3, d_ptr->m_troughColor.darker(110));
        troughGrad.setColorAt(0.7, d_ptr->m_troughColor);
        troughGrad.setColorAt(1.0, d_ptr->m_troughColor.darker(140));

        painter.setPen(QPen(d_ptr->m_bezelColor.darker(180), 1.2));
        painter.setBrush(troughGrad);

        if (d_ptr->m_thermometerMode) {
            // Merge tube and bulb into single path
            QPainterPath troughPath;
            troughPath.addRoundedRect(d_ptr->m_tubeRect, tubeWidth * 0.5, tubeWidth * 0.5);
            troughPath.addEllipse(d_ptr->m_bulbCenter, d_ptr->m_bulbRadius, d_ptr->m_bulbRadius);
            painter.drawPath(troughPath.simplified());
        } else {
            painter.drawRoundedRect(d_ptr->m_tubeRect, 4.0, 4.0);
        }

        // Draw Scale (Ticks and numeric labels)
        if (d_ptr->m_scaleVisible) {
            const double tickStartX = d_ptr->m_tubeRect.right() + 6.0;
            const double majorTickLen = std::max(6.0, w * 0.12);
            const double minorTickLen = majorTickLen * 0.55;
            const double labelX = tickStartX + majorTickLen + 5.0;

            int totalMinorDivisions = d_ptr->m_majorTicks * (d_ptr->m_minorTicks + 1);
            double valRange = d_ptr->m_maximum - d_ptr->m_minimum;

            // Minor ticks
            if (d_ptr->m_minorTicks > 0) {
                painter.setPen(QPen(d_ptr->m_scaleColor.darker(130), 1.0));
                for (int i = 0; i <= totalMinorDivisions; ++i) {
                    if (i % (d_ptr->m_minorTicks + 1) == 0) continue;
                    double frac = static_cast<double>(i) / totalMinorDivisions;
                    double y = d_ptr->m_tubeRect.bottom() - frac * d_ptr->m_tubeRect.height();
                    painter.drawLine(QPointF(tickStartX, y), QPointF(tickStartX + minorTickLen, y));
                }
            }

            // Major ticks and labels
            int fontSize = std::clamp(static_cast<int>(h * 0.04), 8, 12);
            QFont f = painter.font();
            f.setPixelSize(fontSize);
            f.setBold(true);
            painter.setFont(f);

            painter.setPen(QPen(d_ptr->m_scaleColor, 1.6));

            for (int i = 0; i <= d_ptr->m_majorTicks; ++i) {
                double frac = static_cast<double>(i) / d_ptr->m_majorTicks;
                double val = d_ptr->m_minimum + frac * valRange;
                double y = d_ptr->m_tubeRect.bottom() - frac * d_ptr->m_tubeRect.height();

                painter.setPen(QPen(d_ptr->m_scaleColor, 1.6));
                painter.drawLine(QPointF(tickStartX, y), QPointF(tickStartX + majorTickLen, y));

                QString s = (d_ptr->m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                               : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);

                QFontMetricsF fm(f);
                QRectF textR = fm.boundingRect(s);
                QRectF drawR(labelX, y - textR.height() * 0.5, w - labelX - 4.0, textR.height());
                painter.setPen(d_ptr->m_textColor);
                painter.drawText(drawR, Qt::AlignLeft | Qt::AlignVCenter, s);
            }
        }

        // Digital display static pod (top)
        if (d_ptr->m_digitalDisplayVisible) {
            QRectF podRect(6.0, 6.0, w - 12.0, 24.0);
            painter.setPen(QPen(d_ptr->m_bezelColor.darker(140), 1.0));
            painter.setBrush(QColor(16, 20, 26, 220));
            painter.drawRoundedRect(podRect, 3.0, 3.0);
        }
    } else {
        // Horizontal orientation
        double leftPadding = 16.0;
        double rightPadding = d_ptr->m_digitalDisplayVisible ? 55.0 : 16.0;
        double tubeHeight = std::clamp(h * 0.16, 10.0, 26.0);

        if (d_ptr->m_thermometerMode) {
            d_ptr->m_bulbRadius = tubeHeight * 1.35;
            leftPadding = d_ptr->m_bulbRadius * 2.0 + 12.0;
        } else {
            d_ptr->m_bulbRadius = 0.0;
        }

        double tubeY = d_ptr->m_scaleVisible ? (h * 0.40 - tubeHeight * 0.5) : (h * 0.5 - tubeHeight * 0.5);
        double tubeLeft = leftPadding;
        double tubeRight = w - rightPadding;
        double tubeWidth = std::max(10.0, tubeRight - tubeLeft);

        d_ptr->m_tubeRect = QRectF(tubeLeft, tubeY, tubeWidth, tubeHeight);
        d_ptr->m_bulbCenter = QPointF(d_ptr->m_bulbRadius + 8.0, tubeY + tubeHeight * 0.5);

        // Glass trough
        QLinearGradient troughGrad(d_ptr->m_tubeRect.topLeft(), d_ptr->m_tubeRect.bottomLeft());
        troughGrad.setColorAt(0.0, d_ptr->m_troughColor.darker(150));
        troughGrad.setColorAt(0.3, d_ptr->m_troughColor.darker(110));
        troughGrad.setColorAt(0.7, d_ptr->m_troughColor);
        troughGrad.setColorAt(1.0, d_ptr->m_troughColor.darker(140));

        painter.setPen(QPen(d_ptr->m_bezelColor.darker(180), 1.2));
        painter.setBrush(troughGrad);

        if (d_ptr->m_thermometerMode) {
            QPainterPath troughPath;
            troughPath.addRoundedRect(d_ptr->m_tubeRect, tubeHeight * 0.5, tubeHeight * 0.5);
            troughPath.addEllipse(d_ptr->m_bulbCenter, d_ptr->m_bulbRadius, d_ptr->m_bulbRadius);
            painter.drawPath(troughPath.simplified());
        } else {
            painter.drawRoundedRect(d_ptr->m_tubeRect, 4.0, 4.0);
        }

        // Scale
        if (d_ptr->m_scaleVisible) {
            const double tickStartY = d_ptr->m_tubeRect.bottom() + 6.0;
            const double majorTickLen = std::max(6.0, h * 0.12);
            const double minorTickLen = majorTickLen * 0.55;
            const double labelY = tickStartY + majorTickLen + 2.0;

            int totalMinorDivisions = d_ptr->m_majorTicks * (d_ptr->m_minorTicks + 1);
            double valRange = d_ptr->m_maximum - d_ptr->m_minimum;

            if (d_ptr->m_minorTicks > 0) {
                painter.setPen(QPen(d_ptr->m_scaleColor.darker(130), 1.0));
                for (int i = 0; i <= totalMinorDivisions; ++i) {
                    if (i % (d_ptr->m_minorTicks + 1) == 0) continue;
                    double frac = static_cast<double>(i) / totalMinorDivisions;
                    double x = d_ptr->m_tubeRect.left() + frac * d_ptr->m_tubeRect.width();
                    painter.drawLine(QPointF(x, tickStartY), QPointF(x, tickStartY + minorTickLen));
                }
            }

            int fontSize = std::clamp(static_cast<int>(h * 0.12), 8, 12);
            QFont f = painter.font();
            f.setPixelSize(fontSize);
            f.setBold(true);
            painter.setFont(f);

            for (int i = 0; i <= d_ptr->m_majorTicks; ++i) {
                double frac = static_cast<double>(i) / d_ptr->m_majorTicks;
                double val = d_ptr->m_minimum + frac * valRange;
                double x = d_ptr->m_tubeRect.left() + frac * d_ptr->m_tubeRect.width();

                painter.setPen(QPen(d_ptr->m_scaleColor, 1.6));
                painter.drawLine(QPointF(x, tickStartY), QPointF(x, tickStartY + majorTickLen));

                QString s = (d_ptr->m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                               : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);

                QFontMetricsF fm(f);
                QRectF textR = fm.boundingRect(s);
                QRectF drawR(x - textR.width() * 0.5, labelY, textR.width(), textR.height());
                painter.setPen(d_ptr->m_textColor);
                painter.drawText(drawR, Qt::AlignCenter, s);
            }
        }

        // Digital display static pod (right)
        if (d_ptr->m_digitalDisplayVisible) {
            QRectF podRect(w - rightPadding + 4.0, 6.0, rightPadding - 10.0, h - 12.0);
            painter.setPen(QPen(d_ptr->m_bezelColor.darker(140), 1.0));
            painter.setBrush(QColor(16, 20, 26, 220));
            painter.drawRoundedRect(podRect, 3.0, 3.0);
        }
    }

    d_ptr->m_cacheDirty = false;
}

void LinearGauge::paintEvent(QPaintEvent *)
{
    if (d_ptr->m_cacheDirty || d_ptr->m_cachePixmap.size() != (size() * devicePixelRatioF())) {
        renderStaticScale(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast blit of cached background & scale
    painter.drawPixmap(0, 0, d_ptr->m_cachePixmap);

    // 2. Liquid Column & Bulb Dynamic Rendering
    QColor activeColor = determineActiveLiquidColor();
    double factor = (d_ptr->m_maximum > d_ptr->m_minimum) ? (d_ptr->m_value - d_ptr->m_minimum) / (d_ptr->m_maximum - d_ptr->m_minimum) : 0.0;
    factor = std::clamp(factor, 0.0, 1.0);

    if (d_ptr->m_orientation == Qt::Vertical) {
        double fillHeight = d_ptr->m_tubeRect.height() * factor;
        QRectF fillRect(d_ptr->m_tubeRect.left() + 1.0,
                        d_ptr->m_tubeRect.bottom() - fillHeight,
                        d_ptr->m_tubeRect.width() - 2.0,
                        fillHeight);

        // Fill bulb first if in thermometer mode
        if (d_ptr->m_thermometerMode) {
            QRadialGradient bulbGrad(d_ptr->m_bulbCenter, d_ptr->m_bulbRadius);
            bulbGrad.setColorAt(0.0, activeColor.lighter(130));
            bulbGrad.setColorAt(0.7, activeColor);
            bulbGrad.setColorAt(1.0, activeColor.darker(130));

            painter.setPen(Qt::NoPen);
            painter.setBrush(bulbGrad);
            painter.drawEllipse(d_ptr->m_bulbCenter, d_ptr->m_bulbRadius - 1.0, d_ptr->m_bulbRadius - 1.0);
        }

        // Fill liquid column
        if (fillHeight > 0.0) {
            QLinearGradient liquidGrad(fillRect.topLeft(), fillRect.topRight());
            if (d_ptr->m_gradientLiquid) {
                liquidGrad = QLinearGradient(fillRect.bottomLeft(), fillRect.topLeft());
                liquidGrad.setColorAt(0.0, d_ptr->m_normalColor);
                liquidGrad.setColorAt(0.7, d_ptr->m_warningColor);
                liquidGrad.setColorAt(1.0, d_ptr->m_errorColor);
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
        painter.drawLine(QPointF(d_ptr->m_tubeRect.left() + 2.5, d_ptr->m_tubeRect.top() + 4.0),
                         QPointF(d_ptr->m_tubeRect.left() + 2.5, d_ptr->m_tubeRect.bottom() - 2.0));

        // Digital display readout
        if (d_ptr->m_digitalDisplayVisible) {
            QRectF podRect(6.0, 6.0, width() - 12.0, 24.0);
            QString txt = QStringLiteral("%1 %2").arg(QString::number(d_ptr->m_value, 'f', d_ptr->m_precision), d_ptr->m_unit);
            QFont f = font();
            f.setPixelSize(12);
            f.setBold(true);
            painter.setFont(f);
            painter.setPen(activeColor);
            painter.drawText(podRect, Qt::AlignCenter, txt);
        }
    } else {
        // Horizontal orientation
        double fillWidth = d_ptr->m_tubeRect.width() * factor;
        QRectF fillRect(d_ptr->m_tubeRect.left(),
                        d_ptr->m_tubeRect.top() + 1.0,
                        fillWidth,
                        d_ptr->m_tubeRect.height() - 2.0);

        if (d_ptr->m_thermometerMode) {
            QRadialGradient bulbGrad(d_ptr->m_bulbCenter, d_ptr->m_bulbRadius);
            bulbGrad.setColorAt(0.0, activeColor.lighter(130));
            bulbGrad.setColorAt(0.7, activeColor);
            bulbGrad.setColorAt(1.0, activeColor.darker(130));

            painter.setPen(Qt::NoPen);
            painter.setBrush(bulbGrad);
            painter.drawEllipse(d_ptr->m_bulbCenter, d_ptr->m_bulbRadius - 1.0, d_ptr->m_bulbRadius - 1.0);
        }

        if (fillWidth > 0.0) {
            QLinearGradient liquidGrad(fillRect.topLeft(), fillRect.bottomLeft());
            if (d_ptr->m_gradientLiquid) {
                liquidGrad = QLinearGradient(fillRect.topLeft(), fillRect.topRight());
                liquidGrad.setColorAt(0.0, d_ptr->m_normalColor);
                liquidGrad.setColorAt(0.7, d_ptr->m_warningColor);
                liquidGrad.setColorAt(1.0, d_ptr->m_errorColor);
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
        painter.drawLine(QPointF(d_ptr->m_tubeRect.left() + 2.0, d_ptr->m_tubeRect.top() + 2.5),
                         QPointF(d_ptr->m_tubeRect.right() - 2.0, d_ptr->m_tubeRect.top() + 2.5));

        // Digital display readout
        if (d_ptr->m_digitalDisplayVisible) {
            double rightPadding = 55.0;
            QRectF podRect(width() - rightPadding + 4.0, 6.0, rightPadding - 10.0, height() - 12.0);
            QString txt = QString::number(d_ptr->m_value, 'f', d_ptr->m_precision);
            QFont f = font();
            f.setPixelSize(12);
            f.setBold(true);
            painter.setFont(f);
            painter.setPen(activeColor);
            painter.drawText(podRect, Qt::AlignCenter, txt);
        }
    }
}

} // namespace QtIndustrialWidgets
