/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <QtGui/QColor>
#include <QtCore/QString>
#include <memory>

namespace QtIndustrialWidgets {

class CircularBargraphPrivate;

/**
 * \class CircularBargraph
 * \brief High-contrast circular arc indicator composed of discrete curved LED segments.
 *
 * CircularBargraph displays sensor readings, process variables, or audio levels across an
 * adjustable angular arc composed of discrete, curved LED segments (e.g. 16, 24, 32, or 48 segments).
 *
 * Key features:
 * - Operating modes: progressive illuminated LED stack (\c DisplayMode::Bar) or single cursor LED (\c DisplayMode::Dot).
 * - Global threshold color stages: Normal (green), Warning (amber), and Critical (red).
 * - Peak-hold indicator segment with configurable hold duration and ballistic decay falloff.
 * - Optional centered digital readout pod with unit label.
 * - Robust fail-safe hold on NaN, bounded clamping on ±Inf, and D-pointer ABI stability.
 *
 * \code
 * auto *bargraph = new qiw::CircularBargraph(parent);
 * bargraph->setRange(0.0, 100.0);
 * bargraph->setWarningThreshold(70.0);
 * bargraph->setErrorThreshold(85.0);
 * bargraph->setValue(45.0);
 * \endcode
 */
class QTINDUSTRIALWIDGETS_EXPORT CircularBargraph : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(int segmentCount READ segmentCount WRITE setSegmentCount NOTIFY appearanceChanged)
    Q_PROPERTY(double startAngle READ startAngle WRITE setStartAngle NOTIFY appearanceChanged)
    Q_PROPERTY(double spanAngle READ spanAngle WRITE setSpanAngle NOTIFY appearanceChanged)
    Q_PROPERTY(DisplayMode displayMode READ displayMode WRITE setDisplayMode NOTIFY appearanceChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(bool peakHoldEnabled READ isPeakHoldEnabled WRITE setPeakHoldEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(int peakHoldTimeMs READ peakHoldTimeMs WRITE setPeakHoldTimeMs NOTIFY appearanceChanged)
    Q_PROPERTY(double peakDecayRate READ peakDecayRate WRITE setPeakDecayRate NOTIFY appearanceChanged)
    Q_PROPERTY(bool digitalDisplayVisible READ isDigitalDisplayVisible WRITE setDigitalDisplayVisible NOTIFY appearanceChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor ghostColor READ ghostColor WRITE setGhostColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor dialColor READ dialColor WRITE setDialColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)

public:
    /** \brief Operating visual presentation mode of the LED segments. */
    enum class DisplayMode {
        Bar,  ///< Progressive illuminated LED stack from minimum to current value
        Dot   ///< Single active cursor LED indicating current value
    };
    Q_ENUM(DisplayMode)

    /**
     * \brief Constructs a CircularBargraph widget with default styling.
     * \param parent Optional parent widget.
     */
    explicit CircularBargraph(QWidget *parent = nullptr);
    ~CircularBargraph() override;

    /** \brief Returns the current indicated value. */
    [[nodiscard]] double value() const;
    /** \brief Returns the minimum scale value. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the maximum scale value. */
    [[nodiscard]] double maximum() const;
    /** \brief Returns the current peak hold value. */
    [[nodiscard]] double peakValue() const;
    /** \brief Returns the number of discrete curved LED segments. */
    [[nodiscard]] int segmentCount() const;
    /** \brief Returns the starting angle in degrees (0° = 12 o'clock, clockwise). */
    [[nodiscard]] double startAngle() const;
    /** \brief Returns the angular sweep span in degrees. */
    [[nodiscard]] double spanAngle() const;
    /** \brief Returns the display mode (Bar or Dot). */
    [[nodiscard]] DisplayMode displayMode() const;
    /** \brief Returns the warning threshold value. */
    [[nodiscard]] double warningThreshold() const;
    /** \brief Returns the critical overload error threshold value. */
    [[nodiscard]] double errorThreshold() const;
    /** \brief Returns true if peak hold indicator is enabled. */
    [[nodiscard]] bool isPeakHoldEnabled() const;
    /** \brief Returns the peak hold dwell time in milliseconds before decay begins. */
    [[nodiscard]] int peakHoldTimeMs() const;
    /** \brief Returns the peak decay falloff rate in units per second. */
    [[nodiscard]] double peakDecayRate() const;
    /** \brief Returns true if the centered digital readout pod is visible. */
    [[nodiscard]] bool isDigitalDisplayVisible() const;
    /** \brief Returns the decimal precision for the digital readout. */
    [[nodiscard]] int precision() const;
    /** \brief Returns the unit string label. */
    [[nodiscard]] QString unit() const;

    /** \brief Returns the normal operating LED color. */
    [[nodiscard]] QColor normalColor() const;
    /** \brief Returns the warning threshold LED color. */
    [[nodiscard]] QColor warningColor() const;
    /** \brief Returns the critical error threshold LED color. */
    [[nodiscard]] QColor errorColor() const;
    /** \brief Returns the peak indicator LED color. */
    [[nodiscard]] QColor peakColor() const;
    /** \brief Returns the unlit ghost segment color. */
    [[nodiscard]] QColor ghostColor() const;
    /** \brief Returns the dial background color. */
    [[nodiscard]] QColor dialColor() const;
    /** \brief Returns the digital readout text color. */
    [[nodiscard]] QColor textColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the current indicated value. */
    void setValue(double val);
    /** \brief Sets the minimum scale value. */
    void setMinimum(double min);
    /** \brief Sets the maximum scale value. */
    void setMaximum(double max);
    /** \brief Sets both minimum and maximum scale bounds. */
    void setRange(double min, double max);
    /** \brief Sets the number of discrete curved LED segments. */
    void setSegmentCount(int count);
    /** \brief Sets the starting angle in degrees (0° = 12 o'clock, clockwise). */
    void setStartAngle(double angle);
    /** \brief Sets the total angular sweep span in degrees. */
    void setSpanAngle(double angle);
    /** \brief Sets the display presentation mode (Bar or Dot). */
    void setDisplayMode(DisplayMode mode);
    /**
     * \brief Sets the warning threshold value.
     *
     * The thresholds always satisfy warningThreshold() <= errorThreshold(): if \a threshold
     * is above the error threshold, the error threshold is raised to \a threshold too
     * (one thresholdChanged() carries both values). Setting both thresholds to a consistent
     * pair gives that pair whatever the call order.
     */
    void setWarningThreshold(double threshold);
    /**
     * \brief Sets the critical error threshold value.
     *
     * If \a threshold is below the warning threshold, the warning threshold is lowered to
     * \a threshold too, so that warningThreshold() <= errorThreshold() always holds.
     */
    void setErrorThreshold(double threshold);
    /** \brief Enables or disables peak hold indication. */
    void setPeakHoldEnabled(bool enable);
    /** \brief Sets the peak hold dwell time in milliseconds before decay begins. */
    void setPeakHoldTimeMs(int ms);
    /** \brief Sets the peak decay rate in scale units per second. */
    void setPeakDecayRate(double rate);
    /** \brief Enables or disables the centered digital readout pod. */
    void setDigitalDisplayVisible(bool visible);
    /** \brief Sets the decimal precision of the digital readout. */
    void setPrecision(int prec);
    /** \brief Sets the measurement unit label. */
    void setUnit(const QString &unit);
    /** \brief Resets the peak indicator to the current value. */
    void resetPeak();

    /** \brief Sets the normal operating LED color. */
    void setNormalColor(const QColor &color);
    /** \brief Sets the warning threshold LED color. */
    void setWarningColor(const QColor &color);
    /** \brief Sets the critical error threshold LED color. */
    void setErrorColor(const QColor &color);
    /** \brief Sets the peak indicator LED color. */
    void setPeakColor(const QColor &color);
    /** \brief Sets the unlit ghost segment color. */
    void setGhostColor(const QColor &color);
    /** \brief Sets the dial background color. */
    void setDialColor(const QColor &color);
    /** \brief Sets the digital readout text color. */
    void setTextColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when the indicated value changes. */
    void valueChanged(double newValue);
    /** \brief Emitted when the scale range changes. */
    void rangeChanged(double newMin, double newMax);
    /** \brief Emitted when the warning or error threshold changes. */
    void thresholdChanged(double warning, double error);
    /** \brief Emitted when any visual or styling property changes. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<CircularBargraphPrivate> d_ptr;
    Q_DECLARE_PRIVATE(CircularBargraph)
};

} // namespace QtIndustrialWidgets
