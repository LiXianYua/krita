/*
 *  SPDX-FileCopyrightText: 2018 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisReferenceImageCollection.h"

#include <PkStream.h>
#include <PkAuxTypes.h>


#include <libs/store/KoStore.h>
#include <KisReferenceImage.h>
#include <KisReferenceImageDocumentFallback.h>
#include <libs/store/KoStoreDevice.h>

const PkString METADATA_FILE = "reference_images.xml";

KisReferenceImageCollection::KisReferenceImageCollection(const PkVector<KisReferenceImage *> &references)
    : references(references)
{}

const PkVector<KisReferenceImage*> &KisReferenceImageCollection::referenceImages() const
{
    return references;
}

bool KisReferenceImageCollection::save(PkStream *io)
{
    static const char appId[] = "application/x-krita-reference-images";
    PkScopedPointer<KoStore> store(KoStore::createStore(
        io, KoStore::Write, PkByteArray(appId, int(sizeof(appId) - 1)), KoStore::Zip));
    if (store.isNull()) return false;

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("referenceimages");
    doc.insertBefore(root, PkXmlNode());

    std::sort(references.begin(), references.end(), KoShape::compareShapeZIndex);

    int nextId = 0;
    for (KisReferenceImage *reference : references) {
        reference->saveXml(doc, root, nextId++);

        if (reference->embed()) {
            bool ok = reference->saveImage(store.data());
            if (!ok) return false;
        }
    }

    if (!store->open(METADATA_FILE)) {
        return false;
    }

    KoStoreDevice xmlDev(store.data());
    const std::string xml = doc.toByteArray().PkToUtf8();
    xmlDev.write(xml.data(), static_cast<PkStream::pk_int64>(xml.size()));
    xmlDev.close();
    store->close();

    return true;
}

bool KisReferenceImageCollection::load(PkStream *io)
{
    static const char appId[] = "application/x-krita-reference-images";
    PkScopedPointer<KoStore> store(KoStore::createStore(
        io, KoStore::Read, PkByteArray(appId, int(sizeof(appId) - 1)), KoStore::Zip));
    if (!store || store->bad()) {
        return false;
    }

    if (!store->hasFile(METADATA_FILE) || !store->open(METADATA_FILE)) {
        return false;
    }

    PkXmlDocument doc;
    if (!doc.setContent(store->device())) {
        store->close();
        return false;
    }
    store->close();
    PkXmlElement root = doc.documentElement();

    m_loadFailures.clear();

    PkXmlElement element = root.firstChildElement("referenceimage");
    while (!element.isNull()) {
        KisReferenceImage *reference = KisReferenceImage::fromXml(element);

        if (loadReferenceImageWithDocumentFallback(reference, store.data())) {
            references.append(reference);
        } else {
            m_loadFailures << (reference->embed() ? reference->internalFile() : reference->filename());
            delete reference;
        }
        element = element.nextSiblingElement("referenceimage");
    }

    return true;
}
