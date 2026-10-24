// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/LinearGauge.h>

#include "internal/FontScale.h"
#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QLinearGradient>
#include <QtCore/QStringList>
#include <QtCore/QtMath>
#include <QtWidgets/QApplication>
#include <algorithm>
#include <cmath>

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

    // Text of every major scale label, minimum first
    QStringList majorLabelTexts() const
    {
        QStringList texts;
        const double valRange = m_maximum - m_minimum;
        for (int i = 0; i <= m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / m_majorTicks;
            const double val = m_minimum + frac * valRange;
            texts << internal::scaleLabelText(val, m_precision);
        }
        return texts;
    }
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
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max) return;
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
    if (!std::isfinite(threshold) || qFuzzyCompare(d_ptr->m_warningThreshold, threshold)) return;
    d_ptr->m_warningThreshold = threshold;
    invalidateCache();
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void LinearGauge::setErrorThreshold(double threshold)
{
    if (!std::isfinite(threshold) || qFuzzyCompare(d_ptr->m_errorThreshold, threshold)) return;
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
    // #51: the cached text uses the widget font, so a font change invalidates it too
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
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
        double tubeWidth = std::clamp(w * 0.16, 10.0, 26.0);

        if (d_ptr->m_thermometerMode) {
            d_ptr->m_bulbRadius = tubeWidth * 1.35;
        } else {
            d_ptr->m_bulbRadius = 0.0;
        }

        double tubeX = d_ptr->m_scaleVisible ? (w * 0.32 - tubeWidth * 0.5) : (w * 0.5 - tubeWidth * 0.5);
        double tubeTop = topPadding;
        // #37 point 2: the scale length comes from the helper shared with the tests
        double tubeHeight = internal::linearGaugeScaleLength(true, w, h, d_ptr->m_thermometerMode, d_ptr->m_digitalDisplayVisible);

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
            // #37 point 1 / #51: the largest size between 8 px and the height-derived one at which at
            // least every other label is drawn; both limits scaled by the widget font (rule (b))
            const QStringList labelTexts = d_ptr->majorLabelTexts();
            const double fontScale = internal::fontScale(font(), QApplication::font());
            auto labelFontAt = [this](int px) {
                QFont lf = font(); // #51: the widget font, not the application font of the pixmap painter
                lf.setPixelSize(px);
                lf.setBold(true);
                return lf;
            };
            const int fontSize = internal::chooseLabelPixelSize(
                internal::scalePixelSize(internal::linearGaugeBaseLabelPixelSize(true, h), fontScale),
                internal::scalePixelSize(8, fontScale), d_ptr->m_tubeRect.height(),
                [&](int px) {
                    const QFontMetricsF m(labelFontAt(px));
                    std::vector<double> e;
                    for (const QString &t : labelTexts) e.push_back(m.boundingRect(t).height());
                    return e;
                },
                [&](int px) { return internal::scaleLabelMinGap(QFontMetricsF(labelFontAt(px)).height()); });
            QFont f = labelFontAt(fontSize);
            painter.setFont(f);

            painter.setPen(QPen(d_ptr->m_scaleColor, 1.6));

            for (int i = 0; i <= d_ptr->m_majorTicks; ++i) {
                double frac = static_cast<double>(i) / d_ptr->m_majorTicks;
                double y = d_ptr->m_tubeRect.bottom() - frac * d_ptr->m_tubeRect.height();
                painter.drawLine(QPointF(tickStartX, y), QPointF(tickStartX + majorTickLen, y));
            }

            // Labels: only those that keep the minimum gap, the end ones inside the scale.
            // Positions along the scale run upwards from the tube bottom.
            const QFontMetricsF fm(f);
            const QStringList &texts = labelTexts;
            std::vector<double> extents;
            for (const QString &s : texts) {
                extents.push_back(fm.boundingRect(s).height());
            }
            const internal::ScaleLabelLayout layout = internal::layoutScaleLabels(
                extents, d_ptr->m_tubeRect.height(), internal::scaleLabelMinGap(fm.height()));
            painter.setPen(d_ptr->m_textColor);
            for (const internal::ScaleLabelSlot &slot : layout.labels) {
                QRectF drawR(labelX, d_ptr->m_tubeRect.bottom() - slot.end, w - labelX - 4.0, slot.end - slot.start);
                // TextDontClip: the glyphs can exceed boundingRect() (CoreText on macOS); the
                // rectangle only positions the label
                painter.drawText(drawR, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, texts.at(slot.index));
            }
        }

        // Digital display static pod (top)
        if (d_ptr->m_digitalDisplayVisible) {
            const internal::GaugeRect pod = internal::linearGaugeDisplayRect(true, w, h);
            QRectF podRect(pod.x, pod.y, pod.w, pod.h);
            painter.setPen(QPen(d_ptr->m_bezelColor.darker(140), 1.0));
            painter.setBrush(QColor(16, 20, 26, 220));
            painter.drawRoundedRect(podRect, 3.0, 3.0);
        }
    } else {
        // Horizontal orientation
        double leftPadding = 16.0;
        double tubeHeight = std::clamp(h * 0.16, 10.0, 26.0);

        if (d_ptr->m_thermometerMode) {
            d_ptr->m_bulbRadius = tubeHeight * 1.35;
            leftPadding = d_ptr->m_bulbRadius * 2.0 + 12.0;
        } else {
            d_ptr->m_bulbRadius = 0.0;
        }

        double tubeY = d_ptr->m_scaleVisible ? (h * 0.40 - tubeHeight * 0.5) : (h * 0.5 - tubeHeight * 0.5);
        double tubeLeft = leftPadding;
        // #37 point 2: the scale length comes from the helper shared with the tests
        double tubeWidth = internal::linearGaugeScaleLength(false, w, h, d_ptr->m_thermometerMode, d_ptr->m_digitalDisplayVisible);

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

            if (d_ptr->m_minorTicks > 0) {
                painter.setPen(QPen(d_ptr->m_scaleColor.darker(130), 1.0));
                for (int i = 0; i <= totalMinorDivisions; ++i) {
                    if (i % (d_ptr->m_minorTicks + 1) == 0) continue;
                    double frac = static_cast<double>(i) / totalMinorDivisions;
                    double x = d_ptr->m_tubeRect.left() + frac * d_ptr->m_tubeRect.width();
                    painter.drawLine(QPointF(x, tickStartY), QPointF(x, tickStartY + minorTickLen));
                }
            }

            // #37 point 1 / #51: the largest size between 8 px and the height-derived one at which at
            // least every other label is drawn; both limits scaled by the widget font (rule (b))
            const QStringList labelTexts = d_ptr->majorLabelTexts();
            const double fontScale = internal::fontScale(font(), QApplication::font());
            auto labelFontAt = [this](int px) {
                QFont lf = font(); // #51: the widget font, not the application font of the pixmap painter
                lf.setPixelSize(px);
                lf.setBold(true);
                return lf;
            };
            const int fontSize = internal::chooseLabelPixelSize(
                internal::scalePixelSize(internal::linearGaugeBaseLabelPixelSize(false, h), fontScale),
                internal::scalePixelSize(8, fontScale), d_ptr->m_tubeRect.width(),
                [&](int px) {
                    const QFontMetricsF m(labelFontAt(px));
                    std::vector<double> e;
                    for (const QString &t : labelTexts) e.push_back(m.boundingRect(t).width());
                    return e;
                },
                [&](int px) { return internal::scaleLabelMinGap(QFontMetricsF(labelFontAt(px)).height()); });
            QFont f = labelFontAt(fontSize);
            painter.setFont(f);

            painter.setPen(QPen(d_ptr->m_scaleColor, 1.6));
            for (int i = 0; i <= d_ptr->m_majorTicks; ++i) {
                double frac = static_cast<double>(i) / d_ptr->m_majorTicks;
                double x = d_ptr->m_tubeRect.left() + frac * d_ptr->m_tubeRect.width();
                painter.drawLine(QPointF(x, tickStartY), QPointF(x, tickStartY + majorTickLen));
            }

            // Labels: only those that keep the minimum gap, the end ones inside the scale
            // (the last one no longer reaches under the digital display)
            const QFontMetricsF fm(f);
            const QStringList &texts = labelTexts;
            std::vector<double> extents;
            for (const QString &s : texts) {
                extents.push_back(fm.boundingRect(s).width());
            }
            const internal::ScaleLabelLayout layout = internal::layoutScaleLabels(
                extents, d_ptr->m_tubeRect.width(), internal::scaleLabelMinGap(fm.height()));
            painter.setPen(d_ptr->m_textColor);
            for (const internal::ScaleLabelSlot &slot : layout.labels) {
                const QString &s = texts.at(slot.index);
                QRectF drawR(d_ptr->m_tubeRect.left() + slot.start, labelY, slot.end - slot.start, fm.boundingRect(s).height());
                // TextDontClip: the glyphs can exceed boundingRect() (CoreText on macOS, up to
                // 1 px per label); the rectangle only positions the label
                painter.drawText(drawR, Qt::AlignCenter | Qt::TextDontClip, s);
            }
        }

        // Digital display static pod (right)
        if (d_ptr->m_digitalDisplayVisible) {
            const internal::GaugeRect pod = internal::linearGaugeDisplayRect(false, w, h);
            QRectF podRect(pod.x, pod.y, pod.w, pod.h);
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
            const internal::GaugeRect pod = internal::linearGaugeDisplayRect(true, width(), height());
            QRectF podRect(pod.x, pod.y, pod.w, pod.h);
            QString txt = QStringLiteral("%1 %2").arg(internal::formatFixed(d_ptr->m_value, d_ptr->m_precision), d_ptr->m_unit);
            QFont f = font();
            f.setPixelSize(internal::scalePixelSize(12, internal::fontScale(font(), QApplication::font())));
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
            const internal::GaugeRect pod = internal::linearGaugeDisplayRect(false, width(), height());
            QRectF podRect(pod.x, pod.y, pod.w, pod.h);
            QString txt = internal::formatFixed(d_ptr->m_value, d_ptr->m_precision);
            QFont f = font();
            f.setPixelSize(internal::scalePixelSize(12, internal::fontScale(font(), QApplication::font())));
            f.setBold(true);
            painter.setFont(f);
            painter.setPen(activeColor);
            painter.drawText(podRect, Qt::AlignCenter, txt);
        }
    }
}

} // namespace QtIndustrialWidgets
