/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtCore/QVector>
#include <QtCore/QStringList>
#include <memory>

namespace QtIndustrialWidgets {

class LevelMeterPrivate;

/**
 * \class LevelMeter
 * \brief Multi-channel level meter (VU / dBFS, tank levels, channel amplitudes).
 *
 * LevelMeter displays signal levels, sound volume units (VU/dBFS), tank fill levels,
 * or industrial channel amplitudes across single or multiple concurrent channels (e.g. Stereo Left/Right).
 *
 * Features include:
 * - Segmented mode (LED segments with dark gaps between them) or Continuous mode (gradient bar).
 * - Multi-channel rendering with independent channel values and labels.
 * - Dynamic 3-stage colored threshold bands (Normal / Warning / Error overload).
 * - Peak hold indicator bar: held for peakHoldTimeMs(), then falling linearly at peakDecayRate() units per second.
 * - Scale ticks, channel division gutters and labels cached in a pixmap at the device pixel ratio.
 *
 * Every scale tick is drawn, but only the labels that keep max(4 px, half the scale font height) from
 * each other: drawn labels never overlap. The first and last labels (the ends of the scale) are always
 * drawn; if they are too close to each other only the first one is (an end always wins). On a linear
 * scale every k-th label is drawn, k the smallest divisor of 6 that keeps the gap. On the dB ladder
 * (unit "dB" with a range across 0) labels are taken in priority order, each one only if it keeps the
 * gap from those already taken: the ends, 0 dB, then -20, -6, -40, -12, -30, -3, +3, then any other tick
 * in increasing order; ticks outside the range are skipped. A unit is drawn clear of the scale labels:
 * above the bars of a vertical meter (in a band reserved only when the unit is not empty), right of the
 * bars of a horizontal one. From minimumSizeHint() up every label of a horizontal linear scale is drawn.
 *
 * \code
 * auto *meter = new LevelMeter(parent);
 * meter->setChannelCount(2);
 * meter->setChannelLabels({"CH 1", "CH 2"});
 * meter->setRange(-60.0, 6.0);
 * meter->setUnit("dB");
 * meter->setWarningThreshold(-6.0);
 * meter->setErrorThreshold(0.0);
 * meter->setValues({-12.4, -9.8});
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT LevelMeter : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(int channelCount READ channelCount WRITE setChannelCount NOTIFY appearanceChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY appearanceChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY appearanceChanged)
    Q_PROPERTY(int segmentCount READ segmentCount WRITE setSegmentCount NOTIFY appearanceChanged)
    Q_PROPERTY(DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY appearanceChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(bool peakHoldEnabled READ isPeakHoldEnabled WRITE setPeakHoldEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(int peakHoldTimeMs READ peakHoldTimeMs WRITE setPeakHoldTimeMs NOTIFY appearanceChanged)
    Q_PROPERTY(double peakDecayRate READ peakDecayRate WRITE setPeakDecayRate NOTIFY appearanceChanged)
    Q_PROPERTY(bool scaleVisible READ isScaleVisible WRITE setScaleVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)

public:
    /** \brief Visual bar presentation format. */
    enum class DisplayMode {
        Segmented,  ///< Discrete LED segments ladder
        Continuous  ///< Solid gradient fill bar
    };
    Q_ENUM(DisplayMode)

    /**
     * \brief Constructs a vertical, segmented LevelMeter with 2 channels (labels "L" and "R").
     * \param parent Optional parent widget.
     */
    explicit LevelMeter(QWidget *parent = nullptr);
    ~LevelMeter() override;

    /** \brief Returns the number of concurrent channels rendered (default: 2). */
    [[nodiscard]] int channelCount() const;
    /** \brief Returns the minimum scale value. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the maximum scale value. */
    [[nodiscard]] double maximum() const;
    /** \brief Returns the warning threshold value. */
    [[nodiscard]] double warningThreshold() const;
    /** \brief Returns the critical overload error threshold value. */
    [[nodiscard]] double errorThreshold() const;
    /** \brief Returns the number of discrete LED segments per channel. */
    [[nodiscard]] int segmentCount() const;
    /** \brief Returns the display mode (Segmented or Continuous). */
    [[nodiscard]] DisplayMode displayMode() const;
    /** \brief Returns the orientation (Qt::Vertical or Qt::Horizontal). */
    [[nodiscard]] Qt::Orientation orientation() const;
    /** \brief Returns true if temporary peak hold lines are enabled. */
    [[nodiscard]] bool isPeakHoldEnabled() const;
    /** \brief Returns the peak hold stationary dwell duration in milliseconds. */
    [[nodiscard]] int peakHoldTimeMs() const;
    /** \brief Returns the peak fall decay speed per second. */
    [[nodiscard]] double peakDecayRate() const;
    /** \brief Returns true if scale ticks and numeric labels are visible. */
    [[nodiscard]] bool isScaleVisible() const;
    /** \brief Returns the measurement unit label string (e.g. "dB", "VU", "%"). */
    [[nodiscard]] QString unit() const;
    /** \brief Returns the header title caption. */
    [[nodiscard]] QString title() const;

