// SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QtTest/QtTest>

// QSKIP(message) without its variadic part (#226). Qt declares QSKIP(statement, ...), a form kept from the
// two-argument QSKIP of Qt 4; with -Wpedantic in C++17, AppleClang warns when the variadic part is omitted
// (-Wvariadic-macro-arguments-omitted, an error with warnings as errors in CI). QTest::qSkip() exists since Qt 5.
// A skip must be approved by the maintainer and listed in tests/expected_skips.json (CLAUDE.md).
#define QIW_SKIP(message)                                                                                              \
    do {                                                                                                               \
        QTest::qSkip(message, __FILE__, __LINE__);                                                                     \
        return;                                                                                                        \
    } while (false)
