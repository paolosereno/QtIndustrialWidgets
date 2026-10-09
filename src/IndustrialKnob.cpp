// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/IndustrialKnob.h>

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
};

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

void IndustrialKnob::setStep(double step)
{
    if (step <= 0.0 || qFuzzyCompare(d_ptr->m_step, step)) return;
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
    if (qFuzzyCompare(d_ptr->m_startAngle, angle)) return;
    d_ptr->m_startAngle = angle;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setSpanAngle(double span)
{
    if (span <= 0.0 || qFuzzyCompare(d_ptr->m_spanAngle, span)) return;
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
}

void IndustrialKnob::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void IndustrialKnob::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        invalidateCache();
        update();
    }
    QWidget::changeEvent(event);
}

double IndustrialKnob::valueToAngle(double val) const
{
    if (d_ptr->m_maximum <= d_ptr->m_minimum) return d_ptr->m_startAngle;
    double factor = (val - d_ptr->m_minimum) / (d_ptr->m_maximum - d_ptr->m_minimum);
    factor = std::clamp(factor, 0.0, 1.0);
    return d_ptr->m_startAngle + factor * d_ptr->m_spanAngle;
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
    const double w = width();
    const double h = height();
    const double side = std::min(w, h * 0.85);
    const QPointF center(w * 0.5, side * 0.5 + 4.0);

    double dx = pos.x() - center.x();
    double dy = pos.y() - center.y();

    // Standard polar coordinates where 0° is 12 o'clock, clockwise positive
    double rad = std::atan2(dx, -dy);
    double deg = rad * 180.0 / M_PI;

    // Convert into our [startAngle, startAngle + spanAngle] range
    double relativeAngle = deg - d_ptr->m_startAngle;
    while (relativeAngle < -180.0) relativeAngle += 360.0;
    while (relativeAngle > 180.0) relativeAngle -= 360.0;

    if (relativeAngle < 0.0) {
        // Closer to start or beyond?
        if (relativeAngle < -d_ptr->m_spanAngle * 0.5) {
            setValue(d_ptr->m_maximum);
        } else {
            setValue(d_ptr->m_minimum);
        }
    } else if (relativeAngle > d_ptr->m_spanAngle) {
        setValue(d_ptr->m_maximum);
    } else {
        double factor = relativeAngle / d_ptr->m_spanAngle;
        double newVal = d_ptr->m_minimum + factor * (d_ptr->m_maximum - d_ptr->m_minimum);
        setValue(newVal);
    }
}

void IndustrialKnob::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        d_ptr->m_isDragging = true;
        d_ptr->m_lastMousePos = event->position();
        updateValueFromPoint(event->position());
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
    const double side = std::min(w, h * 0.82);
    const QPointF center(w * 0.5, side * 0.5 + 4.0);
    const double radius = (side * 0.5) - 4.0;

    if (radius <= 10.0) {
        d_ptr->m_cacheDirty = false;
        return;
    }

    // Outer Scale Radius and Track Trough
    const double scaleRadius = radius * 0.92;
    const double tickLen = radius * 0.12;
    const double minorTickLen = tickLen * 0.55;
    const double labelRadius = scaleRadius + radius * 0.12;

    int totalTicks = (d_ptr->m_mode == KnobMode::Discrete) ? (d_ptr->m_discreteSteps - 1) : d_ptr->m_majorTicks;
    totalTicks = std::max(1, totalTicks);
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
    int fontSize = std::clamp(static_cast<int>(radius * 0.12), 7, 11);
    QFont font = painter.font();
    font.setPixelSize(fontSize);
    font.setBold(true);
    painter.setFont(font);

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

        // Numeric Label
        QString labelStr;
        if (d_ptr->m_mode == KnobMode::Discrete) {
            labelStr = QString::number(i + 1);
        } else {
            labelStr = (d_ptr->m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                          : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);
        }

        QFontMetricsF fm(font);
        QRectF textRect = fm.boundingRect(labelStr);
        QPointF textCenter(center.x() + labelRadius * cosR, center.y() + labelRadius * sinR);
        QRectF drawRect(textCenter.x() - textRect.width() * 0.5,
                       textCenter.y() - textRect.height() * 0.5,
                       textRect.width(), textRect.height());

        painter.setPen(d_ptr->m_textColor);
        painter.drawText(drawRect, Qt::AlignCenter, labelStr);
    }

    // Static Digital Pod at bottom (if enabled)
    if (d_ptr->m_valueDisplayVisible) {
        double podW = std::clamp(w * 0.65, 50.0, 110.0);
        double podH = 22.0;
        QRectF podRect((w - podW) * 0.5, h - podH - 2.0, podW, podH);

        painter.setPen(QPen(d_ptr->m_knobColor.darker(160), 1.0));
        painter.setBrush(QColor(16, 20, 26, 220));
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
    const double side = std::min(w, h * 0.82);
    const QPointF center(w * 0.5, side * 0.5 + 4.0);
    const double radius = (side * 0.5) - 4.0;
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
            valStr = QStringLiteral("%1 %2").arg(QString::number(d_ptr->m_value, 'f', d_ptr->m_precision), d_ptr->m_unit).trimmed();
        }

        QFont valFont = font();
        valFont.setPixelSize(11);
        valFont.setBold(true);
        painter.setFont(valFont);
        painter.setPen(d_ptr->m_pointerColor);
        painter.drawText(podRect, Qt::AlignCenter, valStr);
    }
}

} // namespace QtIndustrialWidgets
