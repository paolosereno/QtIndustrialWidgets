// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/IndustrialKnob.h>

#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QFontMetricsF>
#include <QtCore/QtMath>
#include <algorithm>
#include <cmath>
#include <vector>

namespace QtIndustrialWidgets {


class IndustrialKnobPrivate {
public:
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{25.0};
    double m_step{1.0};
    int m_precision{0};
    QString m_unit{QStringLiteral("%")};

    double m_startAngle{-135.0};
    double m_spanAngle{270.0};
    int m_majorTicks{10};
    int m_minorTicks{3};

    IndustrialKnob::KnobMode m_mode{IndustrialKnob::KnobMode::Continuous};
    int m_discreteSteps{5};
    bool m_trackVisible{true};
    bool m_valueDisplayVisible{true};

    QColor m_knobColor{QColor(42, 48, 60)};
    QColor m_pointerColor{QColor(0, 229, 255)};
    QColor m_scaleColor{QColor(190, 200, 215)};
    QColor m_trackColor{QColor(0, 229, 255)};
    QColor m_textColor{QColor(240, 244, 250)};

    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};

    bool m_isDragging{false};
    QPointF m_lastMousePos;
    // Pointer during a drag, in degrees from the start of the scale. Not wrapped: out of [0, span] it has gone
    // past an end, and the value stays there
    double m_dragAngle{0.0};

    // Scale at the current configuration: the same for the static scale, the paint and the mouse (#170)
    struct ScaleLayout {
        internal::KnobScaleGeometry geometry;
        QFont font;
        std::vector<QString> texts;
        std::vector<internal::LayoutRect> boxes;
        internal::RadialScaleLabels labels;
    };
    ScaleLayout m_scale;
    QSize m_scaleSize;   // Size m_scale was computed for
    bool m_scaleValid{false};

    double angleOf(double value) const;
    double valueAtScaleAngle(double angle) const;
    int tickCount() const;
    // Texts, boxes and selection of the labels at a geometry
    ScaleLayout labelsAt(const internal::KnobScaleGeometry &geometry, const QFont &widgetFont) const;
    const ScaleLayout &scaleFor(const QSize &size, const QFont &widgetFont);
    // Clockwise from the start of the scale, in [0, 360), around the dial as it is drawn
    double scaleAngleAt(const QPointF &pos, const QSize &size, const QFont &widgetFont);
};

double IndustrialKnobPrivate::angleOf(double value) const
{
    if (m_maximum <= m_minimum) return m_startAngle;
    double factor = (value - m_minimum) / (m_maximum - m_minimum);
    factor = std::clamp(factor, 0.0, 1.0);
    return m_startAngle + factor * m_spanAngle;
}

double IndustrialKnobPrivate::valueAtScaleAngle(double angle) const
{
    const double factor = std::clamp(angle / m_spanAngle, 0.0, 1.0);
    return m_minimum + factor * (m_maximum - m_minimum);
}

int IndustrialKnobPrivate::tickCount() const
{
    return std::max(1, m_mode == IndustrialKnob::KnobMode::Discrete ? m_discreteSteps - 1 : m_majorTicks);
}

IndustrialKnobPrivate::ScaleLayout IndustrialKnobPrivate::labelsAt(const internal::KnobScaleGeometry &geometry,
                                                                   const QFont &widgetFont) const
{
    ScaleLayout out;
    out.geometry = geometry;
    out.font = widgetFont; // #51: the widget font, not the application font of the pixmap painter
    out.font.setPixelSize(geometry.labelFontPx);
    out.font.setBold(true);

    // #60: every tick is drawn, but only the labels that keep the minimum gap (selection of #50)
    const QFontMetricsF fm(out.font);
    const int ticks = tickCount();
    for (int i = 0; i <= ticks; ++i) {
        const double frac = static_cast<double>(i) / ticks;
        const double val = m_minimum + frac * (m_maximum - m_minimum);
        out.texts.push_back(m_mode == IndustrialKnob::KnobMode::Discrete ? QString::number(i + 1)
                                                                         : internal::scaleLabelText(val, m_precision));
        const QRectF textRect = fm.boundingRect(out.texts.back());
        out.boxes.push_back(internal::knobLabelBox(geometry, angleOf(val), textRect.width(), textRect.height()));
    }
    out.labels = internal::layoutRadialScaleLabels(out.boxes, internal::scaleLabelMinGap(fm.height()));
    return out;
}

