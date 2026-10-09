// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

#include <QtIndustrialWidgets/AnnunciatorPanel.h>

#include <QtCore/QTimer>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QFontMetrics>
#include <algorithm>

namespace QtIndustrialWidgets {


class AnnunciatorPanelPrivate {
public:
    int m_rows{2};
    int m_columns{4};
    AnnunciatorPanel::AnnunciatorSequence m_sequence{AnnunciatorPanel::AnnunciatorSequence::SequenceA_AutomaticReset};
    bool m_lampTest{false};
    bool m_hornActive{false};
    bool m_hornSilenced{false};

    QVector<AnnunciatorPanel::TileData> m_tiles;

    // Flash timer states (Fast: 2 Hz, Slow: 0.8 Hz)
    QTimer m_flashTimer;
    int m_flashTickCounter{0};
    bool m_fastFlashPhase{true};
    bool m_slowFlashPhase{true};

    // Styling colors
    QColor m_frameColor{QColor(30, 36, 46)};       // Heavy industrial dark chassis
    QColor m_gridColor{QColor(55, 65, 80)};        // Metal grid divider bars
    QColor m_criticalColor{QColor(235, 59, 90)};   // Crimson red alarm
    QColor m_warningColor{QColor(254, 211, 48)};   // Amber gold warning
    QColor m_advisoryColor{QColor(0, 229, 255)};   // Cyan advisory
    QColor m_textColor{QColor(240, 244, 250)};     // White engraved text

