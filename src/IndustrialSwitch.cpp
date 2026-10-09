// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include <QtCore/QVariantAnimation>
#include <QtCore/QEasingCurve>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QRadialGradient>
#include <QtGui/QLinearGradient>
#include <cmath>
#include <algorithm>

namespace QtIndustrialWidgets {


class IndustrialSwitchPrivate {
public:
    IndustrialSwitch::SwitchType m_switchType = IndustrialSwitch::SwitchType::ToggleLever;
    int m_positionCount = 2; // 2 or 3
    int m_position = 0;      // 0, 1 (or 2 if 3-pos)
    double m_currentPos = 0.0; // for animation: 0.0 to 1.0 (or 2.0)
    Qt::Orientation m_orientation = Qt::Vertical;

    bool m_hasSafetyGuard = false;
    bool m_isGuardOpen = false;
    double m_guardOpenFactor = 0.0; // 0.0 = closed, 1.0 = fully open
    bool m_animated = true;
    bool m_hasLed = true;

    QString m_label;
    QString m_labelOff = QStringLiteral("OFF");
    QString m_labelOn = QStringLiteral("ON");
    QString m_labelCenter = QStringLiteral("AUTO");

    QColor m_plateColor = QColor(42, 45, 52);
    QColor m_leverColor = QColor(220, 225, 230);
    QColor m_ledColor = QColor(46, 204, 113);
    QColor m_textColor = QColor(200, 205, 215);
    QColor m_guardColor = QColor(220, 53, 69); // Industrial crimson safety red

    QVariantAnimation *m_switchAnim = nullptr;
    QVariantAnimation *m_guardAnim = nullptr;

    QPixmap m_cachedBackground;
    bool m_cacheValid = false;
    bool m_isDragging = false;
};

IndustrialSwitch::IndustrialSwitch(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<IndustrialSwitchPrivate>())
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover, true);

    d_ptr->m_switchAnim = new QVariantAnimation(this);
    d_ptr->m_switchAnim->setDuration(130);
    d_ptr->m_switchAnim->setEasingCurve(QEasingCurve::OutBack);
    connect(d_ptr->m_switchAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        d_ptr->m_currentPos = val.toDouble();
        update();
    });

    d_ptr->m_guardAnim = new QVariantAnimation(this);
    d_ptr->m_guardAnim->setDuration(180);
    d_ptr->m_guardAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(d_ptr->m_guardAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
        d_ptr->m_guardOpenFactor = val.toDouble();
        update();
    });
}

IndustrialSwitch::~IndustrialSwitch() = default;

IndustrialSwitch::SwitchType IndustrialSwitch::switchType() const { Q_D(const IndustrialSwitch); return d->m_switchType; }
int IndustrialSwitch::positionCount() const { Q_D(const IndustrialSwitch); return d->m_positionCount; }
int IndustrialSwitch::position() const { Q_D(const IndustrialSwitch); return d->m_position; }
bool IndustrialSwitch::isChecked() const { Q_D(const IndustrialSwitch); return d->m_position == (d->m_positionCount - 1); }
Qt::Orientation IndustrialSwitch::orientation() const { Q_D(const IndustrialSwitch); return d->m_orientation; }
bool IndustrialSwitch::hasSafetyGuard() const { Q_D(const IndustrialSwitch); return d->m_hasSafetyGuard; }
bool IndustrialSwitch::isGuardOpen() const { Q_D(const IndustrialSwitch); return d->m_isGuardOpen; }
bool IndustrialSwitch::isAnimated() const { Q_D(const IndustrialSwitch); return d->m_animated; }
bool IndustrialSwitch::hasLed() const { Q_D(const IndustrialSwitch); return d->m_hasLed; }
QString IndustrialSwitch::label() const { Q_D(const IndustrialSwitch); return d->m_label; }
QString IndustrialSwitch::labelOff() const { Q_D(const IndustrialSwitch); return d->m_labelOff; }
QString IndustrialSwitch::labelOn() const { Q_D(const IndustrialSwitch); return d->m_labelOn; }
QString IndustrialSwitch::labelCenter() const { Q_D(const IndustrialSwitch); return d->m_labelCenter; }
QColor IndustrialSwitch::plateColor() const { Q_D(const IndustrialSwitch); return d->m_plateColor; }
QColor IndustrialSwitch::leverColor() const { Q_D(const IndustrialSwitch); return d->m_leverColor; }
QColor IndustrialSwitch::ledColor() const { Q_D(const IndustrialSwitch); return d->m_ledColor; }
QColor IndustrialSwitch::textColor() const { Q_D(const IndustrialSwitch); return d->m_textColor; }
QColor IndustrialSwitch::guardColor() const { Q_D(const IndustrialSwitch); return d->m_guardColor; }


QSize IndustrialSwitch::sizeHint() const
{
    if (d_ptr->m_orientation == Qt::Vertical) {
        return {75, 125};
    }
    return {125, 75};
}

QSize IndustrialSwitch::minimumSizeHint() const
{
    if (d_ptr->m_orientation == Qt::Vertical) {
        return {50, 80};
    }
    return {80, 50};
}