const IndustrialKnobPrivate::ScaleLayout &IndustrialKnobPrivate::scaleFor(const QSize &size, const QFont &widgetFont)
{
    if (!m_scaleValid || m_scaleSize != size) {
        const internal::KnobScaleGeometry geometry =
            internal::knobScaleGeometry(size.width(), size.height(), [&](const internal::KnobScaleGeometry &g) {
                const ScaleLayout at = labelsAt(g, widgetFont);
                std::vector<internal::LayoutRect> drawn;
                for (int i : at.labels.drawn) {
                    drawn.push_back(at.boxes[static_cast<size_t>(i)]);
                }
                return drawn;
            });
        m_scale = labelsAt(geometry, widgetFont);
        m_scaleSize = size;
        m_scaleValid = true;
    }
    return m_scale;
}

double IndustrialKnobPrivate::scaleAngleAt(const QPointF &pos, const QSize &size, const QFont &widgetFont)
{
    const internal::KnobScaleGeometry &scale = scaleFor(size, widgetFont).geometry;
    double dx = pos.x() - scale.centerX;
    double dy = pos.y() - scale.centerY;

    // Standard polar coordinates where 0° is 12 o'clock, clockwise positive
    double rad = std::atan2(dx, -dy);
    double deg = rad * 180.0 / M_PI;

    double relativeAngle = std::fmod(deg - m_startAngle, 360.0);
    if (relativeAngle < 0.0) relativeAngle += 360.0;
    return relativeAngle;
}

IndustrialKnob::IndustrialKnob(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<IndustrialKnobPrivate>())
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

IndustrialKnob::~IndustrialKnob() = default;

double IndustrialKnob::minimum() const { Q_D(const IndustrialKnob); return d->m_minimum; }
double IndustrialKnob::maximum() const { Q_D(const IndustrialKnob); return d->m_maximum; }
double IndustrialKnob::value() const { Q_D(const IndustrialKnob); return d->m_value; }
double IndustrialKnob::step() const { Q_D(const IndustrialKnob); return d->m_step; }
int IndustrialKnob::precision() const { Q_D(const IndustrialKnob); return d->m_precision; }
QString IndustrialKnob::unit() const { Q_D(const IndustrialKnob); return d->m_unit; }
double IndustrialKnob::startAngle() const { Q_D(const IndustrialKnob); return d->m_startAngle; }
double IndustrialKnob::spanAngle() const { Q_D(const IndustrialKnob); return d->m_spanAngle; }
int IndustrialKnob::majorTicks() const { Q_D(const IndustrialKnob); return d->m_majorTicks; }
int IndustrialKnob::minorTicks() const { Q_D(const IndustrialKnob); return d->m_minorTicks; }
IndustrialKnob::KnobMode IndustrialKnob::mode() const { Q_D(const IndustrialKnob); return d->m_mode; }
int IndustrialKnob::discreteSteps() const { Q_D(const IndustrialKnob); return d->m_discreteSteps; }
bool IndustrialKnob::isTrackVisible() const { Q_D(const IndustrialKnob); return d->m_trackVisible; }
bool IndustrialKnob::isValueDisplayVisible() const { Q_D(const IndustrialKnob); return d->m_valueDisplayVisible; }
QColor IndustrialKnob::knobColor() const { Q_D(const IndustrialKnob); return d->m_knobColor; }
QColor IndustrialKnob::pointerColor() const { Q_D(const IndustrialKnob); return d->m_pointerColor; }
QColor IndustrialKnob::scaleColor() const { Q_D(const IndustrialKnob); return d->m_scaleColor; }
QColor IndustrialKnob::trackColor() const { Q_D(const IndustrialKnob); return d->m_trackColor; }
QColor IndustrialKnob::textColor() const { Q_D(const IndustrialKnob); return d->m_textColor; }


