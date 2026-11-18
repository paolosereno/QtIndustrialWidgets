/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <QtIndustrialWidgets/IndustrialSwitch.h>
#include "IndustrialSwitch_p.h"

#include <functional>
#include <utility>

namespace QtIndustrialWidgets {
namespace internal {

// Test hook of IndustrialSwitch (#183)
class IndustrialSwitchTestAccess {
public:
    // #183: \a observer sees every accessibility event the switch sends, before QAccessible::updateAccessibility();
    // an empty function removes it
    static void setAccessibleObserver(IndustrialSwitch &sw, std::function<void(QAccessibleEvent *)> observer) {
        sw.d_ptr->m_accessibleObserver = std::move(observer);
    }
    // #243: the disc under the ⊘ of disabled, its centre and radius in logical px, and the box of the ⊘
    static std::pair<QPointF, double> disabledMarkDisc(const IndustrialSwitch &sw) {
        const QRectF body = sw.calculateSwitchRect();
        return {body.center(), IndustrialSwitchPrivate::disabledDiscRadius(body)};
    }
    static QRectF disabledMarkRect(const IndustrialSwitch &sw) {
        return IndustrialSwitchPrivate::disabledMarkRect(sw.calculateSwitchRect());
    }
};

} // namespace internal
} // namespace QtIndustrialWidgets
