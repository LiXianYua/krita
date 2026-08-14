/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QSignalSpy>
#include <type_traits>

#include <simpletest.h>

#include "kis_selection_options.h"

class KisSelectionOptionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testDomainStateIsNotAWidget();
    void testAllValuesAreObservableState();
    void testSelectedColorLabelsAreObservableState();
};

void KisSelectionOptionsTest::testDomainStateIsNotAWidget()
{
    QVERIFY((!std::is_base_of_v<QWidget, KisSelectionOptions>));
}

void KisSelectionOptionsTest::testAllValuesAreObservableState()
{
    KisSelectionOptions options;

    QSignalSpy modeSpy(&options, &KisSelectionOptions::modeChanged);
    QSignalSpy actionSpy(&options, &KisSelectionOptions::actionChanged);
    QSignalSpy antiAliasSpy(
        &options, &KisSelectionOptions::antiAliasSelectionChanged);
    QSignalSpy growSpy(&options, &KisSelectionOptions::growSelectionChanged);
    QSignalSpy darkestSpy(
        &options, &KisSelectionOptions::stopGrowingAtDarkestPixelChanged);
    QSignalSpy featherSpy(
        &options, &KisSelectionOptions::featherSelectionChanged);
    QSignalSpy referenceSpy(
        &options, &KisSelectionOptions::referenceLayersChanged);

    QCOMPARE(options.mode(), SHAPE_PROTECTION);
    QCOMPARE(options.action(), SELECTION_REPLACE);
    QVERIFY(options.antiAliasSelection());
    QCOMPARE(options.growSelection(), 0);
    QVERIFY(!options.stopGrowingAtDarkestPixel());
    QCOMPARE(options.featherSelection(), 0);
    QCOMPARE(options.referenceLayers(), KisSelectionOptions::CurrentLayer);

    options.setMode(PIXEL_SELECTION);
    options.setAction(SELECTION_ADD);
    options.setAntiAliasSelection(false);
    options.setGrowSelection(9);
    options.setStopGrowingAtDarkestPixel(true);
    options.setFeatherSelection(4);
    options.setReferenceLayers(KisSelectionOptions::ColorLabeledLayers);

    QCOMPARE(options.mode(), PIXEL_SELECTION);
    QCOMPARE(options.action(), SELECTION_ADD);
    QVERIFY(!options.antiAliasSelection());
    QCOMPARE(options.growSelection(), 9);
    QVERIFY(options.stopGrowingAtDarkestPixel());
    QCOMPARE(options.featherSelection(), 4);
    QCOMPARE(options.referenceLayers(),
             KisSelectionOptions::ColorLabeledLayers);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(actionSpy.count(), 1);
    QCOMPARE(antiAliasSpy.count(), 1);
    QCOMPARE(growSpy.count(), 1);
    QCOMPARE(darkestSpy.count(), 1);
    QCOMPARE(featherSpy.count(), 1);
    QCOMPARE(referenceSpy.count(), 1);

    options.setMode(PIXEL_SELECTION);
    options.setAction(SELECTION_ADD);
    options.setAntiAliasSelection(false);
    options.setGrowSelection(9);
    options.setStopGrowingAtDarkestPixel(true);
    options.setFeatherSelection(4);
    options.setReferenceLayers(KisSelectionOptions::ColorLabeledLayers);

    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(actionSpy.count(), 1);
    QCOMPARE(antiAliasSpy.count(), 1);
    QCOMPARE(growSpy.count(), 1);
    QCOMPARE(darkestSpy.count(), 1);
    QCOMPARE(featherSpy.count(), 1);
    QCOMPARE(referenceSpy.count(), 1);
}

void KisSelectionOptionsTest::testSelectedColorLabelsAreObservableState()
{
    KisSelectionOptions options;
    QSignalSpy changedSpy(&options,
                          &KisSelectionOptions::selectedColorLabelsChanged);

    QCOMPARE(options.selectedColorLabels(), QList<int>());

    options.setSelectedColorLabels({2, 5, 8});
    QCOMPARE(options.selectedColorLabels(), QList<int>({2, 5, 8}));
    QCOMPARE(changedSpy.count(), 1);

    options.setSelectedColorLabels({2, 5, 8});
    QCOMPARE(changedSpy.count(), 1);

    options.setSelectedColorLabels({8, 2});
    QCOMPARE(options.selectedColorLabels(), QList<int>({8, 2}));
    QCOMPARE(changedSpy.count(), 2);
}

SIMPLE_TEST_MAIN(KisSelectionOptionsTest)

#include "kis_selection_options_test.moc"
