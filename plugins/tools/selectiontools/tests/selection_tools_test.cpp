/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <KConfig>
#include <KConfigGroup>
#include <QKeySequence>
#include <QTemporaryDir>

#include <PkConfigGroup.h>
#include <PkString.h>

#include <KoToolFactoryBase.h>
#include <KoToolRegistry.h>

#include "selection_tools.h"

class SelectionToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void registeredFactoryShortcutsMatchQt515Parser();
    void thresholdFallbackMatchesKConfig();
};

void SelectionToolsTest::registeredFactoryShortcutsMatchQt515Parser()
{
    registerSelectionTools();

    KoToolFactoryBase *rectangular =
        KoToolRegistry::instance()->value(PkString("KisToolSelectRectangular"));
    KoToolFactoryBase *elliptical =
        KoToolRegistry::instance()->value(PkString("KisToolSelectElliptical"));
    QVERIFY(rectangular);
    QVERIFY(elliptical);

    QCOMPARE(rectangular->shortcut()[0],
             QKeySequence(QStringLiteral("Ctrl+R"))[0]);
    QCOMPARE(elliptical->shortcut()[0],
             QKeySequence(QStringLiteral("J"))[0]);
}

void SelectionToolsTest::thresholdFallbackMatchesKConfig()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    KConfig reference(directory.filePath(QStringLiteral("selectionrc")),
                      KConfig::SimpleConfig);
    KConfigGroup referenceGroup(&reference, QStringLiteral("selection-oracle"));
    PkConfigGroup nativeGroup(PkString("selection-oracle"));
    nativeGroup.deleteGroup();

    QCOMPARE(readSelectionThreshold(nativeGroup, 20),
             referenceGroup.readEntry("threshold", referenceGroup.readEntry("fuzziness", 20)));

    nativeGroup.writeEntry(PkString("fuzziness"), 37);
    referenceGroup.writeEntry("fuzziness", 37);
    QCOMPARE(readSelectionThreshold(nativeGroup, 20),
             referenceGroup.readEntry("threshold", referenceGroup.readEntry("fuzziness", 20)));

    nativeGroup.writeEntry(PkString("threshold"), 61);
    referenceGroup.writeEntry("threshold", 61);
    QCOMPARE(readSelectionThreshold(nativeGroup, 20),
             referenceGroup.readEntry("threshold", referenceGroup.readEntry("fuzziness", 20)));

    nativeGroup.deleteGroup();
}

SIMPLE_TEST_MAIN(SelectionToolsTest)

#include "selection_tools_test.moc"
