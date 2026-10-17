// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/AnalysisChart.h>

#include "internal/AnalysisChart_p.h"
#include "internal/AnalysisChartView.h"
#include "internal/FontScale.h"

#include <QtCore/QEvent>
#include <QtCore/QtDebug>
#include <QtGui/QFontMetricsF>
#include <QtGui/QPainter>
#include <QtGui/QPolygonF>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {

namespace {

constexpr qint64 kDefaultSpanNs = 1000000000; // view without data: 1 s

// Pixel size of the axis and legend labels (as StripChart), scaled by the widget font (#51 rule (b))
QFont labelFont(const QWidget &w, double scale)
{
    QFont f = w.font();
    f.setPixelSize(internal::scalePixelSize(9, scale));
    return f;
}

} // namespace

AnalysisChart::AnalysisChart(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<AnalysisChartPrivate>())
{
    d_ptr->view = {0, kDefaultSpanNs};
}

AnalysisChart::~AnalysisChart() = default;

// --- Channels ---

int AnalysisChart::addChannel(const QString &name, const QColor &color, double penWidth)
{
    AnalysisChartPrivate::Channel ch;
    ch.id = d_ptr->nextId++;
    ch.name = name;
    ch.color = color;
    ch.penWidth = (std::isfinite(penWidth) && penWidth > 0.0) ? penWidth : 1.8;
    d_ptr->channels.push_back(ch);
    Q_EMIT channelsChanged();
    update();
    return ch.id;
}

bool AnalysisChart::removeChannel(int channelId)
{
    auto &chs = d_ptr->channels;
    const auto it = std::find_if(chs.begin(), chs.end(), [&](const AnalysisChartPrivate::Channel &c) { return c.id == channelId; });
    if (it == chs.end()) {
        return false;
    }
    chs.erase(it);
    if (!hasData()) {
        d_ptr->viewFromData = false;
    }
    clampViewToData();
    Q_EMIT channelsChanged();
    update();
    return true;
}

void AnalysisChart::removeAllChannels()
{
    d_ptr->channels.clear();
    d_ptr->viewFromData = false;
    Q_EMIT channelsChanged();
    update();
}

void AnalysisChart::clear()
{
    for (auto &ch : d_ptr->channels) {
        ch.t.clear();
        ch.v.clear();
        ch.autoGap = 0;
        ch.meanInterval = 0.0;
        ch.finiteMin = std::numeric_limits<double>::infinity();
        ch.finiteMax = -std::numeric_limits<double>::infinity();
    }
    d_ptr->viewFromData = false;
    Q_EMIT channelsChanged();
    update();
}

QList<int> AnalysisChart::channelIds() const
{
    QList<int> ids;
    for (const auto &ch : d_ptr->channels) ids.append(ch.id);
    return ids;
}

int AnalysisChart::channelCount() const
{
    return static_cast<int>(d_ptr->channels.size());
}

QString AnalysisChart::channelName(int channelId) const
{
    const auto *ch = d_ptr->channel(channelId);
    return ch ? ch->name : QString();
}

void AnalysisChart::setChannelName(int channelId, const QString &name)
{
    auto *ch = d_ptr->channel(channelId);
    if (!ch || ch->name == name) return;
    ch->name = name;
    Q_EMIT channelsChanged();
    update();
}

QColor AnalysisChart::channelColor(int channelId) const
{
    const auto *ch = d_ptr->channel(channelId);
    return ch ? ch->color : QColor();
}

void AnalysisChart::setChannelColor(int channelId, const QColor &color)
{
    auto *ch = d_ptr->channel(channelId);
    if (!ch || ch->color == color) return;
    ch->color = color;
    Q_EMIT channelsChanged();
    update();
}

bool AnalysisChart::isChannelVisible(int channelId) const
{
    const auto *ch = d_ptr->channel(channelId);
    return ch && ch->visible;
}

void AnalysisChart::setChannelVisible(int channelId, bool visible)
{
    auto *ch = d_ptr->channel(channelId);
    if (!ch || ch->visible == visible) return;
    ch->visible = visible;
    clampViewToData(); // the minimum span depends on the visible channels
    Q_EMIT channelsChanged();
    update();
}

double AnalysisChart::channelPenWidth(int channelId) const
{
    const auto *ch = d_ptr->channel(channelId);
    return ch ? ch->penWidth : 0.0;
}

