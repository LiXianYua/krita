/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QWidget>
#include <type_traits>

#include <PkObject.h>
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

    PkObject observer;
    int modeCount = 0;
    int actionCount = 0;
    int antiAliasCount = 0;
    int growCount = 0;
    int darkestCount = 0;
    int featherCount = 0;
    int referenceCount = 0;

    PkObject::connect(&options, &KisSelectionOptions::modeChanged,
                      &observer, [&](SelectionMode) { ++modeCount; });
    PkObject::connect(&options, &KisSelectionOptions::actionChanged,
                      &observer, [&](SelectionAction) { ++actionCount; });
    PkObject::connect(&options, &KisSelectionOptions::antiAliasSelectionChanged,
                      &observer, [&](bool) { ++antiAliasCount; });
    PkObject::connect(&options, &KisSelectionOptions::growSelectionChanged,
                      &observer, [&](int) { ++growCount; });
    PkObject::connect(&options, &KisSelectionOptions::stopGrowingAtDarkestPixelChanged,
                      &observer, [&](bool) { ++darkestCount; });
    PkObject::connect(&options, &KisSelectionOptions::featherSelectionChanged,
                      &observer, [&](int) { ++featherCount; });
    PkObject::connect(&options, &KisSelectionOptions::referenceLayersChanged,
                      &observer, [&](KisSelectionOptions::ReferenceLayers) { ++referenceCount; });

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
    QCOMPARE(modeCount, 1);
    QCOMPARE(actionCount, 1);
    QCOMPARE(antiAliasCount, 1);
    QCOMPARE(growCount, 1);
    QCOMPARE(darkestCount, 1);
    QCOMPARE(featherCount, 1);
    QCOMPARE(referenceCount, 1);

    options.setMode(PIXEL_SELECTION);
    options.setAction(SELECTION_ADD);
    options.setAntiAliasSelection(false);
    options.setGrowSelection(9);
    options.setStopGrowingAtDarkestPixel(true);
    options.setFeatherSelection(4);
    options.setReferenceLayers(KisSelectionOptions::ColorLabeledLayers);

    QCOMPARE(modeCount, 1);
    QCOMPARE(actionCount, 1);
    QCOMPARE(antiAliasCount, 1);
    QCOMPARE(growCount, 1);
    QCOMPARE(darkestCount, 1);
    QCOMPARE(featherCount, 1);
    QCOMPARE(referenceCount, 1);
}

void KisSelectionOptionsTest::testSelectedColorLabelsAreObservableState()
{
    KisSelectionOptions options;
    PkObject observer;
    int changedCount = 0;
    PkObject::connect(&options, &KisSelectionOptions::selectedColorLabelsChanged,
                      &observer, [&] { ++changedCount; });

    QCOMPARE(options.selectedColorLabels(), PkList<int>());

    options.setSelectedColorLabels({2, 5, 8});
    QCOMPARE(options.selectedColorLabels(), PkList<int>({2, 5, 8}));
    QCOMPARE(changedCount, 1);

    options.setSelectedColorLabels({2, 5, 8});
    QCOMPARE(changedCount, 1);

    options.setSelectedColorLabels({8, 2});
    QCOMPARE(options.selectedColorLabels(), PkList<int>({8, 2}));
    QCOMPARE(changedCount, 2);
}

SIMPLE_TEST_MAIN(KisSelectionOptionsTest)

#include "kis_selection_options_test.moc"