QSize IndustrialKnob::sizeHint() const
{
    return QSize(160, 180);
}

QSize IndustrialKnob::minimumSizeHint() const
{
    return QSize(80, 90);
}

void IndustrialKnob::setValue(double val)
{
    if (std::isnan(val)) {
        return;
    }
    if (std::isinf(val)) {
        val = (val > 0.0) ? d_ptr->m_maximum : d_ptr->m_minimum;
    }

    double clamped = std::clamp(val, d_ptr->m_minimum, d_ptr->m_maximum);
    if (d_ptr->m_mode == KnobMode::Discrete && d_ptr->m_discreteSteps > 1) {
        double stepSize = (d_ptr->m_maximum - d_ptr->m_minimum) / (d_ptr->m_discreteSteps - 1);
        int nearestIndex = static_cast<int>(std::round((clamped - d_ptr->m_minimum) / stepSize));
        clamped = d_ptr->m_minimum + nearestIndex * stepSize;
    }

    if (qFuzzyCompare(clamped, d_ptr->m_value)) {
        return;
    }

    d_ptr->m_value = clamped;
    Q_EMIT valueChanged(d_ptr->m_value);
    update();
}

void IndustrialKnob::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void IndustrialKnob::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void IndustrialKnob::setRange(double min, double max)
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

void IndustrialKnob::setStep(double step)
{
    if (!std::isfinite(step) || step <= 0.0 || qFuzzyCompare(d_ptr->m_step, step)) return;
    d_ptr->m_step = step;
    Q_EMIT appearanceChanged();
}

