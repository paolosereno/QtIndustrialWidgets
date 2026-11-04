/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Helpers for the tests of the gallery theme colours (#190).

#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QColor>

#include <map>

namespace ColourPropertyTest {

using Colours = std::map<QString, QColor>;

// Every QColor property of the object, read through its meta-object
inline Colours colours(const QObject &o)
{
    Colours out;
    const QMetaObject *mo = o.metaObject();
    for (int i = 0; i < mo->propertyCount(); ++i) {
        const QMetaProperty p = mo->property(i);
        if (p.metaType().id() == QMetaType::QColor) {
            out[QString::fromLatin1(p.name())] = p.read(&o).value<QColor>();
        }
    }
    return out;
}

} // namespace ColourPropertyTest
