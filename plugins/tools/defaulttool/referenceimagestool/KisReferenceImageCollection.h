/*
 *  SPDX-FileCopyrightText: 2018 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISREFERENCEIMAGECOLLECTION_H
#define KISREFERENCEIMAGECOLLECTION_H

#include <PkVector.h>
#include <PkStringList.h>

#include "KisReferenceImagePlatform.h"

class PkStream;
class KisReferenceImage;

class KisReferenceImageCollection
{
public:
    explicit KisReferenceImageCollection(KisReferenceImageCodec &codec);
    KisReferenceImageCollection(KisReferenceImageCodec &codec,
                                const PkVector<KisReferenceImage*> &references);

    const PkVector<KisReferenceImage*> &referenceImages() const;

    bool save(PkStream *io);
    bool load(PkStream *io);
    const PkStringList &loadFailures() const { return m_loadFailures; }

private:
    KisReferenceImageCodec &m_codec;
    PkVector<KisReferenceImage*> references;
    PkStringList m_loadFailures;
};

const PkString &referenceImageCollectionMetadataFile();

#endif
