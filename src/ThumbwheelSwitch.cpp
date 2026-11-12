// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/ThumbwheelSwitch.h>

#include "internal/ThumbwheelSwitch_p.h"
#include "internal/ThumbwheelSwitchAccessible.h"
#include "internal/PanelFonts.h"
#include "internal/ThumbwheelSwitchLayout.h"
#include "internal/ValueStateDrawing.h"
#include "internal/WheelNotches.h"

#include <QtCore/QEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>
#include <QtGui/QWheelEvent>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {

namespace {

constexpr int kLabelPx = 12; // The unit and the tags, before the font scale (#51) and the 7 pt floor
constexpr int kDigitPx = 20; // The digits, before the font scale and the 7 pt floor

const QString kOverflowTag = QStringLiteral("OVF");
const QString kSetText = QStringLiteral("SET");

// Width of a text: the advance, or the width of the ink box when larger. Not its right edge: without installed fonts
// boundingRect() is placed at 100000
double textWidth(const QFontMetricsF &fm, const QString &text)
{
    return std::max(fm.horizontalAdvance(text), fm.boundingRect(text).width());
}

// The widest digit of a font, to measure the widest text of a value
QChar widestDigit(const QFontMetricsF &fm)
{
    QChar widest = QLatin1Char('0');
    double w = -1.0;
    for (char c = '0'; c <= '9'; ++c) {
        const double cw = textWidth(fm, QString(QLatin1Char(c)));
        if (cw > w) {
            w = cw;
            widest = QLatin1Char(c);
        }
    }
    return widest;
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

using Part = ThumbwheelSwitchPrivate::Part;

// What a point of the widget is on: a button of a drum, a drum, the SET button, or nothing
struct Hit {
    Part part{Part::None};
    int drum{-1};
};

Hit hitOf(const internal::ThumbwheelLayout &layout, const QPointF &pos)
{
    for (int i = 0; i < layout.drums.size(); ++i) {
        if (layout.plus.at(i).contains(pos)) {
            return {Part::Plus, i};
        }
        if (layout.minus.at(i).contains(pos)) {
            return {Part::Minus, i};
        }
        if (layout.drums.at(i).contains(pos)) {
            return {Part::None, i};
        }
    }
    if (!layout.setButton.isEmpty() && layout.setButton.contains(pos)) {
        return {Part::Set, -1};
    }
    return {};
}

// The drum whose column (its + button, the drum, its − button) holds \a pos; -1 for none
int columnOf(const internal::ThumbwheelLayout &layout, const QPointF &pos)
{
    for (int i = 0; i < layout.drums.size(); ++i) {
        const QRectF column(layout.plus.at(i).topLeft(), layout.minus.at(i).bottomRight());
        if (column.contains(pos)) {
            return i;
        }
    }
    return -1;
}

bool isDigitKey(int key) { return key >= Qt::Key_0 && key <= Qt::Key_9; }

// The keys of a key action: they change the value, and in Immediate commit at their release (#182)
bool isValueKey(int key)
{
    return key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Plus || key == Qt::Key_Minus || isDigitKey(key);
}

bool isMoveKey(int key)
{
    return key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Home || key == Qt::Key_End;
}

// Applies a change of the configuration, of the value or of the pending value, then emits what changed, in a fixed
// order: the value, the pending value, the overflow, the limits. Before that, a pending value is dropped when the
// overflow mark is on (#154: no input in overflow) or when it equals the value (a value is pending only while it
// differs)
class ChangeScope
{
public:
    ChangeScope(ThumbwheelSwitch *q, ThumbwheelSwitchPrivate *d)
        : m_q(q)
        , m_d(d)
        , m_value(d->value())
        , m_hasPending(d->m_pending.has_value())
        , m_pendingValue(d->shownValue())
        , m_overflow(d->overflow())
        , m_limits(d->limits())
    {
    }
    ~ChangeScope() = default;
    ChangeScope(const ChangeScope &) = delete;
    ChangeScope &operator=(const ChangeScope &) = delete;

    void emitChanges()
    {
        if (m_d->m_pending && (m_d->overflow() || *m_d->m_pending == m_d->m_count)) {
            m_d->m_pending.reset();
        }
        const bool value = m_d->value() != m_value;
        if (value) {
            Q_EMIT m_q->valueChanged(m_d->value());
        }
        const bool pending = m_d->m_pending.has_value() != m_hasPending || m_d->shownValue() != m_pendingValue;
        if (pending) {
            Q_EMIT m_q->pendingValueChanged(m_d->shownValue());
        }
        const bool overflow = m_d->overflow() != m_overflow;
        if (overflow) {
            Q_EMIT m_q->overflowChanged(m_d->overflow());
        }
        const internal::ThumbwheelLimits now = m_d->limits();
        const bool limits = now.low != m_limits.low || now.high != m_limits.high;
        if (limits) {
            Q_EMIT m_q->rangeChanged(now.low, now.high);
        }
        if (value || pending || overflow || limits) {
            m_d->notifyValue(m_q); // #183: one event, whatever changed of what the value text tells
        }
        m_q->update();
    }

private:
    ThumbwheelSwitch *m_q;
    ThumbwheelSwitchPrivate *m_d;
    double m_value;
    bool m_hasPending;
    double m_pendingValue;
    bool m_overflow;
    internal::ThumbwheelLimits m_limits;
};

} // namespace

internal::ThumbwheelLimits ThumbwheelSwitchPrivate::limits() const
{
    const double top = m_format.valueOf(m_format.capacity());
    const double bottom = m_format.signDrum ? -top : 0.0;
    return {std::clamp(m_minimum, bottom, top), std::clamp(m_maximum, bottom, top)};
}

std::optional<qint64> ThumbwheelSwitchPrivate::operatorTarget(std::optional<qint64> next) const
{
    if (m_readOnly || overflow() || !next) {
        return std::nullopt; // #154: in overflow the drums show no value to change
    }
    if (!m_format.canShow(*next) || !limits().allows(shownValue(), m_format.valueOf(*next))) {
        return std::nullopt;
    }
    return next;
}

std::optional<qint64> ThumbwheelSwitchPrivate::stepOf(int drum, int direction) const
{
    const qint64 shown = shownCount();
    std::optional<qint64> next;
    if (isSignDrum(drum)) {
        next = m_format.withSign(shown, direction < 0);
        if (next && *next == shown) {
            return std::nullopt; // Already that sign
        }
    } else {
        next = m_format.stepDigit(shown, drum - (m_format.signDrum ? 1 : 0), direction);
    }
    return operatorTarget(next);
}

std::optional<qint64> ThumbwheelSwitchPrivate::digitOf(int drum, int digit) const
{
    if (isSignDrum(drum)) {
        return std::nullopt;
    }
    return operatorTarget(m_format.withDigit(shownCount(), drum - (m_format.signDrum ? 1 : 0), digit));
}

void ThumbwheelSwitchPrivate::setShown(ThumbwheelSwitch *q, qint64 count)
{
    ChangeScope scope(q, this);
    if (m_applyMode == ThumbwheelSwitch::ApplyMode::Immediate) {
        m_count = count;
        m_pending.reset();
    } else {
        m_pending = count; // Dropped by emitChanges() if equal to the value
    }
    scope.emitChanges();
}

bool ThumbwheelSwitchPrivate::apply(ThumbwheelSwitch *q)
{
    if (!m_pending) {
        return false;
    }
    ChangeScope scope(q, this);
    m_count = *m_pending;
    m_pending.reset();
    scope.emitChanges();
    Q_EMIT q->valueCommitted(value());
    return true;
}

void ThumbwheelSwitchPrivate::endAction(ThumbwheelSwitch *q)
{
    if (!m_keyAction) return;
    m_keyAction = false;
    m_actionKey = 0;
    if (m_applyMode == ThumbwheelSwitch::ApplyMode::Immediate && m_count != m_actionStartCount) {
        Q_EMIT q->valueCommitted(value());
    }
}

ThumbwheelSwitchPrivate::Fonts ThumbwheelSwitchPrivate::fontsFor(const QFont &widgetFont, double logicalDpi)
{
    return internal::panelFonts(widgetFont, logicalDpi, kLabelPx, kDigitPx);
}

QString ThumbwheelSwitchPrivate::statusText(QChar digitChar) const
{
    QString s = QStringLiteral("SET?  now ") + m_format.text(m_count, digitChar);
    if (!m_unit.isEmpty()) {
        s += QLatin1Char(' ') + m_unit;
    }
    return s;
}

internal::ThumbwheelLayoutInput ThumbwheelSwitchPrivate::layoutInput(double w, double h, const Fonts &fonts) const
{
    internal::ThumbwheelLayoutInput in;
    in.width = w;
    in.height = h;
    in.format = m_format;
    in.setButton = m_applyMode == ThumbwheelSwitch::ApplyMode::OnConfirm;
    in.statusLine = in.setButton || m_readOnly;
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
    in.text.setWidth = in.setButton ? textWidth(lm, kSetText) : 0.0;
    // The widest status line: the widest digit on every drum (the status line shows the applied value)
    in.text.statusWidth = in.setButton ? textWidth(lm, statusText(widestDigit(lm))) : 0.0;
    return in;
}

internal::ThumbwheelLayout ThumbwheelSwitchPrivate::currentLayout(const ThumbwheelSwitch *q) const
{
    const Fonts fonts = fontsFor(q->font(), q->logicalDpiY());
    return internal::layoutThumbwheel(layoutInput(q->width(), q->height(), fonts));
}

ThumbwheelSwitch::ThumbwheelSwitch(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ThumbwheelSwitchPrivate>())
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus); // #182, #183: the keys need focus; the ring shows it
    internal::ThumbwheelSwitchAccessible::installFactory(); // #183
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
QColor ThumbwheelSwitch::pendingColor() const { return d_ptr->m_pendingColor; }
ThumbwheelSwitch::ApplyMode ThumbwheelSwitch::applyMode() const { return d_ptr->m_applyMode; }
bool ThumbwheelSwitch::hasPendingValue() const { return d_ptr->m_pending.has_value(); }
double ThumbwheelSwitch::pendingValue() const { return d_ptr->shownValue(); }
bool ThumbwheelSwitch::isReadOnly() const { return d_ptr->m_readOnly; }
bool ThumbwheelSwitch::isWheelEnabled() const { return d_ptr->m_wheelEnabled; }

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
        scope.emitChanges(); // The overflow mark: a pending value is discarded
        return;
    }
    d_ptr->m_count = *count;
    d_ptr->m_rejectedOverflow = false;
    scope.emitChanges(); // A pending value stays, unless it is now equal to the value
}