void AnalysisChart::setChannelPenWidth(int channelId, double width)
{
    auto *ch = d_ptr->channel(channelId);
    if (!ch || !std::isfinite(width) || width <= 0.0 || ch->penWidth == width) return;
    ch->penWidth = width;
    Q_EMIT channelsChanged();
    update();
}

// --- Data ---

bool AnalysisChart::setChannelData(int channelId, const std::chrono::nanoseconds *t, const double *values, qsizetype count)
{
    auto *ch = d_ptr->channel(channelId);
    if (!ch) {
        qWarning("AnalysisChart::setChannelData: unknown channel %d", channelId);
        return false;
    }
    if (count < 0 || (count > 0 && (t == nullptr || values == nullptr))) {
        qWarning("AnalysisChart::setChannelData: invalid arguments for channel %d (count %lld)", channelId,
                 static_cast<long long>(count));
        return false;
    }
    for (qsizetype i = 1; i < count; ++i) {
        if (t[i] < t[i - 1]) {
            qWarning("AnalysisChart::setChannelData: channel %d rejected, timestamps decrease at index %lld",
                     channelId, static_cast<long long>(i));
            return false;
        }
    }

    const auto n = static_cast<std::size_t>(count);
    std::vector<qint64> ts(n);
    std::vector<double> vs(values, values + n);
    for (std::size_t i = 0; i < n; ++i) {
        ts[i] = static_cast<qint64>(t[i].count());
    }
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (double v : vs) {
        if (std::isfinite(v)) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    }
    ch->t = std::move(ts);
    ch->v = std::move(vs);
    ch->autoGap = internal::gapThresholdFor(ch->t.data(), ch->t.size(), 0);
    ch->meanInterval = n >= 2 ? static_cast<double>(ch->t.back() - ch->t.front()) / static_cast<double>(n - 1) : 0.0;
    ch->finiteMin = lo;
    ch->finiteMax = hi;

    if (!d_ptr->viewFromData && hasData()) {
        resetView(); // the first data sets the view
    } else {
        if (!hasData()) {
            d_ptr->viewFromData = false;
        }
        clampViewToData();
    }
    Q_EMIT channelsChanged();
    update();
    return true;
}

qsizetype AnalysisChart::channelSampleCount(int channelId) const
{
    const auto *ch = d_ptr->channel(channelId);
    return ch ? static_cast<qsizetype>(ch->t.size()) : 0;
}

bool AnalysisChart::hasData() const
{
    return std::any_of(d_ptr->channels.begin(), d_ptr->channels.end(), [](const auto &c) { return !c.t.empty(); });
}

std::chrono::nanoseconds AnalysisChart::dataStart() const
{
    qint64 start = std::numeric_limits<qint64>::max();
    for (const auto &ch : d_ptr->channels) {
        if (!ch.t.empty()) start = std::min(start, ch.t.front());
    }
    return std::chrono::nanoseconds(start == std::numeric_limits<qint64>::max() ? 0 : start);
}

std::chrono::nanoseconds AnalysisChart::dataEnd() const
{
    qint64 end = std::numeric_limits<qint64>::min();
    for (const auto &ch : d_ptr->channels) {
        if (!ch.t.empty()) end = std::max(end, ch.t.back());
    }
    return std::chrono::nanoseconds(end == std::numeric_limits<qint64>::min() ? 0 : end);
}

std::optional<AnalysisChart::Sample> AnalysisChart::nearestSample(int channelId, std::chrono::nanoseconds t) const
{
    const auto *ch = d_ptr->channel(channelId);
    if (!ch) return std::nullopt;
    const qint64 i = internal::nearestSampleIndex(ch->t.data(), ch->t.size(), static_cast<qint64>(t.count()));
    if (i < 0) return std::nullopt;
    const auto k = static_cast<std::size_t>(i);
    return Sample{std::chrono::nanoseconds(ch->t[k]), ch->v[k]};
}

std::chrono::nanoseconds AnalysisChart::gapThreshold() const
{
    return std::chrono::nanoseconds(d_ptr->gapThreshold);
}

void AnalysisChart::setGapThreshold(std::chrono::nanoseconds threshold)
{
    const qint64 v = std::max<qint64>(0, static_cast<qint64>(threshold.count()));
    if (d_ptr->gapThreshold == v) return;
    d_ptr->gapThreshold = v;
    Q_EMIT appearanceChanged();
    update();
}

