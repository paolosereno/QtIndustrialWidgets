// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

// #180: the geometry of IndustrialSlider, without rendering. The parts across the scale never overlap, the scale runs
// the right way, and minimumSliderSize() is the smallest size that keeps every part at its minimum.

#include <QtTest/QtTest>

#include "internal/IndustrialSliderLayout.h"

#include <cmath>
#include <limits>

using namespace QtIndustrialWidgets::internal;

class tst_IndustrialSliderLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void partsAcrossTheScaleInOrder_data();
    void partsAcrossTheScaleInOrder();
    void scaleDirection();
    void minimumSizeKeepsEveryPart_data();
    void minimumSizeKeepsEveryPart();
    void minimumSizeIsTheSmallest_data();
    void minimumSizeIsTheSmallest();
    void podsFollowTheConfiguration();
    void handleAndBar();
    void nonFiniteAndTinyInputs();
};

namespace {

SliderTextSizes typicalText()
{
    SliderTextSizes t;
    t.podHeight = 21.0;
    t.podMinWidth = 58.0;
    t.labelHeight = 14.0;
    t.labelWidth = 24.0;
    t.firstLabelWidth = 8.0;
    t.lastLabelWidth = 24.0;
    t.labelMinGap = 7.0;
    return t;
}

// Wide labels: the scale needs more than kSliderMinimumScale for its end labels
SliderTextSizes wideText()
{
    SliderTextSizes t = typicalText();
    t.labelWidth = 52.0;
    t.firstLabelWidth = 30.0;
    t.lastLabelWidth = 52.0;
    return t;
}

SliderLayoutInput input(bool vertical, const SliderTextSizes &text, double w = 0.0, double h = 0.0)
{
    SliderLayoutInput in;
    in.vertical = vertical;
    in.text = text;
    in.width = w;
    in.height = h;
    return in;
}

double labelExtentAcross(const SliderLayoutInput &in)
{
    return in.vertical ? in.text.labelWidth : in.text.labelHeight;
}

double acrossSize(const SliderLayoutInput &in)
{
    return in.vertical ? in.width : in.height;
}

void addOrientations()
{
    QTest::addColumn<bool>("vertical");
    QTest::newRow("horizontal") << false;
    QTest::newRow("vertical") << true;
}

// Empty when the layout of \a in keeps every part inside the widget and in its order across the scale
QString orderError(const SliderLayoutInput &in, const SliderLayout &l)
{
    const double eps = 1e-9;
    const QRectF widget(0.0, 0.0, in.width, in.height);
    for (const QRectF &pod : {l.setpointPod, l.feedbackPod}) {
        if (!pod.isEmpty() && !widget.adjusted(kSliderMargin, kSliderMargin, -kSliderMargin, -kSliderMargin)
                                  .adjusted(-eps, -eps, eps, eps).contains(pod)) {
            return QStringLiteral("a panel outside the margins");
        }
    }
    if (!l.setpointPod.isEmpty() && !l.feedbackPod.isEmpty() && l.setpointPod.intersects(l.feedbackPod)) {
        return QStringLiteral("the panels overlap");
    }
    // Everything of the scale group comes after the panels
    const double podsEnd = std::max(l.setpointPod.bottom(), l.feedbackPod.bottom());
    if (in.readoutVisible && in.vertical && l.scaleMax - kSliderHandleWidth * 0.5 < podsEnd + kSliderPodGap - eps) {
        return QStringLiteral("the top of a vertical scale reaches the panels");
    }
    if (in.readoutVisible && !in.vertical && l.handleBase < podsEnd + kSliderPodGap - eps) {
        return QStringLiteral("the handle reaches the panels");
    }
    if (!(l.handleBase + kSliderHandleDepth <= l.trackNear() + eps)) {
        return QStringLiteral("the handle passes the track");
    }
    if (!(l.trackFar() + kSliderBarOverhang <= l.zoneNear + eps)) {
        return QStringLiteral("the feedback bar reaches the zone band");
    }
    if (!(l.zoneNear + kSliderZone < l.tickNear + eps)) {
        return QStringLiteral("the zone band reaches the ticks");
    }
    if (!(l.tickNear + kSliderMajorTick < l.labelNear + eps)) {
        return QStringLiteral("the ticks reach the labels");
    }
    if (!(l.handleBase >= kSliderMargin - eps)) {
        return QStringLiteral("the handle leaves the margin");
    }
    if (!(l.labelNear + labelExtentAcross(in) <= acrossSize(in) - kSliderMargin + eps)) {
        return QStringLiteral("the labels leave the margin: %1 > %2")
            .arg(l.labelNear + labelExtentAcross(in)).arg(acrossSize(in) - kSliderMargin);
    }
    return {};
}

} // namespace