void ThumbwheelSwitch::setDigitCount(int count)
{
    count = std::clamp(count, 1, internal::kThumbwheelMaxDigits);
    if (count == d_ptr->m_format.digitCount) return;
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_pending.reset(); // The drums change: a pending value is discarded
    d_ptr->m_format.digitCount = count;
    if (d_ptr->m_format.decimals > count - 1) {
        const double v = d_ptr->value();
        d_ptr->m_format.decimals = count - 1;
        d_ptr->m_count = d_ptr->m_format.countOf(v).value_or(d_ptr->m_count);
    }
    d_ptr->m_current = std::min(d_ptr->m_current, d_ptr->drumCount() - 1);
    updateGeometry();
    Q_EMIT appearanceChanged();
    scope.emitChanges();
}

void ThumbwheelSwitch::setDecimals(int decimals)
{
    decimals = std::clamp(decimals, 0, d_ptr->m_format.digitCount - 1);
    if (decimals == d_ptr->m_format.decimals) return;
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_pending.reset();
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
    d_ptr->m_pending.reset();
    d_ptr->m_format.signDrum = visible;
    d_ptr->m_current = std::min(d_ptr->m_current, d_ptr->drumCount() - 1);
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
QIW_THUMBWHEEL_COLOR_SETTER(setPendingColor, m_pendingColor)

#undef QIW_THUMBWHEEL_COLOR_SETTER

void ThumbwheelSwitch::setApplyMode(ApplyMode mode)
{
    if (mode == d_ptr->m_applyMode) return;
    d_ptr->endAction(this);
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_pending.reset();
    d_ptr->m_applyMode = mode;
    updateGeometry(); // The SET button and the status line
    Q_EMIT appearanceChanged();
    scope.emitChanges();
}

void ThumbwheelSwitch::applyPending()
{
    d_ptr->apply(this);
}

void ThumbwheelSwitch::discardPending()
{
    if (!d_ptr->m_pending) return;
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_pending.reset();
    scope.emitChanges();
}

void ThumbwheelSwitch::setReadOnly(bool readOnly)
{
    if (d_ptr->m_readOnly == readOnly) return;
    if (readOnly) {
        d_ptr->endAction(this); // #182: a key action running ends, and commits if it changed the value
        d_ptr->m_armed = Part::None;
    }
    ChangeScope scope(this, d_ptr.get());
    d_ptr->m_pending.reset();
    d_ptr->m_readOnly = readOnly;
    d_ptr->m_wheelRemainder = 0;
    updateGeometry(); // In Immediate, the status line holds the padlock
    Q_EMIT readOnlyChanged(readOnly);
    d_ptr->notifyReadOnly(this); // #183
    scope.emitChanges();
}

void ThumbwheelSwitch::setWheelEnabled(bool enabled)
{
    if (d_ptr->m_wheelEnabled == enabled) return;
    d_ptr->m_wheelEnabled = enabled;
    d_ptr->m_wheelRemainder = 0;
    Q_EMIT appearanceChanged();
}

void ThumbwheelSwitch::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}