void IndustrialSwitch::setSwitchType(SwitchType type)
{
    if (d_ptr->m_switchType == type) return;
    d_ptr->m_switchType = type;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setPositionCount(int count)
{
    count = std::clamp(count, 2, 3);
    if (d_ptr->m_positionCount == count) return;
    d_ptr->m_positionCount = count;
    if (d_ptr->m_position >= d_ptr->m_positionCount) {
        d_ptr->m_position = d_ptr->m_positionCount - 1;
        d_ptr->m_currentPos = static_cast<double>(d_ptr->m_position);
    }
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setPosition(int position)
{
    position = std::clamp(position, 0, d_ptr->m_positionCount - 1);
    if (d_ptr->m_position == position) return;

    d_ptr->m_position = position;

    if (d_ptr->m_animated) {
        d_ptr->m_switchAnim->stop();
        d_ptr->m_switchAnim->setStartValue(d_ptr->m_currentPos);
        d_ptr->m_switchAnim->setEndValue(static_cast<double>(d_ptr->m_position));
        d_ptr->m_switchAnim->start();
    } else {
        d_ptr->m_currentPos = static_cast<double>(d_ptr->m_position);
        update();
    }

    Q_EMIT positionChanged(d_ptr->m_position);
    Q_EMIT toggled(isChecked());
}

void IndustrialSwitch::setChecked(bool checked)
{
    int target = checked ? (d_ptr->m_positionCount - 1) : 0;
    setPosition(target);
}

void IndustrialSwitch::toggle()
{
    if (d_ptr->m_positionCount == 2) {
        setPosition(d_ptr->m_position == 0 ? 1 : 0);
    } else {
        // 3-position toggle sequence: 0 -> 1 -> 2 -> 1 -> 0
        int next = (d_ptr->m_position + 1) % d_ptr->m_positionCount;
        setPosition(next);
    }
}

void IndustrialSwitch::setOrientation(Qt::Orientation orientation)
{
    if (d_ptr->m_orientation == orientation) return;
    d_ptr->m_orientation = orientation;
    d_ptr->m_cacheValid = false;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setHasSafetyGuard(bool guard)
{
    if (d_ptr->m_hasSafetyGuard == guard) return;
    d_ptr->m_hasSafetyGuard = guard;
    d_ptr->m_isGuardOpen = false;
    d_ptr->m_guardOpenFactor = 0.0;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setGuardOpen(bool open)
{
    if (d_ptr->m_isGuardOpen == open) return;
    d_ptr->m_isGuardOpen = open;

    if (d_ptr->m_animated) {
        d_ptr->m_guardAnim->stop();
        d_ptr->m_guardAnim->setStartValue(d_ptr->m_guardOpenFactor);
        d_ptr->m_guardAnim->setEndValue(open ? 1.0 : 0.0);
        d_ptr->m_guardAnim->start();
    } else {
        d_ptr->m_guardOpenFactor = open ? 1.0 : 0.0;
        update();
    }

    Q_EMIT guardToggled(d_ptr->m_isGuardOpen);
}

void IndustrialSwitch::setAnimated(bool animated)
{
    if (d_ptr->m_animated == animated) return;
    d_ptr->m_animated = animated;
    Q_EMIT appearanceChanged();
}

void IndustrialSwitch::setHasLed(bool hasLed)
{
    if (d_ptr->m_hasLed == hasLed) return;
    d_ptr->m_hasLed = hasLed;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setLabel(const QString &label)
{
    if (d_ptr->m_label == label) return;
    d_ptr->m_label = label;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setLabelOff(const QString &label)
{
    if (d_ptr->m_labelOff == label) return;
    d_ptr->m_labelOff = label;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setLabelOn(const QString &label)
{
    if (d_ptr->m_labelOn == label) return;
    d_ptr->m_labelOn = label;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setLabelCenter(const QString &label)
{
    if (d_ptr->m_labelCenter == label) return;
    d_ptr->m_labelCenter = label;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setPlateColor(const QColor &color)
{
    if (d_ptr->m_plateColor == color) return;
    d_ptr->m_plateColor = color;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setLeverColor(const QColor &color)
{
    if (d_ptr->m_leverColor == color) return;
    d_ptr->m_leverColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setLedColor(const QColor &color)
{
    if (d_ptr->m_ledColor == color) return;
    d_ptr->m_ledColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor == color) return;
    d_ptr->m_textColor = color;
    d_ptr->m_cacheValid = false;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::setGuardColor(const QColor &color)
{
    if (d_ptr->m_guardColor == color) return;
    d_ptr->m_guardColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSwitch::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    d_ptr->m_cacheValid = false;
}

QRectF IndustrialSwitch::calculateSwitchRect() const
{
    QRectF r = rect().adjusted(10, 10, -10, -10);
    double w = r.width();
    double h = r.height();

    if (d_ptr->m_orientation == Qt::Vertical) {
        double switchH = h * 0.55;
        double switchW = std::min(w * 0.7, switchH * 0.65);
        double top = r.top() + (h - switchH) * 0.52;
        double left = r.left() + (w - switchW) * 0.5;
        return {left, top, switchW, switchH};
    } else {
        double switchW = w * 0.55;
        double switchH = std::min(h * 0.7, switchW * 0.65);
        double left = r.left() + (w - switchW) * 0.52;
        double top = r.top() + (h - switchH) * 0.5;
        return {left, top, switchW, switchH};
    }
}

QRectF IndustrialSwitch::calculateGuardRect() const
{
    QRectF sw = calculateSwitchRect();
    return sw.adjusted(-8, -12, 8, 12);
}

void IndustrialSwitch::drawScrew(QPainter &painter, const QPointF &center, double radius)
{
    painter.save();
    painter.setPen(Qt::NoPen);

    // Screw body with bevel
    QRadialGradient grad(center - QPointF(radius * 0.3, radius * 0.3), radius);
    grad.setColorAt(0.0, QColor(220, 222, 225));
    grad.setColorAt(0.7, QColor(130, 135, 140));
    grad.setColorAt(1.0, QColor(60, 65, 70));
    painter.setBrush(grad);
    painter.drawEllipse(center, radius, radius);

    // Rim shadow
    painter.setPen(QPen(QColor(30, 32, 35, 180), 0.7));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);

    // Slot (Phillips or flat)
    painter.setPen(QPen(QColor(40, 42, 45, 230), radius * 0.35, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(center.x() - radius * 0.6, center.y()),
                     QPointF(center.x() + radius * 0.6, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - radius * 0.6),
                     QPointF(center.x(), center.y() + radius * 0.6));

    painter.restore();
}

void IndustrialSwitch::drawLed(QPainter &painter, const QPointF &center, double radius, bool active)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    // Outer bezel ring
    QRadialGradient bezelGrad(center - QPointF(radius * 0.3, radius * 0.3), radius * 1.5);
    bezelGrad.setColorAt(0.0, QColor(200, 205, 210));
    bezelGrad.setColorAt(0.6, QColor(100, 105, 110));
    bezelGrad.setColorAt(1.0, QColor(50, 52, 55));
    painter.setPen(Qt::NoPen);
    painter.setBrush(bezelGrad);
    painter.drawEllipse(center, radius * 1.35, radius * 1.35);

    // Lens
    QRadialGradient lensGrad(center - QPointF(radius * 0.25, radius * 0.25), radius);
    if (active) {
        // Halo glow
        QRadialGradient halo(center, radius * 2.5);
        QColor glow = d_ptr->m_ledColor;
        glow.setAlpha(120);
        halo.setColorAt(0.0, glow);
        glow.setAlpha(0);
        halo.setColorAt(1.0, glow);
        painter.setBrush(halo);
        painter.drawEllipse(center, radius * 2.5, radius * 2.5);

        lensGrad.setColorAt(0.0, QColor(255, 255, 255, 240));
        lensGrad.setColorAt(0.3, d_ptr->m_ledColor.lighter(130));
        lensGrad.setColorAt(0.8, d_ptr->m_ledColor);
        lensGrad.setColorAt(1.0, d_ptr->m_ledColor.darker(160));
    } else {
        lensGrad.setColorAt(0.0, d_ptr->m_ledColor.darker(280));
        lensGrad.setColorAt(0.7, d_ptr->m_ledColor.darker(350));
        lensGrad.setColorAt(1.0, QColor(25, 25, 25));
    }

    painter.setBrush(lensGrad);
    painter.drawEllipse(center, radius, radius);

    // Specular highlight
    QPainterPath highlight;
    highlight.addEllipse(center.x() - radius * 0.5, center.y() - radius * 0.6, radius * 0.6, radius * 0.35);
    painter.setBrush(QColor(255, 255, 255, active ? 160 : 70));
    painter.drawPath(highlight);

    painter.restore();
}

void IndustrialSwitch::renderStaticBackground()
{
    qreal dpr = devicePixelRatioF();
    QSize pixSize = size() * dpr;
    if (pixSize.isEmpty()) return;

    d_ptr->m_cachedBackground = QPixmap(pixSize);
    d_ptr->m_cachedBackground.setDevicePixelRatio(dpr);
    d_ptr->m_cachedBackground.fill(Qt::transparent);

    QPainter painter(&d_ptr->m_cachedBackground);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF plateRect = rect().adjusted(3, 3, -3, -3);
    double radius = 6.0;

    // Plate Outer Drop Shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(15, 15, 18, 160));
    painter.drawRoundedRect(plateRect.translated(1.5, 2.0), radius, radius);

    // Plate Body (Brushed dark industrial metal)
    QLinearGradient plateGrad(plateRect.topLeft(), plateRect.bottomRight());
    plateGrad.setColorAt(0.0, d_ptr->m_plateColor.lighter(120));
    plateGrad.setColorAt(0.5, d_ptr->m_plateColor);
    plateGrad.setColorAt(1.0, d_ptr->m_plateColor.darker(130));
    painter.setBrush(plateGrad);

    // Plate Chamfer / Bevel Border
    QLinearGradient borderGrad(plateRect.topLeft(), plateRect.bottomLeft());
    borderGrad.setColorAt(0.0, QColor(180, 185, 195, 180));
    borderGrad.setColorAt(0.5, QColor(90, 95, 105, 150));
    borderGrad.setColorAt(1.0, QColor(40, 42, 48, 200));
    painter.setPen(QPen(borderGrad, 1.5));
    painter.drawRoundedRect(plateRect, radius, radius);

    // 4 Corner Screws
    double screwRadius = std::clamp(std::min(width(), height()) * 0.04, 3.0, 5.0);
    double screwMargin = screwRadius * 2.2;
    drawScrew(painter, QPointF(plateRect.left() + screwMargin, plateRect.top() + screwMargin), screwRadius);
    drawScrew(painter, QPointF(plateRect.right() - screwMargin, plateRect.top() + screwMargin), screwRadius);
    drawScrew(painter, QPointF(plateRect.left() + screwMargin, plateRect.bottom() - screwMargin), screwRadius);
    drawScrew(painter, QPointF(plateRect.right() - screwMargin, plateRect.bottom() - screwMargin), screwRadius);

    // Title Label
    painter.setFont(font());
    painter.setPen(d_ptr->m_textColor);

    if (!d_ptr->m_label.isEmpty()) {
        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(std::max(7.0, font().pointSizeF() * 0.85));
        painter.setFont(titleFont);

        QRectF titleRect;
        if (d_ptr->m_orientation == Qt::Vertical) {
            titleRect = QRectF(plateRect.left(), plateRect.top() + 6, plateRect.width(), 16);
        } else {
            titleRect = QRectF(plateRect.left() + 6, plateRect.top() + 6, plateRect.width() - 12, 14);
        }
        painter.drawText(titleRect, Qt::AlignCenter, d_ptr->m_label);
    }

    // Position Labels ("ON", "OFF", "AUTO", etc.)
    QRectF switchRect = calculateSwitchRect();
    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPointSizeF(std::max(6.5, font().pointSizeF() * 0.8));
    painter.setFont(labelFont);

    if (d_ptr->m_orientation == Qt::Vertical) {
        // Upper label (ON)
        QRectF topLabelRect(plateRect.left(), switchRect.top() - 16, plateRect.width(), 14);
        painter.setPen(d_ptr->m_textColor.lighter(110));
        painter.drawText(topLabelRect, Qt::AlignCenter, d_ptr->m_labelOn);

        // Lower label (OFF)
        QRectF botLabelRect(plateRect.left(), switchRect.bottom() + 3, plateRect.width(), 14);
        painter.setPen(d_ptr->m_textColor.darker(110));
        painter.drawText(botLabelRect, Qt::AlignCenter, d_ptr->m_labelOff);

        // Center label if 3-position
        if (d_ptr->m_positionCount == 3) {
            QRectF centerLabelRect(switchRect.right() + 4, switchRect.center().y() - 7, plateRect.right() - switchRect.right() - 6, 14);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(centerLabelRect, Qt::AlignLeft | Qt::AlignVCenter, d_ptr->m_labelCenter);
        }
    } else {
        // Horizontal orientation
        // Left label (OFF)
        QRectF leftLabelRect(plateRect.left() + 4, switchRect.top(), switchRect.left() - plateRect.left() - 6, switchRect.height());
        painter.setPen(d_ptr->m_textColor.darker(110));
        painter.drawText(leftLabelRect, Qt::AlignRight | Qt::AlignVCenter, d_ptr->m_labelOff);

        // Right label (ON)
        QRectF rightLabelRect(switchRect.right() + 6, switchRect.top(), plateRect.right() - switchRect.right() - 8, switchRect.height());
        painter.setPen(d_ptr->m_textColor.lighter(110));
        painter.drawText(rightLabelRect, Qt::AlignLeft | Qt::AlignVCenter, d_ptr->m_labelOn);

        // Center label if 3-position
        if (d_ptr->m_positionCount == 3) {
            QRectF centerLabelRect(switchRect.left(), switchRect.bottom() + 2, switchRect.width(), 14);
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(centerLabelRect, Qt::AlignCenter, d_ptr->m_labelCenter);
        }
    }

    // Switch Socket Base
    if (d_ptr->m_switchType == SwitchType::ToggleLever) {
        // Circular threaded bezel collar nut
        QPointF socketCenter = switchRect.center();
        double socketRadius = std::min(switchRect.width(), switchRect.height()) * 0.44;

        // Dark cavity
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(18, 20, 22));
        painter.drawEllipse(socketCenter, socketRadius, socketRadius);

        // Metallic collar ring
        QRadialGradient collarGrad(socketCenter - QPointF(socketRadius * 0.3, socketRadius * 0.3), socketRadius * 1.1);
        collarGrad.setColorAt(0.0, QColor(240, 245, 250));
        collarGrad.setColorAt(0.4, QColor(160, 165, 172));
        collarGrad.setColorAt(0.7, QColor(80, 85, 92));
        collarGrad.setColorAt(1.0, QColor(40, 42, 45));
        painter.setPen(QPen(QColor(30, 32, 35), 1.2));
        painter.setBrush(collarGrad);
        painter.drawEllipse(socketCenter, socketRadius * 0.9, socketRadius * 0.9);

        // Inner pivot well
        QRadialGradient wellGrad(socketCenter, socketRadius * 0.6);
        wellGrad.setColorAt(0.0, QColor(25, 25, 28));
        wellGrad.setColorAt(0.8, QColor(45, 48, 52));
        wellGrad.setColorAt(1.0, QColor(15, 15, 18));
        painter.setPen(Qt::NoPen);
        painter.setBrush(wellGrad);
        painter.drawEllipse(socketCenter, socketRadius * 0.55, socketRadius * 0.55);
    } else {
        // Rocker switch recessed rectangular bezel
        painter.setPen(QPen(QColor(25, 26, 30), 1.5));
        painter.setBrush(QColor(22, 23, 26));
        painter.drawRoundedRect(switchRect, 4.0, 4.0);

        // Bevel highlight around switch opening
        painter.setPen(QPen(QColor(100, 105, 115, 140), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(switchRect.adjusted(1, 1, -1, -1), 3.0, 3.0);
    }

    d_ptr->m_cacheValid = true;
}

void IndustrialSwitch::drawToggleLever(QPainter &painter, const QRectF &switchArea, double currentPos)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    QPointF pivot = switchArea.center();
    double length = std::min(switchArea.width(), switchArea.height()) * 0.72;
    double thickness = length * 0.28;

    // Calculate lever tilt angle and travel
    // pos 0 = off/down (-32 degrees or offset down), pos 1/2 = on/up (+32 degrees or offset up)
    double normPos = 0.0;
    if (d_ptr->m_positionCount == 2) {
        normPos = currentPos; // 0.0 to 1.0
    } else {
        normPos = currentPos * 0.5; // 0.0 to 1.0
    }

    // Map 0.0 -> -32 deg, 1.0 -> +32 deg
    double angleDeg = -32.0 + normPos * 64.0;
    if (d_ptr->m_orientation == Qt::Horizontal) {
        angleDeg = -32.0 + normPos * 64.0;
    }

    painter.translate(pivot);

    if (d_ptr->m_orientation == Qt::Vertical) {
        // In vertical mode: 0 is DOWN (positive Y in Qt), 1 is UP (negative Y in Qt)
        // Let's rotate so that angleDeg tilts lever
        painter.rotate(angleDeg);

        // Cast shadow of lever
        painter.save();
        QPointF shadowOffset(3.0, 4.0);
        painter.translate(shadowOffset);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(10, 12, 15, 130));
        QRectF shadowBat(-thickness * 0.45, -length, thickness * 0.9, length);
        painter.drawRoundedRect(shadowBat, thickness * 0.45, thickness * 0.45);
        painter.drawEllipse(QPointF(0, -length), thickness * 0.65, thickness * 0.65);
        painter.restore();

        // Bat lever shaft (Cylindrical brushed metal)
        QRectF batRect(-thickness * 0.4, -length, thickness * 0.8, length);

        QLinearGradient shaftGrad(batRect.left(), 0, batRect.right(), 0);
        shaftGrad.setColorAt(0.0, d_ptr->m_leverColor.darker(170));
        shaftGrad.setColorAt(0.3, d_ptr->m_leverColor.lighter(130));
        shaftGrad.setColorAt(0.55, QColor(255, 255, 255, 240));
        shaftGrad.setColorAt(0.75, d_ptr->m_leverColor);
        shaftGrad.setColorAt(1.0, d_ptr->m_leverColor.darker(200));

        painter.setPen(QPen(QColor(60, 65, 70), 0.8));
        painter.setBrush(shaftGrad);
        painter.drawRoundedRect(batRect, thickness * 0.3, thickness * 0.3);

        // Spherical / teardrop knob tip
        QPointF tipCenter(0, -length);
        double tipRadius = thickness * 0.62;

        QRadialGradient tipGrad(tipCenter - QPointF(tipRadius * 0.3, tipRadius * 0.3), tipRadius * 1.3);
        tipGrad.setColorAt(0.0, QColor(255, 255, 255, 250));
        tipGrad.setColorAt(0.25, d_ptr->m_leverColor.lighter(120));
        tipGrad.setColorAt(0.65, d_ptr->m_leverColor);
        tipGrad.setColorAt(0.9, d_ptr->m_leverColor.darker(160));
        tipGrad.setColorAt(1.0, d_ptr->m_leverColor.darker(220));

        painter.setPen(QPen(QColor(50, 55, 60), 0.8));
        painter.setBrush(tipGrad);
        painter.drawEllipse(tipCenter, tipRadius, tipRadius);

        // Pivot chrome hemisphere hub
        QRadialGradient hubGrad(QPointF(-thickness * 0.2, -thickness * 0.2), thickness * 0.8);
        hubGrad.setColorAt(0.0, QColor(250, 252, 255));
        hubGrad.setColorAt(0.4, d_ptr->m_leverColor);
        hubGrad.setColorAt(0.85, d_ptr->m_leverColor.darker(180));
        hubGrad.setColorAt(1.0, QColor(35, 38, 42));
        painter.setBrush(hubGrad);
        painter.drawEllipse(QPointF(0, 0), thickness * 0.55, thickness * 0.55);

    } else {
        // Horizontal orientation
        painter.rotate(angleDeg + 90.0);

        QRectF batRect(-thickness * 0.4, -length, thickness * 0.8, length);
        QLinearGradient shaftGrad(batRect.left(), 0, batRect.right(), 0);
        shaftGrad.setColorAt(0.0, d_ptr->m_leverColor.darker(170));
        shaftGrad.setColorAt(0.3, d_ptr->m_leverColor.lighter(130));
        shaftGrad.setColorAt(0.55, QColor(255, 255, 255, 240));
        shaftGrad.setColorAt(0.75, d_ptr->m_leverColor);
        shaftGrad.setColorAt(1.0, d_ptr->m_leverColor.darker(200));

        painter.setPen(QPen(QColor(60, 65, 70), 0.8));
        painter.setBrush(shaftGrad);
        painter.drawRoundedRect(batRect, thickness * 0.3, thickness * 0.3);

        QPointF tipCenter(0, -length);
        double tipRadius = thickness * 0.62;
        QRadialGradient tipGrad(tipCenter - QPointF(tipRadius * 0.3, tipRadius * 0.3), tipRadius * 1.3);
        tipGrad.setColorAt(0.0, QColor(255, 255, 255, 250));
        tipGrad.setColorAt(0.25, d_ptr->m_leverColor.lighter(120));
        tipGrad.setColorAt(0.65, d_ptr->m_leverColor);
        tipGrad.setColorAt(0.9, d_ptr->m_leverColor.darker(160));
        tipGrad.setColorAt(1.0, d_ptr->m_leverColor.darker(220));

        painter.setBrush(tipGrad);
        painter.drawEllipse(tipCenter, tipRadius, tipRadius);
    }

    painter.restore();
}

void IndustrialSwitch::drawRocker(QPainter &painter, const QRectF &switchArea, double currentPos)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF paddleRect = switchArea.adjusted(2, 2, -2, -2);
    double w = paddleRect.width();
    double h = paddleRect.height();

    double normPos = 0.0;
    if (d_ptr->m_positionCount == 2) {
        normPos = currentPos; // 0.0 to 1.0
    } else {
        normPos = currentPos * 0.5; // 0.0 to 1.0
    }

    // Displacement offset for 3D slope illusion
    // In vertical mode: pos 1 (ON) presses top side down, lifts bottom side up
    // pos 0 (OFF) presses bottom side down, lifts top side up
    double tilt = (normPos - 0.5) * 2.0; // -1.0 (OFF) to +1.0 (ON)

    if (d_ptr->m_orientation == Qt::Vertical) {
        QRectF topHalf(paddleRect.left(), paddleRect.top(), w, h * 0.5);
        QRectF botHalf(paddleRect.left(), paddleRect.top() + h * 0.5, w, h * 0.5);

        // Top half paddle
        QLinearGradient topGrad(topHalf.topLeft(), topHalf.bottomLeft());
        if (tilt > 0) {
            // ON: top half is pressed in / recessed (darker)
            topGrad.setColorAt(0.0, QColor(35, 37, 42));
            topGrad.setColorAt(1.0, QColor(50, 53, 60));
        } else {
            // OFF: top half is raised up (brighter highlight)
            topGrad.setColorAt(0.0, QColor(80, 85, 95));
            topGrad.setColorAt(0.8, QColor(60, 64, 72));
            topGrad.setColorAt(1.0, QColor(48, 51, 58));
        }

        // Bottom half paddle
        QLinearGradient botGrad(botHalf.topLeft(), botHalf.bottomLeft());
        if (tilt < 0) {
            // OFF: bottom half is pressed in
            botGrad.setColorAt(0.0, QColor(50, 53, 60));
            botGrad.setColorAt(1.0, QColor(35, 37, 42));
        } else {
            // ON: bottom half is raised up
            botGrad.setColorAt(0.0, QColor(52, 56, 64));
            botGrad.setColorAt(0.3, QColor(75, 80, 90));
            botGrad.setColorAt(1.0, QColor(45, 48, 55));
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(topGrad);
        painter.drawRoundedRect(topHalf, 3.0, 3.0);

        painter.setBrush(botGrad);
        painter.drawRoundedRect(botHalf, 3.0, 3.0);

        // Center seam line
        painter.setPen(QPen(QColor(25, 27, 30), 1.5));
        painter.drawLine(QPointF(paddleRect.left() + 3, paddleRect.center().y()),
                         QPointF(paddleRect.right() - 3, paddleRect.center().y()));

        // Traction ribs (3 horizontal grooves on each side)
        painter.setPen(QPen(QColor(30, 32, 36, 160), 1.0));
        for (int i = 1; i <= 2; ++i) {
            double yTop = topHalf.top() + (topHalf.height() / 3.0) * i;
            painter.drawLine(QPointF(topHalf.left() + 6, yTop), QPointF(topHalf.right() - 6, yTop));

            double yBot = botHalf.top() + (botHalf.height() / 3.0) * i;
            painter.drawLine(QPointF(botHalf.left() + 6, yBot), QPointF(botHalf.right() - 6, yBot));
        }

        // Illuminated status indicator line on ON half
        QRectF ledBar(paddleRect.center().x() - 5, topHalf.top() + 6, 10, 4);
        bool active = (d_ptr->m_position > 0);
        if (active) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(d_ptr->m_ledColor);
            painter.drawRoundedRect(ledBar, 1.5, 1.5);
            // Glow
            painter.setBrush(QColor(d_ptr->m_ledColor.red(), d_ptr->m_ledColor.green(), d_ptr->m_ledColor.blue(), 100));
            painter.drawRoundedRect(ledBar.adjusted(-2, -2, 2, 2), 2.5, 2.5);
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(40, 42, 46));
            painter.drawRoundedRect(ledBar, 1.5, 1.5);
        }

    } else {
        // Horizontal rocker
        QRectF leftHalf(paddleRect.left(), paddleRect.top(), w * 0.5, h);
        QRectF rightHalf(paddleRect.left() + w * 0.5, paddleRect.top(), w * 0.5, h);

        QLinearGradient leftGrad(leftHalf.topLeft(), leftHalf.topRight());
        QLinearGradient rightGrad(rightHalf.topLeft(), rightHalf.topRight());

        if (tilt < 0) {
            leftGrad.setColorAt(0.0, QColor(35, 37, 42));
            leftGrad.setColorAt(1.0, QColor(50, 53, 60));
            rightGrad.setColorAt(0.0, QColor(52, 56, 64));
            rightGrad.setColorAt(1.0, QColor(75, 80, 90));
        } else {
            leftGrad.setColorAt(0.0, QColor(80, 85, 95));
            leftGrad.setColorAt(1.0, QColor(48, 51, 58));
            rightGrad.setColorAt(0.0, QColor(50, 53, 60));
            rightGrad.setColorAt(1.0, QColor(35, 37, 42));
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(leftGrad);
        painter.drawRoundedRect(leftHalf, 3.0, 3.0);
        painter.setBrush(rightGrad);
        painter.drawRoundedRect(rightHalf, 3.0, 3.0);

        painter.setPen(QPen(QColor(25, 27, 30), 1.5));
        painter.drawLine(QPointF(paddleRect.center().x(), paddleRect.top() + 3),
                         QPointF(paddleRect.center().x(), paddleRect.bottom() - 3));
    }

    painter.restore();
}

void IndustrialSwitch::drawSafetyGuard(QPainter &painter, const QRectF & /*switchArea*/)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF guardBase = calculateGuardRect();
    double openFactor = d_ptr->m_guardOpenFactor; // 0.0 = closed, 1.0 = fully open

    // Hinge position (at the bottom or top of switch area)
    QPointF hinge = QPointF(guardBase.center().x(), guardBase.top() + 6);

    painter.translate(hinge);
    // When opening, rotate up by 105 degrees and shrink/scale slightly in perspective
    painter.rotate(-openFactor * 105.0);
    painter.translate(-hinge);

    // Guard Cover Polygon
    QRectF coverRect = guardBase.adjusted(0, 0, 0, 0);

    // Drop shadow under guard cover
    if (openFactor < 0.95) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(10, 10, 12, static_cast<int>(120 * (1.0 - openFactor * 0.6))));
        painter.drawRoundedRect(coverRect.translated(2, 3), 6.0, 6.0);
        painter.restore();
    }

    // Safety Cover outer shell
    QLinearGradient guardGrad(coverRect.topLeft(), coverRect.bottomRight());
    guardGrad.setColorAt(0.0, d_ptr->m_guardColor.lighter(130));
    guardGrad.setColorAt(0.4, d_ptr->m_guardColor);
    guardGrad.setColorAt(1.0, d_ptr->m_guardColor.darker(150));

    painter.setPen(QPen(QColor(40, 10, 15), 1.2));
    painter.setBrush(guardGrad);
    painter.drawRoundedRect(coverRect, 6.0, 6.0);

    // Warning Chevron / Hazard Stripes across the lower cover
    painter.save();
    QPainterPath clipPath;
    clipPath.addRoundedRect(coverRect, 6.0, 6.0);
    painter.setClipPath(clipPath);

    painter.setPen(QPen(QColor(245, 195, 35, 180), 3.0));
    for (double x = coverRect.left() - 20; x < coverRect.right() + 20; x += 12.0) {
        painter.drawLine(QPointF(x, coverRect.bottom()), QPointF(x + 16, coverRect.bottom() - 16));
    }

    // Inspection window (tinted transparent viewing window)
    QRectF winRect = coverRect.adjusted(8, 12, -8, -18);
    painter.setPen(QPen(QColor(20, 20, 20, 190), 1.0));
    painter.setBrush(QColor(30, 30, 35, 110));
    painter.drawRoundedRect(winRect, 3.0, 3.0);

    // Window glass specular streak
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1.5));
    painter.drawLine(QPointF(winRect.left() + 3, winRect.top() + 3),
                     QPointF(winRect.right() - 3, winRect.bottom() - 3));

    painter.restore(); // restore clip

    // Metallic Side Hinge Brackets
    painter.setPen(QPen(QColor(40, 42, 45), 0.8));
    QLinearGradient hingeGrad(coverRect.left(), 0, coverRect.left() + 6, 0);
    hingeGrad.setColorAt(0.0, QColor(220, 225, 230));
    hingeGrad.setColorAt(1.0, QColor(90, 95, 100));
    painter.setBrush(hingeGrad);
    painter.drawRect(QRectF(coverRect.left() - 2, coverRect.top() + 2, 4, 10));
    painter.drawRect(QRectF(coverRect.right() - 2, coverRect.top() + 2, 4, 10));

    // Lift tab at the bottom of the guard
    QRectF liftTab(coverRect.center().x() - 12, coverRect.bottom() - 3, 24, 7);
    painter.setBrush(d_ptr->m_guardColor.darker(120));
    painter.drawRoundedRect(liftTab, 2.0, 2.0);

    painter.restore();
}

void IndustrialSwitch::paintEvent(QPaintEvent * /*event*/)
{
    if (!d_ptr->m_cacheValid || d_ptr->m_cachedBackground.size() != size() * devicePixelRatioF()) {
        renderStaticBackground();
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, d_ptr->m_cachedBackground);

    QRectF switchRect = calculateSwitchRect();

    // Draw active switch mechanism
    if (d_ptr->m_switchType == SwitchType::ToggleLever) {
        drawToggleLever(painter, switchRect, d_ptr->m_currentPos);
    } else {
        drawRocker(painter, switchRect, d_ptr->m_currentPos);
    }

    // Draw Status LED if enabled
    if (d_ptr->m_hasLed) {
        QRectF plateRect = rect().adjusted(3, 3, -3, -3);
        QPointF ledCenter;
        if (d_ptr->m_orientation == Qt::Vertical) {
            ledCenter = QPointF(plateRect.center().x(), switchRect.top() - 26);
        } else {
            ledCenter = QPointF(switchRect.right() + 18, plateRect.center().y());
        }
        drawLed(painter, ledCenter, 4.5, isChecked());
    }

    // Draw Safety Guard over the switch if enabled
    if (d_ptr->m_hasSafetyGuard) {
        drawSafetyGuard(painter, switchRect);
    }
}

void IndustrialSwitch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Safety guard logic
    if (d_ptr->m_hasSafetyGuard) {
        if (!d_ptr->m_isGuardOpen) {
            // Guard is closed: click flips the guard open!
            setGuardOpen(true);
            return;
        } else {
            // Guard is open: click on upper area flips the guard closed
            QRectF guardRect = calculateGuardRect();
            if (event->position().y() < guardRect.top() + 15) {
                setGuardOpen(false);
                return;
            }
        }
    }

    // Switch actuation
    QRectF switchRect = calculateSwitchRect();
    if (d_ptr->m_positionCount == 2) {
        // Toggle state
        toggle();
    } else {
        // 3-position: check whether clicked top, middle, or bottom
        if (d_ptr->m_orientation == Qt::Vertical) {
            double relY = (event->position().y() - switchRect.top()) / switchRect.height();
            if (relY < 0.35) {
                setPosition(2); // Top (ON / POS 2)
            } else if (relY > 0.65) {
                setPosition(0); // Bottom (OFF / POS 0)
            } else {
                setPosition(1); // Center (POS 1)
            }
        } else {
            double relX = (event->position().x() - switchRect.left()) / switchRect.width();
            if (relX < 0.35) {
                setPosition(0);
            } else if (relX > 0.65) {
                setPosition(2);
            } else {
                setPosition(1);
            }
        }
    }

    d_ptr->m_isDragging = true;
}

void IndustrialSwitch::mouseMoveEvent(QMouseEvent *event)
{
    if (!d_ptr->m_isDragging || (d_ptr->m_hasSafetyGuard && !d_ptr->m_isGuardOpen)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QRectF switchRect = calculateSwitchRect();
    if (d_ptr->m_orientation == Qt::Vertical) {
        double relY = (event->position().y() - switchRect.top()) / switchRect.height();
        relY = std::clamp(relY, 0.0, 1.0);
        if (d_ptr->m_positionCount == 2) {
            setPosition(relY < 0.5 ? 1 : 0);
        } else {
            if (relY < 0.33) setPosition(2);
            else if (relY > 0.66) setPosition(0);
            else setPosition(1);
        }
    } else {
        double relX = (event->position().x() - switchRect.left()) / switchRect.width();
        relX = std::clamp(relX, 0.0, 1.0);
        if (d_ptr->m_positionCount == 2) {
            setPosition(relX > 0.5 ? 1 : 0);
        } else {
            if (relX < 0.33) setPosition(0);
            else if (relX > 0.66) setPosition(2);
            else setPosition(1);
        }
    }
}

void IndustrialSwitch::mouseReleaseEvent(QMouseEvent *event)
{
    d_ptr->m_isDragging = false;
    QWidget::mouseReleaseEvent(event);
}

void IndustrialSwitch::keyPressEvent(QKeyEvent *event)
{
    if (d_ptr->m_hasSafetyGuard && !d_ptr->m_isGuardOpen) {
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
            setGuardOpen(true);
            return;
        }
    }

    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_Return:
        toggle();
        break;
    case Qt::Key_Up:
    case Qt::Key_Right:
        setPosition(std::min(d_ptr->m_position + 1, d_ptr->m_positionCount - 1));
        break;
    case Qt::Key_Down:
    case Qt::Key_Left:
        setPosition(std::max(d_ptr->m_position - 1, 0));
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void IndustrialSwitch::wheelEvent(QWheelEvent *event)
{
    if (d_ptr->m_hasSafetyGuard && !d_ptr->m_isGuardOpen) {
        QWidget::wheelEvent(event);
        return;
    }

    int delta = event->angleDelta().y();
    if (delta > 0) {
        setPosition(std::min(d_ptr->m_position + 1, d_ptr->m_positionCount - 1));
    } else if (delta < 0) {
        setPosition(std::max(d_ptr->m_position - 1, 0));
    }
    event->accept();
}

} // namespace QtIndustrialWidgets
