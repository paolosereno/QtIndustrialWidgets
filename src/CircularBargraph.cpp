// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/CircularBargraph.h>

#include "internal/CircularBargraphSegments.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

namespace {
constexpr int kBlinkHalfPeriodMs = 250; // ~2 Hz out-of-range blink

internal::InputState toInputState(CircularBargraph::ValueState st)
{
    switch (st) {
    case CircularBargraph::ValueState::Underrange: return internal::InputState::Underrange;
    case CircularBargraph::ValueState::Overrange: return internal::InputState::Overrange;
    case CircularBargraph::ValueState::Invalid: return internal::InputState::Invalid;
    case CircularBargraph::ValueState::Valid: break;
    }
    return internal::InputState::Valid;
}

CircularBargraph::ValueState toValueState(internal::InputState st)
{
    switch (st) {
    case internal::InputState::Underrange: return CircularBargraph::ValueState::Underrange;
    case internal::InputState::Overrange: return CircularBargraph::ValueState::Overrange;
    case internal::InputState::Invalid: return CircularBargraph::ValueState::Invalid;
    case internal::InputState::Valid: break;
    }
    return CircularBargraph::ValueState::Valid;
}

// value() comparison that treats NaN == NaN (Invalid -> Invalid is not a change).
bool sameValue(double a, double b)
{
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b);
    }
    return qFuzzyCompare(a, b);
}
} // namespace

class CircularBargraphPrivate {
public:
    double m_rawValue{0.0};   // last input, unclamped (may be ±Inf / NaN)
    double m_value{0.0};      // m_rawValue clamped to the range, NaN when Invalid
    CircularBargraph::ValueState m_valueState{CircularBargraph::ValueState::Valid};
    double m_minimum{0.0};
    double m_maximum{100.0};
    int m_segmentCount{24};
    double m_startAngle{-135.0};
    double m_spanAngle{270.0};
    CircularBargraph::DisplayMode m_displayMode{CircularBargraph::DisplayMode::Bar};

    double m_warningThreshold{70.0};
    double m_errorThreshold{85.0};

    bool m_peakHoldEnabled{true};
    int m_peakHoldTimeMs{1200};
    double m_peakDecayRate{25.0};
    double m_peakValue{0.0};

    bool m_digitalDisplayVisible{true};
    int m_precision{1};
    QString m_unit{QStringLiteral("%")};

    // Global color styling
    QColor m_normalColor{QColor(46, 204, 113)};   // Vivid Emerald Green
    QColor m_warningColor{QColor(254, 211, 48)};  // Amber Yellow
    QColor m_errorColor{QColor(235, 59, 90)};     // Danger Red
    QColor m_peakColor{QColor(255, 255, 255)};    // Bright White Peak
    QColor m_ghostColor{QColor(38, 43, 53)};      // Unlit LED segment
    QColor m_dialColor{QColor(24, 28, 36)};       // Deep Slate Chassis
    QColor m_textColor{QColor(245, 246, 250)};    // Crisp Text

    QElapsedTimer m_peakHoldTimer;
    QTimer *m_decayTimer{nullptr};
    // Time of the last decay step on the m_peakHoldTimer clock (ms since the peak was set).
    // Decay starts at max(end of hold, value drop), so the first step never covers the hold.
    qint64 m_lastDecayMs{0};

    QTimer *m_blinkTimer{nullptr};
    bool m_blinkOn{true};

    struct Changes {
        bool value{false};
        bool state{false};
    };
    // Recomputes state, value and peak from m_rawValue and the range. Emits nothing.
    Changes applyInput();
    void updatePeak(double newValue, bool leavingInvalid);
    void updatePeakDecay(CircularBargraph *q);
};

CircularBargraphPrivate::Changes CircularBargraphPrivate::applyInput()
{
    const auto newState = toValueState(internal::classifyInput(m_rawValue, m_minimum, m_maximum));
    const double newValue = (newState == CircularBargraph::ValueState::Invalid)
        ? std::numeric_limits<double>::quiet_NaN()
        : std::clamp(m_rawValue, m_minimum, m_maximum); // ±Inf clamps to the bounds

    const bool wasInvalid = (m_valueState == CircularBargraph::ValueState::Invalid);
    if (newState == CircularBargraph::ValueState::Invalid) {
        // Peak hidden and undefined while the input is invalid
        m_peakValue = newValue;
        m_decayTimer->stop();
    } else {
        updatePeak(newValue, wasInvalid);
    }

    Changes changes;
    changes.value = !sameValue(m_value, newValue);
    changes.state = (newState != m_valueState);
    m_value = newValue;
    m_valueState = newState;

    if (changes.state) {
        m_blinkOn = true;
        if (newState == CircularBargraph::ValueState::Overrange
            || newState == CircularBargraph::ValueState::Underrange) {
            m_blinkTimer->start();
        } else {
            m_blinkTimer->stop();
        }
    }
    return changes;
}

