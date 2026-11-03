// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/TankDisplay.h>

#include "internal/TankDisplay_p.h"
#include "internal/FontScale.h"
#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/TankDisplayLayout.h"
#include "internal/ValueClassification.h"
#include "internal/ValueStateDrawing.h"

#include <QtCore/QEvent>
#include <QtCore/QStringList>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPolygonF>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {

namespace {

constexpr int kLabelPx = 10;   // Scale labels and limit tags, before the font scale (#51) and the 7 pt floor
constexpr int kReadoutPx = 13; // Readout, before the font scale and the 7 pt floor
constexpr double kPodPadding = 3.0;
const QColor kPodColor(16, 20, 26); // Opaque: the readout contrast does not depend on the background (#159)

// Index of each limit in TankDisplayPrivate::m_limits
enum LimitIndex { LowLowIndex = 0, LowIndex = 1, HighIndex = 2, HighHighIndex = 3 };

bool sameLimit(double a, double b)
{
    return (std::isnan(a) && std::isnan(b)) || a == b;
}

constexpr std::array<const char *, 4> kLimitNames{{"lowLowLimit", "lowLimit", "highLimit", "highHighLimit"}};

// Why the limit \a k breaks the rules: ±Inf, or the order lowLow <= low < high <= highHigh with another enabled
// limit. Empty when it does not
QString limitProblem(const std::array<double, 4> &l, size_t k)
{
    if (k >= l.size()) {
        return {};
    }
    const size_t uk = k;
    if (std::isinf(l[uk])) {
        return QStringLiteral("a limit cannot be infinite");
    }
    if (std::isnan(l[uk])) {
        return {};
    }
    for (size_t uj = 0; uj < l.size(); ++uj) {
        if (uj == uk || std::isnan(l[uj]) || std::isinf(l[uj])) {
            continue;
        }
        const size_t lo = std::min(uj, uk);
        const size_t hi = std::max(uj, uk);
        const bool acrossNormal = lo <= static_cast<size_t>(LowIndex) && hi >= static_cast<size_t>(HighIndex);
        const double a = l[lo];
        const double b = l[hi];
        if (acrossNormal ? !(a < b) : !(a <= b)) {
            const char *op = uk < uj ? (acrossNormal ? "<" : "<=") : (acrossNormal ? ">" : ">=");
            return QStringLiteral("%1 must be %2 %3 (%4)")
                .arg(QLatin1String(kLimitNames[uk]), QLatin1String(op), QLatin1String(kLimitNames[uj]))
                .arg(l[uj]);
        }
    }
    return {};
}

// The enabled limits keep lowLow <= low < high <= highHigh, and none is ±Inf
bool validLimits(const std::array<double, 4> &l)
{
    for (size_t k = 0; k < l.size(); ++k) {
        if (!limitProblem(l, k).isEmpty()) {
            return false;
        }
    }
    return true;
}

int highSeverity(TankDisplay::LimitState s)
{
    return s == TankDisplay::LimitState::HighHigh ? 2 : s == TankDisplay::LimitState::High ? 1 : 0;
}

int lowSeverity(TankDisplay::LimitState s)
{
    return s == TankDisplay::LimitState::LowLow ? 2 : s == TankDisplay::LimitState::Low ? 1 : 0;
}

// Limit state of the input \a raw, given the state in force before it (for the hysteresis on leaving)
TankDisplay::LimitState evaluateLimits(double raw, const std::array<double, 4> &l, double hysteresis,
                                       TankDisplay::LimitState previous)
{
    using LS = TankDisplay::LimitState;
    if (std::isnan(raw)) {
        return LS::Unknown;
    }
    const auto enabled = [&](int i) { return !std::isnan(l[i]); };
    // A high limit is in force when entered (>=), or when it was in force and is not left yet (>= limit - h)
    const auto highActive = [&](int i, int severity) {
        return enabled(i) && (raw >= l[i] || (highSeverity(previous) >= severity && raw >= l[i] - hysteresis));
    };
    const auto lowActive = [&](int i, int severity) {
        return enabled(i) && (raw <= l[i] || (lowSeverity(previous) >= severity && raw <= l[i] + hysteresis));
    };
    const LS high = highActive(HighHighIndex, 2) ? LS::HighHigh : highActive(HighIndex, 1) ? LS::High : LS::Normal;
    const LS low = lowActive(LowLowIndex, 2) ? LS::LowLow : lowActive(LowIndex, 1) ? LS::Low : LS::Normal;
    if (high != LS::Normal && low != LS::Normal) {
        // Only one side can be entered (low < high): a limit entered wins over one held by the hysteresis
        const bool lowEntered = (enabled(LowLowIndex) && raw <= l[LowLowIndex]) || (enabled(LowIndex) && raw <= l[LowIndex]);
        return lowEntered ? low : high;
    }
    return high != LS::Normal ? high : low;
}

int limitIndexInForce(TankDisplay::LimitState s)
{
    switch (s) {
    case TankDisplay::LimitState::LowLow:
        return LowLowIndex;
    case TankDisplay::LimitState::Low:
        return LowIndex;
    case TankDisplay::LimitState::High:
        return HighIndex;
    case TankDisplay::LimitState::HighHigh:
        return HighHighIndex;
    case TankDisplay::LimitState::Unknown:
    case TankDisplay::LimitState::Normal:
        break;
    }
    return -1;
}

const char *const kLimitTags[4] = {"LL", "L", "H", "HH"};

} // namespace

