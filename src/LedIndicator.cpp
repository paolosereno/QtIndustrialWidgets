// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/LedIndicator.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QRadialGradient>
#include <QtGui/QLinearGradient>
#include <QtGui/QFontMetrics>
#include <algorithm>

namespace QtIndustrialWidgets {


LedIndicator::LedIndicator(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_offColor = calculateDefaultOffColor(m_onColor);

    connect(&m_blinkTimer, &QTimer::timeout, this, &LedIndicator::onBlinkTimeout);
}

LedIndicator::LedIndicator(const QColor &onColor, QWidget *parent)
    : LedIndicator(parent)
{
    setOnColor(onColor);
}

QSize LedIndicator::sizeHint() const
{
    if (!m_labelText.isEmpty()) {
        QFontMetrics fm(font());
        int textW = fm.horizontalAdvance(m_labelText);
        int textH = fm.height();
        return QSize(28 + 8 + textW + 4, std::max(28, textH + 4));
    }
    return QSize(28, 28);
}

QSize LedIndicator::minimumSizeHint() const
{
    return QSize(16, 16);
}

QColor LedIndicator::calculateDefaultOffColor(const QColor &onCol) const
{
    // Deep dark tint corresponding to the unlit LED dye
    return QColor(onCol.red() / 6, onCol.green() / 6, onCol.blue() / 6, 255);
}

void LedIndicator::setOn(bool on)
{
    if (m_on == on) return;
    m_on = on;
    Q_EMIT stateChanged(m_on);
    update();
}

void LedIndicator::setOff()
{
    setOn(false);
}

void LedIndicator::toggle()
{
    setOn(!m_on);
}

void LedIndicator::setBlinking(bool blinking)
{
    if (m_blinking == blinking) return;
    m_blinking = blinking;

    if (m_blinking) {
        m_blinkState = true;
        m_blinkTimer.start(m_blinkRateMs);
    } else {
        m_blinkTimer.stop();
        m_blinkState = true;
    }

    Q_EMIT blinkingChanged(m_blinking);
    update();
}

void LedIndicator::setBlinkRateMs(int rateMs)
{
    int rate = std::max(50, rateMs);
    if (m_blinkRateMs == rate) return;
    m_blinkRateMs = rate;
    if (m_blinking) {
        m_blinkTimer.start(m_blinkRateMs);
    }
    Q_EMIT appearanceChanged();
}

void LedIndicator::onBlinkTimeout()
{
    m_blinkState = !m_blinkState;
    update();
}

void LedIndicator::setOnColor(const QColor &color)
{
    if (m_onColor == color) return;
    m_onColor = color;
    m_offColor = calculateDefaultOffColor(m_onColor);
    Q_EMIT appearanceChanged();
    update();
}

void LedIndicator::setOffColor(const QColor &color)
{
    if (m_offColor == color) return;
    m_offColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void LedIndicator::setBezelColor(const QColor &color)
{
    if (m_bezelColor == color) return;
    m_bezelColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void LedIndicator::setBezelVisible(bool visible)
{
    if (m_bezelVisible == visible) return;
    m_bezelVisible = visible;
    Q_EMIT appearanceChanged();
    update();
}

void LedIndicator::setGlowEffect(bool glow)
{
    if (m_glowEffect == glow) return;
    m_glowEffect = glow;
    Q_EMIT appearanceChanged();
    update();
}

void LedIndicator::setShape(LedShape shape)
{
    if (m_shape == shape) return;
    m_shape = shape;
    Q_EMIT appearanceChanged();
    update();
}

void LedIndicator::setLabelText(const QString &text)
{
    if (m_labelText == text) return;
    m_labelText = text;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void LedIndicator::setClickable(bool clickable)
{
    if (m_clickable == clickable) return;
    m_clickable = clickable;
    setCursor(m_clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    Q_EMIT appearanceChanged();
}

void LedIndicator::mousePressEvent(QMouseEvent *event)
{
    if (m_clickable && event->button() == Qt::LeftButton) {
        toggle();
        Q_EMIT clicked();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void LedIndicator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const bool isLit = m_on && (!m_blinking || m_blinkState);
    const double w = width();
    const double h = height();

    // Determine LED bounding box (leave room for text if present)
    QRectF ledRect;
    QRectF textRect;

    if (!m_labelText.isEmpty()) {
        double ledSize = std::min(h - 4.0, 36.0);
        ledRect = QRectF(2.0, (h - ledSize) * 0.5, ledSize, ledSize);
        textRect = QRectF(ledRect.right() + 8.0, 0.0, w - ledRect.right() - 8.0, h);
    } else {
        double side = std::min(w, h) - 4.0;
        ledRect = QRectF((w - side) * 0.5, (h - side) * 0.5, side, side);
    }

    if (ledRect.width() <= 4.0 || ledRect.height() <= 4.0) {
        return;
    }

    const QPointF center = ledRect.center();
    const double radius = ledRect.width() * 0.5;

    // 1. Draw Optional Glow Halo (when lit)
    if (isLit && m_glowEffect) {
        QRadialGradient glowGrad(center, radius * 1.35);
        QColor glowCol = m_onColor;
        glowCol.setAlpha(120);
        glowGrad.setColorAt(0.0, glowCol);
        glowCol.setAlpha(40);
        glowGrad.setColorAt(0.7, glowCol);
        glowCol.setAlpha(0);
        glowGrad.setColorAt(1.0, glowCol);

        painter.setPen(Qt::NoPen);
        painter.setBrush(glowGrad);
        if (m_shape == LedShape::Circular) {
            painter.drawEllipse(center, radius * 1.35, radius * 1.35);
        } else {
            painter.drawRoundedRect(ledRect.adjusted(-3.0, -3.0, 3.0, 3.0), 6.0, 6.0);
        }
    }

    // 2. Outer Bezel (Machined aluminum/metal ring)
    QRectF lensRect = ledRect;
    double lensRadius = radius;

    if (m_bezelVisible) {
        if (m_shape == LedShape::Circular) {
            QRadialGradient bezelGrad(center, radius);
            bezelGrad.setColorAt(0.0, m_bezelColor.lighter(130));
            bezelGrad.setColorAt(0.85, m_bezelColor);
            bezelGrad.setColorAt(1.0, m_bezelColor.darker(160));

            painter.setPen(QPen(m_bezelColor.darker(180), 1.0));
            painter.setBrush(bezelGrad);
            painter.drawEllipse(center, radius, radius);

            // Inner dark recess ring
            double recessR = radius * 0.85;
            painter.setPen(QPen(QColor(0, 0, 0, 160), 1.0));
            painter.setBrush(QColor(10, 12, 16));
            painter.drawEllipse(center, recessR, recessR);

            lensRadius = radius * 0.78;
            lensRect = QRectF(center.x() - lensRadius, center.y() - lensRadius,
                              lensRadius * 2.0, lensRadius * 2.0);
        } else {
            // Rectangular Bezel
            QLinearGradient bezelGrad(ledRect.topLeft(), ledRect.bottomRight());
            bezelGrad.setColorAt(0.0, m_bezelColor.lighter(130));
            bezelGrad.setColorAt(1.0, m_bezelColor.darker(150));

            painter.setPen(QPen(m_bezelColor.darker(180), 1.2));
            painter.setBrush(bezelGrad);
            painter.drawRoundedRect(ledRect, 4.0, 4.0);

            lensRect = ledRect.adjusted(3.0, 3.0, -3.0, -3.0);
        }
    }

    // 3. 3D Lens Core
    if (m_shape == LedShape::Circular) {
        QPointF focalPoint(center.x() - lensRadius * 0.25, center.y() - lensRadius * 0.25);
        QRadialGradient lensGrad(center, lensRadius, focalPoint);

        if (isLit) {
            lensGrad.setColorAt(0.0, m_onColor.lighter(170));
            lensGrad.setColorAt(0.5, m_onColor);
            lensGrad.setColorAt(0.9, m_onColor.darker(130));
            lensGrad.setColorAt(1.0, m_onColor.darker(170));
        } else {
            lensGrad.setColorAt(0.0, m_offColor.lighter(130));
            lensGrad.setColorAt(0.6, m_offColor);
            lensGrad.setColorAt(1.0, m_offColor.darker(170));
        }

        painter.setPen(QPen(isLit ? m_onColor.darker(140) : m_offColor.darker(180), 0.8));
        painter.setBrush(lensGrad);
        painter.drawEllipse(center, lensRadius, lensRadius);

        // Specular Reflection (Curved glass dome glare)
        QRectF glareRect(center.x() - lensRadius * 0.65,
                         center.y() - lensRadius * 0.75,
                         lensRadius * 1.3, lensRadius * 0.75);

        QLinearGradient glareGrad(glareRect.topLeft(), glareRect.bottomLeft());
        glareGrad.setColorAt(0.0, QColor(255, 255, 255, isLit ? 220 : 130));
        glareGrad.setColorAt(0.6, QColor(255, 255, 255, isLit ? 80 : 30));
        glareGrad.setColorAt(1.0, QColor(255, 255, 255, 0));

        painter.setPen(Qt::NoPen);
        painter.setBrush(glareGrad);
        painter.drawEllipse(glareRect);

        // Bottom ambient bounce crescent
        QRectF bounceRect(center.x() - lensRadius * 0.45,
                          center.y() + lensRadius * 0.35,
                          lensRadius * 0.9, lensRadius * 0.45);
        QLinearGradient bounceGrad(bounceRect.bottomLeft(), bounceRect.topLeft());
        bounceGrad.setColorAt(0.0, QColor(255, 255, 255, isLit ? 90 : 35));
        bounceGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setBrush(bounceGrad);
        painter.drawEllipse(bounceRect);

    } else {
        // Rectangular Lens
        QLinearGradient lensGrad(lensRect.topLeft(), lensRect.bottomRight());
        if (isLit) {
            lensGrad.setColorAt(0.0, m_onColor.lighter(150));
            lensGrad.setColorAt(0.5, m_onColor);
            lensGrad.setColorAt(1.0, m_onColor.darker(140));
        } else {
            lensGrad.setColorAt(0.0, m_offColor.lighter(120));
            lensGrad.setColorAt(1.0, m_offColor.darker(160));
        }

        painter.setPen(QPen(isLit ? m_onColor.darker(130) : m_offColor.darker(170), 0.8));
        painter.setBrush(lensGrad);
        painter.drawRoundedRect(lensRect, 3.0, 3.0);

        // Glare stripe
        QRectF glareRect(lensRect.left() + 2.0, lensRect.top() + 2.0,
                         lensRect.width() - 4.0, lensRect.height() * 0.38);
        QLinearGradient glareGrad(glareRect.topLeft(), glareRect.bottomLeft());
        glareGrad.setColorAt(0.0, QColor(255, 255, 255, isLit ? 190 : 100));
        glareGrad.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glareGrad);
        painter.drawRoundedRect(glareRect, 2.0, 2.0);
    }

    // 4. Draw Label Text (if present)
    if (!m_labelText.isEmpty()) {
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_labelText);
    }
}

} // namespace QtIndustrialWidgets
