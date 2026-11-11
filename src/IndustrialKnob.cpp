// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/IndustrialKnob.h>
#include "internal/IndustrialKnob_p.h"

#include "internal/FontScale.h"
#include "internal/IndustrialKnobAccessible.h"
#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/ValueStateDrawing.h"
#include "internal/WheelNotches.h"

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
#include <map>
#include <utility>
#include <vector>

namespace QtIndustrialWidgets {

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

QString IndustrialKnobPrivate::labelText(int index) const
{
    ++m_labelFormats;
    if (m_mode == IndustrialKnob::KnobMode::Discrete) {
        return QString::number(index + 1);
    }
    const double frac = static_cast<double>(index) / tickCount();
    return internal::scaleLabelText(m_minimum + frac * (m_maximum - m_minimum), m_precision);
}

IndustrialKnobPrivate::MinimumMetrics IndustrialKnobPrivate::minimumMetrics(const QFont &widgetFont) const
{
    QFont labelFont = widgetFont;
    labelFont.setBold(true); // as labelsAt() draws the labels
    MinimumMetrics metrics(labelFont);
    const int ticks = tickCount();
    metrics.labelTexts.reserve(static_cast<size_t>(ticks) + 1);
    for (int i = 0; i <= ticks; ++i) {
        metrics.labelTexts.push_back(labelText(i));
    }
    return metrics;
}

IndustrialKnobPrivate::ScaleLayout IndustrialKnobPrivate::labelsAt(const internal::KnobScaleGeometry &geometry,
                                                                   const QFont &widgetFont, double dpr,
                                                                   MinimumMetrics *metrics) const
{
    if (m_labelInkDpr != dpr || m_labelInk.size() > kMaximumLabelInks) {
        m_labelInk.clear();
        m_labelInkDpr = dpr;
    }

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
        out.texts.push_back(metrics ? metrics->labelTexts[static_cast<size_t>(i)] : labelText(i));
        const QString &text = out.texts.back();
        const QRectF textRect = metrics ? metrics->table.boundingRect(text, geometry.labelFontPx) : fm.boundingRect(text);
        const internal::LayoutRect box = internal::labelBoxOnDevicePixel(
            internal::knobLabelBox(geometry, angleOf(val), textRect.width(), textRect.height()), dpr);
        out.boxes.push_back(box);

        // The glyphs can leave the rectangle of the metrics (CoreText on macOS, see #31 / #35): the edge of the
        // widget is checked against the pixels. From the box to the ink, only the box moves with the radius
        const std::pair<QString, int> key(text, geometry.labelFontPx);
        auto ink = m_labelInk.find(key);
        if (ink == m_labelInk.end()) {
            ink = m_labelInk.emplace(key, internal::measureCenteredTextInk(text, out.font, box.w, box.h, dpr)).first;
                ++m_labelInkMeasures;
        }
        const double centerX = box.x + box.w * 0.5;
        const double centerY = box.y + box.h * 0.5;
        out.ink.push_back({centerX + ink->second.left, centerY + ink->second.top, ink->second.right - ink->second.left,
                           ink->second.bottom - ink->second.top});
    }
    const double labelHeight = metrics ? metrics->table.height(geometry.labelFontPx) : fm.height();
    out.labels = internal::layoutRadialScaleLabels(out.boxes, internal::scaleLabelMinGap(labelHeight));
    return out;
}

std::vector<internal::LayoutRect> IndustrialKnobPrivate::drawnInk(const ScaleLayout &layout)
{
    std::vector<internal::LayoutRect> drawn;
    for (int i : layout.labels.drawn) {
        drawn.push_back(layout.ink[static_cast<size_t>(i)]);
    }
    return drawn;
}

internal::KnobScaleGeometry IndustrialKnobPrivate::fittedGeometry(const QSize &size, const QFont &widgetFont, double dpr,
                                                                  MinimumMetrics *metrics) const
{
    return internal::knobScaleGeometry(size.width(), size.height(), [&](const internal::KnobScaleGeometry &g) {
        return drawnInk(labelsAt(g, widgetFont, dpr, metrics));
    });
}