    /** \brief Returns the current value of a channel. */
    [[nodiscard]] double value(int channel = 0) const;
    /** \brief Returns the current peak hold value of a channel. */
    [[nodiscard]] double peakValue(int channel = 0) const;
    /** \brief Returns all channel current values. */
    [[nodiscard]] QVector<double> values() const;

    /** \brief Returns channel identifier labels. */
    [[nodiscard]] QStringList channelLabels() const;

    /** \brief Returns the normal operating segment color (e.g. green). */
    [[nodiscard]] QColor normalColor() const;
    /** \brief Returns the caution warning segment color (e.g. amber). */
    [[nodiscard]] QColor warningColor() const;
    /** \brief Returns the critical overload error segment color (e.g. red). */
    [[nodiscard]] QColor errorColor() const;
    /** \brief Returns the peak hold indicator bar color. */
    [[nodiscard]] QColor peakColor() const;
    /** \brief Returns the recessed channel trough background color. */
    [[nodiscard]] QColor backgroundColor() const;
    /** \brief Returns the scale ticks and font lettering color. */
    [[nodiscard]] QColor textColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the value for channel 0. */
    void setValue(double value);
    /** \brief Sets the value for a specific channel. */
    void setValue(int channel, double value);
    /** \brief Sets values for all channels simultaneously. */
    void setValues(const QVector<double> &values);
    /** \brief Immediately resets all peak hold indicators. */
    void resetPeaks();

    /** \brief Sets the number of concurrent channels. */
    void setChannelCount(int count);
    /** \brief Sets the scale range bounds. */
    void setRange(double min, double max);
    /** \brief Sets the minimum scale value. */
    void setMinimum(double min);
    /** \brief Sets the maximum scale value. */
    void setMaximum(double max);
    /** \brief Sets the warning threshold value. */
    void setWarningThreshold(double threshold);
    /** \brief Sets the error threshold value. */
    void setErrorThreshold(double threshold);
    /** \brief Sets the number of discrete LED segments. */
    void setSegmentCount(int count);
    /** \brief Sets the display presentation mode. */
    void setDisplayMode(DisplayMode mode);
    /** \brief Sets orientation (Qt::Vertical or Qt::Horizontal). */
    void setOrientation(Qt::Orientation orientation);
    /** \brief Enables or disables peak hold indicator bars. */
    void setPeakHoldEnabled(bool enabled);
    /** \brief Sets peak hold dwell duration in milliseconds. */
    void setPeakHoldTimeMs(int ms);
    /** \brief Sets the peak decay rate in units per second. */
    void setPeakDecayRate(double rate);
    /** \brief Toggles visibility of the scale marks. */
    void setScaleVisible(bool visible);
    /** \brief Sets the measurement unit label string. */
    void setUnit(const QString &unit);
    /** \brief Sets the header title caption. */
    void setTitle(const QString &title);
    /** \brief Sets display labels for each channel. */
    void setChannelLabels(const QStringList &labels);
    /** \brief Sets normal zone segment color. */
    void setNormalColor(const QColor &color);
    /** \brief Sets warning zone segment color. */
    void setWarningColor(const QColor &color);
    /** \brief Sets error zone segment color. */
    void setErrorColor(const QColor &color);
    /** \brief Sets peak hold indicator color. */
    void setPeakColor(const QColor &color);
    /** \brief Sets trough chassis background color. */
    void setBackgroundColor(const QColor &color);
    /** \brief Sets scale markings and text labels color. */
    void setTextColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when a channel's value changes. */
    void valueChanged(int channel, double value);
    /** \brief Emitted when a channel's value exceeds the error threshold. */
    void overloadOccurred(int channel);
    /** \brief Emitted when the scale range changes. */
    void rangeChanged(double min, double max);
    /** \brief Emitted when visual styling properties change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private Q_SLOTS:
    void updatePeakDecay();

private:
    void renderStaticBackground();
    void drawChannelBar(QPainter &painter, int chIndex, const QRectF &barRect);
    void drawSegmentedBar(QPainter &painter, double val, double peakVal, const QRectF &barRect);
    void drawContinuousBar(QPainter &painter, double val, double peakVal, const QRectF &barRect);
    QColor colorForNormalizedValue(double norm) const;
    QVector<QRectF> calculateBarRects(const QRectF &contentRect) const;

    std::unique_ptr<LevelMeterPrivate> d_ptr;
    Q_DECLARE_PRIVATE(LevelMeter)
};

} // namespace QtIndustrialWidgets
