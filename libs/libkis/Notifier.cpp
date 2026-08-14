/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "Notifier.h"
#include <QCoreApplication>
#include <KisDocument.h>
#include <KisDocumentRegistry.h>
#include <kis_config_notifier.h>
#include "Document.h"

struct Notifier::Private {
    Private() {}
    bool active {true};
};

Notifier::Notifier(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), SIGNAL(applicationClosing()));
    }

    connect(KisDocumentRegistry::instance(), &KisDocumentRegistry::sigDocumentAdded,
            this, qOverload<KisDocument *>(&Notifier::imageCreated));
    connect(KisDocumentRegistry::instance(), &KisDocumentRegistry::sigDocumentSaved,
            this, &Notifier::imageSaved);
    connect(KisDocumentRegistry::instance(), &KisDocumentRegistry::sigDocumentRemoved,
            this, &Notifier::imageClosed);

    connect(KisConfigNotifier::instance(), SIGNAL(configChanged()), SIGNAL(configurationChanged()));

    blockSignals(false);
}


Notifier::~Notifier()
{
    delete d;
}

bool Notifier::active() const
{
    return d->active;
}

void Notifier::setActive(bool value)
{
    d->active = value;
    blockSignals(!value);
}

void Notifier::imageCreated(KisDocument* document)
{
    Document *doc = new Document(document, false);
    Q_EMIT imageCreated(doc);
    delete doc;
}