const IndustrialKnobPrivate::ScaleLayout &IndustrialKnobPrivate::scaleFor(const QSize &size, const QFont &widgetFont, double dpr)
{
    if (!m_scaleValid || m_scaleSize != size || m_scaleDpr != dpr) {
        m_scale = labelsAt(fittedGeometry(size, widgetFont, dpr), widgetFont, dpr);
        m_scaleSize = size;
        m_scaleDpr = dpr;
        m_scaleValid = true;
    }
    return m_scale;
}

bool IndustrialKnobPrivate::widthKeepsScaleEnds(int width, bool lastLabelRequired, const QFont &widgetFont, double dpr,
                                                MinimumMetrics *metrics) const
{
    const int height = minimumHeightOf(width);
    const ScaleLayout layout =
        labelsAt(fittedGeometry(QSize(width, height), widgetFont, dpr, metrics), widgetFont, dpr, metrics);
    const std::vector<int> &drawn = layout.labels.drawn;
    auto shown = [&](int label) { return std::find(drawn.begin(), drawn.end(), label) != drawn.end(); };
    return shown(0) && (!lastLabelRequired || shown(tickCount())) && internal::knobLabelsInside(drawnInk(layout), width, height);
}

int IndustrialKnobPrivate::computeMinimumWidth(int ceiling, const QFont &widgetFont, double dpr) const
{
    // #184: each label formatted once and measured once in the whole search (widths and radii tried), not at every
    // step. The texts and the table are dropped at the end of the computation, so they never hold an earlier font
    // or configuration
    const quint64 formatsBefore = m_labelFormats;
    MinimumMetrics minimum = minimumMetrics(widgetFont);
    MinimumMetrics *metrics = m_minimumWithoutTable ? nullptr : &minimum;
    auto search = [&]() {
        // Ends that lie on each other at the preferred size never both fit (#60, part b): the first one is drawn
        const QSize preferredSize(ceiling, minimumHeightOf(ceiling));
        const ScaleLayout preferred =
            labelsAt(fittedGeometry(preferredSize, widgetFont, dpr, metrics), widgetFont, dpr, metrics);
        const std::vector<int> &drawn = preferred.labels.drawn;
        const bool lastLabelRequired = std::find(drawn.begin(), drawn.end(), tickCount()) != drawn.end();

        // From the top, since a larger size is not always better: the labels grow in steps. The whole
        // range from the result up keeps them, and the ceiling is returned even if it does not
        if (!widthKeepsScaleEnds(ceiling, lastLabelRequired, widgetFont, dpr, metrics)) {
            return ceiling;
        }
        int width = ceiling;
        while (width > kMinimumWidth && widthKeepsScaleEnds(width - 1, lastLabelRequired, widgetFont, dpr, metrics)) {
            --width;
        }
        return width;
    };
    const int width = search();
    m_lastMinimumMeasures = minimum.table.measures();
    m_lastMinimumLabelFormats = m_labelFormats - formatsBefore;
    return width;
}

double IndustrialKnobPrivate::scaleAngleAt(const QPointF &pos, const QSize &size, const QFont &widgetFont, double dpr)
{
    const internal::KnobScaleGeometry &scale = scaleFor(size, widgetFont, dpr).geometry;
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
    internal::IndustrialKnobAccessible::installFactory(); // #183
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
    const double dpr = devicePixelRatioF();
    if (d_ptr->m_minimumWidth == 0 || d_ptr->m_minimumWidthDpr != dpr) {
        d_ptr->m_minimumWidth = d_ptr->computeMinimumWidth(sizeHint().width(), font(), dpr);
        d_ptr->m_minimumWidthDpr = dpr;
    }
    return QSize(d_ptr->m_minimumWidth, IndustrialKnobPrivate::minimumHeightOf(d_ptr->m_minimumWidth));
}

