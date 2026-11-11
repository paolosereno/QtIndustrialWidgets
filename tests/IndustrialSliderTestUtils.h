/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers shared by the IndustrialSlider tests (#180): test colours that no other part of the drawing uses, the
// slider prepared with them on an opaque black background, and the orientation rows.

#include <QtIndustrialWidgets/IndustrialSlider.h>

#include <QtCore/QSize>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPalette>
#include <QtGui/QRgb>
#include <QtTest/QtTest>

#include <algorithm>
#include <cstdlib>

namespace SliderTest {

using QtIndustrialWidgets::IndustrialSlider;

inline const QColor kHandle(0, 0, 255);
inline const QColor kFeedback(255, 0, 255);
inline const QColor kTrack(0, 64, 0);
inline const QColor kScale(0, 255, 0);
inline const QColor kText(255, 255, 0);
inline const QColor kReadout(255, 255, 255);
inline const QColor kNormal(0, 128, 128);
inline const QColor kWarning(255, 128, 0);
inline const QColor kError(128, 0, 255);
inline const QColor kBackground(0, 0, 0);

inline int channelDistance(QRgb a, QRgb b)
{
    return std::max({std::abs(qRed(a) - qRed(b)), std::abs(qGreen(a) - qGreen(b)), std::abs(qBlue(a) - qBlue(b))});
}

inline bool near(QRgb p, const QColor &c, int tolerance = 8)
{
    return channelDistance(p, c.rgb()) <= tolerance;
}

/// The slider with the test colours on an opaque black background, at \a size
inline void prepare(IndustrialSlider &s, QSize size = QSize(300, 90))
{
    s.setHandleColor(kHandle);
    s.setFeedbackColor(kFeedback);
    s.setTrackColor(kTrack);
    s.setScaleColor(kScale);
    s.setTextColor(kText);
    s.setReadoutColor(kReadout);
    s.setNormalColor(kNormal);
    s.setWarningColor(kWarning);
    s.setErrorColor(kError);
    QPalette pal = s.palette();
    pal.setColor(QPalette::Window, kBackground);
    s.setPalette(pal);
    s.setAutoFillBackground(true);
    s.resize(size);
}

inline void setVertical(IndustrialSlider &s, bool vertical)
{
    s.setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
}

/// The data rows "horizontal" and "vertical", column "vertical"
inline void addOrientations()
{
    QTest::addColumn<bool>("vertical");
    QTest::newRow("horizontal") << false;
    QTest::newRow("vertical") << true;
}

inline QImage render(IndustrialSlider &s)
{
    return s.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

} // namespace SliderTest