void TankDisplayPrivate::setOneLimit(TankDisplay *q, size_t index, double limit)
{
    static constexpr std::array<const char *, 4> setters{{"setLowLowLimit", "setLowLimit", "setHighLimit", "setHighHighLimit"}};
    if (index >= setters.size()) {
        return;
    }
    std::array<double, 4> limits = m_limits;
    limits[index] = limit;
    const QString problem = limitProblem(limits, index);
    if (!problem.isEmpty()) {
        qWarning("TankDisplay::%s(%s) rejected: %s; the limits are unchanged", setters[index],
                 qPrintable(QString::number(limit)), qPrintable(problem));
        return;
    }
    q->setLimits(limits[0], limits[1], limits[2], limits[3]);
}

TankDisplayPrivate::Changes TankDisplayPrivate::applyInput()
{
    const double newValue = internal::shownValue(m_rawValue, m_minimum, m_maximum);
    const ValueState newState = internal::publicValueState(internal::classifyInput(m_rawValue, m_minimum, m_maximum));
    const TankDisplay::LimitState newLimit = evaluateLimits(m_rawValue, m_limits, m_hysteresis, m_limitState);
    Changes changes;
    changes.value = !internal::sameShownValue(newValue, m_value);
    changes.state = newState != m_valueState;
    changes.limit = newLimit != m_limitState;
    m_value = newValue;
    m_valueState = newState;
    m_limitState = newLimit;
    return changes;
}

bool TankDisplayPrivate::anyLimitEnabled() const
{
    return std::any_of(m_limits.begin(), m_limits.end(), [](double l) { return !std::isnan(l); });
}

QStringList TankDisplayPrivate::majorLabelTexts() const
{
    QStringList texts;
    for (int i = 0; i <= m_majorTicks; ++i) {
        const double v = m_minimum + (m_maximum - m_minimum) * static_cast<double>(i) / m_majorTicks;
        texts << internal::scaleLabelText(v, m_precision);
    }
    return texts;
}

TankDisplayPrivate::Fonts TankDisplayPrivate::fontsFor(const QFont &widgetFont, double logicalDpi)
{
    Fonts f;
    const double scale = internal::fontScale(widgetFont, QApplication::font());
    // #60: no text below 7 pt
    f.minimumPx = internal::minimumReadoutPixelSize(logicalDpi);
    f.label = widgetFont;
    f.label.setBold(true);
    f.label.setPixelSize(std::max(f.minimumPx, internal::scalePixelSize(kLabelPx, scale)));
    f.readout = widgetFont;
    f.readout.setBold(true);
    f.readout.setPixelSize(std::max(f.minimumPx, internal::scalePixelSize(kReadoutPx, scale)));
    // Greyscale antialiasing on the opaque panel (#159)
    f.readout.setStyleStrategy(QFont::StyleStrategy(f.readout.styleStrategy() | QFont::NoSubpixelAntialias));
    return f;
}