void IndustrialKnob::setValue(double val)
{
    // #182: a control rejects non-finite input, with a warning, and keeps its value
    if (!std::isfinite(val)) {
        qWarning("IndustrialKnob::setValue: non-finite value %s ignored", qPrintable(QString::number(val)));
        return;
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
    d_ptr->notifyValue(this); // #183
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

    const double lowBefore = d_ptr->limitLow();
    const double highBefore = d_ptr->limitHigh();
    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;
    d_ptr->m_value = std::clamp(d_ptr->m_value, d_ptr->m_minimum, d_ptr->m_maximum);

    invalidateCache();
    d_ptr->minimumSizeChanged(this);
    Q_EMIT rangeChanged(d_ptr->m_minimum, d_ptr->m_maximum);
    // #182: the limits are read within the range, so a new range can move them
    if (d_ptr->limitLow() != lowBefore || d_ptr->limitHigh() != highBefore) {
        Q_EMIT valueLimitsChanged(d_ptr->limitLow(), d_ptr->limitHigh());
    }
    Q_EMIT valueChanged(d_ptr->m_value);
    d_ptr->notifyValue(this); // #183: the value, or the range around it, changed
    update();
}

double IndustrialKnob::pageStep() const
{
    Q_D(const IndustrialKnob);
    return d->m_pageStepSet ? d->m_pageStep : d->m_step * 5.0;
}

void IndustrialKnob::setPageStep(double step)
{
    if (!std::isfinite(step) || step <= 0.0) return;
    if (d_ptr->m_pageStepSet && qFuzzyCompare(d_ptr->m_pageStep, step)) return;
    d_ptr->m_pageStep = step;
    d_ptr->m_pageStepSet = true;
    Q_EMIT appearanceChanged();
}

void IndustrialKnob::resetPageStep()
{
    if (!d_ptr->m_pageStepSet) return;
    d_ptr->m_pageStepSet = false;
    Q_EMIT appearanceChanged();
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
    invalidateCache();
    d_ptr->minimumSizeChanged(this);
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
    d_ptr->minimumSizeChanged(this);
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setSpanAngle(double span)
{
    if (!std::isfinite(span) || span <= 0.0 || qFuzzyCompare(d_ptr->m_spanAngle, span)) return;
    d_ptr->m_spanAngle = span;
    invalidateCache();
    d_ptr->minimumSizeChanged(this);
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialKnob::setMajorTicks(int count)
{
    if (d_ptr->m_majorTicks == count || count < 1) return;
    d_ptr->m_majorTicks = count;
    invalidateCache();
    d_ptr->minimumSizeChanged(this);
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
    d_ptr->minimumSizeChanged(this);
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
    d_ptr->minimumSizeChanged(this);
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

bool IndustrialKnob::isReadOnly() const { Q_D(const IndustrialKnob); return d->m_readOnly; }
bool IndustrialKnob::isWheelEnabled() const { Q_D(const IndustrialKnob); return d->m_wheelEnabled; }

double IndustrialKnob::valueLimitLow() const { Q_D(const IndustrialKnob); return d->limitLow(); }
double IndustrialKnob::valueLimitHigh() const { Q_D(const IndustrialKnob); return d->limitHigh(); }

void IndustrialKnob::setReadOnly(bool readOnly)
{
    if (d_ptr->m_readOnly == readOnly) return;
    if (readOnly) {
        d_ptr->endAction(this); // #182: a drag or a held key running now ends, and commits if it changed the value
    }
    d_ptr->m_readOnly = readOnly;
    d_ptr->m_wheelRemainder = 0;
    Q_EMIT readOnlyChanged(readOnly);
    d_ptr->notifyReadOnly(this); // #183
    update();
}

void IndustrialKnob::setWheelEnabled(bool enabled)
{
    if (d_ptr->m_wheelEnabled == enabled) return;
    d_ptr->m_wheelEnabled = enabled;
    d_ptr->m_wheelRemainder = 0;
    Q_EMIT appearanceChanged();
}

void IndustrialKnob::setValueLimits(double low, double high)
{
    if (!std::isfinite(low) || !std::isfinite(high) || low >= high) return;
    const double lowBefore = d_ptr->limitLow();
    const double highBefore = d_ptr->limitHigh();
    d_ptr->m_limitsSet = true;
    d_ptr->m_limitLow = low;
    d_ptr->m_limitHigh = high;
    if (d_ptr->limitLow() != lowBefore || d_ptr->limitHigh() != highBefore) {
        invalidateCache(); // The hatching and the stop marks are in the static scale
        Q_EMIT valueLimitsChanged(d_ptr->limitLow(), d_ptr->limitHigh());
        update();
    }
}

void IndustrialKnob::clearValueLimits()
{
    if (!d_ptr->m_limitsSet) return;
    const double lowBefore = d_ptr->limitLow();
    const double highBefore = d_ptr->limitHigh();
    d_ptr->m_limitsSet = false;
    if (d_ptr->limitLow() != lowBefore || d_ptr->limitHigh() != highBefore) {
        invalidateCache();
        Q_EMIT valueLimitsChanged(d_ptr->limitLow(), d_ptr->limitHigh());
        update();
    }
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
    if (event->type() == QEvent::FontChange) {
        d_ptr->m_labelInk.clear();
        d_ptr->minimumSizeChanged(this);
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
    // #182: relative drag, the value moves by the angle the pointer moves. #175: the shorter way round from the last
    // point, so a jump over the gap between the ends is seen as one. Past an end the value stays there until the
    // pointer comes back on that end, the way it came or after a full turn
    const double span = d_ptr->m_spanAngle;
    const double angle = d_ptr->scaleAngleAt(pos, size(), font(), devicePixelRatioF());
    double dragAngle = d_ptr->m_dragAngle + std::remainder(angle - d_ptr->m_lastPointerAngle, 360.0);
    d_ptr->m_lastPointerAngle = angle;
    if (dragAngle >= span + 360.0) {
        dragAngle -= 360.0;
    } else if (dragAngle <= -360.0) {
        dragAngle += 360.0;
    }
    d_ptr->m_dragAngle = dragAngle;
    // #182: within the value limits; past one, the value stays there until the pointer comes back, as at an end
    setValue(d_ptr->operatorTarget(d_ptr->valueAtScaleAngle(dragAngle)));
}

void IndustrialKnob::mousePressEvent(QMouseEvent *event)
{
    if (d_ptr->m_readOnly) {
        event->ignore(); // #182: read-only, the press goes to the parent
        return;
    }
    if (event->button() == Qt::LeftButton) {
        // #182: a press never changes the value. It starts a relative drag only on the dial as it is drawn, within
        // the radius of its scale geometry; #175: and not in the gap between the ends of the scale
        const internal::KnobScaleGeometry &dial =
            d_ptr->scaleFor(size(), font(), devicePixelRatioF()).geometry;
        const QPointF pos = event->position();
        const bool onDial = std::hypot(pos.x() - dial.centerX, pos.y() - dial.centerY) <= dial.radius;
        const double angle = d_ptr->scaleAngleAt(pos, size(), font(), devicePixelRatioF());
        if (onDial && angle <= d_ptr->m_spanAngle) {
            d_ptr->endAction(this); // A key held while the drag starts ends here
            d_ptr->m_isDragging = true;
            d_ptr->m_lastMousePos = pos;
            d_ptr->m_dragAngle = d_ptr->angleOf(d_ptr->m_value) - d_ptr->m_startAngle;
            d_ptr->m_lastPointerAngle = angle;
            d_ptr->m_actionStartValue = d_ptr->m_value;
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void IndustrialKnob::mouseMoveEvent(QMouseEvent *event)
{
    if (d_ptr->m_readOnly) {
        event->ignore();
        return;
    }
    if (d_ptr->m_isDragging && (event->buttons() & Qt::LeftButton)) {
        updateValueFromPoint(event->position());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void IndustrialKnob::mouseReleaseEvent(QMouseEvent *event)
{
    if (d_ptr->m_readOnly) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        d_ptr->endAction(this);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void IndustrialKnob::wheelEvent(QWheelEvent *event)
{
    // #182: off by default, and only while the knob has focus; otherwise ignored, so that the parent (a scroll area)
    // gets it. Each notch (120 units, summed over the smaller events of a high-resolution wheel or a touchpad) is one
    // step, committed at once
    if (!d_ptr->m_wheelEnabled || d_ptr->m_readOnly || !hasFocus()) {
        event->ignore();
        return;
    }
    internal::applyWheelNotches(d_ptr->m_wheelRemainder, event->angleDelta().y(), [this] { d_ptr->endAction(this); },
                                [this](int direction) {
                                    const double before = d_ptr->m_value;
                                    setValue(d_ptr->operatorTarget(d_ptr->m_value + direction * d_ptr->m_step));
                                    if (!qFuzzyCompare(d_ptr->m_value, before)) {
                                        Q_EMIT valueCommitted(d_ptr->m_value);
                                    }
                                });
    event->accept();
}

namespace {

bool isStepKey(int key)
{
    switch (key) {
    case Qt::Key_Left:
    case Qt::Key_Down:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_PageDown:
    case Qt::Key_PageUp:
    case Qt::Key_Home:
    case Qt::Key_End:
        return true;
    default:
        return false;
    }
}

} // namespace

void IndustrialKnob::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    if (!isStepKey(key)) {
        QWidget::keyPressEvent(event);
        return;
    }
    if (d_ptr->m_readOnly) {
        event->ignore(); // #182: read-only, the key goes to the parent
        return;
    }
    // #182: a key action starts at the first press of a key (or at an auto-repeat, when the first press was not
    // seen) and ends at the release of that key that is not an auto-repeat
    if (!d_ptr->m_keyAction) {
        d_ptr->endAction(this); // A drag running when the key is pressed ends here
        d_ptr->m_keyAction = true;
        d_ptr->m_actionStartValue = d_ptr->m_value;
    }
    d_ptr->m_actionKey = key;
    // #182: within the value limits; Home and End go to them
    double proposed = d_ptr->m_maximum; // Qt::Key_End
    switch (key) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        proposed = d_ptr->m_value - d_ptr->m_step;
        break;
    case Qt::Key_Right:
    case Qt::Key_Up:
        proposed = d_ptr->m_value + d_ptr->m_step;
        break;
    case Qt::Key_PageDown:
        proposed = d_ptr->m_value - pageStep();
        break;
    case Qt::Key_PageUp:
        proposed = d_ptr->m_value + pageStep();
        break;
    case Qt::Key_Home:
        proposed = d_ptr->m_minimum;
        break;
    default:
        break;
    }
    setValue(d_ptr->operatorTarget(proposed));
    event->accept();
}

void IndustrialKnob::keyReleaseEvent(QKeyEvent *event)
{
    if (!isStepKey(event->key())) {
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

void IndustrialKnob::focusOutEvent(QFocusEvent *event)
{
    d_ptr->endAction(this); // #182: the release will not reach the knob
    d_ptr->m_wheelRemainder = 0;
    QWidget::focusOutEvent(event);
}

void IndustrialKnob::hideEvent(QHideEvent *event)
{
    d_ptr->endAction(this);
    QWidget::hideEvent(event);
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
    const IndustrialKnobPrivate::ScaleLayout &layout = d_ptr->scaleFor(targetSize, font(), dpr);
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

    // #182: outside the value limits the band of the ticks is hatched, and each limit inside the range has a radial
    // stop mark that reaches further in than the ticks: the limits do not rely on colour
    const double limitLow = d_ptr->limitLow();
    const double limitHigh = d_ptr->limitHigh();
    if (limitLow > d_ptr->m_minimum || limitHigh < d_ptr->m_maximum) {
        const double inner = scaleRadius - tickLen;
        const auto pointAt = [&](double r, double angle) {
            const double rad = (angle - 90.0) * M_PI / 180.0;
            return QPointF(center.x() + r * std::cos(rad), center.y() + r * std::sin(rad));
        };
        const QRectF outerRect(center.x() - scaleRadius, center.y() - scaleRadius, 2.0 * scaleRadius, 2.0 * scaleRadius);
        const QRectF innerRect(center.x() - inner, center.y() - inner, 2.0 * inner, 2.0 * inner);
        const double spacing = std::max(3.0, radius * 0.05); // Between the hatch lines, at the middle of the band
        const double stepAngle = spacing / ((inner + scaleRadius) * 0.5) * 180.0 / M_PI;
        const auto hatch = [&](double from, double to) {
            const double a0 = valueToAngle(from);
            const double a1 = valueToAngle(to);
            if (a1 <= a0) return;
            // The band between the two angles; QPainterPath angles go counter-clockwise from 3 o'clock
            QPainterPath band;
            band.arcMoveTo(outerRect, 90.0 - a0);
            band.arcTo(outerRect, 90.0 - a0, -(a1 - a0));
            band.arcTo(innerRect, 90.0 - a1, a1 - a0);
            band.closeSubpath();
            painter.save();
            painter.setClipPath(band, Qt::IntersectClip);
            painter.setPen(QPen(d_ptr->m_scaleColor, 1.0));
            for (double a = a0 - stepAngle; a <= a1 + stepAngle; a += stepAngle) {
                painter.drawLine(pointAt(inner, a), pointAt(scaleRadius, a + stepAngle * 1.2));
            }
            painter.restore();
        };
        if (limitLow > d_ptr->m_minimum) hatch(d_ptr->m_minimum, limitLow);
        if (limitHigh < d_ptr->m_maximum) hatch(limitHigh, d_ptr->m_maximum);
        painter.setPen(QPen(d_ptr->m_scaleColor, std::max(2.5, radius * 0.03), Qt::SolidLine, Qt::FlatCap));
        for (const double limit : {limitLow, limitHigh}) {
            if (limit > d_ptr->m_minimum && limit < d_ptr->m_maximum) {
                const double angle = valueToAngle(limit);
                painter.drawLine(pointAt(scaleRadius - tickLen * 1.5, angle), pointAt(scaleRadius, angle));
            }
        }
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
    // #170: a window moved to a monitor of another DPR is repainted. The scale and the ink follow by themselves, but a
    // layout keeps the old minimum until it is told. Here, since a DPR can change on the same screen and has no event
    // of its own before Qt 6.6. Once: the minimum is computed again only when asked for
    if (d_ptr->m_minimumWidth != 0 && d_ptr->m_minimumWidthDpr != devicePixelRatioF()) {
        d_ptr->minimumSizeChanged(this);
    }

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
    const internal::KnobScaleGeometry &scale = d_ptr->scaleFor(size(), font(), devicePixelRatioF()).geometry;
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

    // 3. Focus ring (#183): solid, in textColor, which contrasts with the background like the labels in any theme
    if (hasFocus()) {
        painter.setPen(QPen(d_ptr->m_textColor, 2.0));
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

    // Specular center dot; in read-only the padlock is there instead
    if (!d_ptr->m_readOnly) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 140));
        painter.drawEllipse(QPointF(-1.0, -1.0), 2.5, 2.5);
    }

    painter.restore();

    // #182: read-only. A padlock at the centre of the cap, inside the inner end of the pointer, so the pointer is
    // never covered; a path, so it needs no font
    if (d_ptr->m_readOnly) {
        internal::drawPadlock(painter, IndustrialKnobPrivate::padlockRect(scale), d_ptr->m_textColor);
    }

    // 7. Dynamic Digital Value Readout (Bottom Pod)
    if (d_ptr->m_valueDisplayVisible) {
        double podW = std::clamp(w * 0.65, 50.0, 110.0);
        double podH = 22.0;
        QRectF podRect((w - podW) * 0.5, h - podH - 2.0, podW, podH);

        const QString valStr = d_ptr->readoutText();

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