void ThumbwheelSwitch::mousePressEvent(QMouseEvent *event)
{
    if (d_ptr->m_readOnly) {
        event->ignore(); // #182: read-only, the press goes to the parent
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    // #182: a press never changes the value; it arms a button, whose release acts
    d_ptr->endAction(this); // A key held while the button is pressed ends here
    const Hit hit = hitOf(d_ptr->currentLayout(this), event->position());
    d_ptr->m_armed = hit.part;
    d_ptr->m_armedDrum = hit.drum;
    if (hit.drum >= 0) {
        d_ptr->m_current = hit.drum;
    }
    update();
    event->accept();
}

void ThumbwheelSwitch::mouseReleaseEvent(QMouseEvent *event)
{
    if (d_ptr->m_readOnly) {
        event->ignore();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const Part armed = d_ptr->m_armed;
    const int drum = d_ptr->m_armedDrum;
    d_ptr->m_armed = Part::None;
    d_ptr->m_armedDrum = -1;
    const Hit hit = hitOf(d_ptr->currentLayout(this), event->position());
    if (armed != Part::None && hit.part == armed && hit.drum == drum) {
        if (armed == Part::Set) {
            d_ptr->apply(this);
        } else if (const std::optional<qint64> next = d_ptr->stepOf(drum, armed == Part::Plus ? 1 : -1)) {
            const qint64 before = d_ptr->m_count;
            d_ptr->setShown(this, *next);
            if (d_ptr->m_applyMode == ApplyMode::Immediate && d_ptr->m_count != before) {
                Q_EMIT valueCommitted(value()); // #182: the release of a button is the end of its action
            }
        }
    }
    update();
    event->accept();
}

void ThumbwheelSwitch::wheelEvent(QWheelEvent *event)
{
    // #182: off by default, and only while the switch has focus; otherwise ignored, so that the parent (a scroll area)
    // gets it. It acts on the decade under the pointer; outside the drums and their buttons it reaches the parent
    if (!d_ptr->m_wheelEnabled || d_ptr->m_readOnly || !hasFocus()) {
        event->ignore();
        return;
    }
    const int drum = columnOf(d_ptr->currentLayout(this), event->position());
    if (drum < 0) {
        event->ignore();
        return;
    }
    d_ptr->m_current = drum;
    internal::applyWheelNotches(d_ptr->m_wheelRemainder, event->angleDelta().y(), [this] { d_ptr->endAction(this); },
                                [this, drum](int direction) {
                                    const std::optional<qint64> next = d_ptr->stepOf(drum, direction);
                                    if (!next) {
                                        return;
                                    }
                                    const qint64 before = d_ptr->m_count;
                                    d_ptr->setShown(this, *next);
                                    if (d_ptr->m_applyMode == ApplyMode::Immediate && d_ptr->m_count != before) {
                                        Q_EMIT valueCommitted(value()); // #182: each notch commits
                                    }
                                });
    update();
    event->accept();
}

void ThumbwheelSwitch::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const bool enter = key == Qt::Key_Return || key == Qt::Key_Enter;
    if (!isValueKey(key) && !isMoveKey(key) && !enter && key != Qt::Key_Escape) {
        QWidget::keyPressEvent(event); // PageUp, PageDown, Tab and the others
        return;
    }
    if (d_ptr->m_readOnly) {
        event->ignore(); // #182: read-only, the key goes to the parent
        return;
    }
    if (enter || key == Qt::Key_Escape) {
        // OnConfirm: Enter applies and Escape discards a pending value; without one the key goes to the parent (the
        // default button or the closing of a dialog)
        if (!d_ptr->m_pending) {
            event->ignore();
            return;
        }
        if (enter) {
            d_ptr->apply(this);
        } else {
            discardPending();
        }
        event->accept();
        return;
    }
    const int last = d_ptr->drumCount() - 1;
    if (isMoveKey(key)) {
        // #154: Home and End move the current decade, not the value (an approved deviation from #182)
        int &current = d_ptr->m_current;
        switch (key) {
        case Qt::Key_Left: current = std::max(0, current - 1); break;
        case Qt::Key_Right: current = std::min(last, current + 1); break;
        case Qt::Key_Home: current = 0; break;
        default: current = last; break;
        }
        update();
        event->accept();
        return;
    }
    // #182: a key action starts at the first press of a key and ends at its release that is not an auto-repeat
    if (!d_ptr->m_keyAction) {
        d_ptr->m_keyAction = true;
        d_ptr->m_actionStartCount = d_ptr->m_count;
    }
    d_ptr->m_actionKey = key;
    const int current = d_ptr->m_current;
    std::optional<qint64> next;
    if (isDigitKey(key)) {
        const int digit = key - Qt::Key_0;
        if (!d_ptr->isSignDrum(current) && !d_ptr->overflow()
            && d_ptr->m_format.digit(d_ptr->shownCount(), current - (d_ptr->m_format.signDrum ? 1 : 0)) == digit) {
            d_ptr->m_current = std::min(last, current + 1); // The same digit: nothing to change, on to the next
        } else if ((next = d_ptr->digitOf(current, digit))) {
            d_ptr->setShown(this, *next);
            d_ptr->m_current = std::min(last, current + 1);
        }
    } else if (key == Qt::Key_Plus || key == Qt::Key_Minus) {
        if (d_ptr->isSignDrum(current) && (next = d_ptr->stepOf(current, key == Qt::Key_Plus ? 1 : -1))) {
            d_ptr->setShown(this, *next);
        }
    } else if ((next = d_ptr->stepOf(current, key == Qt::Key_Up ? 1 : -1))) {
        d_ptr->setShown(this, *next);
    }
    update();
    event->accept();
}

void ThumbwheelSwitch::keyReleaseEvent(QKeyEvent *event)
{
    if (!isValueKey(event->key())) {
        QWidget::keyReleaseEvent(event);
        return;
    }
    if (d_ptr->m_readOnly) {
        event->ignore();
        return;
    }
    if (!event->isAutoRepeat() && d_ptr->m_keyAction && event->key() == d_ptr->m_actionKey) {
        d_ptr->endAction(this);
    }
    event->accept();
}

void ThumbwheelSwitch::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update(); // The focus ring and the bar of the current decade
}