void tst_IndustrialSliderLayout::partsAcrossTheScaleInOrder_data()
{
    addOrientations();
}

// From the minimum size up, the handle, the track, the bar overhang, the zone band, the ticks and the labels follow
// each other across the scale without overlapping, inside the margins, after the readout panels
void tst_IndustrialSliderLayout::partsAcrossTheScaleInOrder()
{
    QFETCH(bool, vertical);
    for (const SliderTextSizes &text : {typicalText(), wideText()}) {
        for (bool readouts : {true, false}) {
            for (bool feedbackPlace : {true, false}) {
                SliderLayoutInput in = input(vertical, text);
                in.readoutVisible = readouts;
                in.feedbackReadoutVisible = feedbackPlace;
                const QSizeF min = minimumSliderSize(in);
                for (double extra : {0.0, 0.5, 7.0, 120.0}) {
                    in.width = min.width() + extra;
                    in.height = min.height() + extra;
                    const SliderLayout l = layoutSlider(in);
                    const QString error = orderError(in, l);
                    QVERIFY2(error.isEmpty(), qPrintable(QStringLiteral("%1x%2, readouts %3, feedback place %4: %5")
                                                             .arg(in.width).arg(in.height).arg(readouts)
                                                             .arg(feedbackPlace).arg(error)));
                }
            }
        }
    }
}

// The minimum on the left (horizontal) or at the bottom (vertical); the track spans the scale
void tst_IndustrialSliderLayout::scaleDirection()
{
    const SliderLayout h = layoutSlider(input(false, typicalText(), 300.0, 100.0));
    QVERIFY(h.scaleMin < h.scaleMax);
    QCOMPARE(h.along(0.0), h.scaleMin);
    QCOMPARE(h.along(1.0), h.scaleMax);
    QCOMPARE(h.along(0.25), h.scaleMin + 0.25 * (h.scaleMax - h.scaleMin));
    QCOMPARE(h.track.left(), h.scaleMin);
    QCOMPARE(h.track.right(), h.scaleMax);
    QCOMPARE(h.scaleLength(), h.scaleMax - h.scaleMin);

    const SliderLayout v = layoutSlider(input(true, typicalText(), 100.0, 300.0));
    QVERIFY(v.scaleMin > v.scaleMax); // y grows downwards: the minimum is lower
    QCOMPARE(v.along(0.0), v.scaleMin);
    QCOMPARE(v.along(1.0), v.scaleMax);
    QCOMPARE(v.track.bottom(), v.scaleMin);
    QCOMPARE(v.track.top(), v.scaleMax);
    QCOMPARE(v.scaleLength(), v.scaleMin - v.scaleMax);
    QVERIFY(v.track.width() == kSliderTrack && h.track.height() == kSliderTrack);
}

void tst_IndustrialSliderLayout::minimumSizeKeepsEveryPart_data()
{
    addOrientations();
}

