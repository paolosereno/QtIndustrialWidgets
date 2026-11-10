// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#include "GalleryTheme.h"

#include <QtIndustrialWidgets/CircularBargraph.h>
#include <QtIndustrialWidgets/Compass.h>
#include <QtIndustrialWidgets/IndustrialKnob.h>
#include <QtIndustrialWidgets/IndustrialSlider.h>
#include <QtIndustrialWidgets/LinearGauge.h>
#include <QtIndustrialWidgets/RadialGauge.h>
#include <QtIndustrialWidgets/TankDisplay.h>
#include <QtIndustrialWidgets/ValveIndicator.h>

using namespace QtIndustrialWidgets;

namespace GalleryShowcase {

namespace {
bool isDark(Theme theme) { return theme == Theme::Dark; }
} // namespace

QColor backgroundColor(Theme theme)
{
    return isDark(theme) ? QColor(0x18, 0x1d, 0x27) : QColor(0xff, 0xff, 0xff);
}

void applyGalleryTheme(RadialGauge &gauge, Theme theme)
{
    if (isDark(theme)) {
        gauge.setDialColor(QColor(20, 24, 32));
        gauge.setTextColor(QColor(245, 246, 250));
        gauge.setScaleColor(QColor(210, 218, 226)); // the widget default
        gauge.setBezelColor(QColor(48, 56, 70));
    } else {
        gauge.setDialColor(QColor(242, 244, 248));
        gauge.setTextColor(QColor(30, 39, 46));
        gauge.setScaleColor(QColor(80, 90, 105));
        gauge.setBezelColor(QColor(200, 208, 218));
    }
}

void applyGalleryTheme(LinearGauge &gauge, Theme theme)
{
    if (isDark(theme)) {
        gauge.setBezelColor(QColor(36, 43, 56));
        gauge.setTroughColor(QColor(20, 24, 32));
        gauge.setTextColor(QColor(245, 246, 250));
        gauge.setScaleColor(QColor(200, 208, 218)); // the widget default
    } else {
        gauge.setBezelColor(QColor(215, 222, 230));
        gauge.setTroughColor(QColor(235, 240, 245));
        gauge.setTextColor(QColor(30, 39, 46));
        gauge.setScaleColor(QColor(80, 90, 105));
    }
}

void applyGalleryTheme(IndustrialKnob &knob, Theme theme)
{
    if (isDark(theme)) {
        knob.setKnobColor(QColor(42, 48, 60));
        knob.setScaleColor(QColor(190, 200, 215));
        knob.setTextColor(QColor(240, 244, 250));
    } else {
        knob.setKnobColor(QColor(220, 226, 235));
        knob.setScaleColor(QColor(70, 80, 95));
        knob.setTextColor(QColor(30, 39, 46));
    }
}

void applyGalleryTheme(Compass &compass, Theme theme)
{
    if (isDark(theme)) {
        compass.setDialColor(QColor(20, 24, 32));
        compass.setBezelColor(QColor(48, 56, 70));
        compass.setTextColor(QColor(225, 231, 236));
    } else {
        compass.setDialColor(QColor(242, 244, 248));
        compass.setBezelColor(QColor(200, 208, 218));
        compass.setTextColor(QColor(30, 39, 46));
    }
}

void applyGalleryTheme(CircularBargraph &bargraph, Theme theme)
{
    // Dark: the default dial colour (24, 28, 36) is almost the dark panel colour, so black keeps the disc visible
    // (it is decorative, the segments carry the value). Light: the default
    bargraph.setDialColor(isDark(theme) ? QColor(0, 0, 0) : QColor(24, 28, 36));
}

// TankDisplay (#152) draws on the panel: its texts follow the theme; the readout panel is dark in both
void applyGalleryTheme(TankDisplay &tank, Theme theme)
{
    tank.setTextColor(isDark(theme) ? QColor(240, 244, 248) : QColor(30, 39, 46));
    tank.setScaleColor(isDark(theme) ? QColor(200, 208, 218) : QColor(80, 90, 105));
    tank.setVesselColor(isDark(theme) ? QColor(120, 134, 150) : QColor(110, 122, 138));
}

// ValveIndicator (#153) as well: tags and marks on the panel; the readout panel is dark in both. On white the default
// open, transit and unknown colours are below 3:1: darker, with the same hues (tst_ReadoutContrast)
void applyGalleryTheme(ValveIndicator &valve, Theme theme)
{
    valve.setTextColor(isDark(theme) ? QColor(240, 244, 248) : QColor(30, 39, 46));
    valve.setOutlineColor(isDark(theme) ? QColor(200, 208, 218) : QColor(80, 90, 105));
    valve.setOpenColor(isDark(theme) ? QColor(46, 204, 113) : QColor(36, 160, 89));
    valve.setTransitColor(isDark(theme) ? QColor(241, 196, 15) : QColor(170, 138, 10));
    valve.setUnknownColor(isDark(theme) ? QColor(149, 165, 166) : QColor(125, 145, 146));
}

// IndustrialSlider (#180): the scale, the handle, the bar and the zone band are drawn on the panel; the track and the
// readout panels are dark in both themes. On white the default handle, feedback and zone colours are below 3:1:
// darker, with the same hues (tst_ReadoutContrast)
void applyGalleryTheme(IndustrialSlider &slider, Theme theme)
{
    slider.setTextColor(isDark(theme) ? QColor(240, 244, 248) : QColor(30, 39, 46));
    slider.setScaleColor(isDark(theme) ? QColor(200, 208, 218) : QColor(80, 90, 105));
    slider.setHandleColor(isDark(theme) ? QColor(0, 188, 212) : QColor(0, 131, 148));
    slider.setFeedbackColor(isDark(theme) ? QColor(255, 159, 67) : QColor(196, 98, 0));
    slider.setNormalColor(isDark(theme) ? QColor(46, 204, 113) : QColor(36, 160, 89));
    slider.setWarningColor(isDark(theme) ? QColor(241, 196, 15) : QColor(170, 138, 10));
    slider.setErrorColor(QColor(231, 76, 60));
}

} // namespace GalleryShowcase
