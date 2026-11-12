// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/ThumbwheelSwitch.h>

#include "internal/ThumbwheelSwitch_p.h"
#include "internal/PanelFonts.h"
#include "internal/ThumbwheelSwitchLayout.h"

#include <QtCore/QEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {

namespace {

constexpr int kLabelPx = 12; // The unit and the tags, before the font scale (#51) and the 7 pt floor
constexpr int kDigitPx = 20; // The digits, before the font scale and the 7 pt floor

const QString kOverflowTag = QStringLiteral("OVF");

// Width of a text: the advance, or the width of the ink box when larger. Not its right edge: without installed fonts
// boundingRect() is placed at 100000
double textWidth(const QFontMetricsF &fm, const QString &text)
{
    return std::max(fm.horizontalAdvance(text), fm.boundingRect(text).width());
}

// Size of the triangle of the tag column for a tag font of line height \a lineHeight
double tagTriangle(double lineHeight)
{
    return std::round(lineHeight * 0.8);
}

// Thickness of the bars of the sign and of the overflow dash for a digit font of \a px pixels
double barThickness(double px)
{
    return std::max(2.0, std::round(px * 0.12));
}

// The arrow of a button: up for +, down for −; filled when its step would be applied, outlined when it would not
void drawArrow(QPainter &painter, const QRectF &button, bool up, bool open, const QColor &color)
{
    const double a = std::min(button.width() * 0.25, button.height() * 0.4);
    const QPointF c = button.center();
    QPolygonF arrow;
    if (up) {
        arrow << QPointF(c.x() - a, c.y() + a * 0.5) << QPointF(c.x() + a, c.y() + a * 0.5)
              << QPointF(c.x(), c.y() - a * 0.5);
    } else {
        arrow << QPointF(c.x() - a, c.y() - a * 0.5) << QPointF(c.x() + a, c.y() - a * 0.5)
              << QPointF(c.x(), c.y() + a * 0.5);
    }
    painter.save();
    if (open) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
    } else {
        painter.setPen(QPen(color, 1.2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.setBrush(Qt::NoBrush);
    }
    painter.drawPolygon(arrow);
    painter.restore();
}

// A horizontal bar across the middle of \a drum: the − of the sign drum, the dash of the overflow
QRectF middleBar(const QRectF &drum, double thickness)
{
    const double w = std::round(drum.width() * 0.5);
    return QRectF(drum.center().x() - w * 0.5, drum.center().y() - thickness * 0.5, w, thickness);
}

} // namespace

internal::ThumbwheelLimits ThumbwheelSwitchPrivate::limits() const
{
    const double top = m_format.valueOf(m_format.capacity());
    const double bottom = m_format.signDrum ? -top : 0.0;
    return {std::clamp(m_minimum, bottom, top), std::clamp(m_maximum, bottom, top)};
}

std::optional<qint64> ThumbwheelSwitchPrivate::stepOf(int drum, int direction) const
{
    if (overflow()) {
        return std::nullopt; // The drums show no value to step
    }
    std::optional<qint64> next;
    if (m_format.signDrum && drum == 0) {
        next = m_format.withSign(m_count, direction < 0);
        if (next && *next == m_count) {
            return std::nullopt; // Already that sign
        }
    } else {
        next = m_format.stepDigit(m_count, drum - (m_format.signDrum ? 1 : 0), direction);
    }
    if (!next || !m_format.canShow(*next) || !limits().allows(value(), m_format.valueOf(*next))) {
        return std::nullopt;
    }
    return next;
}

ThumbwheelSwitchPrivate::Fonts ThumbwheelSwitchPrivate::fontsFor(const QFont &widgetFont, double logicalDpi)
{
    return internal::panelFonts(widgetFont, logicalDpi, kLabelPx, kDigitPx);
}

internal::ThumbwheelLayoutInput ThumbwheelSwitchPrivate::layoutInput(double w, double h, const Fonts &fonts) const
{
    internal::ThumbwheelLayoutInput in;
    in.width = w;
    in.height = h;
    in.format = m_format;
    const QFontMetricsF dm(fonts.readout);
    in.text.digitPx = fonts.readout.pixelSize();
    in.text.digitHeight = dm.height();
    for (char c = '0'; c <= '9'; ++c) {
        in.text.digitWidth = std::max(in.text.digitWidth, textWidth(dm, QString(QLatin1Char(c))));
    }
    const QFontMetricsF lm(fonts.label);
    in.text.tagHeight = lm.height();
    in.text.unitWidth = m_unit.isEmpty() ? 0.0 : textWidth(lm, m_unit);
    in.text.tagWidth = std::max(textWidth(lm, kOverflowTag), tagTriangle(lm.height()));
    return in;
}

ThumbwheelSwitch::ThumbwheelSwitch(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ThumbwheelSwitchPrivate>())
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

ThumbwheelSwitch::~ThumbwheelSwitch() = default;

int ThumbwheelSwitch::digitCount() const { return d_ptr->m_format.digitCount; }
int ThumbwheelSwitch::decimals() const { return d_ptr->m_format.decimals; }
bool ThumbwheelSwitch::isSignDrumVisible() const { return d_ptr->m_format.signDrum; }
double ThumbwheelSwitch::minimum() const { return d_ptr->limits().low; }
double ThumbwheelSwitch::maximum() const { return d_ptr->limits().high; }
double ThumbwheelSwitch::value() const { return d_ptr->value(); }
bool ThumbwheelSwitch::isOverflow() const { return d_ptr->overflow(); }
QString ThumbwheelSwitch::unit() const { return d_ptr->m_unit; }
QColor ThumbwheelSwitch::drumColor() const { return d_ptr->m_drumColor; }
QColor ThumbwheelSwitch::digitColor() const { return d_ptr->m_digitColor; }
QColor ThumbwheelSwitch::buttonColor() const { return d_ptr->m_buttonColor; }
QColor ThumbwheelSwitch::textColor() const { return d_ptr->m_textColor; }

QSize ThumbwheelSwitch::sizeHint() const
{
    return minimumSizeHint(); // The drums keep the size of their font
}

QSize ThumbwheelSwitch::minimumSizeHint() const
{
    const double dpi = logicalDpiY();
    d_ptr->m_minimumDpi = dpi;
    const ThumbwheelSwitchPrivate::Fonts fonts = ThumbwheelSwitchPrivate::fontsFor(font(), dpi);
    const QSizeF s = internal::minimumThumbwheelSize(d_ptr->layoutInput(0.0, 0.0, fonts));
    return QSize(static_cast<int>(std::ceil(s.width())), static_cast<int>(std::ceil(s.height())));
}

namespace {

// Applies a change of the configuration or of the value, then emits what changed, in a fixed order: the value, the
// overflow, the limits
class ChangeScope
{
public:
    ChangeScope(ThumbwheelSwitch *q, ThumbwheelSwitchPrivate *d)
        : m_q(q)
        , m_d(d)
        , m_value(d->value())
        , m_overflow(d->overflow())
        , m_limits(d->limits())
    {
    }
    ~ChangeScope() = default;
    ChangeScope(const ChangeScope &) = delete;
    ChangeScope &operator=(const ChangeScope &) = delete;