// At the minimum size: the panels are at least podMinWidth, the scale at least kSliderMinimumScale and long enough for
// the two end labels with the minimum gap between them, and the scale ends keep room for half the handle
void tst_IndustrialSliderLayout::minimumSizeKeepsEveryPart()
{
    QFETCH(bool, vertical);
    for (const SliderTextSizes &text : {typicalText(), wideText()}) {
        SliderLayoutInput in = input(vertical, text);
        const QSizeF min = minimumSliderSize(in);
        QCOMPARE(min.width(), std::ceil(min.width()));
        QCOMPARE(min.height(), std::ceil(min.height()));
        in.width = min.width();
        in.height = min.height();
        const SliderLayout l = layoutSlider(in);
        QVERIFY(l.setpointPod.width() >= text.podMinWidth);
        QVERIFY(l.feedbackPod.width() >= text.podMinWidth);
        QVERIFY(l.scaleLength() >= kSliderMinimumScale);
        const double labels = vertical ? text.labelHeight + text.labelMinGap
                                       : text.firstLabelWidth + text.lastLabelWidth + text.labelMinGap;
        QVERIFY2(l.scaleLength() >= labels, qPrintable(QStringLiteral("scale %1 < %2").arg(l.scaleLength()).arg(labels)));
        const double low = vertical ? std::min(l.scaleMin, l.scaleMax) : l.scaleMin;
        const double high = vertical ? std::max(l.scaleMin, l.scaleMax) : l.scaleMax;
        const double end = vertical ? in.height : in.width;
        QVERIFY(low - kSliderHandleWidth * 0.5 >= (vertical ? l.feedbackPod.bottom() + kSliderPodGap : kSliderMargin));
        QVERIFY(high + kSliderHandleWidth * 0.5 <= end - kSliderMargin);
        if (vertical) {
            // The end labels are centred on the end ticks: half a line past each end, inside the margins
            QVERIFY(high + text.labelHeight * 0.5 <= end - kSliderMargin);
        }
    }
}

void tst_IndustrialSliderLayout::minimumSizeIsTheSmallest_data()
{
    addOrientations();
}

// One pixel less in either direction and some part loses its minimum: the minimum is not padded
void tst_IndustrialSliderLayout::minimumSizeIsTheSmallest()
{
    QFETCH(bool, vertical);
    for (const SliderTextSizes &text : {typicalText(), wideText()}) {
        SliderLayoutInput in = input(vertical, text);
        const QSizeF min = minimumSliderSize(in);
        const auto keeps = [&](double w, double h) {
            in.width = w;
            in.height = h;
            const SliderLayout l = layoutSlider(in);
            const double labels = vertical ? text.labelHeight + text.labelMinGap
                                           : text.firstLabelWidth + text.lastLabelWidth + text.labelMinGap;
            return l.setpointPod.width() >= text.podMinWidth && l.feedbackPod.width() >= text.podMinWidth
                   && l.scaleLength() >= std::max(kSliderMinimumScale, labels) && orderError(in, l).isEmpty();
        };
        QVERIFY(keeps(min.width(), min.height()));
        QVERIFY2(!keeps(min.width() - 1.0, min.height()), "the minimum width is padded");
        QVERIFY2(!keeps(min.width(), min.height() - 1.0), "the minimum height is padded");
    }
}

// The setpoint panel takes the whole width without the place of the feedback; the place is kept with it, so the
// layout does not depend on whether a feedback is shown; no panels without readouts
void tst_IndustrialSliderLayout::podsFollowTheConfiguration()
{
    SliderLayoutInput in = input(false, typicalText(), 300.0, 100.0);
    SliderLayout l = layoutSlider(in);
    QCOMPARE(l.setpointPod.left(), kSliderMargin);
    QCOMPARE(l.feedbackPod.right(), 300.0 - kSliderMargin);
    QCOMPARE(l.setpointPod.width(), l.feedbackPod.width());
    QCOMPARE(l.feedbackPod.left() - l.setpointPod.right(), kSliderPodGap);
    QCOMPARE(l.setpointPod.top(), l.feedbackPod.top());
    in.feedbackReadoutVisible = false;
    l = layoutSlider(in);
    QVERIFY(l.feedbackPod.isEmpty());
    QCOMPARE(l.setpointPod.width(), 300.0 - 2.0 * kSliderMargin);
    in.readoutVisible = false;
    l = layoutSlider(in);
    QVERIFY(l.setpointPod.isEmpty() && l.feedbackPod.isEmpty());
    QVERIFY(minimumSliderSize(in).height() < minimumSliderSize(input(false, typicalText())).height());

    SliderLayoutInput v = input(true, typicalText(), 120.0, 300.0);
    l = layoutSlider(v);
    QCOMPARE(l.setpointPod.width(), 120.0 - 2.0 * kSliderMargin);
    QCOMPARE(l.feedbackPod.top() - l.setpointPod.bottom(), kSliderPodGap);
    QCOMPARE(l.feedbackPod.left(), l.setpointPod.left());
}

