/*
 *  SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "Notifier.h"
#include <QCoreApplication>
#include <KisPart.h>
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

    connect(KisPart::instance(), SIGNAL(sigDocumentAdded(KisDocument*)), SLOT(imageCreated(KisDocument*)));
    connect(KisPart::instance(), SIGNAL(sigDocumentSaved(QString)), SIGNAL(imageSaved(QString)));
    connect(KisPart::instance(), SIGNAL(sigDocumentRemoved(QString)), SIGNAL(imageClosed(QString)));

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