// --- View ---

namespace {

// Smallest span the view may show, from the visible channels (all channels if none is visible)
qint64 minimumSpanOf(const std::vector<AnalysisChartPrivate::Channel> &channels)
{
    std::vector<double> visible;
    std::vector<double> all;
    for (const auto &ch : channels) {
        if (ch.t.size() >= 2) {
            all.push_back(ch.meanInterval);
            if (ch.visible) visible.push_back(ch.meanInterval);
        }
    }
    return internal::minimumXSpan(visible.empty() ? all : visible);
}

} // namespace

void AnalysisChart::clampViewToData()
{
    if (!hasData()) return;
    const internal::XRange clamped = internal::clampXRange(d_ptr->view, dataStart().count(), dataEnd().count(),
                                                          minimumSpanOf(d_ptr->channels));
    if (clamped.start != d_ptr->view.start || clamped.end != d_ptr->view.end) {
        d_ptr->view = clamped;
        Q_EMIT viewChanged();
    }
}

std::chrono::nanoseconds AnalysisChart::xStart() const
{
    return std::chrono::nanoseconds(d_ptr->view.start);
}

std::chrono::nanoseconds AnalysisChart::xEnd() const
{
    return std::chrono::nanoseconds(d_ptr->view.end);
}

void AnalysisChart::setXRange(std::chrono::nanoseconds start, std::chrono::nanoseconds end)
{
    internal::XRange requested{static_cast<qint64>(start.count()), static_cast<qint64>(end.count())};
    if (requested.end < requested.start) std::swap(requested.start, requested.end);
    internal::XRange next = requested;
    if (hasData()) {
        next = internal::clampXRange(requested, dataStart().count(), dataEnd().count(), minimumSpanOf(d_ptr->channels));
    } else if (requested.span() <= 0) {
        return;
    }
    if (next.start == d_ptr->view.start && next.end == d_ptr->view.end) return;
    d_ptr->view = next;
    Q_EMIT viewChanged();
    update();
}

double AnalysisChart::yMinimum() const
{
    return d_ptr->yMinimum;
}

double AnalysisChart::yMaximum() const
{
    return d_ptr->yMaximum;
}

void AnalysisChart::setYRange(double minimum, double maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || !(minimum < maximum)) return;
    if (minimum == d_ptr->yMinimum && maximum == d_ptr->yMaximum) return;
    d_ptr->yMinimum = minimum;
    d_ptr->yMaximum = maximum;
    Q_EMIT viewChanged();
    update();
}

void AnalysisChart::resetView()
{
    if (!hasData()) return;
    const internal::XRange x = internal::fullXRange(dataStart().count(), dataEnd().count(), minimumSpanOf(d_ptr->channels));
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (const auto &ch : d_ptr->channels) {
        if (ch.visible) {
            lo = std::min(lo, ch.finiteMin);
            hi = std::max(hi, ch.finiteMax);
        }
    }
    const internal::YRange y = internal::autoscaledYRange(lo, hi, std::isfinite(lo) && std::isfinite(hi));
    d_ptr->viewFromData = true;
    const bool changed = x.start != d_ptr->view.start || x.end != d_ptr->view.end
        || (y.hasData && (y.minimum != d_ptr->yMinimum || y.maximum != d_ptr->yMaximum));
    d_ptr->view = x;
    if (y.hasData) {
        d_ptr->yMinimum = y.minimum;
        d_ptr->yMaximum = y.maximum;
    }
    if (changed) Q_EMIT viewChanged();
    update();
}

// --- Appearance ---

bool AnalysisChart::isGridVisible() const { return d_ptr->gridVisible; }
bool AnalysisChart::isLegendVisible() const { return d_ptr->legendVisible; }
QColor AnalysisChart::gridColor() const { return d_ptr->gridColor; }
QColor AnalysisChart::backgroundColor() const { return d_ptr->backgroundColor; }
QColor AnalysisChart::bezelColor() const { return d_ptr->bezelColor; }
QColor AnalysisChart::textColor() const { return d_ptr->textColor; }
AnalysisChart::TimeLabelFormat AnalysisChart::timeLabelFormat() const { return d_ptr->timeLabelFormat; }
QDateTime AnalysisChart::timeOrigin() const { return d_ptr->timeOrigin; }

