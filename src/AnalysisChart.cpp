// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/AnalysisChart.h>

#include "internal/AnalysisChart_p.h"
#include "internal/AnalysisChartView.h"
#include "internal/DecimatedTrace.h"
#include "internal/FontScale.h"

#include <QtCore/QEvent>
#include <QtCore/QtDebug>
#include <QtGui/QFontMetricsF>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtGui/QPolygonF>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QtIndustrialWidgets {

namespace {

constexpr qint64 kDefaultSpanNs = 1000000000; // view without data: 1 s
constexpr double kReadoutColumnGap = 16.0;     // logical px between the columns of the readout band
constexpr double kNominalAxisPx = 1000.0;      // axis length of the readout resolution, 10 ticks

// Pixel size of the axis and legend labels (as StripChart), scaled by the widget font (#51 rule (b))
QFont labelFont(const QWidget &w, double scale)
{
    QFont f = w.font();
    f.setPixelSize(internal::scalePixelSize(9, scale));
    return f;
}

// Visible channels as listed in the readout band (the first kReadoutChannelRows of them)
const AnalysisChartPrivate::Channel *readoutChannelAt(const AnalysisChartPrivate &d, int row)
{
    int i = 0;
    for (const auto &ch : d.channels) {
        if (!ch.visible) continue;
        if (i == row) return row < internal::kReadoutChannelRows ? &ch : nullptr;
        ++i;
    }
    return nullptr;
}

// Widest cell of each column (label, A, B, delta), with room for the swatch in the label column
std::vector<double> readoutColumnWidths(const QList<QStringList> &rows, const QFontMetricsF &fm)
{
    std::vector<double> widths(4, 0.0);
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < rows[r].size() && c < 4; ++c) {
            double w = fm.horizontalAdvance(rows[r][c]) + 2.0;
            if (c == 0 && r > 0) w += std::round(fm.height() * 0.7) + 4.0;
            widths[static_cast<std::size_t>(c)] = std::max(widths[static_cast<std::size_t>(c)], w);
        }
    }
    return widths;
}

} // namespace

AnalysisChart::AnalysisChart(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<AnalysisChartPrivate>())
{
    d_ptr->view = {0, kDefaultSpanNs};
    setFocusPolicy(Qt::StrongFocus);
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
    updateGeometry(); // the readout band has a row per visible channel
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
    updateGeometry();
    Q_EMIT channelsChanged();
    Q_EMIT cursorsChanged();
    update();
    return true;
}

void AnalysisChart::removeAllChannels()
{
    d_ptr->channels.clear();
    d_ptr->viewFromData = false;
    updateGeometry();
    Q_EMIT channelsChanged();
    Q_EMIT cursorsChanged();
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
        ch.pyramid.clear();
    }
    d_ptr->viewFromData = false;
    Q_EMIT channelsChanged();
    Q_EMIT cursorsChanged();
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
    updateGeometry();  // the readout band has a row per visible channel
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
    ch->pyramid.build(ch->t.data(), ch->v.data(), ch->v.size());

    if (!d_ptr->viewFromData && hasData()) {
        resetView(); // the first data sets the view and places the cursors at 1/3 and 2/3 of it
        const qint64 span = d_ptr->view.span();
        d_ptr->cursorPos[0] = d_ptr->view.start + span / 3;
        d_ptr->cursorPos[1] = d_ptr->view.start + 2 * (span / 3);
    } else {
        if (!hasData()) {
            d_ptr->viewFromData = false;
        }
        clampViewToData();
    }
    Q_EMIT channelsChanged();
    Q_EMIT cursorsChanged();
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
AnalysisChart::DecimationMode AnalysisChart::decimationMode() const { return d_ptr->decimationMode; }
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

