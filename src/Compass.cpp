// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "QtIndustrialWidgets/Compass.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtCore/QtMath>
#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {

class CompassPrivate {
public:
    double m_heading{0.0};
    double m_targetHeading{0.0};
    Compass::DisplayMode m_displayMode{Compass::DisplayMode::HeadingUp};
    bool m_bugVisible{true};
    bool m_bugInteractive{true};
    bool m_lubberVisible{true};
    bool m_digitalVisible{true};
    bool m_isDraggingBug{false};

    // Styling colors
    QColor m_dialColor{QColor(20, 24, 32)};       // Deep naval slate
    QColor m_bezelColor{QColor(48, 56, 70)};      // Machined metallic rim
    QColor m_textColor{QColor(225, 231, 236)};    // Crisp readout text
    QColor m_cardinalColor{QColor(0, 229, 255)};  // Cyan / amber highlights for N/E/S/W
    QColor m_needleColor{QColor(235, 59, 90)};    // Vivid red North arrow
    QColor m_needleTailColor{QColor(160, 175, 195)}; // Slate South arrow
    QColor m_bugColor{QColor(254, 130, 40)};      // High-visibility orange bug
    QColor m_lubberColor{QColor(254, 211, 48)};   // Safety amber lubber line

    // Card cache pixmap
    QPixmap m_cardCache;
    bool m_cacheDirty{true};
};



Compass::Compass(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<CompassPrivate>())
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(minimumSizeHint());
}

Compass::~Compass() = default;

double Compass::heading() const { Q_D(const Compass); return d->m_heading; }
double Compass::targetHeading() const { Q_D(const Compass); return d->m_targetHeading; }
Compass::DisplayMode Compass::displayMode() const { Q_D(const Compass); return d->m_displayMode; }
bool Compass::isHeadingBugVisible() const { Q_D(const Compass); return d->m_bugVisible; }
bool Compass::isHeadingBugInteractive() const { Q_D(const Compass); return d->m_bugInteractive; }
bool Compass::isLubberLineVisible() const { Q_D(const Compass); return d->m_lubberVisible; }
bool Compass::isDigitalReadoutVisible() const { Q_D(const Compass); return d->m_digitalVisible; }

QColor Compass::dialColor() const { Q_D(const Compass); return d->m_dialColor; }
QColor Compass::bezelColor() const { Q_D(const Compass); return d->m_bezelColor; }
QColor Compass::textColor() const { Q_D(const Compass); return d->m_textColor; }
QColor Compass::cardinalColor() const { Q_D(const Compass); return d->m_cardinalColor; }
QColor Compass::needleColor() const { Q_D(const Compass); return d->m_needleColor; }
QColor Compass::needleTailColor() const { Q_D(const Compass); return d->m_needleTailColor; }
QColor Compass::bugColor() const { Q_D(const Compass); return d->m_bugColor; }
QColor Compass::lubberColor() const { Q_D(const Compass); return d->m_lubberColor; }



QSize Compass::sizeHint() const
{
    return {260, 260};
}

QSize Compass::minimumSizeHint() const
{
    return {110, 110};
}

double Compass::normalizeDegrees(double deg)
{
    double r = std::fmod(deg, 360.0);
    if (r < 0.0) {
        r += 360.0;
    }
    return r;
}

double Compass::courseDeviation() const
{
    double diff = std::fmod(d_ptr->m_heading - d_ptr->m_targetHeading + 540.0, 360.0) - 180.0;
    return diff;
}

void Compass::setHeading(double heading)
{
    double norm = normalizeDegrees(heading);
    if (std::abs(d_ptr->m_heading - norm) > 0.001) {
        d_ptr->m_heading = norm;
        Q_EMIT headingChanged(d_ptr->m_heading);
        update();
    }
}

void Compass::setTargetHeading(double target)
{
    double norm = normalizeDegrees(target);
    if (std::abs(d_ptr->m_targetHeading - norm) > 0.001) {
        d_ptr->m_targetHeading = norm;
        Q_EMIT targetHeadingChanged(d_ptr->m_targetHeading);
        update();
    }
}