internal::TankLayoutInput TankDisplayPrivate::layoutInput(double w, double h, const Fonts &fonts) const
{
    internal::TankLayoutInput in;
    in.width = w;
    in.height = h;
    in.outline = static_cast<internal::TankOutline>(m_shape);
    in.displayVisible = m_digitalDisplayVisible;
    in.scaleVisible = m_scaleVisible;
    in.markersVisible = anyLimitEnabled();
    const QFontMetricsF lm(fonts.label);
    in.text.labelHeight = lm.height();
    if (m_scaleVisible) {
        // The width of the ink box, not its right edge: without installed fonts boundingRect() is placed at 100000
        for (const QString &t : majorLabelTexts()) {
            in.text.labelWidth = std::max({in.text.labelWidth, lm.horizontalAdvance(t), lm.boundingRect(t).width()});
        }
    }
    if (in.markersVisible) {
        for (const char *tag : kLimitTags) {
            const QString t = QString::fromLatin1(tag);
            in.text.tagWidth = std::max({in.text.tagWidth, lm.horizontalAdvance(t), lm.boundingRect(t).width()});
        }
    }
    if (m_digitalDisplayVisible) {
        in.text.podHeight = QFontMetricsF(fonts.readout).height() + 2.0 * kPodPadding;
        // The state marks at the smallest font: "---" and the triangle (internal::drawStateReadout())
        QFont smallest = fonts.readout;
        smallest.setPixelSize(fonts.minimumPx);
        const QFontMetricsF sm(smallest);
        const double marks = std::max(sm.horizontalAdvance(QStringLiteral("---")), sm.capHeight() * 0.9);
        in.text.podMinWidth = marks + 2.0 * std::max(2.0, 0.12 * in.text.podHeight);
    }
    return in;
}

TankDisplay::TankDisplay(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<TankDisplayPrivate>())
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

TankDisplay::~TankDisplay() = default;

TankDisplay::TankShape TankDisplay::shape() const { return d_ptr->m_shape; }
TankDisplay::Content TankDisplay::content() const { return d_ptr->m_content; }
double TankDisplay::minimum() const { return d_ptr->m_minimum; }
double TankDisplay::maximum() const { return d_ptr->m_maximum; }
double TankDisplay::value() const { return d_ptr->m_value; }
double TankDisplay::rawValue() const { return d_ptr->m_rawValue; }
ValueState TankDisplay::valueState() const { return d_ptr->m_valueState; }
int TankDisplay::precision() const { return d_ptr->m_precision; }
QString TankDisplay::unit() const { return d_ptr->m_unit; }
int TankDisplay::majorTicks() const { return d_ptr->m_majorTicks; }
int TankDisplay::minorTicks() const { return d_ptr->m_minorTicks; }
bool TankDisplay::digitalDisplayVisible() const { return d_ptr->m_digitalDisplayVisible; }
bool TankDisplay::scaleVisible() const { return d_ptr->m_scaleVisible; }
double TankDisplay::lowLowLimit() const { return d_ptr->m_limits[LowLowIndex]; }
double TankDisplay::lowLimit() const { return d_ptr->m_limits[LowIndex]; }
double TankDisplay::highLimit() const { return d_ptr->m_limits[HighIndex]; }
double TankDisplay::highHighLimit() const { return d_ptr->m_limits[HighHighIndex]; }
double TankDisplay::limitHysteresis() const { return d_ptr->m_hysteresis; }
TankDisplay::LimitState TankDisplay::limitState() const { return d_ptr->m_limitState; }
QColor TankDisplay::contentColor() const { return d_ptr->m_contentColor; }
QColor TankDisplay::vesselColor() const { return d_ptr->m_vesselColor; }
QColor TankDisplay::scaleColor() const { return d_ptr->m_scaleColor; }
QColor TankDisplay::textColor() const { return d_ptr->m_textColor; }
QColor TankDisplay::readoutColor() const { return d_ptr->m_readoutColor; }
QColor TankDisplay::limitColor() const { return d_ptr->m_limitColor; }

QSize TankDisplay::sizeHint() const
{
    QSize preferred(160, 260);
    if (d_ptr->m_shape == TankShape::HorizontalCylinder) {
        preferred = QSize(260, 200);
    } else if (d_ptr->m_shape == TankShape::Sphere) {
        preferred = QSize(220, 240);
    }
    return preferred.expandedTo(minimumSizeHint());
}

