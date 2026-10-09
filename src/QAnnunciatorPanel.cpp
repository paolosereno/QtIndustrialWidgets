#include <QtIndustrialWidgets/QAnnunciatorPanel.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QFontMetrics>
#include <algorithm>

QAnnunciatorPanel::QAnnunciatorPanel(QWidget *parent)
    : QAnnunciatorPanel(2, 4, parent)
{
}

QAnnunciatorPanel::QAnnunciatorPanel(int rows, int cols, QWidget *parent)
    : QWidget(parent)
    , m_rows(std::max(1, rows))
    , m_columns(std::max(1, cols))
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_tiles.resize(m_rows * m_columns);
    for (int i = 0; i < m_tiles.size(); ++i) {
        m_tiles[i].text = QStringLiteral("ALARM %1").arg(i + 1);
        m_tiles[i].severity = (i % 3 == 0) ? Severity::Critical :
                              (i % 3 == 1) ? Severity::Warning : Severity::Advisory;
        m_tiles[i].state = AlarmState::Normal;
        m_tiles[i].alarmActive = false;
    }

    connect(&m_flashTimer, &QTimer::timeout, this, &QAnnunciatorPanel::onFlashTimerTick);
    m_flashTimer.start(125); // 8 Hz ticks for smooth flash phase synchronization
}

QAnnunciatorPanel::~QAnnunciatorPanel()
{
    m_flashTimer.stop();
}

QSize QAnnunciatorPanel::sizeHint() const
{
    // Approximately 120x65 per tile + margins
    return QSize(m_columns * 130 + 20, m_rows * 75 + 20);
}

QSize QAnnunciatorPanel::minimumSizeHint() const
{
    return QSize(m_columns * 60 + 16, m_rows * 36 + 16);
}

int QAnnunciatorPanel::activeAlarmsCount() const
{
    int count = 0;
    for (const auto &tile : m_tiles) {
        if (tile.alarmActive || tile.state != AlarmState::Normal) {
            count++;
        }
    }
    return count;
}

int QAnnunciatorPanel::unacknowledgedCount() const
{
    int count = 0;
    for (const auto &tile : m_tiles) {
        if (tile.state == AlarmState::Unacknowledged) {
            count++;
        }
    }
    return count;
}

QString QAnnunciatorPanel::tileText(int index) const
{
    if (index >= 0 && index < m_tiles.size()) {
        return m_tiles[index].text;
    }
    return QString();
}

QAnnunciatorPanel::Severity QAnnunciatorPanel::tileSeverity(int index) const
{
    if (index >= 0 && index < m_tiles.size()) {
        return m_tiles[index].severity;
    }
    return Severity::Critical;
}

QAnnunciatorPanel::AlarmState QAnnunciatorPanel::tileState(int index) const
{
    if (index >= 0 && index < m_tiles.size()) {
        return m_tiles[index].state;
    }
    return AlarmState::Normal;
}

bool QAnnunciatorPanel::isAlarmActive(int index) const
{
    if (index >= 0 && index < m_tiles.size()) {
        return m_tiles[index].alarmActive;
    }
    return false;
}

void QAnnunciatorPanel::setRows(int rows)
{
    setGridSize(rows, m_columns);
}

void QAnnunciatorPanel::setColumns(int cols)
{
    setGridSize(m_rows, cols);
}

void QAnnunciatorPanel::setGridSize(int rows, int cols)
{
    int newRows = std::max(1, rows);
    int newCols = std::max(1, cols);
    if (newRows == m_rows && newCols == m_columns) {
        return;
    }

    m_rows = newRows;
    m_columns = newCols;
    m_tiles.resize(m_rows * m_columns);

    invalidateCache();
    updateHornAndSummary();
    Q_EMIT appearanceChanged();
    update();
}

void QAnnunciatorPanel::setSequence(AnnunciatorSequence sequence)
{
    if (m_sequence != sequence) {
        m_sequence = sequence;
        Q_EMIT appearanceChanged();
    }
}

void QAnnunciatorPanel::setTileText(int index, const QString &text)
{
    if (index >= 0 && index < m_tiles.size()) {
        if (m_tiles[index].text != text) {
            m_tiles[index].text = text;
            update();
        }
    }
}

void QAnnunciatorPanel::setTileText(int row, int col, const QString &text)
{
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        setTileText(row * m_columns + col, text);
    }
}

void QAnnunciatorPanel::setTileSeverity(int index, Severity severity)
{
    if (index >= 0 && index < m_tiles.size()) {
        if (m_tiles[index].severity != severity) {
            m_tiles[index].severity = severity;
            update();
        }
    }
}

void QAnnunciatorPanel::setTileSeverity(int row, int col, Severity severity)
{
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        setTileSeverity(row * m_columns + col, severity);
    }
}