void AnalysisChart::setGridVisible(bool visible)
{
    if (d_ptr->gridVisible == visible) return;
    d_ptr->gridVisible = visible;
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setLegendVisible(bool visible)
{
    if (d_ptr->legendVisible == visible) return;
    d_ptr->legendVisible = visible;
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setGridColor(const QColor &color)
{
    if (d_ptr->gridColor == color) return;
    d_ptr->gridColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setBackgroundColor(const QColor &color)
{
    if (d_ptr->backgroundColor == color) return;
    d_ptr->backgroundColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setBezelColor(const QColor &color)
{
    if (d_ptr->bezelColor == color) return;
    d_ptr->bezelColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setTextColor(const QColor &color)
{
    if (d_ptr->textColor == color) return;
    d_ptr->textColor = color;
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setTimeLabelFormat(TimeLabelFormat format)
{
    if (d_ptr->timeLabelFormat == format) return;
    d_ptr->timeLabelFormat = format;
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setTimeOrigin(const QDateTime &origin)
{
    if (d_ptr->timeOrigin == origin) return;
    d_ptr->timeOrigin = origin;
    Q_EMIT appearanceChanged();
    update();
}

QSize AnalysisChart::sizeHint() const
{
    return QSize(640, 360);
}

QSize AnalysisChart::minimumSizeHint() const
{
    return QSize(240, 160);
}

// --- Layout and painting ---

QRectF AnalysisChart::plotArea() const
{
    // #51 rule (b), as StripChart: the room for the labels and the legend grows with the widget font
    const double scale = internal::fontScale(font(), QApplication::font());
    const double top = (d_ptr->legendVisible ? 30.0 : 12.0) * scale;
    const double bottom = 26.0 * scale;
    const double right = 12.0;
    const double height = std::max(10.0, this->height() - top - bottom);
    // Room for the widest value label at the current Y range (StripChart's 42 px at least)
    const QFontMetricsF fm(labelFont(*this, scale));
    double widest = 0.0;
    for (const auto &tick : internal::valueAxisTicks(d_ptr->yMinimum, d_ptr->yMaximum, height, 28.0 * scale)) {
        widest = std::max(widest, fm.horizontalAdvance(tick.label));
    }
    const double left = std::max(42.0 * scale, widest + 10.0);
    return QRectF(left, top, std::max(10.0, width() - left - right), height);
}

void AnalysisChart::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void AnalysisChart::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        update();
    }
    QWidget::changeEvent(event);
}

void AnalysisChart::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const double scale = internal::fontScale(font(), QApplication::font());
    const QFont font = labelFont(*this, scale);
    const QFontMetricsF fm(font);
    const QRectF plot = plotArea();
    const internal::XRange view = d_ptr->view;
    const bool hasView = view.span() > 0;

    AnalysisChartPrivate::FrameDiagnostics diag;
    diag.plotRect = plot;
    diag.view = view;

    // Chassis and screen (as StripChart)
    painter.setPen(Qt::NoPen);
    painter.setBrush(d_ptr->bezelColor);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
    const QRectF screen = plot.adjusted(-4.0, -4.0, 4.0, 4.0);
    painter.setPen(QPen(d_ptr->bezelColor.darker(190), 1.2));
    painter.setBrush(d_ptr->backgroundColor);
    painter.drawRoundedRect(screen, 4.0, 4.0);

    auto mapX = [&](qint64 t) { return plot.left() + static_cast<double>(t - view.start) * plot.width() / static_cast<double>(view.span()); };
    const double ySpan = d_ptr->yMaximum - d_ptr->yMinimum;
    auto mapY = [&](double v) { return plot.bottom() - (v - d_ptr->yMinimum) / ySpan * plot.height(); };

    // Axes: grid and labels
    const bool absolute = d_ptr->timeLabelFormat == TimeLabelFormat::Absolute;
    const std::vector<internal::AxisTick> xTicks =
        hasView ? internal::timeAxisTicks(view, dataStart().count(), plot.width(), 60.0 * scale, absolute, d_ptr->timeOrigin)
                : std::vector<internal::AxisTick>{};
    const std::vector<internal::AxisTick> yTicks = internal::valueAxisTicks(d_ptr->yMinimum, d_ptr->yMaximum, plot.height(), 28.0 * scale);
    painter.setFont(font);
    const QPen gridPen(d_ptr->gridColor, 1.0, Qt::DotLine);
    for (const auto &tick : xTicks) {
        const double x = mapX(static_cast<qint64>(tick.position));
        if (x < plot.left() - 0.5 || x > plot.right() + 0.5) continue;
        if (d_ptr->gridVisible) {
            painter.setPen(gridPen);
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        const double w = fm.horizontalAdvance(tick.label) + 4.0;
        painter.setPen(d_ptr->textColor);
        painter.drawText(QRectF(x - w * 0.5, plot.bottom() + 5.0, w, fm.height()), Qt::AlignCenter, tick.label);
    }
    for (const auto &tick : yTicks) {
        const double y = mapY(tick.position);
        if (d_ptr->gridVisible) {
            painter.setPen(gridPen);
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        painter.setPen(d_ptr->textColor);
        painter.drawText(QRectF(0.0, y - fm.height() * 0.5, plot.left() - 8.0, fm.height()), Qt::AlignRight | Qt::AlignVCenter, tick.label);
    }
    diag.xTicks = xTicks;
    diag.yTicks = yTicks;

    // Traces: raw samples; a non-finite value or a time gap above the threshold breaks the trace
    // (StripChart's raw path)
    if (hasView && ySpan > 0.0) {
        painter.save();
        painter.setClipRect(plot);
        for (const auto &ch : d_ptr->channels) {
            if (!ch.visible || ch.t.empty()) continue;
            AnalysisChartPrivate::ChannelDiagnostics chDiag;
            chDiag.id = ch.id;
            // One sample on each side of the view so the trace enters and leaves it
            const auto first = std::lower_bound(ch.t.begin(), ch.t.end(), view.start);
            const auto last = std::upper_bound(ch.t.begin(), ch.t.end(), view.end);
            std::size_t i0 = static_cast<std::size_t>(first - ch.t.begin());
            std::size_t i1 = static_cast<std::size_t>(last - ch.t.begin());
            if (i0 > 0) --i0;
            if (i1 < ch.t.size()) ++i1;
            const qint64 gap = d_ptr->gapThreshold > 0 ? d_ptr->gapThreshold : ch.autoGap;

            painter.setPen(QPen(ch.color, ch.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPolygonF poly;
            std::vector<qint64> polyT;
            auto flush = [&]() {
                if (!poly.isEmpty()) {
                    painter.drawPolyline(poly);
                    if (d_ptr->recordDiagnostics) chDiag.segments.push_back(polyT);
                    poly.clear();
                    polyT.clear();
                }
            };
            qint64 prevT = std::numeric_limits<qint64>::min();
            for (std::size_t i = i0; i < i1; ++i) {
                const double v = ch.v[i];
                const qint64 t = ch.t[i];
                if (!std::isfinite(v)) {
                    flush();
                    prevT = std::numeric_limits<qint64>::min();
                    continue;
                }
                if (prevT != std::numeric_limits<qint64>::min() && gap > 0 && t - prevT > gap) {
                    flush();
                }
                poly << QPointF(mapX(t), mapY(v));
                if (d_ptr->recordDiagnostics) polyT.push_back(t);
                prevT = t;
            }
            flush();
            diag.channels.push_back(std::move(chDiag));
        }
        painter.restore();
    }

    // Legend: a swatch and the name of each visible channel, above the plot
    if (d_ptr->legendVisible) {
        double x = plot.left() + 4.0;
        const double y = (plot.top() - 4.0) * 0.5 - fm.height() * 0.5; // centred in the band above the screen
        for (const auto &ch : d_ptr->channels) {
            if (!ch.visible) continue;
            const double sw = std::round(fm.height() * 0.7);
            painter.setPen(Qt::NoPen);
            painter.setBrush(ch.color);
            painter.drawRoundedRect(QRectF(x, y + (fm.height() - sw) * 0.5, sw, sw), 2.0, 2.0);
            x += sw + 4.0;
            painter.setPen(d_ptr->textColor);
            const double w = fm.horizontalAdvance(ch.name);
            painter.drawText(QRectF(x, y, w + 2.0, fm.height()), Qt::AlignLeft | Qt::AlignVCenter, ch.name);
            x += w + 14.0;
        }
    }

    if (d_ptr->recordDiagnostics) {
        d_ptr->lastFrame = std::move(diag);
    }
}

} // namespace QtIndustrialWidgets
