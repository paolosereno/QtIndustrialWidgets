/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "FontTestUtils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QPaintDevice>
#include <QtTest/QtTest>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <cmath>
#include <functional>

// #177: a visible widget whose window moves to a screen with another device pixel ratio or logical DPI, with one
// screen in the test. The widget reads both from metric(), so overriding it stands for the new screen
namespace ScreenChange {

template <class W>
class FakeScreen : public W
{
public:
    using W::W;
    double fakeDpr{1.0};
    int fakeDpi{96};

protected:
    int metric(QPaintDevice::PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case QPaintDevice::PdmDevicePixelRatio:
            return static_cast<int>(std::ceil(fakeDpr));
        case QPaintDevice::PdmDevicePixelRatioScaled:
            return static_cast<int>(fakeDpr * QPaintDevice::devicePixelRatioFScale());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        // From Qt 6.8 a DPR other than 1 and 2 is read from these two, not from the scaled one
        case QPaintDevice::PdmDevicePixelRatioF_EncodedA:
        case QPaintDevice::PdmDevicePixelRatioF_EncodedB:
            return QPaintDevice::encodeMetricF(metric, fakeDpr);
#endif
        case QPaintDevice::PdmDpiX:
        case QPaintDevice::PdmDpiY:
            return fakeDpi;
        default:
            return W::metric(metric);
        }
    }
};

struct LayoutRequests : QObject {
    int count{0};
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::LayoutRequest) {
            ++count;
        }
        return false;
    }
};

// Moves a shown widget, configured by \a configure, to a screen of \a dpr and \a dpi and repaints it once. Then its
// minimum, the minimum of its layout and its pixels must be those of a new widget on that screen, the layout must
// have been told if the minimum changed, and later repaints must not tell it again. Sets \a minimumChanged.
// Reports failures through QtTest; the caller checks QTest::currentTestFailed()
template <class W>
void checkScreenChange(const std::function<void(W &)> &configure, double dpr, int dpi, bool *minimumChanged)
{
    QWidget parent;
    auto *layout = new QVBoxLayout(&parent);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *widget = new FakeScreen<W>(&parent);
    widget->setFocusPolicy(Qt::NoFocus);
    configure(*widget);
    layout->addWidget(widget);
    parent.resize(240, 240);
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));

    const QSize before = widget->minimumSizeHint();
    QCOMPARE(layout->minimumSize(), before);
    widget->repaint();
    QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);

    LayoutRequests requests;
    parent.installEventFilter(&requests);
    widget->fakeDpr = dpr;
    widget->fakeDpi = dpi;
    QCOMPARE(widget->devicePixelRatioF(), dpr);
    QCOMPARE(widget->logicalDpiY(), dpi);
    widget->repaint();
    QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);
    const int requestsAfterMove = requests.count;

    // The same parent and size, on the new screen from the start
    QWidget freshParent;
    auto *freshLayout = new QVBoxLayout(&freshParent);
    freshLayout->setContentsMargins(0, 0, 0, 0);
    auto *fresh = new FakeScreen<W>(&freshParent);
    fresh->fakeDpr = dpr;
    fresh->fakeDpi = dpi;
    fresh->setFocusPolicy(Qt::NoFocus);
    configure(*fresh);
    freshLayout->addWidget(fresh);
    freshParent.resize(parent.size());
    freshParent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&freshParent));
    QCOMPARE(fresh->size(), widget->size());

    *minimumChanged = fresh->minimumSizeHint() != before;
    QCOMPARE(widget->minimumSizeHint(), fresh->minimumSizeHint());
    QCOMPARE(layout->minimumSize(), fresh->minimumSizeHint());
    if (*minimumChanged) {
        QVERIFY2(requestsAfterMove > 0, "the minimum changed and the layout got no request");
    }
    const QImage drawn = FontTest::render(*widget);
    const QImage expected = FontTest::render(*fresh);
    QVERIFY2(FontTest::differingPixelCount(drawn, expected) == 0,
             qPrintable(QStringLiteral("%1 pixels differ").arg(FontTest::differingPixelCount(drawn, expected))));

    // Once: the next repaints on this screen do not disturb the layout
    requests.count = 0;
    widget->repaint();
    widget->repaint();
    QCoreApplication::sendPostedEvents(&parent, QEvent::LayoutRequest);
    QCOMPARE(requests.count, 0);
    parent.removeEventFilter(&requests);
}

// The screens of the tests: DPR alone, logical DPI alone, both
inline void addScreens()
{
    QTest::addColumn<double>("dpr");
    QTest::addColumn<int>("dpi");
    QTest::addRow("DPR 1.25") << 1.25 << 96;
    QTest::addRow("DPR 1.5") << 1.5 << 96;
    QTest::addRow("DPR 2") << 2.0 << 96;
    QTest::addRow("DPI 120") << 1.0 << 120;
    QTest::addRow("DPI 144") << 1.0 << 144;
    QTest::addRow("DPR 1.5, DPI 144") << 1.5 << 144;
}

} // namespace ScreenChange