    void emitChanges()
    {
        if (m_d->value() != m_value) {
            Q_EMIT m_q->valueChanged(m_d->value());
        }
        if (m_d->overflow() != m_overflow) {
            Q_EMIT m_q->overflowChanged(m_d->overflow());
        }
        const internal::ThumbwheelLimits now = m_d->limits();
        if (now.low != m_limits.low || now.high != m_limits.high) {
            Q_EMIT m_q->rangeChanged(now.low, now.high);
        }
        m_q->update();
    }

private:
    ThumbwheelSwitch *m_q;
    ThumbwheelSwitchPrivate *m_d;
    double m_value;
    bool m_overflow;
    internal::ThumbwheelLimits m_limits;
};

} // namespace

void ThumbwheelSwitch::setValue(double value)
{
    ChangeScope scope(this, d_ptr.get());
    if (!std::isfinite(value)) {
        qWarning("ThumbwheelSwitch::setValue: non-finite value %s ignored", qPrintable(QString::number(value)));
        return; // #182: a control has no invalid state; the overflow mark is for values the drums cannot show
    }
    const std::optional<qint64> count = d_ptr->m_format.countOf(value);
    if (!count || !d_ptr->m_format.canShow(*count)) {
        qWarning("ThumbwheelSwitch::setValue: %s cannot be shown on the drums, ignored",
                 qPrintable(QString::number(value, 'g', 17)));
        d_ptr->m_rejectedOverflow = true;
        scope.emitChanges();
        return;
    }
    d_ptr->m_count = *count;
    d_ptr->m_rejectedOverflow = false;
    scope.emitChanges();
}

void ThumbwheelSwitch::setDigitCount(int count)
{
    count = std::clamp(count, 1, internal::kThumbwheelMaxDigits);
    if (count == d_ptr->m_format.digitCount) return;
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_format.digitCount = count;
    if (d_ptr->m_format.decimals > count - 1) {
        const double v = d_ptr->value();
        d_ptr->m_format.decimals = count - 1;
        d_ptr->m_count = d_ptr->m_format.countOf(v).value_or(d_ptr->m_count);
    }
    updateGeometry();
    Q_EMIT appearanceChanged();
    scope.emitChanges();
}

void ThumbwheelSwitch::setDecimals(int decimals)
{
    decimals = std::clamp(decimals, 0, d_ptr->m_format.digitCount - 1);
    if (decimals == d_ptr->m_format.decimals) return;
    ChangeScope scope(this, d_ptr.get());
    const double v = d_ptr->value();
    d_ptr->m_format.decimals = decimals;
    d_ptr->m_count = d_ptr->m_format.countOf(v).value_or(d_ptr->m_count); // The value kept, at the new resolution
    updateGeometry();
    Q_EMIT appearanceChanged();
    scope.emitChanges();
}

void ThumbwheelSwitch::setSignDrumVisible(bool visible)
{
    if (visible == d_ptr->m_format.signDrum) return;
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_format.signDrum = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    scope.emitChanges();
}

void ThumbwheelSwitch::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void ThumbwheelSwitch::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void ThumbwheelSwitch::setRange(double min, double max)
{
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max) return;
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;
    scope.emitChanges();
}

