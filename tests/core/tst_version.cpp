// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/version.h"

#include <QRegularExpression>
#include <QtTest>

class TestVersion : public QObject
{
    Q_OBJECT
private slots:
    void versionIsSemantic();
};

void TestVersion::versionIsSemantic()
{
    static const QRegularExpression semver(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
    QVERIFY2(semver.match(appVersion()).hasMatch(), qPrintable(appVersion()));
}

QTEST_APPLESS_MAIN(TestVersion)
#include "tst_version.moc"