// The tip of the handle touches the track; the feedback bar crosses the track and passes it by the overhang on each
// side, centred on its value
void tst_IndustrialSliderLayout::handleAndBar()
{
    for (bool vertical : {false, true}) {
        const SliderLayout l = layoutSlider(input(vertical, typicalText(), vertical ? 120.0 : 300.0, vertical ? 300.0 : 120.0));
        const double at = l.along(0.4);
        const QPolygonF handle = sliderHandlePolygon(l, at);
        QCOMPARE(handle.size(), 5);
        const QRectF hb = handle.boundingRect();
        if (vertical) {
            QCOMPARE(hb.left(), l.handleBase);
            QCOMPARE(hb.right(), l.trackNear());
            QCOMPARE(hb.center().y(), at);
            QCOMPARE(hb.height(), kSliderHandleWidth);
        } else {
            QCOMPARE(hb.top(), l.handleBase);
            QCOMPARE(hb.bottom(), l.trackNear());
            QCOMPARE(hb.center().x(), at);
            QCOMPARE(hb.width(), kSliderHandleWidth);
        }
        // The tip is the one point at the track
        int tips = 0;
        for (const QPointF &p : handle) {
            tips += (vertical ? p.x() : p.y()) == l.trackNear() ? 1 : 0;
        }
        QCOMPARE(tips, 1);

        const QRectF bar = sliderFeedbackBar(l, at);
        if (vertical) {
            QCOMPARE(bar.center().y(), at);
            QCOMPARE(bar.left(), l.trackNear() - kSliderBarOverhang);
            QCOMPARE(bar.right(), l.trackFar() + kSliderBarOverhang);
            QCOMPARE(bar.height(), kSliderBarWidth);
        } else {
            QCOMPARE(bar.center().x(), at);
            QCOMPARE(bar.top(), l.trackNear() - kSliderBarOverhang);
            QCOMPARE(bar.bottom(), l.trackFar() + kSliderBarOverhang);
            QCOMPARE(bar.width(), kSliderBarWidth);
        }
    }
}

// Below the minimum, at zero size and with non-finite inputs: finite values and no rectangle of negative size
void tst_IndustrialSliderLayout::nonFiniteAndTinyInputs()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (bool vertical : {false, true}) {
        for (double w : {0.0, 1.0, 30.0, -5.0, nan, inf}) {
            for (double h : {0.0, 2.0, 40.0, -1.0, nan}) {
                SliderLayoutInput in = input(vertical, typicalText(), w, h);
                in.text.labelWidth = (w == 30.0) ? nan : in.text.labelWidth;
                in.text.podHeight = (h == 40.0) ? -inf : in.text.podHeight;
                const SliderLayout l = layoutSlider(in);
                for (const QRectF &r : {l.setpointPod, l.feedbackPod, l.track}) {
                    QVERIFY(std::isfinite(r.x()) && std::isfinite(r.y()) && std::isfinite(r.width()) && std::isfinite(r.height()));
                    QVERIFY(r.width() >= 0.0 && r.height() >= 0.0);
                }
                for (double v : {l.scaleMin, l.scaleMax, l.handleBase, l.zoneNear, l.tickNear, l.labelNear}) {
                    QVERIFY(std::isfinite(v));
                }
                QVERIFY(l.scaleLength() >= 0.0);
                const QSizeF min = minimumSliderSize(in);
                QVERIFY(std::isfinite(min.width()) && std::isfinite(min.height()) && min.width() > 0.0 && min.height() > 0.0);
            }
        }
    }
}

QTEST_GUILESS_MAIN(tst_IndustrialSliderLayout)
#include "tst_IndustrialSliderLayout.moc"
