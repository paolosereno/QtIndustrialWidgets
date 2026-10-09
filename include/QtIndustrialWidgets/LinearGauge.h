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
 * \class LinearGauge
 * \brief High-performance linear bar and thermometer instrument for industrial SCADA and telemetry dashboards.
 *
 * LinearGauge supports both vertical and horizontal orientations, bulb thermometer or flat panel bar designs,
 * major and minor graduation scales, dynamic threshold liquid color shifts (Normal -> Warning -> Error),
 * optional gradient liquid fills, and an integrated digital readout pod.
 *
 * Scale geometry, graduation ticks, and recessed trough background are cached in a Hi-DPI QPixmap for high-frequency updates.
 *
 * \code
 * auto *gauge = new LinearGauge(parent);
 * gauge->setOrientation(Qt::Vertical);
 * gauge->setThermometerMode(true);
 * gauge->setRange(-20.0, 100.0);
 * gauge->setValue(36.6);
 * gauge->setUnit("°C");
 * \endcode
 */
namespace QtIndustrialWidgets {

class LinearGaugePrivate;

class QTINDUSTRIALWIDGETS_EXPORT LinearGauge : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(bool thermometerMode READ isThermometerMode WRITE setThermometerMode NOTIFY appearanceChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY rangeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY rangeChanged)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY appearanceChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY appearanceChanged)
    Q_PROPERTY(int majorTicks READ majorTicks WRITE setMajorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(int minorTicks READ minorTicks WRITE setMinorTicks NOTIFY appearanceChanged)
    Q_PROPERTY(double warningThreshold READ warningThreshold WRITE setWarningThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(double errorThreshold READ errorThreshold WRITE setErrorThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(bool dynamicLiquidColor READ isDynamicLiquidColor WRITE setDynamicLiquidColor NOTIFY appearanceChanged)
    Q_PROPERTY(bool gradientLiquid READ isGradientLiquid WRITE setGradientLiquid NOTIFY appearanceChanged)
    Q_PROPERTY(bool digitalDisplayVisible READ digitalDisplayVisible WRITE setDigitalDisplayVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool scaleVisible READ scaleVisible WRITE setScaleVisible NOTIFY appearanceChanged)
    Q_PROPERTY(QColor liquidColor READ liquidColor WRITE setLiquidColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor troughColor READ troughColor WRITE setTroughColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor bezelColor READ bezelColor WRITE setBezelColor NOTIFY appearanceChanged)

public:
    /**
     * \brief Constructs a LinearGauge widget with default vertical thermometer styling.
     * \param parent Optional parent widget.
     */
    explicit LinearGauge(QWidget *parent = nullptr);
    ~LinearGauge() override;

    /** \brief Returns the orientation (Qt::Vertical or Qt::Horizontal). */
    [[nodiscard]] Qt::Orientation orientation() const;
    /** \brief Returns true if operating in bulb thermometer mode, false if flat panel bar. */
    [[nodiscard]] bool isThermometerMode() const;
    /** \brief Returns the minimum scale value. */
    [[nodiscard]] double minimum() const;
    /** \brief Returns the maximum scale value. */
    [[nodiscard]] double maximum() const;
    /** \brief Returns the current indicated value. */
    [[nodiscard]] double value() const;
    /** \brief Returns the decimal precision for the digital readout. */
    [[nodiscard]] int precision() const;
    /** \brief Returns the measurement unit label (e.g. "°C", "mm"). */
    [[nodiscard]] QString unit() const;
    /** \brief Returns the number of major graduation intervals. */
    [[nodiscard]] int majorTicks() const;
    /** \brief Returns the number of minor subdivisions between major intervals. */
    [[nodiscard]] int minorTicks() const;
    /** \brief Returns the warning threshold value. */
    [[nodiscard]] double warningThreshold() const;
    /** \brief Returns the critical error threshold value. */
    [[nodiscard]] double errorThreshold() const;
    /** \brief Returns true if liquid fill shifts color based on thresholds. */
    [[nodiscard]] bool isDynamicLiquidColor() const;
    /** \brief Returns true if liquid fill uses a multi-color gradient. */
    [[nodiscard]] bool isGradientLiquid() const;
    /** \brief Returns true if the digital LCD readout pod is displayed. */
    [[nodiscard]] bool digitalDisplayVisible() const;
    /** \brief Returns true if graduation scale marks and numeric labels are visible. */
    [[nodiscard]] bool scaleVisible() const;

    /** \brief Returns the liquid column fill color. */
    [[nodiscard]] QColor liquidColor() const;
    /** \brief Returns the normal operating zone color. */
    [[nodiscard]] QColor normalColor() const;
    /** \brief Returns the warning zone color. */
    [[nodiscard]] QColor warningColor() const;
    /** \brief Returns the critical error zone color. */
    [[nodiscard]] QColor errorColor() const;
    /** \brief Returns the recessed glass trough background color. */
    [[nodiscard]] QColor troughColor() const;
    /** \brief Returns the graduation tick marks color. */
    [[nodiscard]] QColor scaleColor() const;
    /** \brief Returns the numeric label font color. */
    [[nodiscard]] QColor textColor() const;
    /** \brief Returns the outer rim bezel frame color. */
    [[nodiscard]] QColor bezelColor() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** \brief Sets the orientation (Qt::Vertical or Qt::Horizontal). */
    void setOrientation(Qt::Orientation orientation);
    /** \brief Toggles bulb thermometer mode vs flat panel bar mode. */
    void setThermometerMode(bool thermometer);
    /** \brief Sets the indicated value, automatically clamped within [minimum, maximum]. */
    void setValue(double value);
    /** \brief Sets the minimum scale value. */
    void setMinimum(double min);
    /** \brief Sets the maximum scale value. */
    void setMaximum(double max);
    /** \brief Sets both the minimum and maximum range. */
    void setRange(double min, double max);
    /** \brief Sets the decimal precision for digital display readout. */
    void setPrecision(int precision);
    /** \brief Sets the measurement unit label string. */
    void setUnit(const QString &unit);
    /** \brief Sets the number of major graduation intervals. */
    void setMajorTicks(int count);
    /** \brief Sets the number of minor subdivisions between major intervals. */
    void setMinorTicks(int count);
    /** \brief Sets the warning threshold value. */
    void setWarningThreshold(double threshold);
    /** \brief Sets the critical error threshold value. */
    void setErrorThreshold(double threshold);
    /** \brief Toggles dynamic liquid color shifting based on thresholds. */
    void setDynamicLiquidColor(bool dynamic);
    /** \brief Toggles continuous multi-color gradient fill. */
    void setGradientLiquid(bool gradient);
    /** \brief Toggles visibility of the digital readout box. */
    void setDigitalDisplayVisible(bool visible);
    /** \brief Toggles scale ticks and numbers visibility. */
    void setScaleVisible(bool visible);

    /** \brief Sets the liquid column fill color. */
    void setLiquidColor(const QColor &color);
    /** \brief Sets the normal operating zone color. */
    void setNormalColor(const QColor &color);
    /** \brief Sets the warning zone color. */
    void setWarningColor(const QColor &color);
    /** \brief Sets the critical error zone color. */
    void setErrorColor(const QColor &color);
    /** \brief Sets the trough background color. */
    void setTroughColor(const QColor &color);
    /** \brief Sets the graduation scale marks color. */
    void setScaleColor(const QColor &color);
    /** \brief Sets the numeric text labels color. */
    void setTextColor(const QColor &color);
    /** \brief Sets the outer rim bezel frame color. */
    void setBezelColor(const QColor &color);

Q_SIGNALS:
    /** \brief Emitted when the indicated value changes. */
    void valueChanged(double value);
    /** \brief Emitted when the scale range changes. */
    void rangeChanged(double min, double max);
    /** \brief Emitted when warning or error thresholds change. */
    void thresholdChanged(double warning, double error);
    /** \brief Emitted when visual appearance properties change. */
    void appearanceChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void invalidateCache();
    void renderStaticScale(const QSize &size);
    [[nodiscard]] QColor determineActiveLiquidColor() const;

    std::unique_ptr<LinearGaugePrivate> d_ptr;
    Q_DECLARE_PRIVATE(LinearGauge)
};

} // namespace QtIndustrialWidgets