QSize TankDisplay::minimumSizeHint() const
{
    const double dpi = logicalDpiY();
    d_ptr->m_minimumDpi = dpi;
    const TankDisplayPrivate::Fonts fonts = TankDisplayPrivate::fontsFor(font(), dpi);
    const QSizeF s = internal::minimumTankSize(d_ptr->layoutInput(0.0, 0.0, fonts));
    return QSize(static_cast<int>(s.width()), static_cast<int>(s.height()));
}

void TankDisplay::setShape(TankShape shape)
{
    if (d_ptr->m_shape == shape) return;
    d_ptr->m_shape = shape;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void TankDisplay::setContent(Content content)
{
    if (d_ptr->m_content == content) return;
    d_ptr->m_content = content;
    Q_EMIT appearanceChanged();
    update();
}

void TankDisplay::setValue(double value)
{
    const bool sameInput = (std::isnan(value) && std::isnan(d_ptr->m_rawValue)) || value == d_ptr->m_rawValue;
    d_ptr->m_rawValue = value;
    const TankDisplayPrivate::Changes changes = d_ptr->applyInput();
    if (changes.value) {
        Q_EMIT valueChanged(d_ptr->m_value);
    }
    if (changes.state) {
        Q_EMIT valueStateChanged(d_ptr->m_valueState);
    }
    if (changes.limit) {
        Q_EMIT limitStateChanged(d_ptr->m_limitState);
    }
    // An out-of-range input can change the readout without changing value()
    if (!sameInput || changes.limit) {
        update();
    }
}

void TankDisplay::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void TankDisplay::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void TankDisplay::setRange(double min, double max)
{
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max) return;
    if (min == d_ptr->m_minimum && max == d_ptr->m_maximum) return;
    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;
    const TankDisplayPrivate::Changes changes = d_ptr->applyInput();
    updateGeometry(); // the width of the scale labels
    Q_EMIT rangeChanged(d_ptr->m_minimum, d_ptr->m_maximum);
    if (changes.value) {
        Q_EMIT valueChanged(d_ptr->m_value);
    }
    if (changes.state) {
        Q_EMIT valueStateChanged(d_ptr->m_valueState);
    }
    if (changes.limit) {
        Q_EMIT limitStateChanged(d_ptr->m_limitState);
    }
    update();
}

