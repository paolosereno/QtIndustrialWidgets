// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

// Consumer of an installed QtIndustrialWidgets package (#66). It renders a LedIndicator offscreen, lit and unlit, and
// checks the pixel at the centre of the LED: lit, it has the hue of the LED colour and is brighter than unlit.
// Exit code 0 on success, 1 with a message otherwise.

#include <QtIndustrialWidgets/LedIndicator.h>

#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtWidgets/QApplication>

#include <cstdio>
#include <cstdlib>

namespace {

QColor centrePixel(bool on, const QColor &colour)
{
    QtIndustrialWidgets::LedIndicator led(colour);
    led.setOn(on);
    led.resize(64, 64);
    QImage image(led.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    led.render(&image);
    return image.pixelColor(image.width() / 2, image.height() / 2);
}

int hueDistance(int a, int b)
{
    const int d = std::abs(a - b) % 360;
    return d > 180 ? 360 - d : d;
}

} // namespace

int main(int argc, char **argv)
{
    // The check runs headless: without a display, use the offscreen platform
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    const QColor colour(46, 204, 113); // green, hue 145
    const QColor lit = centrePixel(true, colour);
    const QColor unlit = centrePixel(false, colour);
    std::printf("LED centre: lit rgb(%d, %d, %d) hue %d, unlit rgb(%d, %d, %d); LED colour hue %d\n", lit.red(), lit.green(),
                lit.blue(), lit.hsvHue(), unlit.red(), unlit.green(), unlit.blue(), colour.hsvHue());

    if (lit.hsvSaturation() < 64 || hueDistance(lit.hsvHue(), QColor(235, 59, 90).hsvHue()) > 20) { // SABOTAGE (#66): wrong colour
        std::printf("FAIL: the lit LED's centre does not have the hue of the LED colour\n");
        return 1;
    }
    if (lit.value() <= unlit.value()) {
        std::printf("FAIL: the lit LED's centre is not brighter than the unlit one\n");
        return 1;
    }
    std::printf("PASS: QtIndustrialWidgets found, linked and drawing\n");
    return 0;
}