void ThumbwheelSwitch::setUnit(const QString &unit)
{
    if (unit == d_ptr->m_unit) return;
    d_ptr->m_unit = unit;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

#define QIW_THUMBWHEEL_COLOR_SETTER(Setter, member)                                                                   \
    void ThumbwheelSwitch::Setter(const QColor &color)                                                                \
    {                                                                                                                  \
        if (d_ptr->member == color) return;                                                                            \
        d_ptr->member = color;                                                                                         \
        Q_EMIT appearanceChanged();                                                                                    \
        update();                                                                                                      \
    }

QIW_THUMBWHEEL_COLOR_SETTER(setDrumColor, m_drumColor)
QIW_THUMBWHEEL_COLOR_SETTER(setDigitColor, m_digitColor)
QIW_THUMBWHEEL_COLOR_SETTER(setButtonColor, m_buttonColor)
QIW_THUMBWHEEL_COLOR_SETTER(setTextColor, m_textColor)

#undef QIW_THUMBWHEEL_COLOR_SETTER

void ThumbwheelSwitch::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}

void ThumbwheelSwitch::paintEvent(QPaintEvent *)
{
    // #177: on a screen of another logical DPI the minimum changes (the texts at 7 pt): the layout gets it, once
    if (d_ptr->m_minimumDpi != 0.0 && d_ptr->m_minimumDpi != logicalDpiY()) {
        d_ptr->m_minimumDpi = 0.0;
        updateGeometry();
    }

    const ThumbwheelSwitchPrivate::Fonts fonts = ThumbwheelSwitchPrivate::fontsFor(font(), logicalDpiY());
    const internal::ThumbwheelLayoutInput in = d_ptr->layoutInput(width(), height(), fonts);
    const internal::ThumbwheelLayout layout = internal::layoutThumbwheel(in);
    const internal::ThumbwheelFormat &format = d_ptr->m_format;
    const bool overflow = d_ptr->overflow();
    const double bar = barThickness(in.text.digitPx);

    d_ptr->m_lastLayout = layout;
    d_ptr->m_lastDrums.clear();
    d_ptr->m_lastTag.clear();
    d_ptr->m_lastPlusOpen.clear();
    d_ptr->m_lastMinusOpen.clear();
    d_ptr->m_lastDigitPx = fonts.readout.pixelSize();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. The buttons, with their arrows
    for (int i = 0; i < layout.drums.size(); ++i) {
        const bool plusOpen = d_ptr->stepOf(i, +1).has_value();
        const bool minusOpen = d_ptr->stepOf(i, -1).has_value();
        for (const QRectF &button : {layout.plus.at(i), layout.minus.at(i)}) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(d_ptr->m_buttonColor);
            painter.drawRoundedRect(button, 2.0, 2.0);
        }
        drawArrow(painter, layout.plus.at(i), true, plusOpen, d_ptr->m_digitColor);
        drawArrow(painter, layout.minus.at(i), false, minusOpen, d_ptr->m_digitColor);
        d_ptr->m_lastPlusOpen.push_back(plusOpen);
        d_ptr->m_lastMinusOpen.push_back(minusOpen);
    }

    // 2. The drums: opaque faces (#159), the digits, the sign, or the dash of the overflow
    painter.setFont(fonts.readout);
    for (int i = 0; i < layout.drums.size(); ++i) {
        const QRectF &drum = layout.drums.at(i);
        painter.setPen(QPen(d_ptr->m_buttonColor, 1.0));
        painter.setBrush(d_ptr->m_drumColor);
        painter.drawRoundedRect(drum.adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(d_ptr->m_digitColor);
        const bool signDrum = layout.signDrum && i == 0;
        if (overflow) {
            painter.drawRect(middleBar(drum, bar));
            d_ptr->m_lastDrums << QStringLiteral("-");
        } else if (signDrum) {
            // + and − as shapes: they do not depend on the installed fonts
            const QRectF across = middleBar(drum, bar);
            painter.drawRect(across);
            if (d_ptr->m_count >= 0) {
                painter.drawRect(QRectF(drum.center().x() - bar * 0.5, drum.center().y() - across.width() * 0.5, bar,
                                        across.width()));
                d_ptr->m_lastDrums << QStringLiteral("+");
            } else {
                d_ptr->m_lastDrums << QStringLiteral("−");
            }
        } else {
            const QString text = QString::number(format.digit(d_ptr->m_count, i - (layout.signDrum ? 1 : 0)));
            painter.setPen(d_ptr->m_digitColor);
            painter.drawText(drum, Qt::AlignCenter, text);
            d_ptr->m_lastDrums << text;
        }
    }

    // 3. The decimal point and the unit
    painter.setPen(Qt::NoPen);
    painter.setBrush(d_ptr->m_textColor);
    if (!layout.decimalPoint.isEmpty()) {
        painter.drawEllipse(layout.decimalPoint);
    }
    painter.setFont(fonts.label);
    painter.setPen(d_ptr->m_textColor);
    if (!layout.unit.isEmpty()) {
        painter.drawText(layout.unit, Qt::AlignLeft | Qt::AlignVCenter, d_ptr->m_unit);
    }

    // 4. The tag column: the overflow, or a value outside the limits, never told by colour alone
    if (overflow) {
        painter.drawText(layout.tag, Qt::AlignCenter, kOverflowTag);
        d_ptr->m_lastTag = kOverflowTag;
    } else {
        const internal::ThumbwheelLimitState state = d_ptr->limits().stateOf(d_ptr->value());
        if (state != internal::ThumbwheelLimitState::Inside) {
            const bool above = state == internal::ThumbwheelLimitState::Above;
            const double t = std::min(layout.tag.width(), tagTriangle(in.text.tagHeight));
            const QPointF c = layout.tag.center();
            QPolygonF tri;
            if (above) {
                tri << QPointF(c.x() - t * 0.5, c.y() + t * 0.43) << QPointF(c.x() + t * 0.5, c.y() + t * 0.43)
                    << QPointF(c.x(), c.y() - t * 0.43);
            } else {
                tri << QPointF(c.x() - t * 0.5, c.y() - t * 0.43) << QPointF(c.x() + t * 0.5, c.y() - t * 0.43)
                    << QPointF(c.x(), c.y() + t * 0.43);
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(d_ptr->m_textColor);
            painter.drawPolygon(tri);
            d_ptr->m_lastTag = above ? QStringLiteral("▲") : QStringLiteral("▼");
        }
    }
}

} // namespace QtIndustrialWidgets