void CircularBargraphPrivate::updatePeak(double newValue, bool leavingInvalid)
{
    // With peak hold disabled the peak is always the value; after an invalid input the
    // first valid value resets the peak.
    if (!m_peakHoldEnabled) {
        m_peakValue = newValue;
    } else if (leavingInvalid || newValue >= m_peakValue) {
        m_peakValue = newValue;
        m_peakHoldTimer.restart();
        m_decayTimer->stop();
    } else if (!m_decayTimer->isActive()) {
        if (!m_peakHoldTimer.isValid()) {
            m_peakHoldTimer.start(); // peak never set by a rise: hold from now
        }
        m_lastDecayMs = std::max<qint64>(m_peakHoldTimeMs, m_peakHoldTimer.elapsed());
        m_decayTimer->start();
    }
}

void CircularBargraphPrivate::updatePeakDecay(CircularBargraph *q)
{
    if (!m_peakHoldEnabled) {
        m_peakValue = m_value;
        if (m_decayTimer) {
            m_decayTimer->stop();
        }
        q->update();
        return;
    }

    if (!m_peakHoldTimer.isValid() || m_valueState == CircularBargraph::ValueState::Invalid) {
        return;
    }
    const qint64 now = m_peakHoldTimer.elapsed();
    if (now < m_peakHoldTimeMs) {
        return;
    }

    double dt = std::max<qint64>(0, now - m_lastDecayMs) / 1000.0;
    m_lastDecayMs = now;
    if (dt > 1.0) {
        dt = 0.033;
    }

    const internal::PeakDecayStep step = internal::decayPeakStep(m_peakValue, m_value, m_peakDecayRate, dt);
    m_peakValue = step.peak;
    if (step.finished && m_decayTimer) {
        m_decayTimer->stop();
    }
    q->update();
}

CircularBargraph::CircularBargraph(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<CircularBargraphPrivate>())
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    d_ptr->m_decayTimer = new QTimer(this);
    d_ptr->m_decayTimer->setInterval(33); // ~30 FPS ballistic decay
    connect(d_ptr->m_decayTimer, &QTimer::timeout, this, [this]() {
        d_ptr->updatePeakDecay(this);
    });

    d_ptr->m_blinkTimer = new QTimer(this);
    d_ptr->m_blinkTimer->setTimerType(Qt::PreciseTimer);
    d_ptr->m_blinkTimer->setInterval(kBlinkHalfPeriodMs);
    connect(d_ptr->m_blinkTimer, &QTimer::timeout, this, [this]() {
        d_ptr->m_blinkOn = !d_ptr->m_blinkOn;
        update();
    });
}

CircularBargraph::~CircularBargraph() = default;

double CircularBargraph::value() const { Q_D(const CircularBargraph); return d->m_value; }
double CircularBargraph::rawValue() const { Q_D(const CircularBargraph); return d->m_rawValue; }
CircularBargraph::ValueState CircularBargraph::valueState() const { Q_D(const CircularBargraph); return d->m_valueState; }
double CircularBargraph::minimum() const { Q_D(const CircularBargraph); return d->m_minimum; }
double CircularBargraph::maximum() const { Q_D(const CircularBargraph); return d->m_maximum; }
double CircularBargraph::peakValue() const { Q_D(const CircularBargraph); return d->m_peakValue; }
int CircularBargraph::segmentCount() const { Q_D(const CircularBargraph); return d->m_segmentCount; }
double CircularBargraph::startAngle() const { Q_D(const CircularBargraph); return d->m_startAngle; }
double CircularBargraph::spanAngle() const { Q_D(const CircularBargraph); return d->m_spanAngle; }
CircularBargraph::DisplayMode CircularBargraph::displayMode() const { Q_D(const CircularBargraph); return d->m_displayMode; }
double CircularBargraph::warningThreshold() const { Q_D(const CircularBargraph); return d->m_warningThreshold; }
double CircularBargraph::errorThreshold() const { Q_D(const CircularBargraph); return d->m_errorThreshold; }
bool CircularBargraph::isPeakHoldEnabled() const { Q_D(const CircularBargraph); return d->m_peakHoldEnabled; }
int CircularBargraph::peakHoldTimeMs() const { Q_D(const CircularBargraph); return d->m_peakHoldTimeMs; }
double CircularBargraph::peakDecayRate() const { Q_D(const CircularBargraph); return d->m_peakDecayRate; }
bool CircularBargraph::isDigitalDisplayVisible() const { Q_D(const CircularBargraph); return d->m_digitalDisplayVisible; }
int CircularBargraph::precision() const { Q_D(const CircularBargraph); return d->m_precision; }
QString CircularBargraph::unit() const { Q_D(const CircularBargraph); return d->m_unit; }