    // Frame cache
    QPixmap m_cachePixmap;
    bool m_cacheDirty{true};
};

AnnunciatorPanel::AnnunciatorPanel(QWidget *parent)
    : AnnunciatorPanel(2, 4, parent)
{
}

AnnunciatorPanel::AnnunciatorPanel(int rows, int cols, QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<AnnunciatorPanelPrivate>())
{
    d_ptr->m_rows = std::max(1, rows);
    d_ptr->m_columns = std::max(1, cols);

    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    d_ptr->m_tiles.resize(d_ptr->m_rows * d_ptr->m_columns);
    for (int i = 0; i < d_ptr->m_tiles.size(); ++i) {
        d_ptr->m_tiles[i].text = QStringLiteral("ALARM %1").arg(i + 1);
        d_ptr->m_tiles[i].severity = (i % 3 == 0) ? Severity::Critical :
                              (i % 3 == 1) ? Severity::Warning : Severity::Advisory;
        d_ptr->m_tiles[i].state = AlarmState::Normal;
        d_ptr->m_tiles[i].alarmActive = false;
    }

    connect(&d_ptr->m_flashTimer, &QTimer::timeout, this, &AnnunciatorPanel::onFlashTimerTick);
    d_ptr->m_flashTimer.start(125); // 8 Hz ticks for smooth flash phase synchronization
}

AnnunciatorPanel::~AnnunciatorPanel()
{
    d_ptr->m_flashTimer.stop();
}

int AnnunciatorPanel::rows() const { Q_D(const AnnunciatorPanel); return d->m_rows; }
int AnnunciatorPanel::columns() const { Q_D(const AnnunciatorPanel); return d->m_columns; }
int AnnunciatorPanel::tileCount() const { Q_D(const AnnunciatorPanel); return d->m_tiles.size(); }
AnnunciatorPanel::AnnunciatorSequence AnnunciatorPanel::sequence() const { Q_D(const AnnunciatorPanel); return d->m_sequence; }
bool AnnunciatorPanel::isLampTestActive() const { Q_D(const AnnunciatorPanel); return d->m_lampTest; }
bool AnnunciatorPanel::isAudibleHornActive() const { Q_D(const AnnunciatorPanel); return d->m_hornActive; }

QColor AnnunciatorPanel::frameColor() const { Q_D(const AnnunciatorPanel); return d->m_frameColor; }
QColor AnnunciatorPanel::gridColor() const { Q_D(const AnnunciatorPanel); return d->m_gridColor; }
QColor AnnunciatorPanel::criticalColor() const { Q_D(const AnnunciatorPanel); return d->m_criticalColor; }
QColor AnnunciatorPanel::warningColor() const { Q_D(const AnnunciatorPanel); return d->m_warningColor; }
QColor AnnunciatorPanel::advisoryColor() const { Q_D(const AnnunciatorPanel); return d->m_advisoryColor; }
QColor AnnunciatorPanel::textColor() const { Q_D(const AnnunciatorPanel); return d->m_textColor; }


QSize AnnunciatorPanel::sizeHint() const
{
    // Approximately 120x65 per tile + margins
    return QSize(d_ptr->m_columns * 130 + 20, d_ptr->m_rows * 75 + 20);
}

QSize AnnunciatorPanel::minimumSizeHint() const
{
    return QSize(d_ptr->m_columns * 60 + 16, d_ptr->m_rows * 36 + 16);
}

int AnnunciatorPanel::activeAlarmsCount() const
{
    int count = 0;
    for (const auto &tile : d_ptr->m_tiles) {
        if (tile.alarmActive || tile.state != AlarmState::Normal) {
            count++;
        }
    }
    return count;
}

int AnnunciatorPanel::unacknowledgedCount() const
{
    int count = 0;
    for (const auto &tile : d_ptr->m_tiles) {
        if (tile.state == AlarmState::Unacknowledged) {
            count++;
        }
    }
    return count;
}

QString AnnunciatorPanel::tileText(int index) const
{
    if (index >= 0 && index < d_ptr->m_tiles.size()) {
        return d_ptr->m_tiles[index].text;
    }
    return QString();
}

AnnunciatorPanel::Severity AnnunciatorPanel::tileSeverity(int index) const
{
    if (index >= 0 && index < d_ptr->m_tiles.size()) {
        return d_ptr->m_tiles[index].severity;
    }
    return Severity::Critical;
}

AnnunciatorPanel::AlarmState AnnunciatorPanel::tileState(int index) const
{
    if (index >= 0 && index < d_ptr->m_tiles.size()) {
        return d_ptr->m_tiles[index].state;
    }
    return AlarmState::Normal;
}

bool AnnunciatorPanel::isAlarmActive(int index) const
{
    if (index >= 0 && index < d_ptr->m_tiles.size()) {
        return d_ptr->m_tiles[index].alarmActive;
    }
    return false;
}

void AnnunciatorPanel::setRows(int rows)
{
    setGridSize(rows, d_ptr->m_columns);
}

void AnnunciatorPanel::setColumns(int cols)
{
    setGridSize(d_ptr->m_rows, cols);
}

void AnnunciatorPanel::setGridSize(int rows, int cols)
{
    int newRows = std::max(1, rows);
    int newCols = std::max(1, cols);
    if (newRows == d_ptr->m_rows && newCols == d_ptr->m_columns) {
        return;
    }

    d_ptr->m_rows = newRows;
    d_ptr->m_columns = newCols;
    d_ptr->m_tiles.resize(d_ptr->m_rows * d_ptr->m_columns);

    invalidateCache();
    updateHornAndSummary();
    Q_EMIT appearanceChanged();
    update();
}

void AnnunciatorPanel::setSequence(AnnunciatorSequence sequence)
{
    if (d_ptr->m_sequence != sequence) {
        d_ptr->m_sequence = sequence;
        Q_EMIT appearanceChanged();
    }
}

void AnnunciatorPanel::setTileText(int index, const QString &text)
{
    if (index >= 0 && index < d_ptr->m_tiles.size()) {
        if (d_ptr->m_tiles[index].text != text) {
            d_ptr->m_tiles[index].text = text;
            update();
        }
    }
}

void AnnunciatorPanel::setTileText(int row, int col, const QString &text)
{
    if (row >= 0 && row < d_ptr->m_rows && col >= 0 && col < d_ptr->m_columns) {
        setTileText(row * d_ptr->m_columns + col, text);
    }
}

void AnnunciatorPanel::setTileSeverity(int index, Severity severity)
{
    if (index >= 0 && index < d_ptr->m_tiles.size()) {
        if (d_ptr->m_tiles[index].severity != severity) {
            d_ptr->m_tiles[index].severity = severity;
            update();
        }
    }
}

void AnnunciatorPanel::setTileSeverity(int row, int col, Severity severity)
{
    if (row >= 0 && row < d_ptr->m_rows && col >= 0 && col < d_ptr->m_columns) {
        setTileSeverity(row * d_ptr->m_columns + col, severity);
    }
}

void AnnunciatorPanel::setAlarmActive(int index, bool active)
{
    if (index < 0 || index >= d_ptr->m_tiles.size()) {
        return;
    }

    TileData &tile = d_ptr->m_tiles[index];
    if (tile.alarmActive == active) {
        return;
    }

    tile.alarmActive = active;

    if (active) {
        // Entering alarm: triggers unacknowledged state and resets silence
        tile.state = AlarmState::Unacknowledged;
        d_ptr->m_hornSilenced = false;
        Q_EMIT tileStateChanged(index, tile.state);
    } else {
        // Clearing alarm condition
        if (tile.state == AlarmState::Acknowledged) {
            if (d_ptr->m_sequence == AnnunciatorSequence::SequenceA_AutomaticReset) {
                tile.state = AlarmState::Normal;
            } else {
                tile.state = AlarmState::Ringback;
            }
            Q_EMIT tileStateChanged(index, tile.state);
        } else if (tile.state == AlarmState::Unacknowledged) {
            // Alarm cleared before operator acknowledged
            // In Sequence A and M, typically continues flashing until ack'd
        }
    }

    updateHornAndSummary();
    update();
}

void AnnunciatorPanel::setAlarmActive(int row, int col, bool active)
{
    if (row >= 0 && row < d_ptr->m_rows && col >= 0 && col < d_ptr->m_columns) {
        setAlarmActive(row * d_ptr->m_columns + col, active);
    }
}

void AnnunciatorPanel::triggerAlarm(int index)
{
    setAlarmActive(index, true);
}

void AnnunciatorPanel::clearAlarm(int index)
{
    setAlarmActive(index, false);
}

void AnnunciatorPanel::acknowledgeAll()
{
    bool changed = false;
    for (int i = 0; i < d_ptr->m_tiles.size(); ++i) {
        if (d_ptr->m_tiles[i].state == AlarmState::Unacknowledged) {
            if (d_ptr->m_tiles[i].alarmActive) {
                d_ptr->m_tiles[i].state = AlarmState::Acknowledged;
            } else {
                d_ptr->m_tiles[i].state = AlarmState::Normal;
            }
            Q_EMIT tileStateChanged(i, d_ptr->m_tiles[i].state);
            changed = true;
        }
    }

    d_ptr->m_hornSilenced = true;
    updateHornAndSummary();
    if (changed) {
        update();
    }
}

void AnnunciatorPanel::acknowledge(int index)
{
    if (index < 0 || index >= d_ptr->m_tiles.size()) {
        return;
    }

    TileData &tile = d_ptr->m_tiles[index];
    if (tile.state == AlarmState::Unacknowledged) {
        if (tile.alarmActive) {
            tile.state = AlarmState::Acknowledged;
        } else {
            tile.state = AlarmState::Normal;
        }
        Q_EMIT tileStateChanged(index, tile.state);
        updateHornAndSummary();
        update();
    }
}

void AnnunciatorPanel::silence()
{
    if (!d_ptr->m_hornSilenced) {
        d_ptr->m_hornSilenced = true;
        updateHornAndSummary();
    }
}

void AnnunciatorPanel::resetAll()
{
    bool changed = false;
    for (int i = 0; i < d_ptr->m_tiles.size(); ++i) {
        if (!d_ptr->m_tiles[i].alarmActive) {
            if (d_ptr->m_tiles[i].state == AlarmState::Ringback || d_ptr->m_tiles[i].state == AlarmState::Acknowledged) {
                d_ptr->m_tiles[i].state = AlarmState::Normal;
                Q_EMIT tileStateChanged(i, AlarmState::Normal);
                changed = true;
            }
        }
    }

    updateHornAndSummary();
    if (changed) {
        update();
    }
}

void AnnunciatorPanel::reset(int index)
{
    if (index < 0 || index >= d_ptr->m_tiles.size()) {
        return;
    }

    TileData &tile = d_ptr->m_tiles[index];
    if (!tile.alarmActive && (tile.state == AlarmState::Ringback || tile.state == AlarmState::Acknowledged)) {
        tile.state = AlarmState::Normal;
        Q_EMIT tileStateChanged(index, AlarmState::Normal);
        updateHornAndSummary();
        update();
    }
}

void AnnunciatorPanel::setLampTest(bool active)
{
    if (d_ptr->m_lampTest != active) {
        d_ptr->m_lampTest = active;
        Q_EMIT appearanceChanged();
        update();
    }
}

void AnnunciatorPanel::setFrameColor(const QColor &color)
{
    if (d_ptr->m_frameColor != color) {
        d_ptr->m_frameColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void AnnunciatorPanel::setGridColor(const QColor &color)
{
    if (d_ptr->m_gridColor != color) {
        d_ptr->m_gridColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void AnnunciatorPanel::setCriticalColor(const QColor &color)
{
    if (d_ptr->m_criticalColor != color) {
        d_ptr->m_criticalColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void AnnunciatorPanel::setWarningColor(const QColor &color)
{
    if (d_ptr->m_warningColor != color) {
        d_ptr->m_warningColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void AnnunciatorPanel::setAdvisoryColor(const QColor &color)
{
    if (d_ptr->m_advisoryColor != color) {
        d_ptr->m_advisoryColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void AnnunciatorPanel::setTextColor(const QColor &color)
{
    if (d_ptr->m_textColor != color) {
        d_ptr->m_textColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void AnnunciatorPanel::updateHornAndSummary()
{
    int unack = unacknowledgedCount();
    bool shouldHorn = (unack > 0) && !d_ptr->m_hornSilenced;
    if (d_ptr->m_hornActive != shouldHorn) {
        d_ptr->m_hornActive = shouldHorn;
        Q_EMIT audibleHornChanged(d_ptr->m_hornActive);
    }

    Q_EMIT activeAlarmsCountChanged(activeAlarmsCount());
    Q_EMIT unacknowledgedCountChanged(unack);
}

void AnnunciatorPanel::onFlashTimerTick()
{
    d_ptr->m_flashTickCounter++;

    // Fast flash: 2 Hz (toggle every 250ms = 2 ticks of 125ms)
    bool newFastPhase = ((d_ptr->m_flashTickCounter / 2) % 2) == 0;
    // Slow flash: 0.8 Hz (toggle every 625ms = 5 ticks of 125ms)
    bool newSlowPhase = ((d_ptr->m_flashTickCounter / 5) % 2) == 0;

    bool needsRedraw = false;
    if (newFastPhase != d_ptr->m_fastFlashPhase) {
        d_ptr->m_fastFlashPhase = newFastPhase;
        if (unacknowledgedCount() > 0) {
            needsRedraw = true;
        }
    }

    if (newSlowPhase != d_ptr->m_slowFlashPhase) {
        d_ptr->m_slowFlashPhase = newSlowPhase;
        for (const auto &tile : d_ptr->m_tiles) {
            if (tile.state == AlarmState::Ringback) {
                needsRedraw = true;
                break;
            }
        }
    }

    if (needsRedraw) {
        update();
    }
}

void AnnunciatorPanel::invalidateCache()
{
    d_ptr->m_cacheDirty = true;
}

void AnnunciatorPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void AnnunciatorPanel::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        invalidateCache();
        update();
    }
}

void AnnunciatorPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int idx = tileIndexAt(event->position());
        if (idx >= 0 && idx < d_ptr->m_tiles.size()) {
            Q_EMIT tileClicked(idx);
            // Click to acknowledge unacknowledged alarm
            if (d_ptr->m_tiles[idx].state == AlarmState::Unacknowledged) {
                acknowledge(idx);
            } else if (d_ptr->m_tiles[idx].state == AlarmState::Ringback) {
                reset(idx);
            }
        }
    }
    QWidget::mousePressEvent(event);
}

QRectF AnnunciatorPanel::calculateTileRect(int row, int col) const
{
    const double margin = 12.0;
    const double spacing = 4.0;

    double availableW = width() - 2.0 * margin - (d_ptr->m_columns - 1) * spacing;
    double availableH = height() - 2.0 * margin - (d_ptr->m_rows - 1) * spacing;

    double tileW = std::max(20.0, availableW / d_ptr->m_columns);
    double tileH = std::max(15.0, availableH / d_ptr->m_rows);

    double x = margin + col * (tileW + spacing);
    double y = margin + row * (tileH + spacing);

    return QRectF(x, y, tileW, tileH);
}

int AnnunciatorPanel::tileIndexAt(const QPointF &pos) const
{
    for (int r = 0; r < d_ptr->m_rows; ++r) {
        for (int c = 0; c < d_ptr->m_columns; ++c) {
            if (calculateTileRect(r, c).contains(pos)) {
                return r * d_ptr->m_columns + c;
            }
        }
    }
    return -1;
}

QColor AnnunciatorPanel::colorForSeverity(Severity severity, bool lit) const
{
    QColor base;
    switch (severity) {
    case Severity::Critical: base = d_ptr->m_criticalColor; break;
    case Severity::Warning:  base = d_ptr->m_warningColor; break;
    case Severity::Advisory: base = d_ptr->m_advisoryColor; break;
    }

    if (lit) {
        return base;
    } else {
        // Deep unlit subdued background
        return QColor(base.red() / 6, base.green() / 6, base.blue() / 6, 220);
    }
}

void AnnunciatorPanel::renderStaticFrame(const QSize &size)
{
    qreal dpr = devicePixelRatioF();
    QSize pixelSize = size * dpr;
    if (pixelSize.isEmpty()) {
        return;
    }

    d_ptr->m_cachePixmap = QPixmap(pixelSize);
    d_ptr->m_cachePixmap.setDevicePixelRatio(dpr);
    d_ptr->m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&d_ptr->m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF frameRect(0.5, 0.5, size.width() - 1.0, size.height() - 1.0);

    // Chassis outer drop border
    QPainterPath outerPath;
    outerPath.addRoundedRect(frameRect, 6.0, 6.0);

    QLinearGradient chassisGrad(frameRect.topLeft(), frameRect.bottomRight());
    chassisGrad.setColorAt(0.0, d_ptr->m_frameColor.lighter(135));
    chassisGrad.setColorAt(0.4, d_ptr->m_frameColor);
    chassisGrad.setColorAt(1.0, d_ptr->m_frameColor.darker(140));

    painter.fillPath(outerPath, chassisGrad);

    // Chassis bevel highlight
    painter.setPen(QPen(d_ptr->m_frameColor.lighter(150), 1.2));
    painter.drawRoundedRect(frameRect.adjusted(1, 1, -1, -1), 5.0, 5.0);

    painter.setPen(QPen(QColor(10, 14, 20, 200), 1.5));
    painter.drawRoundedRect(frameRect, 6.0, 6.0);

    // Corner decorative industrial hex bolts
    const double screwOffset = 6.0;
    const double screwRadius = 3.0;
    QList<QPointF> screwPoints = {
        QPointF(screwOffset, screwOffset),
        QPointF(size.width() - screwOffset, screwOffset),
        QPointF(screwOffset, size.height() - screwOffset),
        QPointF(size.width() - screwOffset, size.height() - screwOffset)
    };

    painter.setPen(QPen(QColor(20, 24, 30), 0.8));
    for (const auto &pt : screwPoints) {
        QRadialGradient screwGrad(pt, screwRadius);
        screwGrad.setColorAt(0.0, QColor(140, 150, 165));
        screwGrad.setColorAt(0.7, QColor(70, 78, 90));
        screwGrad.setColorAt(1.0, QColor(35, 40, 48));
        painter.setBrush(screwGrad);
        painter.drawEllipse(pt, screwRadius, screwRadius);
    }

    // Grid divider channels
    painter.setPen(QPen(d_ptr->m_gridColor, 1.0));
    painter.setBrush(Qt::NoBrush);

    for (int r = 0; r < d_ptr->m_rows; ++r) {
        for (int c = 0; c < d_ptr->m_columns; ++c) {
            QRectF tileRect = calculateTileRect(r, c);
            QRectF cellFrame = tileRect.adjusted(-1.5, -1.5, 1.5, 1.5);
            painter.setPen(QPen(QColor(15, 20, 28, 220), 1.0));
            painter.drawRoundedRect(cellFrame, 2.0, 2.0);
        }
    }

    d_ptr->m_cacheDirty = false;
}

void AnnunciatorPanel::paintEvent(QPaintEvent *)
{
    if (d_ptr->m_cacheDirty || d_ptr->m_cachePixmap.size() != (size() * devicePixelRatioF())) {
        renderStaticFrame(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Draw static cached frame
    painter.drawPixmap(0, 0, d_ptr->m_cachePixmap);

    // Draw dynamic alarm tiles
    for (int r = 0; r < d_ptr->m_rows; ++r) {
        for (int c = 0; c < d_ptr->m_columns; ++c) {
            int idx = r * d_ptr->m_columns + c;
            if (idx >= d_ptr->m_tiles.size()) {
                continue;
            }

            const TileData &tile = d_ptr->m_tiles[idx];
            QRectF tileRect = calculateTileRect(r, c);

            // Determine if tile is currently illuminated
            bool lit = false;
            if (d_ptr->m_lampTest) {
                lit = true;
            } else {
                switch (tile.state) {
                case AlarmState::Normal:
                    lit = false;
                    break;
                case AlarmState::Unacknowledged:
                    lit = d_ptr->m_fastFlashPhase;
                    break;
                case AlarmState::Acknowledged:
                    lit = true;
                    break;
                case AlarmState::Ringback:
                    lit = d_ptr->m_slowFlashPhase;
                    break;
                }
            }

            QColor activeColor = colorForSeverity(tile.severity, lit);

            // Draw tile acrylic faceplate
            QLinearGradient tileGrad(tileRect.topLeft(), tileRect.bottomLeft());
            if (lit) {
                tileGrad.setColorAt(0.0, activeColor.lighter(130));
                tileGrad.setColorAt(0.5, activeColor);
                tileGrad.setColorAt(1.0, activeColor.darker(120));
            } else {
                tileGrad.setColorAt(0.0, activeColor.lighter(120));
                tileGrad.setColorAt(0.6, activeColor);
                tileGrad.setColorAt(1.0, activeColor.darker(140));
            }

            painter.setPen(QPen(lit ? activeColor.lighter(150) : QColor(45, 52, 65), lit ? 1.4 : 1.0));
            painter.setBrush(tileGrad);
            painter.drawRoundedRect(tileRect, 2.5, 2.5);

            // Acrylic glass top reflection highlight
            if (tileRect.height() > 18.0) {
                QRectF glossRect = tileRect.adjusted(1.0, 1.0, -1.0, -tileRect.height() * 0.6);
                QLinearGradient glossGrad(glossRect.topLeft(), glossRect.bottomLeft());
                glossGrad.setColorAt(0.0, QColor(255, 255, 255, lit ? 80 : 35));
                glossGrad.setColorAt(1.0, QColor(255, 255, 255, lit ? 15 : 5));
                painter.setPen(Qt::NoPen);
                painter.setBrush(glossGrad);
                painter.drawRoundedRect(glossRect, 2.0, 2.0);
            }

            // Engraved text legend
            if (!tile.text.isEmpty()) {
                QRectF textRect = tileRect.adjusted(4.0, 3.0, -4.0, -3.0);

                // Auto-scale font size to fit tile comfortably
                QFont font = painter.font();
                font.setBold(true);
                int ptSize = std::clamp(static_cast<int>(tileRect.height() * 0.22), 7, 13);
                font.setPointSize(ptSize);
                painter.setFont(font);

                if (lit) {
                    // Slight dark text shadow for high legibility on luminous background
                    painter.setPen(QPen(QColor(10, 10, 15, 140)));
                    painter.drawText(textRect.translated(0.6, 0.8), Qt::AlignCenter | Qt::TextWordWrap, tile.text);

                    // High contrast white text
                    painter.setPen(d_ptr->m_textColor);
                    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, tile.text);
                } else {
                    // Subdued unlit engraved legend
                    painter.setPen(QColor(d_ptr->m_textColor.red() / 2, d_ptr->m_textColor.green() / 2, d_ptr->m_textColor.blue() / 2, 190));
                    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, tile.text);
                }
            }
        }
    }
}

} // namespace QtIndustrialWidgets