void AnalysisChart::setDecimationMode(DecimationMode mode)
{
    if (d_ptr->decimationMode == mode) return;
    d_ptr->decimationMode = mode;
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
    // #44 rule: from the minimum size up the readout band is whole (its rows and its widest columns)
    const double scale = internal::fontScale(font(), QApplication::font());
    const double top = (d_ptr->legendVisible ? 30.0 : 12.0) * scale;
    const double height = top + 60.0 + 26.0 * scale + readoutBandHeight();
    const double width = std::max(240.0, readoutBandWidth() + 16.0);
    return QSize(static_cast<int>(std::ceil(width)), std::max(160, static_cast<int>(std::ceil(height))));
}

// --- Layout and painting ---

QRectF AnalysisChart::plotArea() const
{
    // #51 rule (b), as StripChart: the room for the labels and the legend grows with the widget font
    const double scale = internal::fontScale(font(), QApplication::font());
    const double top = (d_ptr->legendVisible ? 30.0 : 12.0) * scale;
    const double bottom = 26.0 * scale + readoutBandHeight();
    const double right = 12.0;
    const double height = std::max(10.0, this->height() - top - bottom);
    // Room for the widest value label at the current Y range (StripChart's 42 px at least)
    const QFontMetricsF fm(labelFont(*this, scale));
    double widest = 0.0;
    for (const auto &tick : internal::valueAxisTicks(d_ptr->yMinimum, d_ptr->yMaximum, height, 28.0 * scale)) {
        widest = std::max(widest, fm.horizontalAdvance(tick.label));
    }
    // Left and right edges on device pixel boundaries (for a widget at a whole device position), so
    // that the plot spans whole device columns, one decimation bucket each
    const double dpr = devicePixelRatioF();
    const double left = std::ceil(std::max(42.0 * scale, widest + 10.0) * dpr - 1e-9) / dpr;
    const double plotWidth = std::floor(std::max(10.0, width() - left - right) * dpr + 1e-9) / dpr;
    return QRectF(left, top, plotWidth, height);
}