QColor CircularBargraph::normalColor() const { Q_D(const CircularBargraph); return d->m_normalColor; }
QColor CircularBargraph::warningColor() const { Q_D(const CircularBargraph); return d->m_warningColor; }
QColor CircularBargraph::errorColor() const { Q_D(const CircularBargraph); return d->m_errorColor; }
QColor CircularBargraph::peakColor() const { Q_D(const CircularBargraph); return d->m_peakColor; }
QColor CircularBargraph::ghostColor() const { Q_D(const CircularBargraph); return d->m_ghostColor; }
QColor CircularBargraph::dialColor() const { Q_D(const CircularBargraph); return d->m_dialColor; }
QColor CircularBargraph::textColor() const { Q_D(const CircularBargraph); return d->m_textColor; }

QSize CircularBargraph::sizeHint() const
{
    return QSize(180, 180);
}

QSize CircularBargraph::minimumSizeHint() const
{
    return QSize(50, 50);
}

void CircularBargraph::setValue(double val)
{
    d_ptr->m_rawValue = val;
    const auto changes = d_ptr->applyInput();
    if (changes.value) {
        Q_EMIT valueChanged(d_ptr->m_value);
    }
    if (changes.state) {
        Q_EMIT valueStateChanged(d_ptr->m_valueState);
    }
    update();
}

void CircularBargraph::setMinimum(double min)
{
    setRange(min, d_ptr->m_maximum);
}

void CircularBargraph::setMaximum(double max)
{
    setRange(d_ptr->m_minimum, max);
}

