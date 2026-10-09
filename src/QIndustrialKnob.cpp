// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/QIndustrialKnob.h>

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

QIndustrialKnob::QIndustrialKnob(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize QIndustrialKnob::sizeHint() const
{
    return QSize(160, 180);
}

QSize QIndustrialKnob::minimumSizeHint() const
{
    return QSize(80, 90);
}

void QIndustrialKnob::setValue(double val)
{
    double clamped = std::clamp(val, m_minimum, m_maximum);
    if (m_mode == KnobMode::Discrete && m_discreteSteps > 1) {
        double stepSize = (m_maximum - m_minimum) / (m_discreteSteps - 1);
        int nearestIndex = static_cast<int>(std::round((clamped - m_minimum) / stepSize));
        clamped = m_minimum + nearestIndex * stepSize;
    }

    if (qFuzzyCompare(clamped, m_value)) {
        return;
    }

    m_value = clamped;
    Q_EMIT valueChanged(m_value);
    update();
}

void QIndustrialKnob::setMinimum(double min)
{
    setRange(min, m_maximum);
}

void QIndustrialKnob::setMaximum(double max)
{
    setRange(m_minimum, max);
}

void QIndustrialKnob::setRange(double min, double max)
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

void QIndustrialKnob::setStep(double step)
{
    if (step <= 0.0 || qFuzzyCompare(m_step, step)) return;
    m_step = step;
    Q_EMIT appearanceChanged();
}

void QIndustrialKnob::setPrecision(int precision)
{
    if (m_precision == precision) return;
    m_precision = std::max(0, precision);
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setUnit(const QString &unit)
{
    if (m_unit == unit) return;
    m_unit = unit;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setStartAngle(double angle)
{
    if (qFuzzyCompare(m_startAngle, angle)) return;
    m_startAngle = angle;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setSpanAngle(double span)
{
    if (span <= 0.0 || qFuzzyCompare(m_spanAngle, span)) return;
    m_spanAngle = span;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setMajorTicks(int count)
{
    if (m_majorTicks == count || count < 1) return;
    m_majorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setMinorTicks(int count)
{
    if (m_minorTicks == count || count < 0) return;
    m_minorTicks = count;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setMode(KnobMode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    invalidateCache();
    setValue(m_value); // Re-clamp if discrete
    Q_EMIT modeChanged(m_mode);
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setDiscreteSteps(int steps)
{
    int s = std::max(2, steps);
    if (m_discreteSteps == s) return;
    m_discreteSteps = s;
    invalidateCache();
    if (m_mode == KnobMode::Discrete) {
        setValue(m_value);
    }
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setTrackVisible(bool visible)
{
    if (m_trackVisible == visible) return;
    m_trackVisible = visible;
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setValueDisplayVisible(bool visible)
{
    if (m_valueDisplayVisible == visible) return;
    m_valueDisplayVisible = visible;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setKnobColor(const QColor &color)
{
    if (m_knobColor == color) return;
    m_knobColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setPointerColor(const QColor &color)
{
    if (m_pointerColor == color) return;
    m_pointerColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setScaleColor(const QColor &color)
{
    if (m_scaleColor == color) return;
    m_scaleColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setTrackColor(const QColor &color)
{
    if (m_trackColor == color) return;
    m_trackColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::setTextColor(const QColor &color)
{
    if (m_textColor == color) return;
    m_textColor = color;
    invalidateCache();
    Q_EMIT appearanceChanged();
    update();
}

void QIndustrialKnob::invalidateCache()
{
    m_cacheDirty = true;
}

void QIndustrialKnob::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void QIndustrialKnob::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        invalidateCache();
        update();
    }
    QWidget::changeEvent(event);
}

double QIndustrialKnob::valueToAngle(double val) const
{
    if (m_maximum <= m_minimum) return m_startAngle;
    double factor = (val - m_minimum) / (m_maximum - m_minimum);
    factor = std::clamp(factor, 0.0, 1.0);
    return m_startAngle + factor * m_spanAngle;
}

double QIndustrialKnob::angleToValue(double angle) const
{
    if (m_spanAngle <= 0.0) return m_minimum;
    double normAngle = angle - m_startAngle;
    while (normAngle < 0.0) normAngle += 360.0;
    while (normAngle > 360.0) normAngle -= 360.0;

    double factor = normAngle / m_spanAngle;
    factor = std::clamp(factor, 0.0, 1.0);
    return m_minimum + factor * (m_maximum - m_minimum);
}

void QIndustrialKnob::updateValueFromPoint(const QPointF &pos)
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
    double relativeAngle = deg - m_startAngle;
    while (relativeAngle < -180.0) relativeAngle += 360.0;
    while (relativeAngle > 180.0) relativeAngle -= 360.0;

    if (relativeAngle < 0.0) {
        // Closer to start or beyond?
        if (relativeAngle < -m_spanAngle * 0.5) {
            setValue(m_maximum);
        } else {
            setValue(m_minimum);
        }
    } else if (relativeAngle > m_spanAngle) {
        setValue(m_maximum);
    } else {
        double factor = relativeAngle / m_spanAngle;
        double newVal = m_minimum + factor * (m_maximum - m_minimum);
        setValue(newVal);
    }
}

void QIndustrialKnob::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->position();
        updateValueFromPoint(event->position());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void QIndustrialKnob::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        updateValueFromPoint(event->position());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void QIndustrialKnob::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void QIndustrialKnob::wheelEvent(QWheelEvent *event)
{
    double numSteps = event->angleDelta().y() / 120.0;
    double delta = numSteps * m_step;
    setValue(m_value + delta);
    event->accept();
}

void QIndustrialKnob::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Down:
            setValue(m_value - m_step);
            event->accept();
            return;
        case Qt::Key_Right:
        case Qt::Key_Up:
            setValue(m_value + m_step);
            event->accept();
            return;
        case Qt::Key_PageDown:
            setValue(m_value - m_step * 5.0);
            event->accept();
            return;
        case Qt::Key_PageUp:
            setValue(m_value + m_step * 5.0);
            event->accept();
            return;
        case Qt::Key_Home:
            setValue(m_minimum);
            event->accept();
            return;
        case Qt::Key_End:
            setValue(m_maximum);
            event->accept();
            return;
        default:
            QWidget::keyPressEvent(event);
    }
}

void QIndustrialKnob::renderStaticScale(const QSize &targetSize)
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
    const double side = std::min(w, h * 0.82);
    const QPointF center(w * 0.5, side * 0.5 + 4.0);
    const double radius = (side * 0.5) - 4.0;

    if (radius <= 10.0) {
        m_cacheDirty = false;
        return;
    }

    // Outer Scale Radius and Track Trough
    const double scaleRadius = radius * 0.92;
    const double tickLen = radius * 0.12;
    const double minorTickLen = tickLen * 0.55;
    const double labelRadius = scaleRadius + radius * 0.12;

    int totalTicks = (m_mode == KnobMode::Discrete) ? (m_discreteSteps - 1) : m_majorTicks;
    totalTicks = std::max(1, totalTicks);
    int totalMinor = (m_mode == KnobMode::Discrete) ? 0 : m_minorTicks;
    int totalDivisions = totalTicks * (totalMinor + 1);
    double valRange = m_maximum - m_minimum;

    // Draw Minor Ticks
    if (totalMinor > 0) {
        painter.setPen(QPen(m_scaleColor.darker(130), 1.0));
        for (int i = 0; i <= totalDivisions; ++i) {
            if (i % (totalMinor + 1) == 0) continue;
            double frac = static_cast<double>(i) / totalDivisions;
            double val = m_minimum + frac * valRange;
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

    QPen majorPen(m_scaleColor, 1.6, Qt::SolidLine, Qt::RoundCap);

    for (int i = 0; i <= totalTicks; ++i) {
        double frac = static_cast<double>(i) / totalTicks;
        double val = m_minimum + frac * valRange;
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
        if (m_mode == KnobMode::Discrete) {
            labelStr = QString::number(i + 1);
        } else {
            labelStr = (m_precision == 0) ? QString::number(static_cast<qint64>(std::round(val)))
                                          : QString::number(val, 'f', (val == std::floor(val)) ? 0 : 1);
        }

        QFontMetricsF fm(font);
        QRectF textRect = fm.boundingRect(labelStr);
        QPointF textCenter(center.x() + labelRadius * cosR, center.y() + labelRadius * sinR);
        QRectF drawRect(textCenter.x() - textRect.width() * 0.5,
                       textCenter.y() - textRect.height() * 0.5,
                       textRect.width(), textRect.height());

        painter.setPen(m_textColor);
        painter.drawText(drawRect, Qt::AlignCenter, labelStr);
    }

    // Static Digital Pod at bottom (if enabled)
    if (m_valueDisplayVisible) {
        double podW = std::clamp(w * 0.65, 50.0, 110.0);
        double podH = 22.0;
        QRectF podRect((w - podW) * 0.5, h - podH - 2.0, podW, podH);

        painter.setPen(QPen(m_knobColor.darker(160), 1.0));
        painter.setBrush(QColor(16, 20, 26, 220));
        painter.drawRoundedRect(podRect, 3.0, 3.0);
    }

    m_cacheDirty = false;
}

void QIndustrialKnob::paintEvent(QPaintEvent *)
{
    if (m_cacheDirty || m_cachePixmap.size() != (QSizeF(size()) * devicePixelRatioF()).toSize()) {
        renderStaticScale(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. Fast Blit of Cached Scale & Frame
    painter.drawPixmap(0, 0, m_cachePixmap);

    const double w = width();
    const double h = height();
    const double side = std::min(w, h * 0.82);
    const QPointF center(w * 0.5, side * 0.5 + 4.0);
    const double radius = (side * 0.5) - 4.0;
    const double knobRadius = radius * 0.65;

    if (knobRadius <= 5.0) return;

    // 2. Active Illuminated Track Arc
    if (m_trackVisible) {
        double trackRadius = radius * 0.84;
        double aStart = m_startAngle;
        double aEnd = valueToAngle(m_value);
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

        QPen trackPen(m_trackColor, std::max(2.5, radius * 0.04), Qt::SolidLine, Qt::RoundCap);
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
    painter.rotate(valueToAngle(m_value));

    // Outer Knurling Base Ring
    QRadialGradient knurlGrad(QPointF(0, 0), knobRadius);
    knurlGrad.setColorAt(0.0, m_knobColor.lighter(130));
    knurlGrad.setColorAt(0.85, m_knobColor);
    knurlGrad.setColorAt(1.0, m_knobColor.darker(150));

    painter.setPen(QPen(m_knobColor.darker(180), 1.2));
    painter.setBrush(knurlGrad);
    painter.drawEllipse(QPointF(0, 0), knobRadius, knobRadius);

    // Serration teeth (32 teeth)
    const int teethCount = 32;
    const double toothLen = knobRadius * 0.09;
    QPen toothPen(m_knobColor.darker(170), 1.2);
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
    faceGrad.setColorAt(0.0, m_knobColor.lighter(140));
    faceGrad.setColorAt(0.5, m_knobColor.lighter(105));
    faceGrad.setColorAt(1.0, m_knobColor.darker(140));

    painter.setPen(QPen(m_knobColor.darker(180), 1.0));
    painter.setBrush(faceGrad);
    painter.drawEllipse(QPointF(0, 0), faceRadius, faceRadius);

    // Inner Specular Glare Ring
    const double innerFace = faceRadius * 0.90;
    QRadialGradient innerGrad(QPointF(-innerFace * 0.3, -innerFace * 0.3), innerFace * 1.2);
    innerGrad.setColorAt(0.0, m_knobColor.lighter(150));
    innerGrad.setColorAt(0.4, m_knobColor);
    innerGrad.setColorAt(1.0, m_knobColor.darker(160));

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

    painter.setPen(QPen(m_pointerColor.darker(150), 0.8));
    painter.setBrush(m_pointerColor);
    painter.drawPolygon(ptrPoly);

    // Specular center dot
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 140));
    painter.drawEllipse(QPointF(-1.0, -1.0), 2.5, 2.5);

    painter.restore();

    // 7. Dynamic Digital Value Readout (Bottom Pod)
    if (m_valueDisplayVisible) {
        double podW = std::clamp(w * 0.65, 50.0, 110.0);
        double podH = 22.0;
        QRectF podRect((w - podW) * 0.5, h - podH - 2.0, podW, podH);

        QString valStr;
        if (m_mode == KnobMode::Discrete) {
            int stepIdx = static_cast<int>(std::round((m_value - m_minimum) / ((m_maximum - m_minimum) / (m_discreteSteps - 1))));
            valStr = QStringLiteral("POS %1").arg(stepIdx + 1);
        } else {
            valStr = QStringLiteral("%1 %2").arg(QString::number(m_value, 'f', m_precision), m_unit).trimmed();
        }

        QFont valFont = font();
        valFont.setPixelSize(11);
        valFont.setBold(true);
        painter.setFont(valFont);
        painter.setPen(m_pointerColor);
        painter.drawText(podRect, Qt::AlignCenter, valStr);
    }
}
