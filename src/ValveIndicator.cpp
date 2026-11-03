// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/ValveIndicator.h>

#include "internal/ValveIndicator_p.h"
#include "internal/FontScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/ValueClassification.h"
#include "internal/ValueStateDrawing.h"
#include "internal/ValveIndicatorLayout.h"

#include <QtCore/QEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cmath>

namespace QtIndustrialWidgets {

namespace {

constexpr int kTagPx = 10;     // Status tags, before the font scale (#51) and the 7 pt floor
constexpr int kReadoutPx = 13; // Readout, before the font scale and the 7 pt floor
constexpr double kPodPadding = 3.0;
constexpr double kBodyPen = 2.0;   // Outline of the body and lines of the marks
constexpr double kCrossPen = 2.4;  // Cross of a fault
const QColor kPodColor(16, 20, 26); // Opaque: the readout contrast does not depend on the background (#159)

const QString kUnknownTag = QStringLiteral("?");
const QString kFaultTag = QStringLiteral("FAULT");

} // namespace

ValveIndicatorPrivate::Fonts ValveIndicatorPrivate::fontsFor(const QFont &widgetFont, double logicalDpi)
{
    Fonts f;
    const double scale = internal::fontScale(widgetFont, QApplication::font());
    // #60: no text below 7 pt
    f.minimumPx = internal::minimumReadoutPixelSize(logicalDpi);
    f.tag = widgetFont;
    f.tag.setBold(true);
    f.tag.setPixelSize(std::max(f.minimumPx, internal::scalePixelSize(kTagPx, scale)));
    f.readout = widgetFont;
    f.readout.setBold(true);
    f.readout.setPixelSize(std::max(f.minimumPx, internal::scalePixelSize(kReadoutPx, scale)));
    // Greyscale antialiasing on the opaque panel (#159)
    f.readout.setStyleStrategy(QFont::StyleStrategy(f.readout.styleStrategy() | QFont::NoSubpixelAntialias));
    return f;
}

internal::ValveLayoutInput ValveIndicatorPrivate::layoutInput(double w, double h, const Fonts &fonts) const
{
    internal::ValveLayoutInput in;
    in.width = w;
    in.height = h;
    in.symbol = static_cast<internal::ValveSymbol>(m_type);
    in.vertical = m_orientation == Qt::Vertical;
    in.pipeStubs = m_pipeStubsVisible;
    in.openingShown = openingShown();
    const QFontMetricsF tm(fonts.tag);
    in.text.tagHeight = tm.height();
    // The width of the ink box, not its right edge: without installed fonts boundingRect() is placed at 100000
    for (const QString &t : {kUnknownTag, kFaultTag}) {
        in.text.tagWidth = std::max({in.text.tagWidth, tm.horizontalAdvance(t), tm.boundingRect(t).width()});
    }
    if (in.openingShown) {
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

ValveIndicator::ValveIndicator(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ValveIndicatorPrivate>())
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

ValveIndicator::~ValveIndicator() = default;

ValveIndicator::ValveType ValveIndicator::valveType() const { return d_ptr->m_type; }
ValveIndicator::ValveStatus ValveIndicator::status() const { return d_ptr->m_status; }
double ValveIndicator::opening() const { return d_ptr->m_opening; }
double ValveIndicator::rawOpening() const { return d_ptr->m_rawOpening; }
ValueState ValveIndicator::openingState() const { return d_ptr->m_openingState; }
bool ValveIndicator::openingVisible() const { return d_ptr->m_openingVisible; }
int ValveIndicator::precision() const { return d_ptr->m_precision; }
Qt::Orientation ValveIndicator::orientation() const { return d_ptr->m_orientation; }
bool ValveIndicator::pipeStubsVisible() const { return d_ptr->m_pipeStubsVisible; }
QColor ValveIndicator::openColor() const { return d_ptr->m_openColor; }
QColor ValveIndicator::closedColor() const { return d_ptr->m_closedColor; }
QColor ValveIndicator::transitColor() const { return d_ptr->m_transitColor; }
QColor ValveIndicator::faultColor() const { return d_ptr->m_faultColor; }
QColor ValveIndicator::unknownColor() const { return d_ptr->m_unknownColor; }
QColor ValveIndicator::outlineColor() const { return d_ptr->m_outlineColor; }
QColor ValveIndicator::textColor() const { return d_ptr->m_textColor; }
QColor ValveIndicator::readoutColor() const { return d_ptr->m_readoutColor; }

QSize ValveIndicator::sizeHint() const
{
    const ValveIndicatorPrivate::Fonts fonts = ValveIndicatorPrivate::fontsFor(font(), logicalDpiY());
    const QSizeF s = internal::valveSizeAtScale(d_ptr->layoutInput(0.0, 0.0, fonts), internal::kValvePreferredScale);
    return QSize(static_cast<int>(s.width()), static_cast<int>(s.height())).expandedTo(minimumSizeHint());
}

QSize ValveIndicator::minimumSizeHint() const
{
    const double dpi = logicalDpiY();
    d_ptr->m_minimumDpi = dpi;
    const ValveIndicatorPrivate::Fonts fonts = ValveIndicatorPrivate::fontsFor(font(), dpi);
    const QSizeF s = internal::minimumValveSize(d_ptr->layoutInput(0.0, 0.0, fonts));
    return QSize(static_cast<int>(s.width()), static_cast<int>(s.height()));
}

void ValveIndicator::setValveType(ValveType type)
{
    if (d_ptr->m_type == type) return;
    d_ptr->m_type = type;
    updateGeometry(); // the actuator, the opening of a control valve
    Q_EMIT appearanceChanged();
    update();
}

void ValveIndicator::setStatus(ValveStatus status)
{
    if (d_ptr->m_status == status) return;
    d_ptr->m_status = status;
    Q_EMIT statusChanged(status);
    update();
}

void ValveIndicator::setOpening(double opening)
{
    const bool sameInput = (std::isnan(opening) && std::isnan(d_ptr->m_rawOpening)) || opening == d_ptr->m_rawOpening;
    d_ptr->m_rawOpening = opening;
    const double shown = internal::shownValue(opening, 0.0, 100.0);
    const ValueState state = internal::publicValueState(internal::classifyInput(opening, 0.0, 100.0));
    const bool valueChanged = !internal::sameShownValue(shown, d_ptr->m_opening);
    const bool stateChanged = state != d_ptr->m_openingState;
    d_ptr->m_opening = shown;
    d_ptr->m_openingState = state;
    if (valueChanged) {
        Q_EMIT openingChanged(shown);
    }
    if (stateChanged) {
        Q_EMIT openingStateChanged(state);
    }
    // An out-of-range input can change the readout without changing opening()
    if (!sameInput) {
        update();
    }
}

void ValveIndicator::setOpeningVisible(bool visible)
{
    if (d_ptr->m_openingVisible == visible) return;
    d_ptr->m_openingVisible = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void ValveIndicator::setPrecision(int precision)
{
    precision = std::max(0, precision);
    if (d_ptr->m_precision == precision) return;
    d_ptr->m_precision = precision;
    Q_EMIT appearanceChanged();
    update();
}

void ValveIndicator::setOrientation(Qt::Orientation orientation)
{
    if (d_ptr->m_orientation == orientation) return;
    d_ptr->m_orientation = orientation;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void ValveIndicator::setPipeStubsVisible(bool visible)
{
    if (d_ptr->m_pipeStubsVisible == visible) return;
    d_ptr->m_pipeStubsVisible = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

#define QIW_VALVE_COLOR_SETTER(Setter, member)                                                                         \
    void ValveIndicator::Setter(const QColor &color)                                                                   \
    {                                                                                                                  \
        if (d_ptr->member == color) return;                                                                            \
        d_ptr->member = color;                                                                                         \
        Q_EMIT appearanceChanged();                                                                                    \
        update();                                                                                                      \
    }

QIW_VALVE_COLOR_SETTER(setOpenColor, m_openColor)
QIW_VALVE_COLOR_SETTER(setClosedColor, m_closedColor)
QIW_VALVE_COLOR_SETTER(setTransitColor, m_transitColor)
QIW_VALVE_COLOR_SETTER(setFaultColor, m_faultColor)
QIW_VALVE_COLOR_SETTER(setUnknownColor, m_unknownColor)
QIW_VALVE_COLOR_SETTER(setOutlineColor, m_outlineColor)
QIW_VALVE_COLOR_SETTER(setTextColor, m_textColor)
QIW_VALVE_COLOR_SETTER(setReadoutColor, m_readoutColor)

#undef QIW_VALVE_COLOR_SETTER

void ValveIndicator::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}

void ValveIndicator::paintEvent(QPaintEvent *)
{
    // #177: on a screen of another logical DPI the minimum changes (the texts at 7 pt): the layout gets it, once
    if (d_ptr->m_minimumDpi != 0.0 && d_ptr->m_minimumDpi != logicalDpiY()) {
        d_ptr->m_minimumDpi = 0.0;
        updateGeometry();
    }

    const ValveIndicatorPrivate::Fonts fonts = ValveIndicatorPrivate::fontsFor(font(), logicalDpiY());
    const internal::ValveLayoutInput in = d_ptr->layoutInput(width(), height(), fonts);
    const internal::ValveLayout layout = internal::layoutValve(in);
    d_ptr->m_lastLayout = layout;
    if (!(layout.scale > 0.0)) {
        return;
    }
    const QTransform &t = layout.transform;
    const internal::ValveSymbol symbol = in.symbol;
    const QPainterPath body = t.map(internal::valveBodyPath(symbol));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. The pipe stubs and the marks of the type, in the outline colour
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(d_ptr->m_outlineColor, kBodyPen, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    if (in.pipeStubs) {
        painter.drawPath(t.map(internal::valveStubsPath()));
    }
    painter.drawPath(t.map(internal::valveMarkPath(symbol)));

    // 2. The body in its status: the shape tells it, the colour only helps (#167)
    const auto outline = [&](const QColor &c, Qt::PenStyle style) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(c, kBodyPen, style, Qt::FlatCap, Qt::MiterJoin));
        painter.drawPath(body);
    };
    const auto fill = [&](const QPainterPath &area, const QColor &c) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c);
        painter.drawPath(area);
    };
    const QRectF bodyBox = body.boundingRect();
    switch (d_ptr->m_status) {
    case ValveStatus::Unknown:
        internal::drawInvalidHatch(painter, body, bodyBox.height(), d_ptr->m_unknownColor);
        outline(d_ptr->m_unknownColor, Qt::SolidLine);
        break;
    case ValveStatus::Closed:
        fill(body, d_ptr->m_closedColor);
        outline(d_ptr->m_closedColor, Qt::SolidLine);
        break;
    case ValveStatus::Open:
        outline(d_ptr->m_openColor, Qt::SolidLine);
        break;
    case ValveStatus::Transit:
        fill(t.map(internal::valveUpstreamHalf(symbol)), d_ptr->m_transitColor);
        outline(d_ptr->m_transitColor, Qt::DashLine);
        break;
    case ValveStatus::Fault: {
        outline(d_ptr->m_faultColor, Qt::SolidLine);
        painter.setPen(QPen(d_ptr->m_faultColor, kCrossPen, Qt::SolidLine, Qt::FlatCap));
        QPainterPath cross;
        cross.moveTo(bodyBox.topLeft());
        cross.lineTo(bodyBox.bottomRight());
        cross.moveTo(bodyBox.bottomLeft());
        cross.lineTo(bodyBox.topRight());
        painter.drawPath(cross);
        break;
    }
    }

    // 3. The status tag, for the statuses whose shape needs a word
    const QString tag = d_ptr->m_status == ValveStatus::Unknown ? kUnknownTag
                        : d_ptr->m_status == ValveStatus::Fault ? kFaultTag
                                                                : QString();
    if (!tag.isEmpty() && !layout.tag.isEmpty()) {
        painter.setFont(fonts.tag);
        painter.setPen(d_ptr->m_textColor);
        painter.drawText(layout.tag, Qt::AlignCenter | Qt::TextDontClip, tag);
    }

    if (!in.openingShown || layout.pod.isEmpty()) {
        return;
    }

    // 4. The opening bar: none for an invalid input; at the bound out of range (#151)
    const ValueState state = d_ptr->m_openingState;
    if (state != ValueState::Invalid) {
        const QRectF bar = layout.bar;
        painter.setPen(QPen(d_ptr->m_outlineColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(bar.adjusted(0.5, 0.5, -0.5, -0.5));
        const double fraction = std::clamp(d_ptr->m_opening / 100.0, 0.0, 1.0);
        if (fraction > 0.0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(d_ptr->m_outlineColor);
            painter.drawRect(QRectF(bar.left(), bar.top(), bar.width() * fraction, bar.height()));
        }
    }

    // 5. The readout on its opaque panel (#159). Not below 7 pt: without room the unit goes first, then the number,
    // never the triangle or "---" (#60, #167)
    const QRectF pod = layout.pod;
    painter.setPen(QPen(d_ptr->m_outlineColor.darker(150), 1.0));
    painter.setBrush(kPodColor);
    painter.drawRoundedRect(pod, 3.0, 3.0);
    const auto withUnit = [](const QString &text) { return QStringLiteral("%1 %").arg(text); };
    const double available = pod.width() - 2.0 * std::max(2.0, std::min(pod.width(), pod.height()) * 0.12);
    QFont f = fonts.readout;
    if (state == ValueState::Valid) {
        const QString number = internal::formatFixed(d_ptr->m_opening, d_ptr->m_precision);
        QString text;
        for (const QString &candidate : {withUnit(number), number}) {
            for (int px = fonts.readout.pixelSize(); px >= fonts.minimumPx && text.isEmpty(); --px) {
                f.setPixelSize(px);
                if (QFontMetricsF(f).horizontalAdvance(candidate) <= available) {
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
        QString text = internal::gaugeReadoutText(inputState, d_ptr->m_rawOpening, 0.0, 100.0, d_ptr->m_precision);
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

} // namespace QtIndustrialWidgets