void CircularBargraph::setRange(double min, double max)
{
    if (!std::isfinite(min) || !std::isfinite(max) || min >= max) {
        return;
    }
    if (qFuzzyCompare(d_ptr->m_minimum, min) && qFuzzyCompare(d_ptr->m_maximum, max)) {
        return;
    }

    d_ptr->m_minimum = min;
    d_ptr->m_maximum = max;
    if (!std::isnan(d_ptr->m_peakValue)) {
        d_ptr->m_peakValue = std::clamp(d_ptr->m_peakValue, d_ptr->m_minimum, d_ptr->m_maximum);
    }
    // State and value are recomputed from the raw input against the new range
    const auto changes = d_ptr->applyInput();

    Q_EMIT rangeChanged(d_ptr->m_minimum, d_ptr->m_maximum);
    if (changes.value) {
        Q_EMIT valueChanged(d_ptr->m_value);
    }
    if (changes.state) {
        Q_EMIT valueStateChanged(d_ptr->m_valueState);
    }
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setSegmentCount(int count)
{
    if (count < 4 || d_ptr->m_segmentCount == count) {
        return;
    }
    d_ptr->m_segmentCount = count;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setStartAngle(double angle)
{
    if (!std::isfinite(angle) || qFuzzyCompare(d_ptr->m_startAngle, angle)) {
        return;
    }
    d_ptr->m_startAngle = angle;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setSpanAngle(double angle)
{
    if (!std::isfinite(angle) || angle <= 0.0 || angle > 360.0 || qFuzzyCompare(d_ptr->m_spanAngle, angle)) {
        return;
    }
    d_ptr->m_spanAngle = angle;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setDisplayMode(DisplayMode mode)
{
    if (d_ptr->m_displayMode == mode) {
        return;
    }
    d_ptr->m_displayMode = mode;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setWarningThreshold(double threshold)
{
    if (!std::isfinite(threshold) || qFuzzyCompare(d_ptr->m_warningThreshold, threshold)) {
        return;
    }
    d_ptr->m_warningThreshold = threshold;
    // Keep warning <= error: a warning above error drags error up (QAbstractSlider-like)
    if (d_ptr->m_errorThreshold < threshold) {
        d_ptr->m_errorThreshold = threshold;
    }
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setErrorThreshold(double threshold)
{
    if (!std::isfinite(threshold) || qFuzzyCompare(d_ptr->m_errorThreshold, threshold)) {
        return;
    }
    d_ptr->m_errorThreshold = threshold;
    // Keep warning <= error: an error below warning drags warning down
    if (d_ptr->m_warningThreshold > threshold) {
        d_ptr->m_warningThreshold = threshold;
    }
    Q_EMIT thresholdChanged(d_ptr->m_warningThreshold, d_ptr->m_errorThreshold);
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setPeakHoldEnabled(bool enable)
{
    if (d_ptr->m_peakHoldEnabled == enable) {
        return;
    }
    d_ptr->m_peakHoldEnabled = enable;
    if (!enable) {
        d_ptr->m_peakValue = d_ptr->m_value;
        if (d_ptr->m_decayTimer) {
            d_ptr->m_decayTimer->stop();
        }
    }
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setPeakHoldTimeMs(int ms)
{
    if (ms < 0 || d_ptr->m_peakHoldTimeMs == ms) {
        return;
    }
    d_ptr->m_peakHoldTimeMs = ms;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setPeakDecayRate(double rate)
{
    if (!std::isfinite(rate) || rate <= 0.0 || qFuzzyCompare(d_ptr->m_peakDecayRate, rate)) {
        return;
    }
    d_ptr->m_peakDecayRate = rate;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setDigitalDisplayVisible(bool visible)
{
    if (d_ptr->m_digitalDisplayVisible == visible) {
        return;
    }
    d_ptr->m_digitalDisplayVisible = visible;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setPrecision(int prec)
{
    if (prec < 0 || d_ptr->m_precision == prec) {
        return;
    }
    d_ptr->m_precision = prec;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setUnit(const QString &unit)
{
    if (d_ptr->m_unit == unit) {
        return;
    }
    d_ptr->m_unit = unit;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::resetPeak()
{
    d_ptr->m_peakValue = d_ptr->m_value;
    if (d_ptr->m_decayTimer && d_ptr->m_decayTimer->isActive()) {
        d_ptr->m_decayTimer->stop();
    }
    update();
}

void CircularBargraph::setNormalColor(const QColor &color)
{
    if (d_ptr->m_normalColor == color) return;
    d_ptr->m_normalColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setWarningColor(const QColor &color)
{
    if (d_ptr->m_warningColor == color) return;
    d_ptr->m_warningColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setErrorColor(const QColor &color)
{
    if (d_ptr->m_errorColor == color) return;
    d_ptr->m_errorColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setPeakColor(const QColor &color)
{
    if (d_ptr->m_peakColor == color) return;
    d_ptr->m_peakColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setGhostColor(const QColor &color)
{
    if (d_ptr->m_ghostColor == color) return;
    d_ptr->m_ghostColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setDialColor(const QColor &color)
{
    if (d_ptr->m_dialColor == color) return;
    d_ptr->m_dialColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor == color) return;
    d_ptr->m_textColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void CircularBargraph::paintEvent(QPaintEvent *)
{
    if (width() <= 4 || height() <= 4) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double w = width();
    const double h = height();
    const double side = std::min(w, h);
    const QPointF center(w * 0.5, h * 0.5);
    const double radius = (side * 0.5) - 4.0;

    if (radius <= 6.0) {
        return;
    }

    // 1. Dial Background Chassis
    if (d_ptr->m_dialColor.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(d_ptr->m_dialColor);
        painter.drawEllipse(center, radius, radius);
    }

    // 2. Discrete Curved LED Segment Geometry
    const double rOuter = radius * 0.90;
    const double rInner = radius * 0.66;
    const int N = std::max(4, d_ptr->m_segmentCount);
    const double stepAngle = d_ptr->m_spanAngle / static_cast<double>(N);

    // Gap calculation between segments (proportional, clamped)
    double gapAngle = stepAngle * 0.18;
    gapAngle = std::clamp(gapAngle, 0.4, 4.0);
    const double segSpan = std::max(0.2, stepAngle - gapAngle);

    // Colour zones by position: each segment takes the zone containing its centre
    const std::vector<internal::Zone> zones = internal::computeSegmentZones(
        d_ptr->m_minimum, d_ptr->m_maximum, d_ptr->m_warningThreshold, d_ptr->m_errorThreshold, N);
    const auto zoneColor = [this](internal::Zone z) {
        switch (z) {
        case internal::Zone::Warning: return d_ptr->m_warningColor;
        case internal::Zone::Error: return d_ptr->m_errorColor;
        case internal::Zone::Normal: break;
        }
        return d_ptr->m_normalColor;
    };

    internal::SegmentInput segIn;
    segIn.value = d_ptr->m_value;
    segIn.minimum = d_ptr->m_minimum;
    segIn.maximum = d_ptr->m_maximum;
    segIn.peakValue = d_ptr->m_peakValue;
    segIn.segmentCount = N;
    segIn.dotMode = (d_ptr->m_displayMode == DisplayMode::Dot);
    segIn.peakHoldEnabled = d_ptr->m_peakHoldEnabled;
    segIn.state = toInputState(d_ptr->m_valueState);
    segIn.blinkOn = d_ptr->m_blinkOn;
    const std::vector<internal::SegmentState> states = internal::computeSegmentStates(segIn);

    painter.setPen(Qt::NoPen);

    for (int i = 0; i < N; ++i) {
        const double aStart = d_ptr->m_startAngle + i * stepAngle + (gapAngle * 0.5);
        const double aEnd = aStart + segSpan;
        const int arcSteps = std::max(2, static_cast<int>(std::ceil(segSpan / 3.0)));

        QPainterPath path;

        // Outer arc: aStart -> aEnd
        for (int s = 0; s <= arcSteps; ++s) {
            const double ang = aStart + (aEnd - aStart) * (static_cast<double>(s) / arcSteps);
            const double rad = (ang - 90.0) * kPi / 180.0;
            const double px = center.x() + rOuter * std::cos(rad);
            const double py = center.y() + rOuter * std::sin(rad);
            if (s == 0) {
                path.moveTo(px, py);
            } else {
                path.lineTo(px, py);
            }
        }

        // Inner arc: aEnd -> aStart (reversed)
        for (int s = arcSteps; s >= 0; --s) {
            const double ang = aStart + (aEnd - aStart) * (static_cast<double>(s) / arcSteps);
            const double rad = (ang - 90.0) * kPi / 180.0;
            const double px = center.x() + rInner * std::cos(rad);
            const double py = center.y() + rInner * std::sin(rad);
            path.lineTo(px, py);
        }

        path.closeSubpath();

        const QColor segZoneColor = zoneColor(zones[static_cast<size_t>(i)]);
        switch (states[static_cast<size_t>(i)]) {
        case internal::SegmentState::Peak:
            painter.fillPath(path, d_ptr->m_peakColor);
            break;
        case internal::SegmentState::Lit:
            painter.fillPath(path, segZoneColor);
            break;
        case internal::SegmentState::Off:
            painter.fillPath(path, internal::unlitTint(segZoneColor, d_ptr->m_ghostColor));
            break;
        case internal::SegmentState::Ghost:
            painter.fillPath(path, d_ptr->m_ghostColor);
            break;
        }
    }

    // 3. Optional Centered Digital Readout Pod
    if (d_ptr->m_digitalDisplayVisible && rInner > 15.0) {
        const QString valStr = internal::formatReadout(toInputState(d_ptr->m_valueState), d_ptr->m_value,
                                                       d_ptr->m_minimum, d_ptr->m_maximum,
                                                       d_ptr->m_precision);
        const double podRadius = rInner * 0.90;

        // Dark recessed pod circular well
        painter.setBrush(d_ptr->m_dialColor.darker(130));
        painter.drawEllipse(center, podRadius, podRadius);

        const double fontSize = std::max(7.0, rInner * 0.36);
        QFont valFont = font();
        valFont.setBold(true);
        valFont.setPointSizeF(fontSize);
        painter.setFont(valFont);
        painter.setPen(d_ptr->m_textColor);

        if (d_ptr->m_unit.isEmpty()) {
            const QRectF valRect(center.x() - podRadius, center.y() - podRadius, podRadius * 2.0, podRadius * 2.0);
            painter.drawText(valRect, Qt::AlignCenter, valStr);
        } else {
            const QRectF valRect(center.x() - podRadius, center.y() - podRadius * 0.75, podRadius * 2.0, podRadius * 0.9);
            painter.drawText(valRect, Qt::AlignCenter, valStr);

            QFont unitFont = font();
            unitFont.setPointSizeF(std::max(5.5, fontSize * 0.52));
            painter.setFont(unitFont);
            painter.setPen(d_ptr->m_textColor.darker(125));

            const QRectF unitRect(center.x() - podRadius, center.y() + podRadius * 0.10, podRadius * 2.0, podRadius * 0.65);
            painter.drawText(unitRect, Qt::AlignCenter, d_ptr->m_unit);
        }
    }
}

void CircularBargraph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

} // namespace QtIndustrialWidgets