void ThumbwheelSwitch::focusOutEvent(QFocusEvent *event)
{
    d_ptr->endAction(this); // #182: the release will not reach the switch; a pending value stays pending
    d_ptr->m_armed = Part::None;
    d_ptr->m_wheelRemainder = 0;
    QWidget::focusOutEvent(event);
    update();
}

void ThumbwheelSwitch::hideEvent(QHideEvent *event)
{
    d_ptr->endAction(this);
    d_ptr->m_armed = Part::None;
    QWidget::hideEvent(event);
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
    const bool pending = d_ptr->m_pending.has_value();
    const qint64 shown = d_ptr->shownCount();
    const double bar = barThickness(in.text.digitPx);

    d_ptr->m_lastLayout = layout;
    d_ptr->m_lastDrums.clear();
    d_ptr->m_lastTag.clear();
    d_ptr->m_lastPlusOpen.clear();
    d_ptr->m_lastMinusOpen.clear();
    d_ptr->m_lastDigitPx = fonts.readout.pixelSize();
    d_ptr->m_lastPendingOutline = pending;
    d_ptr->m_lastStatus.clear();
    d_ptr->m_lastPadlock = false;
    d_ptr->m_lastSetOpen = false;
    d_ptr->m_lastSetDrawn = false;
    d_ptr->m_lastCursor = -1;
    d_ptr->m_lastFocusRing = QRectF();

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

    // 2. The drums: opaque faces (#159), the digits, the sign, or the dash of the overflow. A pending value: a dashed
    // outline in pendingColor(), so that it does not rely on colour
    painter.setFont(fonts.readout);
    for (int i = 0; i < layout.drums.size(); ++i) {
        const QRectF &drum = layout.drums.at(i);
        painter.setBrush(d_ptr->m_drumColor);
        if (pending) {
            painter.setPen(QPen(d_ptr->m_pendingColor, 2.0, Qt::DashLine, Qt::FlatCap, Qt::MiterJoin));
            painter.drawRoundedRect(drum.adjusted(1.0, 1.0, -1.0, -1.0), 2.0, 2.0);
        } else {
            painter.setPen(QPen(d_ptr->m_buttonColor, 1.0));
            painter.drawRoundedRect(drum.adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);
        }
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
            if (shown >= 0) {
                painter.drawRect(QRectF(drum.center().x() - bar * 0.5, drum.center().y() - across.width() * 0.5, bar,
                                        across.width()));
                d_ptr->m_lastDrums << QStringLiteral("+");
            } else {
                d_ptr->m_lastDrums << QStringLiteral("−");
            }
        } else {
            const QString text = QString::number(format.digit(shown, i - (layout.signDrum ? 1 : 0)));
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

    // 4. The tag column: the overflow, or a value shown outside the limits, never told by colour alone
    if (overflow) {
        painter.drawText(layout.tag, Qt::AlignCenter, kOverflowTag);
        d_ptr->m_lastTag = kOverflowTag;
    } else {
        const internal::ThumbwheelLimitState state = d_ptr->limits().stateOf(d_ptr->shownValue());
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

    // 5. The SET button (OnConfirm): filled when a value is pending, an outline when there is nothing to apply
    if (!layout.setButton.isEmpty()) {
        const QRectF &set = layout.setButton;
        if (pending) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(d_ptr->m_buttonColor);
            painter.drawRoundedRect(set, 2.0, 2.0);
            painter.setPen(d_ptr->m_digitColor);
        } else {
            painter.setPen(QPen(d_ptr->m_textColor, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(set.adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);
            painter.setPen(d_ptr->m_textColor);
        }
        painter.drawText(set, Qt::AlignCenter, kSetText);
        d_ptr->m_lastSetDrawn = true;
        d_ptr->m_lastSetOpen = pending;
    }

    // 6. The status line: the padlock of read-only, or the applied value while a value is pending
    if (!layout.status.isEmpty()) {
        if (d_ptr->m_readOnly) {
            internal::drawPadlock(painter, layout.padlock, d_ptr->m_textColor);
            d_ptr->m_lastPadlock = true;
        } else if (pending) {
            const QString text = d_ptr->statusText();
            painter.setPen(d_ptr->m_textColor);
            painter.drawText(layout.status, Qt::AlignLeft | Qt::AlignVCenter, text);
            d_ptr->m_lastStatus = text;
        }
    }

    // 7. Focus (#183): a bar under the current decade, and a solid 2 px ring in textColor() along the edge, in the
    // margin kept free of the drawing
    if (hasFocus()) {
        const int current = std::clamp(d_ptr->m_current, 0, static_cast<int>(layout.cursors.size()) - 1);
        if (current >= 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(d_ptr->m_textColor);
            painter.drawRect(layout.cursors.at(current));
            d_ptr->m_lastCursor = current;
        }
        const QRectF ring = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        if (ring.width() > 0.0 && ring.height() > 0.0) {
            painter.setPen(QPen(d_ptr->m_textColor, 2.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(ring); // On whole pixels at any integer DPR: every pixel of the pen is covered in full
            d_ptr->m_lastFocusRing = ring;
        }
    }
}

} // namespace QtIndustrialWidgets
