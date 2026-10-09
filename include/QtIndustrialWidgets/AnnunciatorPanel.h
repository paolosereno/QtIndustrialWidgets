/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtCore/QVector>
#include <QtCore/QString>
#include <memory>

/**
 * \class AnnunciatorPanel
 * \brief Industrial alarm annunciator window matrix conforming to the ANSI/ISA-18.1 standard.
 *
 * AnnunciatorPanel emulates hardwired and SCADA alarm annunciator light panels used in
 * process control rooms, power generation stations, maritime vessels, and industrial plants.
 *
 * Features include:
 * - Configurable N x M grid of backlit acrylic indicator tiles with engraved legends.
 * - Conformance with ANSI/ISA-18.1 alarm sequences (Sequence A: Automatic Reset, Sequence M: Manual Reset).
 * - Multi-stage tile states: Normal (dark), Unacknowledged (rapid flash), Acknowledged (solid on), and Ringback (slow flash).
 * - Alarm severity classification: Critical (Red), Warning (Amber), Advisory (Cyan/White).
 * - Global operations: Acknowledge (ACK), Silence (Mute horn), Reset, and Lamp Test (simultaneous bulb check).
 * - Audible horn output signal (`audibleHornChanged(bool)`).
 * - Interactive tile acknowledgement via direct mouse click.
 * - Hardware-accelerated Hi-DPI background caching for high rendering performance.
 *
 * \code
 * auto *panel = new AnnunciatorPanel(parent);
 * panel->setGridSize(2, 4); // 2 rows x 4 columns = 8 alarm windows
 * panel->setTileText(0, "TURBINE 1\nBEARING TRIP");
 * panel->setTileSeverity(0, AnnunciatorPanel::Severity::Critical);
 *
 * panel->setTileText(1, "MAIN STEAM\nPRESS LOW");
 * panel->setTileSeverity(1, AnnunciatorPanel::Severity::Warning);
 *
 * // Trigger an alarm event:
 * panel->setAlarmActive(0, true);
 * \endcode
 */
namespace QtIndustrialWidgets {

class AnnunciatorPanelPrivate;

class QTINDUSTRIALWIDGETS_EXPORT AnnunciatorPanel : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int rows READ rows WRITE setRows NOTIFY appearanceChanged)
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY appearanceChanged)
    Q_PROPERTY(int tileCount READ tileCount NOTIFY appearanceChanged)
    Q_PROPERTY(AnnunciatorSequence sequence READ sequence WRITE setSequence NOTIFY appearanceChanged)
    Q_PROPERTY(bool lampTestActive READ isLampTestActive WRITE setLampTest NOTIFY appearanceChanged)
    Q_PROPERTY(bool audibleHornActive READ isAudibleHornActive NOTIFY audibleHornChanged)
    Q_PROPERTY(int activeAlarmsCount READ activeAlarmsCount NOTIFY activeAlarmsCountChanged)
    Q_PROPERTY(int unacknowledgedCount READ unacknowledgedCount NOTIFY unacknowledgedCountChanged)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor criticalColor READ criticalColor WRITE setCriticalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor advisoryColor READ advisoryColor WRITE setAdvisoryColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)

