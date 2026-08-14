/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DOCUMENT_DESKTOP_SERVICES_TEST_H
#define KIS_DOCUMENT_DESKTOP_SERVICES_TEST_H

#include <QObject>

class KisDocumentDesktopServicesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCanvasResourcesForActiveImage();
};

#endif // KIS_DOCUMENT_DESKTOP_SERVICES_TEST_H
