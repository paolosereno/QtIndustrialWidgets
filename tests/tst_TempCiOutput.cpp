// SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
//
// SPDX-License-Identifier: MIT

// TEMP (#36): one passing and one deliberately failing test, to see whether the QtTest log
// reaches the CI output on Windows. Remove before merging.
#include <QtTest/QtTest>

class tst_TempCiOutput : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void passes();
    void failsOnPurpose();
};

void tst_TempCiOutput::passes()
{
    qInfo("TEMP-36 marker: passing test ran");
    QCOMPARE(1 + 1, 2);
}

void tst_TempCiOutput::failsOnPurpose()
{
    qInfo("TEMP-36 marker: failing test ran");
    QCOMPARE(1 + 1, 3); // deliberate failure (#36)
}

QTEST_APPLESS_MAIN(tst_TempCiOutput)
#include "tst_TempCiOutput.moc"