void IndustrialKnob::setPrecision(int precision)
{
    if (d_ptr->m_precision == precision) return;
    d_ptr->m_precision = std::max(0, precision);
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setUnit(const QString &unit)
{
    if (d_ptr->m_unit == unit) return;
    d_ptr->m_unit = unit;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setStartAngle(double angle)
{
    if (!std::isfinite(angle) || qFuzzyCompare(d_ptr->m_startAngle, angle)) return;
    d_ptr->m_startAngle = angle;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setSpanAngle(double span)
{
    if (!std::isfinite(span) || span <= 0.0 || qFuzzyCompare(d_ptr->m_spanAngle, span)) return;
    d_ptr->m_spanAngle = span;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setMajorTicks(int count)
{
    if (d_ptr->m_majorTicks == count || count < 1) return;
    d_ptr->m_majorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setMinorTicks(int count)
{
    if (d_ptr->m_minorTicks == count || count < 0) return;
    d_ptr->m_minorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setMode(KnobMode mode)
{
    if (d_ptr->m_mode == mode) return;
    d_ptr->m_mode = mode;
    invalidateCache();
    setValue(d_ptr->m_value); // Re-clamp if discrete
    Q_EMIT modeChanged(d_ptr->m_mode);
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setDiscreteSteps(int steps)
{
    int s = std::max(2, steps);
    if (d_ptr->m_discreteSteps == s) return;
    d_ptr->m_discreteSteps = s;
    invalidateCache();
    if (d_ptr->m_mode == KnobMode::Discrete) {
        setValue(d_ptr->m_value);
    }
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setTrackVisible(bool visible)
{
    if (d_ptr->m_trackVisible == visible) return;
    d_ptr->m_trackVisible = visible;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setValueDisplayVisible(bool visible)
{
    if (d_ptr->m_valueDisplayVisible == visible) return;
    d_ptr->m_valueDisplayVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setKnobColor(const QColor &color)
{
    if (d_ptr->m_knobColor == color) return;
    d_ptr->m_knobColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setPointerColor(const QColor &color)
{
    if (d_ptr->m_pointerColor == color) return;
    d_ptr->m_pointerColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setScaleColor(const QColor &color)
{
    if (d_ptr->m_scaleColor == color) return;
    d_ptr->m_scaleColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setTrackColor(const QColor &color)
{
    if (d_ptr->m_trackColor == color) return;
    d_ptr->m_trackColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor == color) return;
    d_ptr->m_textColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::invalidateCache()
{
    d_ptr->m_cacheDirty = true;
    d_ptr->m_scaleValid = false;
}

void IndustrialKnob::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void IndustrialKnob::changeEvent(QEvent *event)
{
    // #51: the cached text uses the widget font, so a font change invalidates it too
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        invalidateCache();
        update();
    }
    QWidget::changeEvent(event);
}

double IndustrialKnob::valueToAngle(double val) const
{
    return d_ptr->angleOf(val);
}

double IndustrialKnob::angleToValue(double angle) const
{
    if (d_ptr->m_spanAngle <= 0.0) return d_ptr->m_minimum;
    double normAngle = angle - d_ptr->m_startAngle;
    while (normAngle < 0.0) normAngle += 360.0;
    while (normAngle > 360.0) normAngle -= 360.0;

    double factor = normAngle / d_ptr->m_spanAngle;
    factor = std::clamp(factor, 0.0, 1.0);
    return d_ptr->m_minimum + factor * (d_ptr->m_maximum - d_ptr->m_minimum);
}

void IndustrialKnob::updateValueFromPoint(const QPointF &pos)
{
    // #175: the shorter way round from the last point, so a jump over the gap between the ends is seen as one. Past
    // an end the value stays there until the pointer comes back on that end, the way it came or after a full turn
    const double span = d_ptr->m_spanAngle;
    const double angle = d_ptr->scaleAngleAt(pos, size(), font());
    double dragAngle = d_ptr->m_dragAngle + std::remainder(angle - d_ptr->m_dragAngle, 360.0);
    if (dragAngle >= span + 360.0) {
        dragAngle -= 360.0;
    } else if (dragAngle <= -360.0) {
        dragAngle += 360.0;
    }
    d_ptr->m_dragAngle = dragAngle;
    setValue(d_ptr->valueAtScaleAngle(dragAngle));
}

void IndustrialKnob::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // #175: a press in the gap between the ends of the scale selects nothing and starts no drag
        const double angle = d_ptr->scaleAngleAt(event->position(), size(), font());
        if (angle <= d_ptr->m_spanAngle) {
            d_ptr->m_isDragging = true;
            d_ptr->m_lastMousePos = event->position();
            d_ptr->m_dragAngle = angle;
            setValue(d_ptr->valueAtScaleAngle(angle));
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void IndustrialKnob::mouseMoveEvent(QMouseEvent *event)
{
    if (d_ptr->m_isDragging && (event->buttons() & Qt::LeftButton)) {
        updateValueFromPoint(event->position());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void IndustrialKnob::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        d_ptr->m_isDragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void IndustrialKnob::wheelEvent(QWheelEvent *event)
{
    double numSteps = event->angleDelta().y() / 120.0;
    double delta = numSteps * d_ptr->m_step;
    setValue(d_ptr->m_value + delta);
    event->accept();
}

void IndustrialKnob::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Down:
            setValue(d_ptr->m_value - d_ptr->m_step);
            event->accept();
            return;
        case Qt::Key_Right:
        case Qt::Key_Up:
            setValue(d_ptr->m_value + d_ptr->m_step);
            event->accept();
            return;
        case Qt::Key_PageDown:
            setValue(d_ptr->m_value - d_ptr->m_step * 5.0);
            event->accept();
            return;
        case Qt::Key_PageUp:
            setValue(d_ptr->m_value + d_ptr->m_step * 5.0);
            event->accept();
            return;
        case Qt::Key_Home:
            setValue(d_ptr->m_minimum);
            event->accept();
            return;
        case Qt::Key_End:
            setValue(d_ptr->m_maximum);
            event->accept();
            return;
        default:
            QWidget::keyPressEvent(event);
    }
}

void IndustrialKnob::renderStaticScale(const QSize &targetSize)
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
    const IndustrialKnobPrivate::ScaleLayout &layout = d_ptr->scaleFor(targetSize, font());
    const internal::KnobScaleGeometry &scale = layout.geometry;
    const QPointF center(scale.centerX, scale.centerY);
    const double radius = scale.radius;

    if (radius <= 10.0) {
        d_ptr->m_cacheDirty = false;
        return;
    }

    // Outer Scale Radius and Track Trough
    const double scaleRadius = scale.scaleRadius;
    const double tickLen = radius * 0.12;
    const double minorTickLen = tickLen * 0.55;

    const int totalTicks = d_ptr->tickCount();
    int totalMinor = (d_ptr->m_mode == KnobMode::Discrete) ? 0 : d_ptr->m_minorTicks;
    int totalDivisions = totalTicks * (totalMinor + 1);
    double valRange = d_ptr->m_maximum - d_ptr->m_minimum;

    // Draw Minor Ticks
    if (totalMinor > 0) {
        painter.setPen(QPen(d_ptr->m_scaleColor.darker(130), 1.0));
        for (int i = 0; i <= totalDivisions; ++i) {
            if (i % (totalMinor + 1) == 0) continue;
            double frac = static_cast<double>(i) / totalDivisions;
            double val = d_ptr->m_minimum + frac * valRange;
            double angle = valueToAngle(val);
            double rad = (angle - 90.0) * M_PI / 180.0;
            double cosR = std::cos(rad);
            double sinR = std::sin(rad);

            QPointF p1(center.x() + (scaleRadius - minorTickLen) * cosR, center.y() + (scaleRadius - minorTickLen) * sinR);
            QPointF p2(center.x() + scaleRadius * cosR, center.y() + scaleRadius * sinR);
            painter.drawLine(p1, p2);
        }
    }

    // Draw Major Ticks and Aligned Numeric Labels
    painter.setFont(layout.font);

    QPen majorPen(d_ptr->m_scaleColor, 1.6, Qt::SolidLine, Qt::RoundCap);

    for (int i = 0; i <= totalTicks; ++i) {
        double frac = static_cast<double>(i) / totalTicks;
        double val = d_ptr->m_minimum + frac * valRange;
        double angle = valueToAngle(val);
        double rad = (angle - 90.0) * M_PI / 180.0;
        double cosR = std::cos(rad);
        double sinR = std::sin(rad);

        // Tick mark
        painter.setPen(majorPen);
        QPointF p1(center.x() + (scaleRadius - tickLen) * cosR, center.y() + (scaleRadius - tickLen) * sinR);
        QPointF p2(center.x() + scaleRadius * cosR, center.y() + scaleRadius * sinR);
        painter.drawLine(p1, p2);
    }

    // Numeric Labels
    painter.setPen(d_ptr->m_textColor);
    for (int i : layout.labels.drawn) {
        const internal::LayoutRect &box = layout.boxes[static_cast<size_t>(i)];
        // TextDontClip: the glyphs can exceed boundingRect() (CoreText on macOS, see #31 / #35);
        // the rectangle only positions the label
        painter.drawText(QRectF(box.x, box.y, box.w, box.h), Qt::AlignCenter | Qt::TextDontClip,
                         layout.texts[static_cast<size_t>(i)]);
    }

    // Static Digital Pod at bottom (if enabled)
    if (d_ptr->m_valueDisplayVisible) {
        double podW = std::clamp(w * 0.65, 50.0, 110.0);
        double podH = 22.0;
        QRectF podRect((w - podW) * 0.5, h - podH - 2.0, podW, podH);

        painter.setPen(QPen(d_ptr->m_knobColor.darker(160), 1.0));
        painter.setBrush(QColor(16, 20, 26)); // opaque, as the gauge pods (#159)
        painter.drawRoundedRect(podRect, 3.0, 3.0);
    }

    d_ptr->m_cacheDirty = false;
}

void IndustrialKnob::paintEvent(QPaintEvent *)
{
    if (d_ptr->m_cacheDirty || d_ptr->m_cachePixmap.size() != (QSizeF(size()) * devicePixelRatioF()).toSize()) {
        renderStaticScale(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast Blit of Cached Scale & Frame
    painter.drawPixmap(0, 0, d_ptr->m_cachePixmap);

    const double w = width();
    const double h = height();
    const internal::KnobScaleGeometry &scale = d_ptr->scaleFor(size(), font()).geometry;
    const QPointF center(scale.centerX, scale.centerY);
    const double radius = scale.radius;
    const double knobRadius = radius * 0.65;

    if (knobRadius <= 5.0) return;

    // 2. Active Illuminated Track Arc
    if (d_ptr->m_trackVisible) {
        double trackRadius = radius * 0.84;
        double aStart = d_ptr->m_startAngle;
        double aEnd = valueToAngle(d_ptr->m_value);
        int steps = std::max(6, static_cast<int>(std::abs(aEnd - aStart) * 0.6));

        QPainterPath trackPath;
        for (int i = 0; i <= steps; ++i) {
            double a = aStart + (aEnd - aStart) * (static_cast<double>(i) / steps);
            double rad = (a - 90.0) * M_PI / 180.0;
            double px = center.x() + trackRadius * std::cos(rad);
            double py = center.y() + trackRadius * std::sin(rad);
            if (i == 0) trackPath.moveTo(px, py);
            else trackPath.lineTo(px, py);
        }

        QPen trackPen(d_ptr->m_trackColor, std::max(2.5, radius * 0.04), Qt::SolidLine, Qt::RoundCap);
        painter.strokePath(trackPath, trackPen);
    }

    // 3. Focus Highlight Ring (when widget has keyboard/tab focus)
    if (hasFocus()) {
        painter.setPen(QPen(QColor(0, 229, 255, 140), 1.5, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, knobRadius + 4.0, knobRadius + 4.0);
    }

    // 4. Outer Knurled Grip Texture (CNC Serrations / Knurling effect)
    painter.save();
    painter.translate(center);
    painter.rotate(valueToAngle(d_ptr->m_value));

    // Outer Knurling Base Ring
    QRadialGradient knurlGrad(QPointF(0, 0), knobRadius);
    knurlGrad.setColorAt(0.0, d_ptr->m_knobColor.lighter(130));
    knurlGrad.setColorAt(0.85, d_ptr->m_knobColor);
    knurlGrad.setColorAt(1.0, d_ptr->m_knobColor.darker(150));

    painter.setPen(QPen(d_ptr->m_knobColor.darker(180), 1.2));
    painter.setBrush(knurlGrad);
    painter.drawEllipse(QPointF(0, 0), knobRadius, knobRadius);

    // Serration teeth (32 teeth)
    const int teethCount = 32;
    const double toothLen = knobRadius * 0.09;
    QPen toothPen(d_ptr->m_knobColor.darker(170), 1.2);
    painter.setPen(toothPen);

    for (int i = 0; i < teethCount; ++i) {
        double deg = i * (360.0 / teethCount);
        double rad = deg * M_PI / 180.0;
        double c = std::cos(rad);
        double s = std::sin(rad);
        painter.drawLine(QPointF((knobRadius - toothLen) * c, (knobRadius - toothLen) * s),
                         QPointF(knobRadius * c, knobRadius * s));
    }

    // 5. Beveled Face Rim & Brushed Metallic Face
    const double faceRadius = knobRadius * 0.84;
    QLinearGradient faceGrad(QPointF(-faceRadius, -faceRadius), QPointF(faceRadius, faceRadius));
    faceGrad.setColorAt(0.0, d_ptr->m_knobColor.lighter(140));
    faceGrad.setColorAt(0.5, d_ptr->m_knobColor.lighter(105));
    faceGrad.setColorAt(1.0, d_ptr->m_knobColor.darker(140));

    painter.setPen(QPen(d_ptr->m_knobColor.darker(180), 1.0));
    painter.setBrush(faceGrad);
    painter.drawEllipse(QPointF(0, 0), faceRadius, faceRadius);

    // Inner Specular Glare Ring
    const double innerFace = faceRadius * 0.90;
    QRadialGradient innerGrad(QPointF(-innerFace * 0.3, -innerFace * 0.3), innerFace * 1.2);
    innerGrad.setColorAt(0.0, d_ptr->m_knobColor.lighter(150));
    innerGrad.setColorAt(0.4, d_ptr->m_knobColor);
    innerGrad.setColorAt(1.0, d_ptr->m_knobColor.darker(160));

    painter.setPen(Qt::NoPen);
    painter.setBrush(innerGrad);
    painter.drawEllipse(QPointF(0, 0), innerFace, innerFace);

    // 6. Laser-Etched Pointer Indicator (High contrast pointer)
    const double ptrInner = innerFace * 0.32;
    const double ptrOuter = innerFace * 0.92;
    const double ptrWidth = std::max(2.5, knobRadius * 0.07);

    // Pointer notch polygon (pointing towards negative Y, which rotates with painter)
    QPolygonF ptrPoly;
    ptrPoly << QPointF(-ptrWidth * 0.5, -ptrInner)
            << QPointF(-ptrWidth * 0.5, -ptrOuter + 2.0)
            << QPointF(0.0, -ptrOuter)
            << QPointF(ptrWidth * 0.5, -ptrOuter + 2.0)
            << QPointF(ptrWidth * 0.5, -ptrInner);

    painter.setPen(QPen(d_ptr->m_pointerColor.darker(150), 0.8));
    painter.setBrush(d_ptr->m_pointerColor);
    painter.drawPolygon(ptrPoly);

    // Specular center dot
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 140));
    painter.drawEllipse(QPointF(-1.0, -1.0), 2.5, 2.5);

    painter.restore();

    // 7. Dynamic Digital Value Readout (Bottom Pod)
    if (d_ptr->m_valueDisplayVisible) {
        double podW = std::clamp(w * 0.65, 50.0, 110.0);
        double podH = 22.0;
        QRectF podRect((w - podW) * 0.5, h - podH - 2.0, podW, podH);

        QString valStr;
        if (d_ptr->m_mode == KnobMode::Discrete) {
            int stepIdx = static_cast<int>(std::round((d_ptr->m_value - d_ptr->m_minimum) / ((d_ptr->m_maximum - d_ptr->m_minimum) / (d_ptr->m_discreteSteps - 1))));
            valStr = QStringLiteral("POS %1").arg(stepIdx + 1);
        } else {
            valStr = QStringLiteral("%1 %2").arg(internal::formatFixed(d_ptr->m_value, d_ptr->m_precision), d_ptr->m_unit).trimmed();
        }

        QFont valFont = font();
        valFont.setPixelSize(11);
        valFont.setBold(true);
        // Greyscale antialiasing, as before the pod was opaque (#159): no coloured glyph edges
        valFont.setStyleStrategy(QFont::StyleStrategy(valFont.styleStrategy() | QFont::NoSubpixelAntialias));
        painter.setFont(valFont);
        painter.setPen(d_ptr->m_pointerColor);
        painter.drawText(podRect, Qt::AlignCenter, valStr);
    }
}

} // namespace QtIndustrialWidgets
