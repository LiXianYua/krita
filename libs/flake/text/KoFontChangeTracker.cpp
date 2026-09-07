/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoFontChangeTracker.h"
#include <PkFlakeBridge.h>

#include <QFileSystemWatcher>
#include <kis_signal_compressor.h>
#include <QDebug>

struct KoFontChangeTracker::Private {

    Private(PkStringList paths = PkStringList())
        : fileSystemWatcher(toQStringList(paths)) {

    }
    QFileSystemWatcher fileSystemWatcher;

    bool filesChanged = false;
    bool configStale = false;
};

KoFontChangeTracker::KoFontChangeTracker(PkStringList paths, QObject *parent)
    : QObject(parent)
    , d(new Private(paths))
{
    QObject::connect(&d->fileSystemWatcher, &QFileSystemWatcher::directoryChanged, this, &KoFontChangeTracker::directoriesChanged);
}

KoFontChangeTracker::~KoFontChangeTracker()
{
}

void KoFontChangeTracker::resetChangeTracker()
{
    d->filesChanged = false;
}


void KoFontChangeTracker::directoriesChanged()
{
    if (!d->filesChanged) {
        d->filesChanged = true;
        emit (sigUpdateConfig());
    }
}