void AnalysisChart::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void AnalysisChart::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        if (event->type() == QEvent::FontChange) updateGeometry(); // the readout band follows the font
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

    // Device-space placement, as StripChart: the plot's left edge snapped to a device pixel boundary
    // (from the painter's device transform, so also for a widget at a fractional device position) and
    // widthDev whole device columns, one decimation bucket each. Traces, ticks and cursors share it.
    const double dpr = devicePixelRatioF();
    const QTransform deviceTransform = painter.deviceTransform();
    const bool deviceAxisAligned = deviceTransform.type() <= QTransform::TxScale && std::abs(deviceTransform.m11() - dpr) < 1e-9;
    const double devScale = deviceAxisAligned ? deviceTransform.m11() : dpr;
    const double devOffsetX = deviceAxisAligned ? deviceTransform.dx() : 0.0;
    auto toLogicalX = [devScale, devOffsetX](double xDev) { return (xDev - devOffsetX) / devScale; };
    const int widthDev = std::max(1, static_cast<int>(std::lround(plot.width() * devScale)));
    const double leftDev = std::round(plot.left() * devScale + devOffsetX);
    diag.widthDev = widthDev;
    diag.leftDev = leftDev;
    diag.devScale = devScale;
    auto mapX = [&](qint64 t) {
        return toLogicalX(leftDev + static_cast<double>(t - view.start) * widthDev / static_cast<double>(view.span()));
    };
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

    // Traces, clipped to the whole device columns of the plot. A non-finite value or a time gap above
    // the threshold breaks the trace, raw or decimated, with StripChart's rules and pens.
    if (hasView && ySpan > 0.0) {
        painter.save();
        painter.setClipRect(QRectF(toLogicalX(leftDev), plot.top(), widthDev / devScale, plot.height()));
        const internal::BucketGrid grid(widthDev, view.span()); // bucket k = floor((t - xStart) * widthDev / span)
        for (const auto &ch : d_ptr->channels) {
            if (!ch.visible || ch.t.empty()) continue;
            AnalysisChartPrivate::ChannelDiagnostics chDiag;
            chDiag.id = ch.id;
            // One sample on each side of the view so the raw trace enters and leaves it
            const auto first = std::lower_bound(ch.t.begin(), ch.t.end(), view.start);
            const auto last = std::upper_bound(ch.t.begin(), ch.t.end(), view.end);
            const auto inView = static_cast<std::size_t>(last - first);
            std::size_t i0 = static_cast<std::size_t>(first - ch.t.begin());
            std::size_t i1 = static_cast<std::size_t>(last - ch.t.begin());
            if (i0 > 0) --i0;
            if (i1 < ch.t.size()) ++i1;
            const qint64 gap = d_ptr->gapThreshold > 0 ? d_ptr->gapThreshold : ch.autoGap;
            const bool decimate = d_ptr->decimationMode == DecimationMode::Always
                || (d_ptr->decimationMode == DecimationMode::Auto && inView > static_cast<std::size_t>(widthDev));
            chDiag.decimated = decimate;

            if (decimate) {
                // One M4 bucket per device column; vertices at the centre of their column
                QPen pen(ch.color, 0.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
                pen.setCosmetic(true);
                painter.setPen(pen);
                painter.setRenderHint(QPainter::Antialiasing, false);
                // Buckets and segments in the buffers kept between frames (#97); segments [0, segmentCount) are valid
                ch.pyramid.buckets(ch.t.data(), ch.v.data(), ch.t.size(), view.start, grid, 0, widthDev, d_ptr->reusableBuckets);
                const std::size_t segmentCount =
                    internal::segmentsFromBuckets(d_ptr->reusableBuckets, gap, d_ptr->reusableSegments);
                // StripChart's drawing (#91), with buckets counted from the view start: kStart 0, origin xStart
                internal::PlotXMapping xmap;
                xmap.grid = grid;
                xmap.kStart = 0;
                xmap.leftDev = leftDev;
                QPolygonF &poly = d_ptr->reusablePoly;
                for (std::size_t si = 0; si < segmentCount; ++si) {
                    const auto &seg = d_ptr->reusableSegments[si];
                    if (seg.empty()) continue;
                    internal::drawDecimatedSegment(painter, xmap, seg, poly, mapY, toLogicalX, view.start);
                    if (d_ptr->recordDiagnostics) {
                        std::vector<qint64> ts;
                        for (const auto &pt : seg) ts.push_back(pt.t);
                        chDiag.segments.push_back(std::move(ts));
                        chDiag.polylines.push_back(poly);
                    }
                }
            } else {
                painter.setPen(QPen(ch.color, ch.penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.setRenderHint(QPainter::Antialiasing, true);
                QPolygonF poly;
                std::vector<qint64> polyT;
                auto flush = [&]() {
                    if (!poly.isEmpty()) {
                        painter.drawPolyline(poly);
                        if (d_ptr->recordDiagnostics) {
                            chDiag.segments.push_back(polyT);
                            chDiag.polylines.push_back(poly);
                        }
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
            }
            diag.channels.push_back(std::move(chDiag));
        }
        painter.restore();
    }

    // Cursors: vertical lines with their letter at the top of the plot
    {
        const QFont cursorFont = font;
        painter.setFont(cursorFont);
        for (int c = 0; c < 2; ++c) {
            if (!d_ptr->cursorVisible[c] || !hasView) continue;
            const double x = mapX(d_ptr->cursorPos[c]);
            if (x < plot.left() - 0.5 || x > plot.right() + 0.5) continue;
            painter.setPen(QPen(d_ptr->cursorColor, 1.0));
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            const QString letter = c == 0 ? QStringLiteral("A") : QStringLiteral("B");
            const double w = fm.horizontalAdvance(letter) + 6.0;
            const QRectF tag(std::min(x + 2.0, plot.right() - w), plot.top() + 2.0, w, fm.height());
            painter.setPen(Qt::NoPen);
            painter.setBrush(d_ptr->cursorColor);
            painter.drawRoundedRect(tag, 2.0, 2.0);
            painter.setPen(d_ptr->backgroundColor);
            painter.drawText(tag, Qt::AlignCenter, letter);
            diag.cursorX[c] = x;
        }
    }

    // Box zoom being dragged
    if (d_ptr->drag == AnalysisChartPrivate::Drag::Box) {
        const QRectF box = QRectF(d_ptr->pressPos, d_ptr->currentPos).normalized().intersected(plot);
        painter.setPen(QPen(d_ptr->textColor, 1.0, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(box);
        diag.boxRect = box;
    }

    // Readout band below the time axis
    if (readoutBandHeight() > 0.0) {
        const QList<QStringList> rows = readoutRows();
        const std::vector<double> widths = readoutColumnWidths(rows, fm);
        const double rowH = fm.height() + 3.0;
        const QRectF bandRect(8.0, plot.bottom() + 26.0 * scale, width() - 16.0, readoutBandHeight());
        diag.bandRect = bandRect;
        diag.bandFont = font;
        painter.setFont(font);
        for (int r = 0; r < rows.size(); ++r) {
            AnalysisChartPrivate::BandRow bandRow;
            bandRow.cells = rows[r];
            double x = bandRect.left();
            const double y = bandRect.top() + 3.0 + static_cast<double>(r) * rowH;
            for (int c = 0; c < rows[r].size(); ++c) {
                const QRectF cell(x, y, widths[static_cast<std::size_t>(c)], fm.height());
                // Channel rows start with the channel's colour swatch
                if (c == 0 && r > 0) {
                    const auto *ch = readoutChannelAt(*d_ptr, r - 1);
                    if (ch) {
                        const double sw = std::round(fm.height() * 0.7);
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(ch->color);
                        painter.drawRoundedRect(QRectF(x, y + (fm.height() - sw) * 0.5, sw, sw), 2.0, 2.0);
                        bandRow.swatch = ch->color;
                    }
                }
                painter.setPen(d_ptr->textColor);
                const QRectF textRect = (c == 0 && r > 0) ? cell.adjusted(std::round(fm.height() * 0.7) + 4.0, 0, 0, 0) : cell;
                painter.drawText(textRect, (c == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, rows[r][c]);
                bandRow.cellRects.push_back(textRect);
                x += widths[static_cast<std::size_t>(c)] + kReadoutColumnGap;
            }
            diag.band.push_back(std::move(bandRow));
        }
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

// --- Cursors and readout band ---

bool AnalysisChart::isCursorVisible(Cursor cursor) const
{
    return d_ptr->cursorVisible[cursor == Cursor::A ? 0 : 1];
}

void AnalysisChart::setCursorVisible(Cursor cursor, bool visible)
{
    bool &v = d_ptr->cursorVisible[cursor == Cursor::A ? 0 : 1];
    if (v == visible) return;
    v = visible;
    updateGeometry(); // the readout band appears with the first visible cursor
    Q_EMIT cursorsChanged();
    update();
}

std::chrono::nanoseconds AnalysisChart::cursorPosition(Cursor cursor) const
{
    return std::chrono::nanoseconds(d_ptr->cursorPos[cursor == Cursor::A ? 0 : 1]);
}

void AnalysisChart::setCursorPosition(Cursor cursor, std::chrono::nanoseconds t)
{
    qint64 &p = d_ptr->cursorPos[cursor == Cursor::A ? 0 : 1];
    const qint64 v = static_cast<qint64>(t.count());
    if (p == v) return;
    p = v;
    Q_EMIT cursorsChanged();
    update();
}

std::optional<AnalysisChart::Sample> AnalysisChart::cursorReadout(int channelId, Cursor cursor) const
{
    if (!isCursorVisible(cursor)) return std::nullopt;
    return nearestSample(channelId, cursorPosition(cursor));
}

std::chrono::nanoseconds AnalysisChart::cursorDeltaTime() const
{
    return cursorPosition(Cursor::B) - cursorPosition(Cursor::A);
}

std::optional<double> AnalysisChart::cursorDeltaValue(int channelId) const
{
    const auto a = cursorReadout(channelId, Cursor::A);
    const auto b = cursorReadout(channelId, Cursor::B);
    if (!a || !b || !std::isfinite(a->value) || !std::isfinite(b->value)) return std::nullopt;
    return b->value - a->value;
}

bool AnalysisChart::isReadoutVisible() const { return d_ptr->readoutVisible; }
QColor AnalysisChart::cursorColor() const { return d_ptr->cursorColor; }

void AnalysisChart::setReadoutVisible(bool visible)
{
    if (d_ptr->readoutVisible == visible) return;
    d_ptr->readoutVisible = visible;
    updateGeometry();
    Q_EMIT appearanceChanged();
    update();
}

void AnalysisChart::setCursorColor(const QColor &color)
{
    if (d_ptr->cursorColor == color) return;
    d_ptr->cursorColor = color;
    Q_EMIT appearanceChanged();
    update();
}

qint64 AnalysisChart::readoutResolution() const
{
    // A tenth of the 1-2-5 step of a nominal 10-tick time axis over the view. It does not depend on
    // the widget size, so the band texts (and minimumSizeHint()) stay the same when the widget is
    // resized to its minimum size.
    const auto ticks = internal::timeAxisTicks(d_ptr->view, dataStart().count(), kNominalAxisPx, kNominalAxisPx / 10.0, false, QDateTime());
    const qint64 step = ticks.size() >= 2 ? static_cast<qint64>(ticks[1].position - ticks[0].position) : d_ptr->view.span();
    return std::max<qint64>(1, step / 10);
}

QList<QStringList> AnalysisChart::readoutRows() const
{
    QList<QStringList> rows;
    const bool a = d_ptr->cursorVisible[0];
    const bool b = d_ptr->cursorVisible[1];
    if (!d_ptr->readoutVisible || (!a && !b)) return rows;

    const qint64 res = readoutResolution();
    const bool absolute = d_ptr->timeLabelFormat == TimeLabelFormat::Absolute;
    const qint64 origin = dataStart().count();
    auto timeText = [&](int c) {
        return internal::readoutTimeText(d_ptr->cursorPos[c], origin, absolute, d_ptr->timeOrigin, res);
    };
    // One more decimal than a nominal 10-tick value axis (independent of the widget size, see readoutResolution())
    const int decimals = internal::valueAxisDecimals(d_ptr->yMinimum, d_ptr->yMaximum, kNominalAxisPx, kNominalAxisPx / 10.0) + 1;

    rows.append({QString(), a ? QStringLiteral("A  ") + timeText(0) : QString(), b ? QStringLiteral("B  ") + timeText(1) : QString(),
                 (a && b) ? QString(QChar(0x0394)) + QStringLiteral("t  ") + internal::durationText(cursorDeltaTime().count(), res) : QString()});
    int listed = 0;
    int more = 0;
    for (const auto &ch : d_ptr->channels) {
        if (!ch.visible) continue;
        if (listed == internal::kReadoutChannelRows) {
            ++more;
            continue;
        }
        ++listed;
        auto valueText = [&](Cursor c) {
            const auto s = cursorReadout(ch.id, c);
            return internal::readoutValueText(s ? &s->value : nullptr, decimals);
        };
        const auto dy = cursorDeltaValue(ch.id);
        rows.append({ch.name, a ? valueText(Cursor::A) : QString(), b ? valueText(Cursor::B) : QString(),
                     (a && b) ? QString(QChar(0x0394)) + QLatin1Char(' ') + internal::readoutValueText(dy ? &*dy : nullptr, decimals) : QString()});
    }
    if (more > 0) {
        rows.append({QStringLiteral("+%1 more").arg(more), QString(), QString(), QString()});
    }
    return rows;
}

double AnalysisChart::readoutBandHeight() const
{
    const int rows = static_cast<int>(readoutRows().size());
    if (rows == 0) return 0.0;
    const double scale = internal::fontScale(font(), QApplication::font());
    const QFontMetricsF fm(labelFont(*this, scale));
    return rows * (fm.height() + 3.0) + 6.0;
}

double AnalysisChart::readoutBandWidth() const
{
    const QList<QStringList> rows = readoutRows();
    if (rows.isEmpty()) return 0.0;
    const double scale = internal::fontScale(font(), QApplication::font());
    const QFontMetricsF fm(labelFont(*this, scale));
    double w = 0.0;
    for (double c : readoutColumnWidths(rows, fm)) w += c + kReadoutColumnGap;
    return w - kReadoutColumnGap;
}

// --- Interaction ---

namespace {

constexpr double kCursorGrabPx = 4.0; // a left drag within this distance of a cursor moves it

} // namespace

void AnalysisChart::mousePressEvent(QMouseEvent *event)
{
    const QRectF plot = plotArea();
    const QPointF pos = event->position();
    if (!plot.contains(pos) || !hasData()) {
        QWidget::mousePressEvent(event);
        return;
    }
    auto &d = *d_ptr;
    d.pressPos = pos;
    d.currentPos = pos;
    d.pressView = d.view;
    d.pressYMinimum = d.yMinimum;
    d.pressYMaximum = d.yMaximum;
    d.drag = AnalysisChartPrivate::Drag::None;
    if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier)) {
        // The nearest visible cursor within kCursorGrabPx, otherwise pan
        double best = kCursorGrabPx;
        for (int c = 0; c < 2; ++c) {
            if (!d.cursorVisible[c] || d.view.span() <= 0) continue;
            const double x = plot.left() + static_cast<double>(d.cursorPos[c] - d.view.start) * plot.width() / static_cast<double>(d.view.span());
            if (std::abs(x - pos.x()) <= best) {
                best = std::abs(x - pos.x());
                d.drag = c == 0 ? AnalysisChartPrivate::Drag::CursorA : AnalysisChartPrivate::Drag::CursorB;
            }
        }
        if (d.drag == AnalysisChartPrivate::Drag::None) d.drag = AnalysisChartPrivate::Drag::Pan;
    } else if (event->button() == Qt::RightButton || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ShiftModifier))) {
        d.drag = AnalysisChartPrivate::Drag::Box;
    }
    event->accept();
}

void AnalysisChart::mouseMoveEvent(QMouseEvent *event)
{
    auto &d = *d_ptr;
    if (d.drag == AnalysisChartPrivate::Drag::None) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QRectF plot = plotArea();
    const QPointF pos = event->position();
    d.currentPos = pos;
    switch (d.drag) {
    case AnalysisChartPrivate::Drag::Pan: {
        const double dt = -(pos.x() - d.pressPos.x()) * static_cast<double>(d.pressView.span()) / plot.width();
        const qint64 shift = static_cast<qint64>(std::llround(dt));
        setXRange(std::chrono::nanoseconds(d.pressView.start + shift), std::chrono::nanoseconds(d.pressView.end + shift));
        const double dv = (pos.y() - d.pressPos.y()) * (d.pressYMaximum - d.pressYMinimum) / plot.height();
        setYRange(d.pressYMinimum + dv, d.pressYMaximum + dv);
        break;
    }
    case AnalysisChartPrivate::Drag::CursorA:
    case AnalysisChartPrivate::Drag::CursorB: {
        const double t = static_cast<double>(d.view.start) + (pos.x() - plot.left()) * static_cast<double>(d.view.span()) / plot.width();
        setCursorPosition(d.drag == AnalysisChartPrivate::Drag::CursorA ? Cursor::A : Cursor::B,
                          std::chrono::nanoseconds(static_cast<qint64>(std::llround(t))));
        break;
    }
    case AnalysisChartPrivate::Drag::Box:
        update();
        break;
    case AnalysisChartPrivate::Drag::None:
        break;
    }
    event->accept();
}

void AnalysisChart::mouseReleaseEvent(QMouseEvent *event)
{
    auto &d = *d_ptr;
    if (d.drag == AnalysisChartPrivate::Drag::Box) {
        const QRectF plot = plotArea();
        const QRectF box = QRectF(d.pressPos, event->position()).normalized().intersected(plot);
        // A box smaller than 4 logical px in either direction is ignored (a click, not a zoom)
        if (box.width() >= 4.0 && box.height() >= 4.0 && d.view.span() > 0) {
            auto tAt = [&](double x) {
                return std::chrono::nanoseconds(static_cast<qint64>(std::llround(
                    static_cast<double>(d.view.start) + (x - plot.left()) * static_cast<double>(d.view.span()) / plot.width())));
            };
            auto vAt = [&](double y) { return d.yMinimum + (plot.bottom() - y) * (d.yMaximum - d.yMinimum) / plot.height(); };
            const double yLow = vAt(box.bottom());
            const double yHigh = vAt(box.top());
            setXRange(tAt(box.left()), tAt(box.right()));
            setYRange(yLow, yHigh);
        }
    }
    d.drag = AnalysisChartPrivate::Drag::None;
    update();
    event->accept();
}

void AnalysisChart::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        d_ptr->drag = AnalysisChartPrivate::Drag::None;
        resetView();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AnalysisChart::wheelEvent(QWheelEvent *event)
{
    const QRectF plot = plotArea();
    const QPointF pos = event->position();
    const int delta = event->angleDelta().y();
    if (!plot.contains(pos) || delta == 0 || !hasData()) {
        QWidget::wheelEvent(event);
        return;
    }
    const double factor = internal::wheelZoomFactor(delta);
    auto &d = *d_ptr;
    if (event->modifiers() & Qt::ControlModifier) {
        const double anchor = d.yMinimum + (plot.bottom() - pos.y()) * (d.yMaximum - d.yMinimum) / plot.height();
        const internal::YRange y = internal::zoomYAround(d.yMinimum, d.yMaximum, anchor, factor);
        setYRange(y.minimum, y.maximum);
    } else {
        const double anchor = static_cast<double>(d.view.start) + (pos.x() - plot.left()) * static_cast<double>(d.view.span()) / plot.width();
        const internal::XRange x = internal::zoomXAround(d.view, anchor, factor);
        setXRange(std::chrono::nanoseconds(x.start), std::chrono::nanoseconds(x.end));
    }
    event->accept();
}

void AnalysisChart::keyPressEvent(QKeyEvent *event)
{
    auto &d = *d_ptr;
    const qint64 span = d.view.span();
    const double ySpan = d.yMaximum - d.yMinimum;
    switch (event->key()) {
    case Qt::Key_Home:
        resetView();
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal: {
        const internal::XRange x = internal::zoomXAround(d.view, static_cast<double>(d.view.start) + span * 0.5, 1.25);
        setXRange(std::chrono::nanoseconds(x.start), std::chrono::nanoseconds(x.end));
        break;
    }
    case Qt::Key_Minus: {
        const internal::XRange x = internal::zoomXAround(d.view, static_cast<double>(d.view.start) + span * 0.5, 1.0 / 1.25);
        setXRange(std::chrono::nanoseconds(x.start), std::chrono::nanoseconds(x.end));
        break;
    }
    case Qt::Key_Left:
    case Qt::Key_Right: {
        const qint64 step = std::max<qint64>(1, span / 10) * (event->key() == Qt::Key_Left ? -1 : 1);
        setXRange(std::chrono::nanoseconds(d.view.start + step), std::chrono::nanoseconds(d.view.end + step));
        break;
    }
    case Qt::Key_Up:
    case Qt::Key_Down: {
        const double step = ySpan * 0.1 * (event->key() == Qt::Key_Up ? 1.0 : -1.0);
        setYRange(d.yMinimum + step, d.yMaximum + step);
        break;
    }
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

} // namespace QtIndustrialWidgets