void TankDisplay::setPrecision(int precision)
{
    precision = std::max(0, precision);
    if (d_ptr->m_precision == precision) return;
    d_ptr->m_precision = precision;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void TankDisplay::setUnit(const QString &unit)
{
    if (d_ptr->m_unit == unit) return;
    d_ptr->m_unit = unit;
    Q_EMIT appearanceChanged();
    update();
}

void TankDisplay::setMajorTicks(int count)
{
    if (count < 1 || d_ptr->m_majorTicks == count) return;
    d_ptr->m_majorTicks = count;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void TankDisplay::setMinorTicks(int count)
{
    if (count < 0 || d_ptr->m_minorTicks == count) return;
    d_ptr->m_minorTicks = count;
    Q_EMIT appearanceChanged();
    update();
}

void TankDisplay::setDigitalDisplayVisible(bool visible)
{
    if (d_ptr->m_digitalDisplayVisible == visible) return;
    d_ptr->m_digitalDisplayVisible = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void TankDisplay::setScaleVisible(bool visible)
{
    if (d_ptr->m_scaleVisible == visible) return;
    d_ptr->m_scaleVisible = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

bool TankDisplay::setLimits(double lowLow, double low, double high, double highHigh)
{
    const std::array<double, 4> limits{{lowLow, low, high, highHigh}};
    if (!validLimits(limits)) {
        return false;
    }
    bool changed = false;
    for (int i = 0; i < 4; ++i) {
        changed = changed || !sameLimit(limits[static_cast<size_t>(i)], d_ptr->m_limits[static_cast<size_t>(i)]);
    }
    if (!changed) {
        return true;
    }
    const bool markersBefore = d_ptr->anyLimitEnabled();
    d_ptr->m_limits = limits;
    const TankDisplayPrivate::Changes changes = d_ptr->applyInput();
    if (markersBefore != d_ptr->anyLimitEnabled()) {
        updateGeometry(); // the column of the limit tags
    }
    Q_EMIT limitsChanged();
    if (changes.limit) {
        Q_EMIT limitStateChanged(d_ptr->m_limitState);
    }
    update();
    return true;
}

void TankDisplay::setLowLowLimit(double limit)
{
    d_ptr->setOneLimit(this, LowLowIndex, limit);
}

void TankDisplay::setLowLimit(double limit)
{
    d_ptr->setOneLimit(this, LowIndex, limit);
}

void TankDisplay::setHighLimit(double limit)
{
    d_ptr->setOneLimit(this, HighIndex, limit);
}

void TankDisplay::setHighHighLimit(double limit)
{
    d_ptr->setOneLimit(this, HighHighIndex, limit);
}

void TankDisplay::setLimitHysteresis(double hysteresis)
{
    if (!std::isfinite(hysteresis) || hysteresis < 0.0 || hysteresis == d_ptr->m_hysteresis) return;
    d_ptr->m_hysteresis = hysteresis;
    const TankDisplayPrivate::Changes changes = d_ptr->applyInput();
    Q_EMIT limitsChanged();
    if (changes.limit) {
        Q_EMIT limitStateChanged(d_ptr->m_limitState);
    }
    update();
}

#define QIW_TANK_COLOR_SETTER(Setter, member)                                                                          \
    void TankDisplay::Setter(const QColor &color)                                                                      \
    {                                                                                                                  \
        if (d_ptr->member == color) return;                                                                            \
        d_ptr->member = color;                                                                                         \
        Q_EMIT appearanceChanged();                                                                                    \
        update();                                                                                                      \
    }

QIW_TANK_COLOR_SETTER(setContentColor, m_contentColor)
QIW_TANK_COLOR_SETTER(setVesselColor, m_vesselColor)
QIW_TANK_COLOR_SETTER(setScaleColor, m_scaleColor)
QIW_TANK_COLOR_SETTER(setTextColor, m_textColor)
QIW_TANK_COLOR_SETTER(setReadoutColor, m_readoutColor)
QIW_TANK_COLOR_SETTER(setLimitColor, m_limitColor)

#undef QIW_TANK_COLOR_SETTER

void TankDisplay::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}

void TankDisplay::paintEvent(QPaintEvent *)
{
    // #177: on a screen of another logical DPI the minimum changes (the texts at 7 pt): the layout gets it, once
    if (d_ptr->m_minimumDpi != 0.0 && d_ptr->m_minimumDpi != logicalDpiY()) {
        d_ptr->m_minimumDpi = 0.0;
        updateGeometry();
    }

    const TankDisplayPrivate::Fonts fonts = TankDisplayPrivate::fontsFor(font(), logicalDpiY());
    const internal::TankLayoutInput in = d_ptr->layoutInput(width(), height(), fonts);
    const internal::TankLayout layout = internal::layoutTank(in);
    d_ptr->m_lastLayout = layout;
    const QRectF vessel = layout.vessel;
    if (vessel.width() <= 0.0 || vessel.height() <= 0.0) {
        return;
    }
    const internal::TankOutline outline = in.outline;
    const QPainterPath outlinePath = internal::tankOutlinePath(outline, vessel);
    const ValueState state = d_ptr->m_valueState;
    const double span = d_ptr->m_maximum - d_ptr->m_minimum;
    const auto levelY = [&](double v) { return internal::tankLevelY(vessel, (v - d_ptr->m_minimum) / span); };

    const double dpr = devicePixelRatioF();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. The empty inside, then the fill up to the value (#151: none for an invalid input, hatched instead)
    painter.setPen(Qt::NoPen);
    const QColor inside = d_ptr->m_vesselColor.darker(400);
    painter.setBrush(inside);
    painter.drawPath(outlinePath);
    if (state == ValueState::Invalid) {
        internal::drawInvalidHatch(painter, outlinePath, vessel.width(), internal::tankHatchColor(inside));
    } else {
        const double surface = levelY(d_ptr->m_value);
        if (surface < vessel.bottom()) {
            QPainterPath below;
            below.addRect(QRectF(vessel.left() - 1.0, surface, vessel.width() + 2.0, vessel.bottom() + 1.0 - surface));
            const QPainterPath fill = outlinePath.intersected(below);
            painter.setBrush(d_ptr->m_contentColor);
            painter.drawPath(fill);
            if (d_ptr->m_content == Content::Granular) {
                // Grains on a fixed grid of the vessel, so that they do not move with the level
                painter.save();
                painter.setClipPath(fill);
                const QColor dark = d_ptr->m_contentColor.darker(145);
                const QColor light = d_ptr->m_contentColor.lighter(130);
                const double pitch = 4.0;
                int row = 0;
                for (double y = vessel.bottom() - pitch * 0.5; y > surface - pitch; y -= pitch, ++row) {
                    int col = 0;
                    for (double x = vessel.left() + ((row % 2) ? pitch * 0.5 : 0.0); x < vessel.right() + pitch;
                         x += pitch, ++col) {
                        painter.setBrush(((row * 7 + col * 3) % 5 == 0) ? light : dark);
                        painter.drawEllipse(QPointF(x, y), 0.9, 0.9);
                    }
                }
                painter.restore();
            }
        }
    }

    // 2. The wall
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(d_ptr->m_vesselColor, 2.0));
    painter.drawPath(outlinePath);

    const QFontMetricsF lm(fonts.label);

    // 3. Limits inside the range: a dashed line across the vessel and a marker on its left; the marker of the
    // limit in force filled and tagged, so that the state is not told by colour alone. The limit in force outside
    // the range: its filled and tagged marker at that end of the scale, with a "beyond" sign past it, pointing away
    // from the scale
    const int inForce = limitIndexInForce(d_ptr->m_limitState);
    for (int i = 0; i < 4; ++i) {
        const double limit = d_ptr->m_limits[static_cast<size_t>(i)];
        const bool below = limit < d_ptr->m_minimum;
        const bool above = limit > d_ptr->m_maximum;
        if (std::isnan(limit) || ((below || above) && i != inForce)) {
            continue;
        }
        const double y = below ? vessel.bottom() : above ? vessel.top() : levelY(limit);
        const double tip = layout.markerTipX;
        const double size = internal::kTankMarkerSize;
        const double half = size * 0.5;
        if (!below && !above) {
            // 1 logical px rounded to whole device pixels (at least 1), on whole device rows, so that the line
            // reaches its colour at every device pixel ratio instead of spreading over two rows
            const int rows = std::max(1, static_cast<int>(std::lround(dpr)));
            const double top = std::round(y * dpr - rows * 0.5);
            const double lineY = (top + rows * 0.5) / dpr;
            const double width = rows / dpr;
            QPen pen(d_ptr->m_limitColor, width, Qt::CustomDashLine, Qt::FlatCap);
            pen.setDashPattern({4.0 / width, 2.0 / width}); // 4 px dashes and 2 px gaps at any width
            painter.save();
            painter.setClipPath(outlinePath);
            painter.setPen(pen);
            painter.drawLine(QPointF(vessel.left(), lineY), QPointF(vessel.right(), lineY));
            painter.restore();
        }

        QPolygonF marker;
        marker << QPointF(tip - size, y - half) << QPointF(tip, y) << QPointF(tip - size, y + half);
        painter.setPen(QPen(d_ptr->m_limitColor, internal::kTankMarkerPen, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
        painter.setBrush(i == inForce ? QBrush(d_ptr->m_limitColor) : QBrush(Qt::NoBrush));
        painter.drawPolygon(marker);
        if (below || above) {
            // A filled triangle past the marker, its tip away from the scale
            const double dir = above ? -1.0 : 1.0;
            const double cx = tip - half;
            const double base = y + dir * (half + internal::kTankBeyondGap);
            const double w = size * 0.375;
            QPolygonF sign;
            sign << QPointF(cx - w, base) << QPointF(cx + w, base) << QPointF(cx, base + dir * internal::kTankBeyondSign);
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(sign);
        }
        if (i == inForce) {
            painter.setFont(fonts.label);
            painter.setPen(d_ptr->m_textColor);
            const QRectF tagRect(layout.tagRight - in.text.tagWidth, y - lm.height() * 0.5, in.text.tagWidth, lm.height());
            painter.drawText(tagRect, Qt::AlignRight | Qt::AlignVCenter | Qt::TextDontClip,
                             QString::fromLatin1(kLimitTags[i]));
        }
    }

    // 4. The scale: ticks from the bottom of the vessel (minimum) to its top (maximum), and the labels that keep
    // the minimum gap (#37), centred on their ticks
    if (d_ptr->m_scaleVisible) {
        const int major = d_ptr->m_majorTicks;
        const int divisions = major * (d_ptr->m_minorTicks + 1);
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i <= divisions; ++i) {
            const bool isMajor = i % (d_ptr->m_minorTicks + 1) == 0;
            const double y = internal::tankLevelY(vessel, static_cast<double>(i) / divisions);
            painter.setPen(QPen(isMajor ? d_ptr->m_scaleColor : d_ptr->m_scaleColor.darker(130), isMajor ? 1.6 : 1.0));
            painter.drawLine(QPointF(layout.tickX, y),
                             QPointF(layout.tickX + (isMajor ? internal::kTankMajorTick : internal::kTankMinorTick), y));
        }
        const QStringList texts = d_ptr->majorLabelTexts();
        std::vector<internal::VerticalLabelMetrics> metrics;
        for (int i = 0; i < texts.size(); ++i) {
            metrics.push_back(internal::verticalLabelMetrics(lm.height(), lm.ascent(), -lm.ascent(), lm.descent()));
        }
        const double room = lm.height() * 0.5;
        const internal::ScaleLabelLayout labels = internal::layoutVerticalScaleLabels(
            metrics, vessel.height(), internal::scaleLabelMinGap(lm.height()), internal::VerticalScaleRoom{room, room});
        painter.setFont(fonts.label);
        painter.setPen(d_ptr->m_textColor);
        for (const internal::ScaleLabelSlot &slot : labels.labels) {
            const double centre = vessel.bottom() - (slot.start + slot.end) * 0.5;
            const double baseline = std::round((centre + lm.ascent() - lm.height() * 0.5) * dpr) / dpr;
            painter.drawText(QPointF(layout.labelX, baseline), texts.at(slot.index));
        }
    }

    // 5. The readout on its opaque panel (#159). Not below 7 pt: without room the unit goes first, then the number,
    // never the triangle or "---" (#60, #167)
    if (d_ptr->m_digitalDisplayVisible && !layout.pod.isEmpty()) {
        const QRectF pod = layout.pod;
        painter.setPen(QPen(d_ptr->m_vesselColor.darker(150), 1.0));
        painter.setBrush(kPodColor);
        painter.drawRoundedRect(pod, 3.0, 3.0);

        const QString unit = d_ptr->m_unit;
        const auto withUnit = [&unit](const QString &text) {
            return unit.isEmpty() ? text : QStringLiteral("%1 %2").arg(text, unit);
        };
        const double available = pod.width() - 2.0 * std::max(2.0, std::min(pod.width(), pod.height()) * 0.12);
        QFont f = fonts.readout;
        if (state == ValueState::Valid) {
            const QString number = internal::formatFixed(d_ptr->m_value, d_ptr->m_precision);
            const auto fits = [&](const QString &text, int px) {
                f.setPixelSize(px);
                return QFontMetricsF(f).horizontalAdvance(text) <= available;
            };
            QString text;
            for (const QString &candidate : {withUnit(number), number}) {
                for (int px = fonts.readout.pixelSize(); px >= fonts.minimumPx && text.isEmpty(); --px) {
                    if (fits(candidate, px)) {
                        text = candidate;
                    }
                }
                if (!text.isEmpty()) {
                    break;
                }
            }
            if (!text.isEmpty()) {
                painter.setFont(f);
                painter.setPen(d_ptr->m_readoutColor);
                painter.drawText(pod, Qt::AlignCenter, text);
            }
        } else {
            const internal::InputState inputState = internal::inputStateOf(state);
            QString text = internal::gaugeReadoutText(inputState, d_ptr->m_rawValue, d_ptr->m_minimum,
                                                      d_ptr->m_maximum, d_ptr->m_precision);
            if (state != ValueState::Invalid) {
                // The unit only if the triangle, the number and the unit fit at the smallest font
                f.setPixelSize(fonts.minimumPx);
                const QFontMetricsF fm(f);
                const double triangle = fm.capHeight() * 0.9 * 1.35;
                if (triangle + fm.horizontalAdvance(withUnit(text)) <= available) {
                    text = withUnit(text);
                }
            }
            internal::drawStateReadout(painter, pod, fonts.readout, text, inputState, d_ptr->m_readoutColor,
                                       fonts.minimumPx);
        }
    }
}

} // namespace QtIndustrialWidgets
