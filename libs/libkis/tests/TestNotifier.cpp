/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestNotifier.h"
#include <simpletest.h>

#include <Notifier.h>
#include <KisDocumentRegistry.h>
#include <Document.h>

#include <testui.h>

void TestNotifier::testNotifier()
{
    KisDocumentRegistry *registry = KisDocumentRegistry::instance();

    Notifier *notifier = new Notifier();
    connect(notifier, SIGNAL(imageCreated(Document*)), SLOT(documentAdded(Document*)), Qt::DirectConnection);

    QVERIFY(notifier->active());
    notifier->setActive(false);
    QVERIFY(!notifier->active());
    notifier->setActive(true);
    KisDocument *doc = registry->createDocument();
    registry->addDocument(doc);

    QVERIFY(m_document);

    registry->removeDocument(doc);

}

void TestNotifier::documentAdded(Document *image)
{
    m_document = image;
}

KISTEST_MAIN(TestNotifier)
