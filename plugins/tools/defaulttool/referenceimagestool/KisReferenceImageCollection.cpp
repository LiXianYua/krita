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
#include <libs/store/KoStoreDevice.h>

namespace {

const PkString METADATA_FILE = "reference_images.xml";

void appendWireRecord(PkXmlDocument &document, PkXmlElement &root,
                      const KisReferenceImageWireRecord &wire)
{
    PkXmlElement element = document.createElement("referenceimage");
    element.setAttribute("src", wire.embedded
        ? wire.source : PkString("file://") + wire.source);
    element.setAttribute("width", wire.width);
    element.setAttribute("height", wire.height);
    element.setAttribute("keepAspectRatio", wire.keepAspectRatio);
    element.setAttribute("transform", wire.transform);
    element.setAttribute("opacity", wire.opacity);
    element.setAttribute("saturation", wire.saturation);
    root.appendChild(element);
}

KisReferenceImageWireRecord wireRecord(const PkXmlElement &element)
{
    KisReferenceImageWireRecord wire;
    const PkString src = element.attribute("src");
    wire.embedded = !src.startsWith("file://");
    wire.source = wire.embedded ? src : src.mid(7);
    wire.width = element.attribute("width", "100");
    wire.height = element.attribute("height", "100");
    wire.keepAspectRatio = element.attribute("keepAspectRatio", "true");
    wire.transform = element.attribute("transform");
    wire.opacity = element.attribute("opacity", "1");
    wire.saturation = element.attribute("saturation", "1");
    return wire;
}

}

const PkString &referenceImageCollectionMetadataFile()
{
    return METADATA_FILE;
}

KisReferenceImageCollection::KisReferenceImageCollection(KisReferenceImageCodec &codec)
    : m_codec(codec)
{}

KisReferenceImageCollection::KisReferenceImageCollection(
    KisReferenceImageCodec &codec,
    const PkVector<KisReferenceImage *> &references)
    : m_codec(codec)
    , references(references)
{}

const PkVector<KisReferenceImage*> &KisReferenceImageCollection::referenceImages() const
{
    return references;
}

bool KisReferenceImageCollection::save(PkStream *io)
{
    // Preserve the historical writer's trailing bracket byte. Readers use the
    // registered MIME without it; the byte is part of existing .krf archives.
    static const char appId[] = "application/x-krita-reference-images]";
    PkScopedPointer<KoStore> store(KoStore::createStore(
        io, KoStore::Write, PkByteArray(appId, int(sizeof(appId) - 1)), KoStore::Zip));
    if (store.isNull()) return false;

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("referenceimages");
    doc.insertBefore(root, PkXmlNode());

    std::sort(references.begin(), references.end(), KoShape::compareShapeZIndex);

    int nextId = 0;
    for (KisReferenceImage *reference : references) {
        KisReferenceImageWireRecord wire;
        if (!m_codec.describeReferenceImage(reference, &wire)) return false;
        if (wire.embedded) {
            wire.source = PkString("reference_images/%1.png").arg(nextId);
        }
        ++nextId;
        appendWireRecord(doc, root, wire);

        if (wire.embedded &&
            !m_codec.saveReferenceImagePayload(reference, wire.source, store.data())) {
            return false;
        }
    }

    if (!store->open(METADATA_FILE)) {
        return false;
    }

    KoStoreDevice xmlDev(store.data());
    const std::string xml = doc.toByteArray().PkToUtf8();
    const auto written = xmlDev.write(xml.data(), static_cast<PkStream::pk_int64>(xml.size()));
    xmlDev.close();
    const bool storeClosed = store->close();
    return written == static_cast<PkStream::pk_int64>(xml.size()) &&
        storeClosed && store->finalize();
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
    const bool parsed = doc.setContent(store->device());
    store->close();

    m_loadFailures.clear();

    // The original DOM parser's failure was ignored by the loader; malformed
    // metadata therefore loaded as an empty collection.
    if (!parsed) return true;

    PkXmlElement root = doc.documentElement();

    PkXmlElement element = root.firstChildElement("referenceimage");
    while (!element.isNull()) {
        const KisReferenceImageWireRecord wire = wireRecord(element);
        KisReferenceImage *reference = m_codec.loadReferenceImage(wire, store.data());

        if (reference) {
            references.append(reference);
        } else {
            m_loadFailures << wire.source;
        }
        element = element.nextSiblingElement("referenceimage");
    }

    return true;
}
