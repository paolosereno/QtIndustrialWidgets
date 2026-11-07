/*
 * SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

// A UTC QDateTime without the QDateTime(QDate, QTime, Qt::TimeSpec) constructor, deprecated since Qt 6.9 (#215).
// From Qt 6.5 QTimeZone(QTimeZone::UTC) gives timeSpec() == Qt::UTC, the same QDateTime as Qt::UTC; before 6.5
// QTimeZone::utc() would give Qt::TimeZone instead, so Qt::UTC (not deprecated there) is kept.

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>
#include <QtCore/QtGlobal>

namespace TestDateTime {

inline QDateTime utc(QDate date, QTime time)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QDateTime(date, time, QTimeZone(QTimeZone::UTC));
#else
    return QDateTime(date, time, Qt::UTC);
#endif
}

} // namespace TestDateTime