void QAnnunciatorPanel::setAlarmActive(int index, bool active)
{
    if (index < 0 || index >= m_tiles.size()) {
        return;
    }

    TileData &tile = m_tiles[index];
    if (tile.alarmActive == active) {
        return;
    }

    tile.alarmActive = active;

    if (active) {
        // Entering alarm: triggers unacknowledged state and resets silence
        tile.state = AlarmState::Unacknowledged;
        m_hornSilenced = false;
        Q_EMIT tileStateChanged(index, tile.state);
    } else {
        // Clearing alarm condition
        if (tile.state == AlarmState::Acknowledged) {
            if (m_sequence == AnnunciatorSequence::SequenceA_AutomaticReset) {
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

void QAnnunciatorPanel::setAlarmActive(int row, int col, bool active)
{
    if (row >= 0 && row < m_rows && col >= 0 && col < m_columns) {
        setAlarmActive(row * m_columns + col, active);
    }
}

void QAnnunciatorPanel::triggerAlarm(int index)
{
    setAlarmActive(index, true);
}

void QAnnunciatorPanel::clearAlarm(int index)
{
    setAlarmActive(index, false);
}

void QAnnunciatorPanel::acknowledgeAll()
{
    bool changed = false;
    for (int i = 0; i < m_tiles.size(); ++i) {
        if (m_tiles[i].state == AlarmState::Unacknowledged) {
            if (m_tiles[i].alarmActive) {
                m_tiles[i].state = AlarmState::Acknowledged;
            } else {
                m_tiles[i].state = AlarmState::Normal;
            }
            Q_EMIT tileStateChanged(i, m_tiles[i].state);
            changed = true;
        }
    }

    m_hornSilenced = true;
    updateHornAndSummary();
    if (changed) {
        update();
    }
}

void QAnnunciatorPanel::acknowledge(int index)
{
    if (index < 0 || index >= m_tiles.size()) {
        return;
    }

    TileData &tile = m_tiles[index];
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

void QAnnunciatorPanel::silence()
{
    if (!m_hornSilenced) {
        m_hornSilenced = true;
        updateHornAndSummary();
    }
}

void QAnnunciatorPanel::resetAll()
{
    bool changed = false;
    for (int i = 0; i < m_tiles.size(); ++i) {
        if (!m_tiles[i].alarmActive) {
            if (m_tiles[i].state == AlarmState::Ringback || m_tiles[i].state == AlarmState::Acknowledged) {
                m_tiles[i].state = AlarmState::Normal;
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

void QAnnunciatorPanel::reset(int index)
{
    if (index < 0 || index >= m_tiles.size()) {
        return;
    }

    TileData &tile = m_tiles[index];
    if (!tile.alarmActive && (tile.state == AlarmState::Ringback || tile.state == AlarmState::Acknowledged)) {
        tile.state = AlarmState::Normal;
        Q_EMIT tileStateChanged(index, AlarmState::Normal);
        updateHornAndSummary();
        update();
    }
}

void QAnnunciatorPanel::setLampTest(bool active)
{
    if (m_lampTest != active) {
        m_lampTest = active;
        Q_EMIT appearanceChanged();
        update();
    }
}

void QAnnunciatorPanel::setFrameColor(const QColor &color)
{
    if (m_frameColor != color) {
        m_frameColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void QAnnunciatorPanel::setGridColor(const QColor &color)
{
    if (m_gridColor != color) {
        m_gridColor = color;
        invalidateCache();
        Q_EMIT appearanceChanged();
        update();
    }
}

void QAnnunciatorPanel::setCriticalColor(const QColor &color)
{
    if (m_criticalColor != color) {
        m_criticalColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void QAnnunciatorPanel::setWarningColor(const QColor &color)
{
    if (m_warningColor != color) {
        m_warningColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void QAnnunciatorPanel::setAdvisoryColor(const QColor &color)
{
    if (m_advisoryColor != color) {
        m_advisoryColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void QAnnunciatorPanel::setTextColor(const QColor &color)
{
    if (m_textColor != color) {
        m_textColor = color;
        Q_EMIT appearanceChanged();
        update();
    }
}

void QAnnunciatorPanel::updateHornAndSummary()
{
    int unack = unacknowledgedCount();
    bool shouldHorn = (unack > 0) && !m_hornSilenced;
    if (m_hornActive != shouldHorn) {
        m_hornActive = shouldHorn;
        Q_EMIT audibleHornChanged(m_hornActive);
    }

    Q_EMIT activeAlarmsCountChanged(activeAlarmsCount());
    Q_EMIT unacknowledgedCountChanged(unack);
}

void QAnnunciatorPanel::onFlashTimerTick()
{
    m_flashTickCounter++;

    // Fast flash: 2 Hz (toggle every 250ms = 2 ticks of 125ms)
    bool newFastPhase = ((m_flashTickCounter / 2) % 2) == 0;
    // Slow flash: 0.8 Hz (toggle every 625ms = 5 ticks of 125ms)
    bool newSlowPhase = ((m_flashTickCounter / 5) % 2) == 0;

    bool needsRedraw = false;
    if (newFastPhase != m_fastFlashPhase) {
        m_fastFlashPhase = newFastPhase;
        if (unacknowledgedCount() > 0) {
            needsRedraw = true;
        }
    }

    if (newSlowPhase != m_slowFlashPhase) {
        m_slowFlashPhase = newSlowPhase;
        for (const auto &tile : m_tiles) {
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

void QAnnunciatorPanel::invalidateCache()
{
    m_cacheDirty = true;
}

void QAnnunciatorPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    invalidateCache();
}

void QAnnunciatorPanel::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::FontChange) {
        invalidateCache();
        update();
    }
}

void QAnnunciatorPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int idx = tileIndexAt(event->position());
        if (idx >= 0 && idx < m_tiles.size()) {
            Q_EMIT tileClicked(idx);
            // Click to acknowledge unacknowledged alarm
            if (m_tiles[idx].state == AlarmState::Unacknowledged) {
                acknowledge(idx);
            } else if (m_tiles[idx].state == AlarmState::Ringback) {
                reset(idx);
            }
        }
    }
    QWidget::mousePressEvent(event);
}

QRectF QAnnunciatorPanel::calculateTileRect(int row, int col) const
{
    const double margin = 12.0;
    const double spacing = 4.0;

    double availableW = width() - 2.0 * margin - (m_columns - 1) * spacing;
    double availableH = height() - 2.0 * margin - (m_rows - 1) * spacing;

    double tileW = std::max(20.0, availableW / m_columns);
    double tileH = std::max(15.0, availableH / m_rows);

    double x = margin + col * (tileW + spacing);
    double y = margin + row * (tileH + spacing);

    return QRectF(x, y, tileW, tileH);
}

int QAnnunciatorPanel::tileIndexAt(const QPointF &pos) const
{
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            if (calculateTileRect(r, c).contains(pos)) {
                return r * m_columns + c;
            }
        }
    }
    return -1;
}

QColor QAnnunciatorPanel::colorForSeverity(Severity severity, bool lit) const
{
    QColor base;
    switch (severity) {
    case Severity::Critical: base = m_criticalColor; break;
    case Severity::Warning:  base = m_warningColor; break;
    case Severity::Advisory: base = m_advisoryColor; break;
    }

    if (lit) {
        return base;
    } else {
        // Deep unlit subdued background
        return QColor(base.red() / 6, base.green() / 6, base.blue() / 6, 220);
    }
}

void QAnnunciatorPanel::renderStaticFrame(const QSize &size)
{
    qreal dpr = devicePixelRatioF();
    QSize pixelSize = size * dpr;
    if (pixelSize.isEmpty()) {
        return;
    }

    m_cachePixmap = QPixmap(pixelSize);
    m_cachePixmap.setDevicePixelRatio(dpr);
    m_cachePixmap.fill(Qt::transparent);

    QPainter painter(&m_cachePixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF frameRect(0.5, 0.5, size.width() - 1.0, size.height() - 1.0);

    // Chassis outer drop border
    QPainterPath outerPath;
    outerPath.addRoundedRect(frameRect, 6.0, 6.0);

    QLinearGradient chassisGrad(frameRect.topLeft(), frameRect.bottomRight());
    chassisGrad.setColorAt(0.0, m_frameColor.lighter(135));
    chassisGrad.setColorAt(0.4, m_frameColor);
    chassisGrad.setColorAt(1.0, m_frameColor.darker(140));

    painter.fillPath(outerPath, chassisGrad);

    // Chassis bevel highlight
    painter.setPen(QPen(m_frameColor.lighter(150), 1.2));
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
    painter.setPen(QPen(m_gridColor, 1.0));
    painter.setBrush(Qt::NoBrush);

    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            QRectF tileRect = calculateTileRect(r, c);
            QRectF cellFrame = tileRect.adjusted(-1.5, -1.5, 1.5, 1.5);
            painter.setPen(QPen(QColor(15, 20, 28, 220), 1.0));
            painter.drawRoundedRect(cellFrame, 2.0, 2.0);
        }
    }

    m_cacheDirty = false;
}

void QAnnunciatorPanel::paintEvent(QPaintEvent *)
{
    if (m_cacheDirty || m_cachePixmap.size() != (size() * devicePixelRatioF())) {
        renderStaticFrame(size());
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Draw static cached frame
    painter.drawPixmap(0, 0, m_cachePixmap);

    // Draw dynamic alarm tiles
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_columns; ++c) {
            int idx = r * m_columns + c;
            if (idx >= m_tiles.size()) {
                continue;
            }

            const TileData &tile = m_tiles[idx];
            QRectF tileRect = calculateTileRect(r, c);

            // Determine if tile is currently illuminated
            bool lit = false;
            if (m_lampTest) {
                lit = true;
            } else {
                switch (tile.state) {
                case AlarmState::Normal:
                    lit = false;
                    break;
                case AlarmState::Unacknowledged:
                    lit = m_fastFlashPhase;
                    break;
                case AlarmState::Acknowledged:
                    lit = true;
                    break;
                case AlarmState::Ringback:
                    lit = m_slowFlashPhase;
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
                    painter.setPen(m_textColor);
                    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, tile.text);
                } else {
                    // Subdued unlit engraved legend
                    painter.setPen(QColor(m_textColor.red() / 2, m_textColor.green() / 2, m_textColor.blue() / 2, 190));
                    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, tile.text);
                }
            }
        }
    }
}
