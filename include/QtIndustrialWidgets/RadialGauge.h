/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/qtindustrialwidgets_global.h>
#include <QtWidgets/QWidget>
#include <memory>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

/**
 * \class RadialGauge
 * \brief High-performance circular dial instrument for industrial SCADA and telemetry displays.
 *
 * RadialGauge renders a high-precision circular dial gauge featuring an anti-aliased needle pointer,
 * customizable angular spans (e.g. 270° industrial sweep, 180° semi-circular, etc.), major and minor graduation ticks,
 * color-coded warning/error threshold arc bands, and an integrated recessed digital readout pod.
 *
 * The dial scale, graduation ticks, numbers, and threshold bands are rendered into a high-DPI cached QPixmap
 * that updates only when geometry or visual properties change, allowing 60+ FPS needle animation with minimal CPU usage.
 *
 * \code
 * auto *gauge = new RadialGauge(parent);
 * gauge->setRange(0.0, 100.0);
 * gauge->setValue(42.5);
 * gauge->setUnit("bar");
 * gauge->setWarningThreshold(70.0);
 * gauge->setErrorThreshold(85.0);
 * \endcode
 */
namespace QtIndustrialWidgets {

class RadialGaugePrivate;

class QTINDUSTRIALWIDGETS_EXPORT RadialGauge : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(double startAngle READ startAngle WRITE setStartAngle NOTIFY appearanceChanged)
    Q_PROPERTY(double spanAngle READ spanAngle WRITE setSpanAngle NOTIFY appearanceChanged)
    Q_PROPERTY(int majorTicks READ majorTicks WRITE setMajorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(int minorTicks READ minorTicks WRITE setMinorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(bool thresholdBandsVisible READ thresholdBandsVisible WRITE setThresholdBandsVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool digitalDisplayVisible READ digitalDisplayVisible WRITE setDigitalDisplayVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor needleColor READ needleColor WRITE setNeedleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor dialColor READ dialColor WRITE setDialColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)

public:
    /**
     * \brief Constructs a RadialGauge widget with default industrial styling.
     * \param parent Optional parent widget.
     */
    explicit RadialGauge(QWidget *parent = nullptr);
    ~RadialGauge() override;

    /** \brief Returns the minimum scale value. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the maximum scale value. */
    [[nodiscard]] double maximum() const;
    /** \brief Returns the current indicated value. */
    [[nodiscard]] double value() const;
    /** \brief Returns the decimal precision for the digital readout display. */
    [[nodiscard]] int precision() const;
    /** \brief Returns the measurement unit label (e.g. "bar", "PSI", "RPM"). */
    [[nodiscard]] QString unit() const;
    /** \brief Returns the starting angle in degrees (default: -135°). */
    [[nodiscard]] double startAngle() const;
    /** \brief Returns the total angular sweep span in degrees (default: 270°). */
    [[nodiscard]] double spanAngle() const;
    /** \brief Returns the number of major graduation intervals. */
    [[nodiscard]] int majorTicks() const;
    /** \brief Returns the number of minor subdivisions per major tick interval. */
    [[nodiscard]] int minorTicks() const;
    /** \brief Returns the warning threshold value. */
    [[nodiscard]] double warningThreshold() const;
    /** \brief Returns the error threshold value. */
    [[nodiscard]] double errorThreshold() const;
    /** \brief Returns true if colored arc threshold bands are displayed. */
    [[nodiscard]] bool thresholdBandsVisible() const;
    /** \brief Returns true if the digital LCD readout pod is displayed. */
    [[nodiscard]] bool digitalDisplayVisible() const;

    /** \brief Returns the needle pointer color. */
    [[nodiscard]] QColor needleColor() const;
    /** \brief Returns the normal operating arc band color. */
    [[nodiscard]] QColor normalColor() const;
    /** \brief Returns the warning arc band color. */
    [[nodiscard]] QColor warningColor() const;
    /** \brief Returns the critical error arc band color. */
    [[nodiscard]] QColor errorColor() const;
    /** \brief Returns the dial face background color. */
    [[nodiscard]] QColor dialColor() const;
    /** \brief Returns the graduation tick marks color. */
    [[nodiscard]] QColor scaleColor() const;
    /** \brief Returns the numeric label font color. */
    [[nodiscard]] QColor textColor() const;
    /** \brief Returns the outer rim bezel color. */
    [[nodiscard]] QColor bezelColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the indicated value, automatically clamped within [minimum, maximum]. */
    void setValue(double value);
    /** \brief Sets the minimum scale value. */
    void setMinimum(double min);
    /** \brief Sets the maximum scale value. */
    void setMaximum(double max);
    /** \brief Sets both the minimum and maximum scale range. */
    void setRange(double min, double max);
    /** \brief Sets the decimal precision for digital display readout. */
    void setPrecision(int precision);
    /** \brief Sets the measurement unit label string. */
    void setUnit(const QString &unit);
    /** \brief Sets the starting angle in degrees. */
    void setStartAngle(double angle);
    /** \brief Sets the total sweep span in degrees. */
    void setSpanAngle(double span);
    /** \brief Sets the number of major graduation intervals. */
    void setMajorTicks(int count);
    /** \brief Sets the number of minor subdivisions between major intervals. */
    void setMinorTicks(int count);
    /** \brief Sets the warning threshold value. */
    void setWarningThreshold(double threshold);
    /** \brief Sets the critical error threshold value. */
    void setErrorThreshold(double threshold);
    /** \brief Toggles visibility of the colored threshold arc bands. */
    void setThresholdBandsVisible(bool visible);
    /** \brief Toggles visibility of the digital readout box. */
    void setDigitalDisplayVisible(bool visible);

    /** \brief Sets the needle pointer color. */
    void setNeedleColor(const QColor &color);
    /** \brief Sets the normal operating zone color. */
    void setNormalColor(const QColor &color);
    /** \brief Sets the warning zone color. */
    void setWarningColor(const QColor &color);
    /** \brief Sets the critical error zone color. */
    void setErrorColor(const QColor &color);
    /** \brief Sets the dial face background color. */
    void setDialColor(const QColor &color);
    /** \brief Sets the graduation scale marks color. */
    void setScaleColor(const QColor &color);
    /** \brief Sets the numeric text labels color. */
    void setTextColor(const QColor &color);
    /** \brief Sets the outer rim bezel color. */
    void setBezelColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when the indicated value changes. */
    void valueChanged(double value);
    /** \brief Emitted when the scale range changes. */
    void rangeChanged(double min, double max);
    /** \brief Emitted when warning or error thresholds change. */
    void thresholdChanged(double warning, double error);
    /** \brief Emitted when the indicated value enters or leaves the warning zone. */
    void warningExceeded(bool exceeded);
    /** \brief Emitted when the indicated value enters or leaves the critical error zone. */
    void errorExceeded(bool exceeded);
    /** \brief Emitted when visual appearance properties change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void invalidateCache();
    void renderStaticScale(const QSize &size);
    double valueToAngle(double val) const;

    std::unique_ptr<RadialGaugePrivate> d_ptr;
    Q_DECLARE_PRIVATE(RadialGauge)
};

} // namespace QtIndustrialWidgets