public:
    /** \brief Alarm severity categorization determining the illuminated tile color. */
    enum class Severity {
        Critical, ///< Red indicator (Emergency trip / fault)
        Warning,  ///< Amber indicator (Caution / threshold exceeded)
        Advisory  ///< Cyan/Blue indicator (Status / auxiliary advisory)
    };
    Q_ENUM(Severity)

    /** \brief Operational state of an individual alarm window according to ISA-18.1. */
    enum class AlarmState {
        Normal,         ///< Inactive, unlit or faint glow
        Unacknowledged, ///< Active alarm, fast flashing with horn sound
        Acknowledged,   ///< Active alarm acknowledged, steady steady illumination
        Ringback        ///< Cleared condition waiting for manual reset, slow flashing
    };
    Q_ENUM(AlarmState)

    /** \brief Operating alarm sequence logic standard. */
    enum class AnnunciatorSequence {
        SequenceA_AutomaticReset, ///< ISA-18.1 Sequence A (Auto reset upon condition clearing)
        SequenceM_ManualReset     ///< ISA-18.1 Sequence M (Requires operator manual reset)
    };
    Q_ENUM(AnnunciatorSequence)

    /** \brief Data container describing an individual alarm window. */
    struct TileData {
        QString text;
        Severity severity{Severity::Critical};
        AlarmState state{AlarmState::Normal};
        bool alarmActive{false};
    };

    /**
     * \brief Constructs a AnnunciatorPanel with a 2x4 matrix of alarm windows.
     * \param parent Optional parent widget.
     */
    explicit AnnunciatorPanel(QWidget *parent = nullptr);
    /**
     * \brief Constructs a AnnunciatorPanel with custom rows and columns.
     * \param rows Number of vertical grid rows.
     * \param cols Number of horizontal grid columns.
     * \param parent Optional parent widget.
     */
    AnnunciatorPanel(int rows, int cols, QWidget *parent = nullptr);
    ~AnnunciatorPanel() override;

    /** \brief Returns the number of grid rows. */
    [[nodiscard]] int rows() const;
    /** \brief Returns the number of grid columns. */
    [[nodiscard]] int columns() const;
    /** \brief Returns the total number of alarm tiles (rows * columns). */
    [[nodiscard]] int tileCount() const;
    /** \brief Returns the operational sequence standard. */
    [[nodiscard]] AnnunciatorSequence sequence() const;
    /** \brief Returns true if all windows are currently illuminated by Lamp Test. */
    [[nodiscard]] bool isLampTestActive() const;
    /** \brief Returns true if the audible horn signal is currently requested. */
    [[nodiscard]] bool isAudibleHornActive() const;
    /** \brief Returns the total number of currently active alarms. */
    [[nodiscard]] int activeAlarmsCount() const;
    /** \brief Returns the number of unacknowledged alarms. */
    [[nodiscard]] int unacknowledgedCount() const;

    /** \brief Returns the engraved text for a given tile index. */
    [[nodiscard]] QString tileText(int index) const;
    /** \brief Returns the severity category for a given tile index. */
    [[nodiscard]] Severity tileSeverity(int index) const;
    /** \brief Returns the current ISA-18.1 state for a given tile index. */
    [[nodiscard]] AlarmState tileState(int index) const;
    /** \brief Returns true if the underlying sensor contact is currently in alarm. */
    [[nodiscard]] bool isAlarmActive(int index) const;

    /** \brief Returns the outer chassis bezel frame color. */
    [[nodiscard]] QColor frameColor() const;
    /** \brief Returns the internal matrix grid dividers color. */
    [[nodiscard]] QColor gridColor() const;
    /** \brief Returns the critical red alarm illumination color. */
    [[nodiscard]] QColor criticalColor() const;
    /** \brief Returns the warning amber alarm illumination color. */
    [[nodiscard]] QColor warningColor() const;
    /** \brief Returns the advisory cyan/blue alarm illumination color. */
    [[nodiscard]] QColor advisoryColor() const;
    /** \brief Returns the tile engraved lettering font color. */
    [[nodiscard]] QColor textColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the number of grid rows. */
    void setRows(int rows);
    /** \brief Sets the number of grid columns. */
    void setColumns(int cols);
    /** \brief Resizes the annunciator matrix grid dimensions. */
    void setGridSize(int rows, int cols);
    /** \brief Sets the alarm sequence logic. */
    void setSequence(AnnunciatorSequence sequence);

    /** \brief Configures the engraved label text for a tile by linear index. */
    void setTileText(int index, const QString &text);
    /** \brief Configures the engraved label text for a tile by row and column. */
    void setTileText(int row, int col, const QString &text);
    /** \brief Sets the severity category for a tile by linear index. */
    void setTileSeverity(int index, Severity severity);
    /** \brief Sets the severity category for a tile by row and column. */
    void setTileSeverity(int row, int col, Severity severity);

    /** \brief Activates or clears an alarm condition on a specific tile. */
    void setAlarmActive(int index, bool active);
    /** \brief Activates or clears an alarm condition on a specific tile by row and column. */
    void setAlarmActive(int row, int col, bool active);
    /** \brief Convenience slot to trip an alarm condition. */
    void triggerAlarm(int index);
    /** \brief Convenience slot to clear an alarm condition. */
    void clearAlarm(int index);

    /** \brief Operator Acknowledge button: silences horn and turns flashing alarms to steady. */
    void acknowledgeAll();
    /** \brief Acknowledges an individual alarm tile. */
    void acknowledge(int index);
    /** \brief Operator Silence button: silences audible horn while leaving visual flash active. */
    void silence();
    /** \brief Operator Reset button: clears acknowledged return-to-normal alarms. */
    void resetAll();
    /** \brief Resets an individual alarm tile. */
    void reset(int index);

    /** \brief Toggles simultaneous Lamp Test mode on all windows. */
    void setLampTest(bool active);

    /** \brief Sets the outer chassis bezel frame color. */
    void setFrameColor(const QColor &color);
    /** \brief Sets the internal grid divider lines color. */
    void setGridColor(const QColor &color);
    /** \brief Sets the critical red alarm illumination color. */
    void setCriticalColor(const QColor &color);
    /** \brief Sets the warning amber alarm illumination color. */
    void setWarningColor(const QColor &color);
    /** \brief Sets the advisory cyan/blue alarm illumination color. */
    void setAdvisoryColor(const QColor &color);
    /** \brief Sets the tile engraved lettering font color. */
    void setTextColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when an operator clicks on an alarm window tile. */
    void tileClicked(int index);
    /** \brief Emitted when an alarm tile changes its operational state. */
    void tileStateChanged(int index, AlarmState newState);
    /** \brief Emitted when the audible alarm horn state changes (true = sounding). */
    void audibleHornChanged(bool active);
    /** \brief Emitted when the total active alarms count changes. */
    void activeAlarmsCountChanged(int count);
    /** \brief Emitted when the count of unacknowledged alarms changes. */
    void unacknowledgedCountChanged(int count);
    /** \brief Emitted when visual appearance styling properties change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private Q_SLOTS:
    void onFlashTimerTick();

private:
    void invalidateCache();
    void renderStaticFrame(const QSize &size);
    void updateHornAndSummary();
    [[nodiscard]] QRectF calculateTileRect(int row, int col) const;
    [[nodiscard]] int tileIndexAt(const QPointF &pos) const;
    [[nodiscard]] QColor colorForSeverity(Severity severity, bool lit) const;

    std::unique_ptr<AnnunciatorPanelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(AnnunciatorPanel)
};

} // namespace QtIndustrialWidgets
