/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_IMAGE_CONFIG_TEST_H
#define KIS_IMAGE_CONFIG_TEST_H

#include <QObject>

#include <QMap>
#include <QString>
#include <QStringList>

class KisImageConfigTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testLegacyCursorStyleMapping_data();
    void testLegacyCursorStyleMapping();
    void testModernCursorStyleValidation();
    void testLegacyCursorStyleKeyCleanup();
    void testPressureTabletCurve();
    void testOutlinePaintingFlags();
    void testLineSmoothingType();
    void testCompressKra();

private:
    void clearTestKeys();

    QStringList m_testKeys;
    QMap<QString, QString> m_originalEntries;
};

#endif