void Compass::setDisplayMode(DisplayMode mode)
{
    if (d_ptr->m_displayMode != mode) {
        d_ptr->m_displayMode = mode;
        invalidateCache();
        Q_EMIT displayModeChanged(d_ptr->m_displayMode);
        update();
    }
}

void Compass::setHeadingBugVisible(bool visible)
{
    if (d_ptr->m_bugVisible != visible) {
        d_ptr->m_bugVisible = visible;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setHeadingBugInteractive(bool interactive)
{
    if (d_ptr->m_bugInteractive != interactive) {
        d_ptr->m_bugInteractive = interactive;
        Q_EMIT appearanceChanged();
    }
}

void Compass::setLubberLineVisible(bool visible)
{
    if (d_ptr->m_lubberVisible != visible) {
        d_ptr->m_lubberVisible = visible;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setDigitalReadoutVisible(bool visible)
{
    if (d_ptr->m_digitalVisible != visible) {
        d_ptr->m_digitalVisible = visible;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setDialColor(const QColor &color)
{
    if (d_ptr->m_dialColor != color) {
        d_ptr->m_dialColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setBezelColor(const QColor &color)
{
    if (d_ptr->m_bezelColor != color) {
        d_ptr->m_bezelColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor != color) {
        d_ptr->m_textColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setCardinalColor(const QColor &color)
{
    if (d_ptr->m_cardinalColor != color) {
        d_ptr->m_cardinalColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setNeedleColor(const QColor &color)
{
    if (d_ptr->m_needleColor != color) {
        d_ptr->m_needleColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setNeedleTailColor(const QColor &color)
{
    if (d_ptr->m_needleTailColor != color) {
        d_ptr->m_needleTailColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setBugColor(const QColor &color)
{
    if (d_ptr->m_bugColor != color) {
        d_ptr->m_bugColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::setLubberColor(const QColor &color)
{
    if (d_ptr->m_lubberColor != color) {
        d_ptr->m_lubberColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void Compass::invalidateCache()
{
    d_ptr->m_cacheDirty = true;
}

void Compass::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void Compass::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        invalidateCache();
        update();
    }
}

double Compass::angleFromPoint(const QPointF &pos) const
{
    QPointF center = rect().center();
    double dx = pos.x() - center.x();
    double dy = pos.y() - center.y();

    // In screen coordinates: Y is down, X is right.
    // 12 o'clock is (dx=0, dy<0), angle = 0°.
    // Clockwise: 3 o'clock is 90°, 6 o'clock is 180°, 9 o'clock is 270°.
    double rad = std::atan2(dx, -dy);
    double deg = qRadiansToDegrees(rad);
    return normalizeDegrees(deg);
}

void Compass::mousePressEvent(QMouseEvent *event)
{
    if (d_ptr->m_bugInteractive && event->button() == Qt::LeftButton) {
        double r = std::min(width(), height()) / 2.0;
        QPointF center = rect().center();
        double dist = std::hypot(event->position().x() - center.x(), event->position().y() - center.y());
        if (dist > r * 0.35 && dist <= r * 1.1) {
            d_ptr->m_isDraggingBug = true;
            double angle = angleFromPoint(event->position());
            if (d_ptr->m_displayMode == DisplayMode::HeadingUp) {
                // In HeadingUp mode, dial is rotated by -d_ptr->m_heading, so cursor angle corresponds to:
                setTargetHeading(normalizeDegrees(angle + d_ptr->m_heading));
            } else {
                setTargetHeading(angle);
            }
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void Compass::mouseMoveEvent(QMouseEvent *event)
{
    if (d_ptr->m_isDraggingBug && (event->buttons() & Qt::LeftButton)) {
        double angle = angleFromPoint(event->position());
        if (d_ptr->m_displayMode == DisplayMode::HeadingUp) {
            setTargetHeading(normalizeDegrees(angle + d_ptr->m_heading));
        } else {
            setTargetHeading(angle);
        }
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void Compass::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && d_ptr->m_isDraggingBug) {
        d_ptr->m_isDraggingBug = false;
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void Compass::renderCompassCard(const QSize &size)
{
    const qreal dpr = devicePixelRatioF();
    d_ptr->m_cardCache = QPixmap(size * dpr);
    d_ptr->m_cardCache.setDevicePixelRatio(dpr);
    d_ptr->m_cardCache.fill(Qt::transparent);

    QPainter p(&d_ptr->m_cardCache);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const double side = std::min(size.width(), size.height());
    const QPointF center(size.width() / 2.0, size.height() / 2.0);
    const double radius = (side / 2.0) - 6.0;

    if (radius <= 10.0) {
        d_ptr->m_cacheDirty = false;
        return;
    }

    // 1. Outer Bezel Ring (Machined metal chamfer)
    QRadialGradient bezelGrad(center, radius + 5.0, center - QPointF(radius * 0.3, radius * 0.3));
    bezelGrad.setColorAt(0.0, d_ptr->m_bezelColor.lighter(140));
    bezelGrad.setColorAt(0.7, d_ptr->m_bezelColor);
    bezelGrad.setColorAt(1.0, d_ptr->m_bezelColor.darker(170));

    p.setPen(QPen(d_ptr->m_bezelColor.darker(200), 1.5));
    p.setBrush(bezelGrad);
    p.drawEllipse(center, radius + 5.0, radius + 5.0);

    // Bezel inner groove
    p.setPen(QPen(QColor(15, 18, 25), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, radius, radius);

    // 2. Dial Face Background
    QRadialGradient dialGrad(center, radius, center - QPointF(0.0, radius * 0.25));
    dialGrad.setColorAt(0.0, d_ptr->m_dialColor.lighter(125));
    dialGrad.setColorAt(0.8, d_ptr->m_dialColor);
    dialGrad.setColorAt(1.0, d_ptr->m_dialColor.darker(140));

    p.setPen(Qt::NoPen);
    p.setBrush(dialGrad);
    p.drawEllipse(center, radius - 1.0, radius - 1.0);

    // 3. Central Decorative Nautical Star (8-point compass rose watermark)
    p.save();
    p.translate(center);
    const double starRadius = radius * 0.32;
    for (int pt = 0; pt < 8; ++pt) {
        p.save();
        p.rotate(pt * 45.0);

        QPainterPath lightHalf;
        lightHalf.moveTo(0.0, 0.0);
        lightHalf.lineTo(0.0, -starRadius);
        lightHalf.lineTo(starRadius * 0.25, -starRadius * 0.3);
        lightHalf.closeSubpath();

        QPainterPath darkHalf;
        darkHalf.moveTo(0.0, 0.0);
        darkHalf.lineTo(0.0, -starRadius);
        darkHalf.lineTo(-starRadius * 0.25, -starRadius * 0.3);
        darkHalf.closeSubpath();

        QColor baseC = (pt % 2 == 0) ? d_ptr->m_cardinalColor : d_ptr->m_textColor;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(baseC.red(), baseC.green(), baseC.blue(), 38));
        p.drawPath(lightHalf);

        p.setBrush(QColor(baseC.red(), baseC.green(), baseC.blue(), 18));
        p.drawPath(darkHalf);

        p.restore();
    }
    p.restore();

    // 4. Dial Graduation Scale & Tick Marks
    p.save();
    p.translate(center);

    const double outerTickR = radius - 3.0;
    const double minorTickR = radius - 3.0 - (radius * 0.045);
    const double medTickR   = radius - 3.0 - (radius * 0.08);
    const double majorTickR = radius - 3.0 - (radius * 0.12);
    const double textR      = radius - 3.0 - (radius * 0.22);

    QFont cardinalFont = font();
    cardinalFont.setBold(true);
    cardinalFont.setPointSizeF(std::clamp(radius * 0.13, 8.0, 22.0));

    QFont intercardinalFont = font();
    intercardinalFont.setBold(true);
    intercardinalFont.setPointSizeF(std::clamp(radius * 0.08, 6.5, 14.0));

    QFont numberFont = font();
    numberFont.setBold(false);
    numberFont.setPointSizeF(std::clamp(radius * 0.07, 6.0, 12.0));

    // Outer track circle line
    p.setPen(QPen(QColor(d_ptr->m_textColor.red(), d_ptr->m_textColor.green(), d_ptr->m_textColor.blue(), 60), 1.0));
    p.drawEllipse(QPointF(0.0, 0.0), outerTickR, outerTickR);
    p.drawEllipse(QPointF(0.0, 0.0), majorTickR, majorTickR);

    for (int deg = 0; deg < 360; ++deg) {
        if (deg % 5 != 0) continue;

        p.save();
        p.rotate(deg);

        if (deg % 30 == 0) {
            // Major tick mark (every 30°)
            p.setPen(QPen(d_ptr->m_textColor, 1.8));
            p.drawLine(QPointF(0.0, -outerTickR), QPointF(0.0, -majorTickR));
        } else if (deg % 10 == 0) {
            // Medium tick mark (every 10°)
            p.setPen(QPen(QColor(d_ptr->m_textColor.red(), d_ptr->m_textColor.green(), d_ptr->m_textColor.blue(), 180), 1.2));
            p.drawLine(QPointF(0.0, -outerTickR), QPointF(0.0, -medTickR));
        } else {
            // Minor tick mark (every 5°)
            p.setPen(QPen(QColor(d_ptr->m_textColor.red(), d_ptr->m_textColor.green(), d_ptr->m_textColor.blue(), 100), 0.8));
            p.drawLine(QPointF(0.0, -outerTickR), QPointF(0.0, -minorTickR));
        }

        // Labels
        if (deg == 0) {
            // North Cardinal
            p.setFont(cardinalFont);
            p.setPen(d_ptr->m_needleColor);
            QRectF textRect(-24.0, -textR - 12.0, 48.0, 24.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("N"));
        } else if (deg == 90) {
            p.setFont(cardinalFont);
            p.setPen(d_ptr->m_cardinalColor);
            QRectF textRect(-24.0, -textR - 12.0, 48.0, 24.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("E"));
        } else if (deg == 180) {
            p.setFont(cardinalFont);
            p.setPen(d_ptr->m_cardinalColor);
            QRectF textRect(-24.0, -textR - 12.0, 48.0, 24.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("S"));
        } else if (deg == 270) {
            p.setFont(cardinalFont);
            p.setPen(d_ptr->m_cardinalColor);
            QRectF textRect(-24.0, -textR - 12.0, 48.0, 24.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("W"));
        } else if (deg == 45) {
            p.setFont(intercardinalFont);
            p.setPen(d_ptr->m_textColor);
            QRectF textRect(-20.0, -textR - 10.0, 40.0, 20.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("NE"));
        } else if (deg == 135) {
            p.setFont(intercardinalFont);
            p.setPen(d_ptr->m_textColor);
            QRectF textRect(-20.0, -textR - 10.0, 40.0, 20.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("SE"));
        } else if (deg == 225) {
            p.setFont(intercardinalFont);
            p.setPen(d_ptr->m_textColor);
            QRectF textRect(-20.0, -textR - 10.0, 40.0, 20.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("SW"));
        } else if (deg == 315) {
            p.setFont(intercardinalFont);
            p.setPen(d_ptr->m_textColor);
            QRectF textRect(-20.0, -textR - 10.0, 40.0, 20.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("NW"));
        } else if (deg % 30 == 0) {
            // Multiples of 30°: 3-digit aeronautical/marine format: 030, 060, 120, etc.
            p.setFont(numberFont);
            p.setPen(d_ptr->m_textColor);
            QRectF textRect(-22.0, -textR - 9.0, 44.0, 18.0);
            p.drawText(textRect, Qt::AlignCenter, QStringLiteral("%1").arg(deg, 3, 10, QLatin1Char('0')));
        }

        p.restore();
    }
    p.restore();

    d_ptr->m_cacheDirty = false;
}

void Compass::paintEvent(QPaintEvent *)
{
    const int side = std::min(width(), height());
    if (side <= 10) return;

    if (d_ptr->m_cacheDirty || d_ptr->m_cardCache.isNull() || d_ptr->m_cardCache.size() != size() * devicePixelRatioF()) {
        renderCompassCard(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPointF center = rect().center();
    const double radius = (side / 2.0) - 6.0;

    // 1. Draw Compass Card
    if (d_ptr->m_displayMode == DisplayMode::HeadingUp) {
        // HeadingUp: Card rotates by -d_ptr->m_heading around center
        painter.save();
        painter.translate(center);
        painter.rotate(-d_ptr->m_heading);
        painter.drawPixmap(-width() / 2, -height() / 2, d_ptr->m_cardCache);

        // Draw Target Heading Bug rotated on the moving card:
        if (d_ptr->m_bugVisible) {
            painter.save();
            painter.rotate(d_ptr->m_targetHeading);
            // Draw Heading Bug Chevron at top
            QPainterPath bug;
            const double bugR = radius - 2.0;
            const double bugW = std::clamp(radius * 0.12, 8.0, 18.0);
            const double bugH = std::clamp(radius * 0.10, 6.0, 15.0);

            bug.moveTo(-bugW, -bugR);
            bug.lineTo(-bugW * 0.6, -bugR + bugH);
            bug.lineTo(0.0, -bugR + bugH * 0.4);
            bug.lineTo(bugW * 0.6, -bugR + bugH);
            bug.lineTo(bugW, -bugR);
            bug.lineTo(0.0, -bugR - 2.0);
            bug.closeSubpath();

            painter.setPen(QPen(d_ptr->m_bugColor.darker(140), 1.2));
            painter.setBrush(d_ptr->m_bugColor);
            painter.drawPath(bug);
            painter.restore();
        }

        painter.restore();

        // 2. Static Lubber Line at 12 o'clock (Fixed vessel bow reference)
        if (d_ptr->m_lubberVisible) {
            painter.save();
            painter.translate(center);

            const double lubberR = radius + 2.0;
            const double lubberW = std::clamp(radius * 0.08, 6.0, 14.0);
            const double lubberH = std::clamp(radius * 0.12, 8.0, 18.0);

            QPainterPath lubber;
            lubber.moveTo(0.0, -lubberR + lubberH);
            lubber.lineTo(-lubberW / 2.0, -lubberR);
            lubber.lineTo(lubberW / 2.0, -lubberR);
            lubber.closeSubpath();

            painter.setPen(QPen(QColor(20, 20, 20), 1.0));
            painter.setBrush(d_ptr->m_lubberColor);
            painter.drawPath(lubber);

            // Center lubber vertical pointer line
            painter.setPen(QPen(d_ptr->m_lubberColor, 2.0));
            painter.drawLine(QPointF(0.0, -lubberR), QPointF(0.0, -lubberR + radius * 0.16));

            painter.restore();
        }
    } else {
        // NorthUp: Card is static (North at top)
        painter.save();
        painter.translate(center);
        painter.drawPixmap(-width() / 2, -height() / 2, d_ptr->m_cardCache);

        // Draw Target Heading Bug rotated to targetHeading
        if (d_ptr->m_bugVisible) {
            painter.save();
            painter.rotate(d_ptr->m_targetHeading);
            const double bugR = radius - 2.0;
            const double bugW = std::clamp(radius * 0.12, 8.0, 18.0);
            const double bugH = std::clamp(radius * 0.10, 6.0, 15.0);

            QPainterPath bug;
            bug.moveTo(-bugW, -bugR);
            bug.lineTo(-bugW * 0.6, -bugR + bugH);
            bug.lineTo(0.0, -bugR + bugH * 0.4);
            bug.lineTo(bugW * 0.6, -bugR + bugH);
            bug.lineTo(bugW, -bugR);
            bug.lineTo(0.0, -bugR - 2.0);
            bug.closeSubpath();

            painter.setPen(QPen(d_ptr->m_bugColor.darker(140), 1.2));
            painter.setBrush(d_ptr->m_bugColor);
            painter.drawPath(bug);
            painter.restore();
        }

        // Draw Rotating Magnetic Needle pointing to d_ptr->m_heading
        painter.save();
        painter.rotate(d_ptr->m_heading);

        const double needleLen = radius * 0.68;
        const double needleW = std::clamp(radius * 0.08, 6.0, 16.0);

        // North half (Vivid red arrow)
        QPainterPath northHalfLight;
        northHalfLight.moveTo(0.0, 0.0);
        northHalfLight.lineTo(0.0, -needleLen);
        northHalfLight.lineTo(needleW / 2.0, -needleLen * 0.2);
        northHalfLight.closeSubpath();

        QPainterPath northHalfDark;
        northHalfDark.moveTo(0.0, 0.0);
        northHalfDark.lineTo(0.0, -needleLen);
        northHalfDark.lineTo(-needleW / 2.0, -needleLen * 0.2);
        northHalfDark.closeSubpath();

        painter.setPen(Qt::NoPen);
        painter.setBrush(d_ptr->m_needleColor);
        painter.drawPath(northHalfLight);
        painter.setBrush(d_ptr->m_needleColor.darker(140));
        painter.drawPath(northHalfDark);

        // South half (Slate / silver tail arrow)
        QPainterPath southHalfLight;
        southHalfLight.moveTo(0.0, 0.0);
        southHalfLight.lineTo(0.0, needleLen * 0.85);
        southHalfLight.lineTo(needleW / 2.0, needleLen * 0.2);
        southHalfLight.closeSubpath();

        QPainterPath southHalfDark;
        southHalfDark.moveTo(0.0, 0.0);
        southHalfDark.lineTo(0.0, needleLen * 0.85);
        southHalfDark.lineTo(-needleW / 2.0, needleLen * 0.2);
        southHalfDark.closeSubpath();

        painter.setBrush(d_ptr->m_needleTailColor);
        painter.drawPath(southHalfLight);
        painter.setBrush(d_ptr->m_needleTailColor.darker(140));
        painter.drawPath(southHalfDark);

        painter.restore();

        // Lubber reference line at 12 o'clock
        if (d_ptr->m_lubberVisible) {
            painter.save();
            painter.translate(center);
            const double lubberR = radius + 2.0;
            const double lubberW = std::clamp(radius * 0.08, 6.0, 14.0);
            const double lubberH = std::clamp(radius * 0.12, 8.0, 18.0);

            QPainterPath lubber;
            lubber.moveTo(0.0, -lubberR + lubberH);
            lubber.lineTo(-lubberW / 2.0, -lubberR);
            lubber.lineTo(lubberW / 2.0, -lubberR);
            lubber.closeSubpath();

            painter.setPen(QPen(QColor(20, 20, 20), 1.0));
            painter.setBrush(d_ptr->m_lubberColor);
            painter.drawPath(lubber);
            painter.restore();
        }

        painter.restore();
    }

    // 3. Central Digital Readout Pod
    if (d_ptr->m_digitalVisible) {
        painter.save();
        painter.translate(center);

        const double podW = std::clamp(radius * 0.52, 48.0, 110.0);
        const double podH = std::clamp(radius * 0.26, 24.0, 50.0);
        QRectF podRect(-podW / 2.0, -podH / 2.0, podW, podH);

        // Recessed bevel pod background
        QLinearGradient podGrad(podRect.topLeft(), podRect.bottomLeft());
        podGrad.setColorAt(0.0, QColor(14, 18, 24));
        podGrad.setColorAt(1.0, QColor(24, 30, 40));

        painter.setPen(QPen(d_ptr->m_bezelColor.darker(150), 1.5));
        painter.setBrush(podGrad);
        painter.drawRoundedRect(podRect, 4.0, 4.0);

        // Digital Heading Readout text (e.g. "045°")
        QFont podFont = font();
        podFont.setBold(true);
        podFont.setPointSizeF(std::clamp(podH * 0.48, 8.0, 20.0));
        painter.setFont(podFont);
        painter.setPen(d_ptr->m_textColor);

        int intHdg = static_cast<int>(std::round(d_ptr->m_heading)) % 360;
        QString text = QStringLiteral("%1°").arg(intHdg, 3, 10, QLatin1Char('0'));
        painter.drawText(podRect, Qt::AlignCenter, text);

        painter.restore();
    }
}

} // namespace QtIndustrialWidgets
