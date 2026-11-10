// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/IndustrialSlider.h>

#include "internal/IndustrialSlider_p.h"
#include "internal/FontScale.h"
#include "internal/IndustrialSliderLayout.h"
#include "internal/LinearGaugeScale.h"
#include "internal/NumberFormat.h"
#include "internal/RadialGaugeLayout.h"
#include "internal/TankDisplayLayout.h"
#include "internal/ValueClassification.h"
#include "internal/ValueStateDrawing.h"

#include <QtCore/QEvent>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPolygonF>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace QtIndustrialWidgets {

namespace {

constexpr int kLabelPx = 10;   // Scale labels, before the font scale (#51) and the 7 pt floor
constexpr int kReadoutPx = 13; // Readouts, before the font scale and the 7 pt floor
constexpr double kPodPadding = 3.0;
const QColor kPodColor(16, 20, 26); // Opaque: the readout contrast does not depend on the background (#159)

const QString kSetpointTag = QStringLiteral("SP");
const QString kFeedbackTag = QStringLiteral("POS");

// Margin of a readout panel on each side, as internal::drawStateReadout()
double podSideMargin(const QRectF &pod)
{
    return std::max(2.0, std::min(pod.width(), pod.height()) * 0.12);
}

double podSideMargin(double podHeight)
{
    return std::max(2.0, podHeight * 0.12);
}

// The parts of one readout panel, left to right
struct Readout {
    enum class Mark { None, Deviation, Unknown };
    Mark mark{Mark::None};      // Never left out
    QString tag;                // Never left out
    int triangle{0};            // 1 up (over range), -1 down (under range), 0 none; never left out
    QString number;             // Left out after the unit, unless essential
    bool numberEssential{false}; // "---": never left out
    QString unit;               // Left out first
};

// What fits of a readout: the font size and the parts kept
struct FittedReadout {
    int px{0};
    bool number{false};
    bool unit{false};
};

double triangleWidth(const QFontMetricsF &fm)
{
    return fm.capHeight() * 0.9; // as in internal::drawStateReadout()
}

double markWidth(const QFontMetricsF &fm, Readout::Mark mark)
{
    switch (mark) {
    case Readout::Mark::Deviation:
        return triangleWidth(fm);
    case Readout::Mark::Unknown:
        return fm.horizontalAdvance(QLatin1Char('?'));
    case Readout::Mark::None:
        break;
    }
    return 0.0;
}

// Width of the parts of \a r kept, at the metrics \a fm
double readoutWidth(const Readout &r, const QFontMetricsF &fm, bool number, bool unit)
{
    const double space = fm.horizontalAdvance(QLatin1Char(' '));
    double w = 0.0;
    int parts = 0;
    const auto add = [&](double width, double gapBefore) {
        w += (parts > 0 ? gapBefore : 0.0) + width;
        ++parts;
    };
    if (r.mark != Readout::Mark::None) {
        add(markWidth(fm, r.mark), space);
    }
    add(fm.horizontalAdvance(r.tag), space);
    if (r.triangle != 0) {
        add(triangleWidth(fm), space);
    }
    if (number && !r.number.isEmpty()) {
        add(fm.horizontalAdvance(r.number), r.triangle != 0 ? triangleWidth(fm) * 0.35 : space);
    }
    if (number && unit && !r.unit.isEmpty()) {
        add(fm.horizontalAdvance(r.unit), space);
    }
    return w;
}

// The largest readout that fits \a available: from \a maxPx down to \a minPx with the unit, then without it, then
// without the number (unless it is essential). When nothing fits, the essential parts at \a minPx
FittedReadout fitReadout(const Readout &r, QFont font, int maxPx, int minPx, double available)
{
    struct Candidate {
        bool number;
        bool unit;
    };
    std::vector<Candidate> candidates;
    if (!r.number.isEmpty() && !r.unit.isEmpty()) {
        candidates.push_back({true, true});
    }
    if (!r.number.isEmpty()) {
        candidates.push_back({true, false});
    }
    if (!r.numberEssential) {
        candidates.push_back({false, false});
    }
    for (const Candidate &c : candidates) {
        for (int px = maxPx; px >= minPx; --px) {
            font.setPixelSize(px);
            if (readoutWidth(r, QFontMetricsF(font), c.number, c.unit) <= available) {
                return {px, c.number, c.unit};
            }
        }
    }
    return {minPx, r.numberEssential, false};
}

// Draws the parts of \a r kept by \a fit, centred in \a pod. Returns them as text, joined by spaces
QString drawReadout(QPainter &painter, const QRectF &pod, const Readout &r, QFont font, const FittedReadout &fit,
                    const QColor &color)
{
    font.setPixelSize(fit.px);
    const QFontMetricsF fm(font);
    const double space = fm.horizontalAdvance(QLatin1Char(' '));
    const double triW = triangleWidth(fm);
    double x = pod.center().x() - readoutWidth(r, fm, fit.number, fit.unit) * 0.5;
    const double baseline = pod.center().y() - fm.height() * 0.5 + fm.ascent();
    const double midY = baseline - fm.capHeight() * 0.5;
    QStringList drawn;
    int parts = 0;
    const auto advance = [&](double gapBefore) {
        if (parts > 0) {
            x += gapBefore;
        }
        ++parts;
    };

    painter.setFont(font);
    painter.setPen(color);
    if (r.mark == Readout::Mark::Deviation) {
        advance(space);
        // An outlined Δ: the filled triangles are the range marks
        QPolygonF delta;
        delta << QPointF(x, midY + triW * 0.5) << QPointF(x + triW, midY + triW * 0.5) << QPointF(x + triW * 0.5, midY - triW * 0.5);
        painter.save();
        painter.setPen(QPen(color, std::max(1.0, fit.px / 10.0), Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(delta);
        painter.restore();
        x += triW;
        drawn << QStringLiteral("Δ");
    } else if (r.mark == Readout::Mark::Unknown) {
        advance(space);
        painter.drawText(QPointF(x, baseline), QStringLiteral("?"));
        x += fm.horizontalAdvance(QLatin1Char('?'));
        drawn << QStringLiteral("?");
    }
    advance(space);
    painter.drawText(QPointF(x, baseline), r.tag);
    x += fm.horizontalAdvance(r.tag);
    drawn << r.tag;
    if (r.triangle != 0) {
        advance(space);
        const double half = triW * 0.5;
        QPolygonF tri;
        if (r.triangle > 0) {
            tri << QPointF(x, midY + half) << QPointF(x + triW, midY + half) << QPointF(x + half, midY - half);
        } else {
            tri << QPointF(x, midY - half) << QPointF(x + triW, midY - half) << QPointF(x + half, midY + half);
        }
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(tri);
        painter.restore();
        x += triW;
        drawn << (r.triangle > 0 ? QStringLiteral("▲") : QStringLiteral("▼"));
    }
    if (fit.number && !r.number.isEmpty()) {
        advance(r.triangle != 0 ? triW * 0.35 : space);
        painter.drawText(QPointF(x, baseline), r.number);
        x += fm.horizontalAdvance(r.number);
        drawn << r.number;
    }
    if (fit.number && fit.unit && !r.unit.isEmpty()) {
        advance(space);
        painter.drawText(QPointF(x, baseline), r.unit);
        drawn << r.unit;
    }
    return drawn.join(QLatin1Char(' '));
}

// Diagonal lines both ways across \a area, \a spacing apart: the mark of a deviation. 1.2 px wide, as the lines of
// internal::drawInvalidHatch(): a 1 px line at 45° covers at most about 70 % of any pixel at DPR 1, so no pixel would
// reach the colour of the hatch
void drawCrossHatch(QPainter &painter, const QRectF &area, const QColor &color, double spacing)
{
    if (area.isEmpty()) {
        return;
    }
    painter.save();
    painter.setClipRect(area, Qt::IntersectClip);
    painter.setPen(QPen(color, 1.2, Qt::SolidLine, Qt::FlatCap));
    for (double d = -area.height(); d <= area.width(); d += spacing) {
        painter.drawLine(QPointF(area.left() + d, area.bottom()), QPointF(area.left() + d + area.height(), area.top()));
        painter.drawLine(QPointF(area.left() + d, area.top()), QPointF(area.left() + d + area.height(), area.bottom()));
    }
    painter.restore();
}

} // namespace

IndustrialSliderPrivate::Changes IndustrialSliderPrivate::apply()
{
    Changes changes;
    const double setpoint = std::clamp(m_setpoint, m_minimum, m_maximum);
    changes.setpoint = setpoint != m_setpoint;
    m_setpoint = setpoint;

    double feedback = std::numeric_limits<double>::quiet_NaN();
    ValueState state = ValueState::Invalid;
    if (m_hasFeedback) {
        feedback = internal::shownValue(m_rawFeedback, m_minimum, m_maximum);
        state = internal::publicValueState(internal::classifyInput(m_rawFeedback, m_minimum, m_maximum));
    }
    changes.feedback = !internal::sameShownValue(feedback, m_feedback);
    changes.feedbackState = state != m_feedbackState;
    m_feedback = feedback;
    m_feedbackState = state;

    const IndustrialSlider::DeviationState deviation = evaluateDeviation();
    changes.deviation = deviation != m_deviationState;
    m_deviationState = deviation;
    return changes;
}

void IndustrialSliderPrivate::emitChanges(IndustrialSlider *q, const Changes &changes) const
{
    if (changes.setpoint) {
        Q_EMIT q->setpointChanged(m_setpoint);
    }
    if (changes.feedback) {
        Q_EMIT q->feedbackChanged(m_feedback);
    }
    if (changes.feedbackState) {
        Q_EMIT q->feedbackStateChanged(m_feedbackState);
    }
    if (changes.deviation) {
        Q_EMIT q->deviationStateChanged(m_deviationState);
    }
}

IndustrialSlider::DeviationState IndustrialSliderPrivate::evaluateDeviation() const
{
    using DS = IndustrialSlider::DeviationState;
    if (!m_hasFeedback || !(m_deviationThreshold > 0.0)) {
        return DS::Off;
    }
    if (!std::isfinite(m_rawFeedback)) {
        return DS::Unknown;
    }
    return std::abs(m_setpoint - m_rawFeedback) > m_deviationThreshold ? DS::Deviating : DS::Normal;
}

QStringList IndustrialSliderPrivate::majorLabelTexts() const
{
    QStringList texts;
    for (int i = 0; i <= m_majorTicks; ++i) {
        const double v = m_minimum + (m_maximum - m_minimum) * static_cast<double>(i) / m_majorTicks;
        texts << internal::scaleLabelText(v, m_precision);
    }
    return texts;
}

IndustrialSliderPrivate::Fonts IndustrialSliderPrivate::fontsFor(const QFont &widgetFont, double logicalDpi)
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

internal::SliderLayoutInput IndustrialSliderPrivate::layoutInput(double w, double h, const Fonts &fonts) const
{
    internal::SliderLayoutInput in;
    in.width = w;
    in.height = h;
    in.vertical = m_orientation == Qt::Vertical;
    in.readoutVisible = m_readoutVisible;
    in.feedbackReadoutVisible = m_feedbackReadoutVisible;

    const QFontMetricsF lm(fonts.label);
    in.text.labelHeight = lm.height();
    in.text.labelMinGap = internal::scaleLabelMinGap(lm.height());
    const QStringList texts = majorLabelTexts();
    for (int i = 0; i < texts.size(); ++i) {
        // The width of the ink box, not its right edge: without installed fonts boundingRect() is placed at 100000
        const double width = std::max(lm.horizontalAdvance(texts.at(i)), lm.boundingRect(texts.at(i)).width());
        in.text.labelWidth = std::max(in.text.labelWidth, width);
        if (i == 0) {
            in.text.firstLabelWidth = width;
        }
        if (i == texts.size() - 1) {
            in.text.lastLabelWidth = width;
        }
    }

    if (m_readoutVisible) {
        in.text.podHeight = QFontMetricsF(fonts.readout).height() + 2.0 * kPodPadding;
        // What a panel never leaves out, at the smallest font: the tag; for the feedback also the deviation mark and
        // the state mark ("---" or the triangle)
        QFont smallest = fonts.readout;
        smallest.setPixelSize(fonts.minimumPx);
        const QFontMetricsF sm(smallest);
        Readout setpoint;
        setpoint.tag = kSetpointTag;
        double needed = readoutWidth(setpoint, sm, false, false);
        if (m_feedbackReadoutVisible) {
            for (Readout::Mark mark : {Readout::Mark::Deviation, Readout::Mark::Unknown}) {
                Readout invalid;
                invalid.mark = mark;
                invalid.tag = kFeedbackTag;
                invalid.number = QStringLiteral("---");
                invalid.numberEssential = true;
                Readout outOfRange = invalid;
                outOfRange.number.clear();
                outOfRange.numberEssential = false;
                outOfRange.triangle = 1;
                needed = std::max({needed, readoutWidth(invalid, sm, true, false), readoutWidth(outOfRange, sm, false, false)});
            }
        }
        in.text.podMinWidth = needed + 2.0 * podSideMargin(in.text.podHeight);
    }
    return in;
}

IndustrialSlider::IndustrialSlider(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<IndustrialSliderPrivate>())
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
}

IndustrialSlider::~IndustrialSlider() = default;

Qt::Orientation IndustrialSlider::orientation() const { return d_ptr->m_orientation; }
double IndustrialSlider::minimum() const { return d_ptr->m_minimum; }
double IndustrialSlider::maximum() const { return d_ptr->m_maximum; }
double IndustrialSlider::setpoint() const { return d_ptr->m_setpoint; }
double IndustrialSlider::feedback() const { return d_ptr->m_feedback; }
double IndustrialSlider::rawFeedback() const { return d_ptr->m_rawFeedback; }
ValueState IndustrialSlider::feedbackState() const { return d_ptr->m_feedbackState; }
bool IndustrialSlider::hasFeedback() const { return d_ptr->m_hasFeedback; }
double IndustrialSlider::deviationThreshold() const { return d_ptr->m_deviationThreshold; }
IndustrialSlider::DeviationState IndustrialSlider::deviationState() const { return d_ptr->m_deviationState; }
int IndustrialSlider::precision() const { return d_ptr->m_precision; }
QString IndustrialSlider::unit() const { return d_ptr->m_unit; }
int IndustrialSlider::majorTicks() const { return d_ptr->m_majorTicks; }
int IndustrialSlider::minorTicks() const { return d_ptr->m_minorTicks; }
double IndustrialSlider::warningThreshold() const { return d_ptr->m_warningThreshold; }
double IndustrialSlider::errorThreshold() const { return d_ptr->m_errorThreshold; }
bool IndustrialSlider::isReadoutVisible() const { return d_ptr->m_readoutVisible; }
bool IndustrialSlider::isFeedbackReadoutVisible() const { return d_ptr->m_feedbackReadoutVisible; }
QColor IndustrialSlider::handleColor() const { return d_ptr->m_handleColor; }
QColor IndustrialSlider::feedbackColor() const { return d_ptr->m_feedbackColor; }
QColor IndustrialSlider::trackColor() const { return d_ptr->m_trackColor; }
QColor IndustrialSlider::scaleColor() const { return d_ptr->m_scaleColor; }
QColor IndustrialSlider::textColor() const { return d_ptr->m_textColor; }
QColor IndustrialSlider::readoutColor() const { return d_ptr->m_readoutColor; }
QColor IndustrialSlider::normalColor() const { return d_ptr->m_normalColor; }
QColor IndustrialSlider::warningColor() const { return d_ptr->m_warningColor; }
QColor IndustrialSlider::errorColor() const { return d_ptr->m_errorColor; }

QSize IndustrialSlider::sizeHint() const
{
    // Room for the whole readouts at their preferred size: the setpoint at the end values of the scale, with the tag
    // and the unit; the feedback as the widest text it can show at those values and at ±Inf (">max", "<min"), with
    // either deviation mark and a range triangle
    const IndustrialSliderPrivate::Fonts fonts = IndustrialSliderPrivate::fontsFor(font(), logicalDpiY());
    const internal::SliderLayoutInput in = d_ptr->layoutInput(0.0, 0.0, fonts);
    double pod = 0.0;
    if (d_ptr->m_readoutVisible) {
        const QFontMetricsF fm(fonts.readout);
        const double lo = d_ptr->m_minimum;
        const double hi = d_ptr->m_maximum;
        const int precision = d_ptr->m_precision;
        for (double v : {lo, hi}) {
            Readout r;
            r.tag = kSetpointTag;
            r.number = internal::formatFixed(v, precision);
            r.unit = d_ptr->m_unit;
            pod = std::max(pod, readoutWidth(r, fm, true, true));
        }
        if (d_ptr->m_feedbackReadoutVisible) {
            const double inf = std::numeric_limits<double>::infinity();
            const QStringList numbers = {
                internal::formatFixed(lo, precision), internal::formatFixed(hi, precision),
                internal::gaugeReadoutText(internal::inputStateOf(ValueState::Overrange), inf, lo, hi, precision),
                internal::gaugeReadoutText(internal::inputStateOf(ValueState::Underrange), -inf, lo, hi, precision)};
            for (Readout::Mark mark : {Readout::Mark::Deviation, Readout::Mark::Unknown}) {
                for (const QString &number : numbers) {
                    Readout r;
                    r.mark = mark;
                    r.tag = kFeedbackTag;
                    r.triangle = 1;
                    r.number = number;
                    r.unit = d_ptr->m_unit;
                    pod = std::max(pod, readoutWidth(r, fm, true, true));
                }
            }
        }
        pod += 2.0 * podSideMargin(in.text.podHeight);
    }
    const double m = internal::kSliderMargin;
    QSize preferred;
    if (d_ptr->m_orientation == Qt::Vertical) {
        preferred = QSize(static_cast<int>(std::ceil(pod + 2.0 * m)), 260);
    } else {
        const double pods = d_ptr->m_feedbackReadoutVisible ? 2.0 * pod + internal::kSliderPodGap : pod;
        preferred = QSize(std::max(260, static_cast<int>(std::ceil(pods + 2.0 * m))), 0);
    }
    return preferred.expandedTo(minimumSizeHint());
}

QSize IndustrialSlider::minimumSizeHint() const
{
    const double dpi = logicalDpiY();
    d_ptr->m_minimumDpi = dpi;
    const IndustrialSliderPrivate::Fonts fonts = IndustrialSliderPrivate::fontsFor(font(), dpi);
    const QSizeF s = internal::minimumSliderSize(d_ptr->layoutInput(0.0, 0.0, fonts));
    return QSize(static_cast<int>(s.width()), static_cast<int>(s.height()));
}

void IndustrialSlider::setOrientation(Qt::Orientation orientation)
{
    if (d_ptr->m_orientation == orientation) return;
    d_ptr->m_orientation = orientation;
    setSizePolicy(orientation == Qt::Vertical ? QSizePolicy::Maximum : QSizePolicy::Expanding,
                  orientation == Qt::Vertical ? QSizePolicy::Expanding : QSizePolicy::Maximum);
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSlider::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void IndustrialSlider::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void IndustrialSlider::setRange(double min, double max)
{
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max) return;
    if (min == d_ptr->m_minimum && max == d_ptr->m_maximum) return;
    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;
    const IndustrialSliderPrivate::Changes changes = d_ptr->apply();
    updateGeometry(); // the widths of the scale labels
    Q_EMIT rangeChanged(d_ptr->m_minimum, d_ptr->m_maximum);
    d_ptr->emitChanges(this, changes);
    update();
}

void IndustrialSlider::setSetpoint(double setpoint)
{
    // #182: a control rejects non-finite input, with a warning, and keeps its value
    if (!std::isfinite(setpoint)) {
        qWarning("IndustrialSlider::setSetpoint: non-finite value %s ignored", qPrintable(QString::number(setpoint)));
        return;
    }
    const double clamped = std::clamp(setpoint, d_ptr->m_minimum, d_ptr->m_maximum);
    if (clamped == d_ptr->m_setpoint) return;
    d_ptr->m_setpoint = clamped;
    IndustrialSliderPrivate::Changes changes = d_ptr->apply();
    changes.setpoint = true;
    d_ptr->emitChanges(this, changes);
    update();
}

void IndustrialSlider::setFeedback(double feedback)
{
    const bool sameInput = d_ptr->m_hasFeedback
                           && ((std::isnan(feedback) && std::isnan(d_ptr->m_rawFeedback)) || feedback == d_ptr->m_rawFeedback);
    if (sameInput) return;
    d_ptr->m_hasFeedback = true;
    d_ptr->m_rawFeedback = feedback;
    const IndustrialSliderPrivate::Changes changes = d_ptr->apply();
    d_ptr->emitChanges(this, changes);
    // An out-of-range input can change the readout without changing feedback()
    update();
}

void IndustrialSlider::clearFeedback()
{
    if (!d_ptr->m_hasFeedback) return;
    d_ptr->m_hasFeedback = false;
    d_ptr->m_rawFeedback = std::numeric_limits<double>::quiet_NaN();
    const IndustrialSliderPrivate::Changes changes = d_ptr->apply();
    d_ptr->emitChanges(this, changes);
    update();
}

void IndustrialSlider::setDeviationThreshold(double threshold)
{
    if (!std::isfinite(threshold) || threshold < 0.0 || threshold == d_ptr->m_deviationThreshold) return;
    d_ptr->m_deviationThreshold = threshold;
    const IndustrialSliderPrivate::Changes changes = d_ptr->apply();
    Q_EMIT deviationThresholdChanged(d_ptr->m_deviationThreshold);
    d_ptr->emitChanges(this, changes);
    update();
}

void IndustrialSlider::setPrecision(int precision)
{
    precision = std::max(0, precision);
    if (d_ptr->m_precision == precision) return;
    d_ptr->m_precision = precision;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSlider::setUnit(const QString &unit)
{
    if (d_ptr->m_unit == unit) return;
    d_ptr->m_unit = unit;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSlider::setMajorTicks(int count)
{
    if (count < 1 || d_ptr->m_majorTicks == count) return;
    d_ptr->m_majorTicks = count;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSlider::setMinorTicks(int count)
{
    if (count < 0 || d_ptr->m_minorTicks == count) return;
    d_ptr->m_minorTicks = count;
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSlider::setWarningThreshold(double threshold)
{
    if (std::isinf(threshold)) return;
    if ((std::isnan(threshold) && std::isnan(d_ptr->m_warningThreshold)) || threshold == d_ptr->m_warningThreshold) return;
    d_ptr->m_warningThreshold = threshold;
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    update();
}

void IndustrialSlider::setErrorThreshold(double threshold)
{
    if (std::isinf(threshold)) return;
    if ((std::isnan(threshold) && std::isnan(d_ptr->m_errorThreshold)) || threshold == d_ptr->m_errorThreshold) return;
    d_ptr->m_errorThreshold = threshold;
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    update();
}

void IndustrialSlider::setReadoutVisible(bool visible)
{
    if (d_ptr->m_readoutVisible == visible) return;
    d_ptr->m_readoutVisible = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void IndustrialSlider::setFeedbackReadoutVisible(bool visible)
{
    if (d_ptr->m_feedbackReadoutVisible == visible) return;
    d_ptr->m_feedbackReadoutVisible = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

#define QIW_SLIDER_COLOR_SETTER(Setter, member)                                                                        \
    void IndustrialSlider::Setter(const QColor &color)                                                                 \
    {                                                                                                                  \
        if (d_ptr->member == color) return;                                                                            \
        d_ptr->member = color;                                                                                         \
        Q_EMIT appearanceChanged();                                                                                    \
        update();                                                                                                      \
    }

QIW_SLIDER_COLOR_SETTER(setHandleColor, m_handleColor)
QIW_SLIDER_COLOR_SETTER(setFeedbackColor, m_feedbackColor)
QIW_SLIDER_COLOR_SETTER(setTrackColor, m_trackColor)
QIW_SLIDER_COLOR_SETTER(setScaleColor, m_scaleColor)
QIW_SLIDER_COLOR_SETTER(setTextColor, m_textColor)
QIW_SLIDER_COLOR_SETTER(setReadoutColor, m_readoutColor)
QIW_SLIDER_COLOR_SETTER(setNormalColor, m_normalColor)
QIW_SLIDER_COLOR_SETTER(setWarningColor, m_warningColor)
QIW_SLIDER_COLOR_SETTER(setErrorColor, m_errorColor)

#undef QIW_SLIDER_COLOR_SETTER

void IndustrialSlider::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}

void IndustrialSlider::paintEvent(QPaintEvent *)
{
    // #177: on a screen of another logical DPI the minimum changes (the texts at 7 pt): the layout gets it, once
    if (d_ptr->m_minimumDpi != 0.0 && d_ptr->m_minimumDpi != logicalDpiY()) {
        d_ptr->m_minimumDpi = 0.0;
        updateGeometry();
    }

    const IndustrialSliderPrivate::Fonts fonts = IndustrialSliderPrivate::fontsFor(font(), logicalDpiY());
    internal::SliderLayoutInput in = d_ptr->layoutInput(width(), height(), fonts);
    in.devicePixelRatio = devicePixelRatioF();
    const internal::SliderLayout layout = internal::layoutSlider(in);
    d_ptr->m_lastLayout = layout;
    d_ptr->m_lastSetpointReadout.clear();
    d_ptr->m_lastFeedbackReadout.clear();
    d_ptr->m_lastReadoutPx = 0;
    d_ptr->m_lastFeedbackReadoutPx = 0;

    const bool vertical = layout.vertical;
    const double span = d_ptr->m_maximum - d_ptr->m_minimum;
    const auto alongOf = [&](double v) { return layout.along((v - d_ptr->m_minimum) / span); };
    // A rectangle across the scale between two along coordinates and two across coordinates
    const auto band = [&](double a0, double a1, double c0, double c1) {
        const double lo = std::min(a0, a1);
        const double hi = std::max(a0, a1);
        return vertical ? QRectF(c0, lo, c1 - c0, hi - lo) : QRectF(lo, c0, hi - lo, c1 - c0);
    };
    const auto point = [&](double along, double across) {
        return vertical ? QPointF(across, along) : QPointF(along, across);
    };
    const double dpr = devicePixelRatioF();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 1. The track
    painter.setPen(QPen(d_ptr->m_scaleColor.darker(160), 1.0));
    painter.setBrush(d_ptr->m_trackColor);
    painter.drawRect(layout.track);

    // 2. The zone band: normalColor up to the first enabled threshold, then warningColor and errorColor
    const double warning = d_ptr->m_warningThreshold;
    const double error = d_ptr->m_errorThreshold;
    if (!std::isnan(warning) || !std::isnan(error)) {
        const double lo = d_ptr->m_minimum;
        const double hi = d_ptr->m_maximum;
        const auto clampToRange = [&](double v) { return std::clamp(v, lo, hi); };
        const double warningStart = std::isnan(warning) ? hi : clampToRange(warning);
        const double errorStart = std::isnan(error) ? hi : clampToRange(error);
        const double firstStart = std::min(warningStart, errorStart);
        painter.setPen(Qt::NoPen);
        const double c0 = layout.zoneNear;
        const double c1 = c0 + internal::kSliderZone;
        painter.setBrush(d_ptr->m_normalColor);
        painter.drawRect(band(alongOf(lo), alongOf(firstStart), c0, c1));
        if (!std::isnan(warning) && warningStart < errorStart) {
            painter.setBrush(d_ptr->m_warningColor);
            painter.drawRect(band(alongOf(warningStart), alongOf(errorStart), c0, c1));
        }
        if (!std::isnan(error)) {
            painter.setBrush(d_ptr->m_errorColor);
            painter.drawRect(band(alongOf(errorStart), alongOf(hi), c0, c1));
        }
    }

    // 3. The ticks, and the labels that keep the minimum gap (#37)
    const QFontMetricsF lm(fonts.label);
    const int divisions = d_ptr->m_majorTicks * (d_ptr->m_minorTicks + 1);
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i <= divisions; ++i) {
        const bool isMajor = i % (d_ptr->m_minorTicks + 1) == 0;
        const double a = layout.along(static_cast<double>(i) / divisions);
        painter.setPen(QPen(isMajor ? d_ptr->m_scaleColor : d_ptr->m_scaleColor.darker(130), isMajor ? 1.6 : 1.0));
        const double length = isMajor ? internal::kSliderMajorTick : internal::kSliderMinorTick;
        painter.drawLine(point(a, layout.tickNear), point(a, layout.tickNear + length));
    }
    const QStringList texts = d_ptr->majorLabelTexts();
    painter.setFont(fonts.label);
    painter.setPen(d_ptr->m_textColor);
    if (vertical) {
        std::vector<internal::VerticalLabelMetrics> metrics;
        for (int i = 0; i < texts.size(); ++i) {
            metrics.push_back(internal::verticalLabelMetrics(lm.height(), lm.ascent(), -lm.ascent(), lm.descent()));
        }
        const double room = lm.height() * 0.5;
        const internal::ScaleLabelLayout labels = internal::layoutVerticalScaleLabels(
            metrics, layout.scaleLength(), in.text.labelMinGap, internal::VerticalScaleRoom{room, room});
        for (const internal::ScaleLabelSlot &slot : labels.labels) {
            const double centre = layout.scaleMin - (slot.start + slot.end) * 0.5;
            const double baseline = std::round((centre + lm.ascent() - lm.height() * 0.5) * dpr) / dpr;
            painter.drawText(QPointF(layout.labelNear, baseline), texts.at(slot.index));
        }
    } else {
        std::vector<double> extents;
        for (const QString &t : texts) {
            extents.push_back(std::max(lm.horizontalAdvance(t), lm.boundingRect(t).width()));
        }
        const internal::ScaleLabelLayout labels =
            internal::layoutScaleLabels(extents, layout.scaleLength(), in.text.labelMinGap);
        const double baseline = std::round((layout.labelNear + lm.ascent()) * dpr) / dpr;
        for (const internal::ScaleLabelSlot &slot : labels.labels) {
            const QString &t = texts.at(slot.index);
            const double x = layout.scaleMin + (slot.start + slot.end) * 0.5 - lm.horizontalAdvance(t) * 0.5;
            painter.drawText(QPointF(x, baseline), t);
        }
    }

    // 4. The feedback: the track hatched for NaN (no position is shown); otherwise the bar, at the end of the scale
    // with a triangle past it when the input is outside the range. A deviation cross-hatches the track between the
    // handle and the bar. Both hatches are white or black mixed into the track up to 4.5:1, never the text colour,
    // which may be dark (texts on a light panel) as the track is
    const double setpointAt = alongOf(d_ptr->m_setpoint);
    const ValueState fbState = d_ptr->m_feedbackState;
    const QColor hatchColor = internal::hatchColorOn(d_ptr->m_trackColor);
    if (d_ptr->m_hasFeedback) {
        if (fbState == ValueState::Invalid) {
            QPainterPath track;
            track.addRect(layout.track);
            internal::drawInvalidHatch(painter, track, layout.trackFar() - layout.trackNear(), hatchColor);
        } else {
            const double at = alongOf(d_ptr->m_feedback);
            if (d_ptr->m_deviationState == DeviationState::Deviating) {
                drawCrossHatch(painter, band(setpointAt, at, layout.trackNear(), layout.trackFar()), hatchColor, 4.0);
            }
            const QRectF bar = internal::sliderFeedbackBar(layout, at);
            painter.setPen(QPen(d_ptr->m_feedbackColor.darker(170), 1.0));
            painter.setBrush(d_ptr->m_feedbackColor);
            painter.drawRect(bar);
            if (fbState == ValueState::Overrange || fbState == ValueState::Underrange) {
                // Towards the larger values: right (horizontal) or up (vertical, where y decreases)
                const double outward = (fbState == ValueState::Overrange ? 1.0 : -1.0) * (vertical ? -1.0 : 1.0);
                const double base = at + outward * (internal::kSliderBarWidth * 0.5 + 1.0);
                const double tip = base + outward * internal::kSliderBeyondSign;
                const double mid = (layout.trackNear() + layout.trackFar()) * 0.5;
                const double half = (layout.trackFar() - layout.trackNear()) * 0.5;
                QPolygonF sign;
                sign << point(base, mid - half) << point(base, mid + half) << point(tip, mid);
                painter.setPen(Qt::NoPen);
                painter.setBrush(d_ptr->m_feedbackColor);
                painter.drawPolygon(sign);
            }
        }
    }

    // 5. The setpoint handle, over the track and the bar
    painter.setPen(QPen(d_ptr->m_handleColor.darker(170), 1.0));
    painter.setBrush(d_ptr->m_handleColor);
    painter.drawPolygon(internal::sliderHandlePolygon(layout, setpointAt));

    // 6. The readouts on their opaque panels (#159), not below 7 pt: the unit goes first, then the number, never
    // the tag, the deviation mark or the state mark (#60, #167)
    if (d_ptr->m_readoutVisible) {
        const auto panel = [&](const QRectF &pod) {
            painter.setPen(QPen(d_ptr->m_scaleColor.darker(250), 1.0));
            painter.setBrush(kPodColor);
            painter.drawRoundedRect(pod, 3.0, 3.0);
        };
        const int maxPx = fonts.readout.pixelSize();
        if (!layout.setpointPod.isEmpty()) {
            panel(layout.setpointPod);
            Readout r;
            r.tag = kSetpointTag;
            r.number = internal::formatFixed(d_ptr->m_setpoint, d_ptr->m_precision);
            r.unit = d_ptr->m_unit;
            const FittedReadout fit = fitReadout(r, fonts.readout, maxPx, fonts.minimumPx,
                                                 layout.setpointPod.width() - 2.0 * podSideMargin(layout.setpointPod));
            d_ptr->m_lastSetpointReadout =
                drawReadout(painter, layout.setpointPod, r, fonts.readout, fit, d_ptr->m_readoutColor);
            d_ptr->m_lastReadoutPx = fit.px;
        }
        if (!layout.feedbackPod.isEmpty() && d_ptr->m_hasFeedback) {
            panel(layout.feedbackPod);
            Readout r;
            r.tag = kFeedbackTag;
            if (d_ptr->m_deviationState == DeviationState::Deviating) {
                r.mark = Readout::Mark::Deviation;
            } else if (d_ptr->m_deviationState == DeviationState::Unknown) {
                r.mark = Readout::Mark::Unknown;
            }
            const internal::InputState inputState = internal::inputStateOf(fbState);
            r.number = internal::gaugeReadoutText(inputState, d_ptr->m_rawFeedback, d_ptr->m_minimum, d_ptr->m_maximum,
                                                  d_ptr->m_precision);
            if (fbState == ValueState::Invalid) {
                r.numberEssential = true; // "---", without the unit
            } else {
                r.unit = d_ptr->m_unit;
                r.triangle = fbState == ValueState::Overrange ? 1 : fbState == ValueState::Underrange ? -1 : 0;
            }
            const FittedReadout fit = fitReadout(r, fonts.readout, maxPx, fonts.minimumPx,
                                                 layout.feedbackPod.width() - 2.0 * podSideMargin(layout.feedbackPod));
            d_ptr->m_lastFeedbackReadout =
                drawReadout(painter, layout.feedbackPod, r, fonts.readout, fit, d_ptr->m_readoutColor);
            d_ptr->m_lastFeedbackReadoutPx = fit.px;
        }
    }
}

} // namespace QtIndustrialWidgets
