/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <klocalizedstring.h>
#include <QDebug>
#include "kra_converter.h"

#include <PkVersionNumber.h>
#include <PkAuxTypes.h> // PkByteArray

#include <KoStore.h>
#include <KoStoreDevice.h>
#include <KoColorSpaceRegistry.h>
#include <KoDocumentInfo.h>
#include <KoXmlWriter.h>

#include <KisDocument.h>
#include <KisImportExportManager.h>
#include <KisImportUserFeedbackInterface.h>
#include <KritaVersionWrapper.h>
#include <kis_clone_layer.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_paint_layer.h>

#include <zlib.h>

#include <cstdint>
#include <vector>

static const char CURRENT_DTD_VERSION[] = "2.0";

namespace {

PkString toPkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

QString toQString(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    return QString::fromUtf8(utf8.data(), int(utf8.size()));
}

// Minimal PNG (8-bit RGBA, non-interlaced) writer. The kernel has no Qt image
// encoder, so the .kra thumbnail is produced directly with zlib.
// Pixel packing follows PkImage::pixel(): uint32_t 0xAARRGGBB.

void writeBe32(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((v >> 24) & 0xFF);
    out.push_back((v >> 16) & 0xFF);
    out.push_back((v >> 8) & 0xFF);
    out.push_back(v & 0xFF);
}

void writePngChunk(std::vector<uint8_t> &out, const char (&type)[5], const std::vector<uint8_t> &data)
{
    writeBe32(out, (uint32_t)data.size());
    const size_t typeStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&out[typeStart]), (uInt)(out.size() - typeStart));
    writeBe32(out, (uint32_t)crc);
}

std::vector<uint8_t> encodePng(const PkImage &image)
{
    const int w = image.width();
    const int h = image.height();
    if (w <= 0 || h <= 0 || image.isNull()) {
        return {};
    }

    std::vector<uint8_t> out;
    out.reserve(8 + 25 + (size_t)h * ((size_t)w * 4 + 1) + 12);
    // PNG signature
    static const uint8_t signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    out.insert(out.end(), signature, signature + 8);

    // IHDR
    std::vector<uint8_t> ihdr;
    ihdr.reserve(13);
    writeBe32(ihdr, (uint32_t)w);
    writeBe32(ihdr, (uint32_t)h);
    ihdr.push_back(8); // bit depth
    ihdr.push_back(6); // color type: RGBA
    ihdr.push_back(0); // compression method
    ihdr.push_back(0); // filter method
    ihdr.push_back(0); // interlace: none
    writePngChunk(out, "IHDR", ihdr);

    // IDAT: raw RGBA scanlines, each prefixed with filter byte 0 (None)
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * ((size_t)w * 4 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        for (int x = 0; x < w; ++x) {
            const uint32_t px = image.pixel(x, y); // 0xAARRGGBB
            raw.push_back((px >> 16) & 0xFF); // R
            raw.push_back((px >> 8) & 0xFF);  // G
            raw.push_back(px & 0xFF);         // B
            raw.push_back((px >> 24) & 0xFF); // A
        }
    }

    uLongf bound = compressBound((uLong)raw.size());
    std::vector<uint8_t> compressed(bound);
    if (compress2(compressed.data(), &bound, raw.data(), (uLong)raw.size(), Z_BEST_COMPRESSION) != Z_OK) {
        return {};
    }
    compressed.resize(bound);
    writePngChunk(out, "IDAT", compressed);

    // IEND
    writePngChunk(out, "IEND", {});

    return out;
}

} // namespace

KraConverter::KraConverter(KisDocument *doc)
    : m_doc(doc)
    , m_image(doc->savingImage())
{
}

KraConverter::KraConverter(KisDocument *doc, PkPointer<KoUpdater> updater, KisImportUserFeedbackInterface *feedbackInterface)
    : m_doc(doc)
    ,  m_image(doc->savingImage())
    ,  m_updater(updater)
    ,  m_feedbackInterface(feedbackInterface)
{
}

KraConverter::~KraConverter()
{
    delete m_store;
    delete m_kraSaver;
    delete m_kraLoader;
}

void fixCloneLayers(KisImageSP image, KisNodeSP root)
{
    KisNodeSP first = root->firstChild();
    KisNodeSP node = first;
    while (!node.isNull()) {
        if (node->inherits("KisCloneLayer")) {
            KisCloneLayer* layer = dynamic_cast<KisCloneLayer*>(node.data());
            if (layer && layer->copyFrom().isNull()) {
                KisLayerSP reincarnation = layer->reincarnateAsPaintLayer();
                image->addNode(reincarnation, node->parent(), node->prevSibling());
                image->removeNode(node);
                node = reincarnation;
            }
        } else if (node->childCount() > 0) {
            fixCloneLayers(image, node);
        }
        node = node->nextSibling();
    }
}

KisImportExportErrorCode KraConverter::buildImage(PkStream *io)
{
    m_store = KoStore::createStore(io, KoStore::Read, PkByteArray(), KoStore::Zip);

    if (m_store->bad()) {
        m_doc->setErrorMessage(toPkString(i18n("Not a valid Krita file")));
        return ImportExportCodes::FileFormatIncorrect;
    }

    bool success = false;
    {
        if (m_store->hasFile("root") || m_store->hasFile("maindoc.xml")) {   // Fallback to "old" file format (maindoc.xml)
            PkXmlDocument doc;

            KisImportExportErrorCode res = oldLoadAndParse(m_store, "root", doc);
            if (res.isOk())
                res = loadXML(doc, m_store);
            if (!res.isOk()) {
                return res;
            }

        } else {
            errUI << "ERROR: No maindoc.xml";
        m_doc->setErrorMessage(toPkString(i18n("Invalid document: no file 'maindoc.xml'.")));
            return ImportExportCodes::FileFormatIncorrect;
        }

        if (m_store->hasFile("documentinfo.xml")) {
            PkXmlDocument doc;
            KisImportExportErrorCode resultHere = oldLoadAndParse(m_store, "documentinfo.xml", doc);
            if (resultHere.isOk()) {
                m_doc->documentInfo()->load(doc);
            }
        }
        success = completeLoading(m_store);
    }

    fixCloneLayers(m_image, m_image->root());

    return success ? ImportExportCodes::OK : ImportExportCodes::Failure;
}

KisImageSP KraConverter::image()
{
    return m_image;
}

vKisNodeSP KraConverter::activeNodes()
{
    return m_activeNodes;
}

PkList<KisPaintingAssistantSP> KraConverter::assistants()
{
    return m_assistants;
}

StoryboardItemList KraConverter::storyboardItemList()
{
    return m_storyboardItemList;
}

StoryboardCommentList KraConverter::storyboardCommentList()
{
    return m_storyboardCommentList;
}

KisImportExportErrorCode KraConverter::buildFile(PkStream *io, const PkString &filename, bool addMergedImage)
{
    if (m_image->size().isEmpty()) {
        return ImportExportCodes::Failure;
    }
    
    setProgress(5);
    const PkByteArray mimeType = m_doc->nativeFormatMimeType();
    m_store = KoStore::createStore(io, KoStore::Write, mimeType, KoStore::Zip);

    bool success = true;

    if (m_store->bad()) {
        m_doc->setErrorMessage(toPkString(i18n("Could not create the file for saving")));
        return ImportExportCodes::CannotCreateFile;
    }

    setProgress(20);

    m_kraSaver = new KisKraSaver(m_doc, filename, addMergedImage);

    KisImportExportErrorCode resultCode = saveRootDocuments(m_store);

    if (!resultCode.isOk()) {
        return resultCode;
    }

    setProgress(40);
    bool result;

    result = m_kraSaver->saveKeyframes(m_store, m_doc->path(), true);
    if (!result) {
        success = false;
        qWarning() << "saving key frames failed";
    }
    setProgress(60);
    result = m_kraSaver->saveBinaryData(m_store, m_image, m_doc->path(), true, addMergedImage);
    if (!result) {
        success = false;
        qWarning() << "saving binary data failed";
    }
    setProgress(70);
    result = m_kraSaver->saveResources(m_store, m_image, m_doc->path());
    if (!result) {
        success = false;
        qWarning() << "saving resources data failed";
    }

    result = m_kraSaver->saveStoryboard(m_store, m_image, m_doc->path());
    if (!result) {
        success = false;
        qWarning() << "Saving storyboard data failed";
    }

    result = m_kraSaver->saveAnimationMetadata(m_store, m_image, m_doc->path());
    if (!result) {
        success = false;
        qWarning() << "Saving animation metadata failed";
    }

    result = m_kraSaver->saveAudio(m_store);
    if (!result) {
        success = false;
        qWarning() << "Saving audio data failed";
    }

    setProgress(80);

    if (!m_store->finalize()) {
        success = false;
    }
    if (!success || !m_kraSaver->errorMessages().isEmpty()) {
        m_doc->setErrorMessage(m_kraSaver->errorMessages().join(".\n"));
        return ImportExportCodes::Failure;
    }

    m_doc->setWarningMessage(m_kraSaver->warningMessages().join(".\n"));

    setProgress(90);
    return ImportExportCodes::OK;
}

KisImportExportErrorCode KraConverter::saveRootDocuments(KoStore *store)
{
    dbgFile << "Saving root";
    if (store->open("root")) {
        KoStoreDevice dev(store);
        if (!saveToStream(&dev) || !store->close()) {
            dbgUI << "saveToStream failed";
            return ImportExportCodes::NoAccessToWrite;
        }
    } else {
        m_doc->setErrorMessage(toPkString(i18n("Not able to write '%1'. Partition full?", QStringLiteral("maindoc.xml"))));
        return ImportExportCodes::ErrorWhileWriting;
    }

    if (store->open("documentinfo.xml")) {
        PkXmlDocument doc = KisDocument::createDomDocument("document-info"
                                                          /*DTD name*/, "document-info" /*tag name*/, "1.1");
        doc = m_doc->documentInfo()->save(doc,
                                          m_doc->isAutosaving(),
                                          m_doc->isModified());
        KoStoreDevice dev(store);
        const PkString s = doc.toByteArray(); // this is already Utf8!
        const std::string sUtf8 = s.PkToUtf8();
        bool success = dev.write(sUtf8.data(), static_cast<long>(sUtf8.size()));
        if (!success) {
            return ImportExportCodes::ErrorWhileWriting;
        }
        store->close();
    } else {
        return ImportExportCodes::Failure;
    }

    if (store->open("preview.png")) {
        // ### TODO: missing error checking (The partition could be full!)
        KisImportExportErrorCode result = savePreview(store);
        (void)store->close();
        if (!result.isOk()) {
            return result;
        }
    } else {
        return ImportExportCodes::Failure;
    }

    dbgUI << "Saving done of url:" << m_doc->path();
    return ImportExportCodes::OK;
}

bool KraConverter::saveToStream(PkStream *dev)
{
    PkXmlDocument doc = createDomDocument();
    // Save to buffer
    const PkString s = doc.toByteArray(); // utf8 already
    const std::string sUtf8 = s.PkToUtf8();
    dev->open(PkStream::WriteOnly);
    const long nwritten = dev->write(sUtf8.data(), static_cast<long>(sUtf8.size()));
    if (nwritten != static_cast<long>(sUtf8.size())) {
        warnUI << "wrote " << nwritten << "- expected" <<  sUtf8.size();
    }
    return nwritten == (int)s.size();
}

PkXmlDocument KraConverter::createDomDocument()
{
    PkXmlDocument doc = m_doc->createDomDocument("DOC", CURRENT_DTD_VERSION);
    PkXmlElement root = doc.documentElement();

    root.setAttribute("editor", "Krita");
    root.setAttribute("syntaxVersion", CURRENT_DTD_VERSION);
    root.setAttribute("kritaVersion", KritaVersionWrapper::versionString(false));

    root.appendChild(m_kraSaver->saveXML(doc, m_image));

    if (!m_kraSaver->errorMessages().isEmpty()) {
        m_doc->setErrorMessage(m_kraSaver->errorMessages().join(".\n"));
    }
    return doc;
}

KisImportExportErrorCode KraConverter::savePreview(KoStore *store)
{
    PkImage preview = m_doc->generatePreview(PkSize(256, 256));
    preview = preview.convertToFormat(PkImage::Format_ARGB32);
    if (preview.isNull() || preview.size().isEmpty()) {
        PkSize newSize = m_doc->savingImage()->bounds().size();
        // make sure dimensions are at least one pixel, because extreme aspect ratios may cause rounding to zero
        newSize = newSize.scaled(PkSize(256, 256), Qt::KeepAspectRatio).expandedTo({1, 1});
        preview = PkImage(newSize, PkImage::Format_ARGB32);
        preview.fill(0u); // ARGB transparent black
    }

    const std::vector<uint8_t> png = encodePng(preview);
    if (png.empty()) {
        return ImportExportCodes::ErrorWhileWriting;
    }

    KoStoreDevice io(store);
    if (!io.open(PkStream::WriteOnly)) {
        return ImportExportCodes::NoAccessToWrite;
    }
    const PkStream::pk_int64 written = io.write(reinterpret_cast<const char *>(png.data()), (PkStream::pk_int64)png.size());
    io.close();
    return written == (PkStream::pk_int64)png.size() ? ImportExportCodes::OK : ImportExportCodes::ErrorWhileWriting;
}


KisImportExportErrorCode KraConverter::oldLoadAndParse(KoStore *store, const PkString &filename, PkXmlDocument &xmldoc)
{
    //dbgUI <<"Trying to open" << filename;

    if (!store->open(filename)) {
        warnUI << "Entry " << filename << " not found!";
        m_doc->setErrorMessage(toPkString(i18n("Could not find %1", toQString(filename))));
        return ImportExportCodes::FileNotExist;
    }
    // Error variables for PkXmlDocument::setContent
    PkString errorMsg;
    int errorLine, errorColumn;
    const bool ok = xmldoc.setContent(store->device(), &errorMsg, &errorLine, &errorColumn);
    store->close();
    if (!ok) {
        errUI << "Parsing error in " << filename << "! Aborting!\n"
              << " In line: " << errorLine << ", column: " << errorColumn << "\n"
              << " Error message: " << errorMsg;
        m_doc->setErrorMessage(toPkString(i18n("Parsing error in %1 at line %2, column %3\nError message: %4",
                                    toQString(filename), errorLine, errorColumn, toQString(errorMsg))));
        return ImportExportCodes::FileFormatIncorrect;
    }
    dbgUI << "File" << filename << " loaded and parsed";
    return ImportExportCodes::OK;
}

KisImportExportErrorCode KraConverter::loadXML(const PkXmlDocument &doc, KoStore *store)
{
    Q_UNUSED(store);

    PkXmlElement root;
    PkXmlNode node;

    if (doc.doctype().name() != "DOC") {
       errUI << "The format is not supported or the file is corrupted";
       m_doc->setErrorMessage(toPkString(i18n("The format is not supported or the file is corrupted")));
       return ImportExportCodes::FileFormatIncorrect;
    }
    root = doc.documentElement();
    
    PkString versionTag = root.attribute("syntaxVersion", "3.0");
    PkVersionNumber parsedVersionNumber = PkVersionNumber::fromString(versionTag);
    const int syntaxVersion = parsedVersionNumber.isNull() ? 3 : parsedVersionNumber.majorVersion();
    
    if (syntaxVersion > 2) {
        errUI << "The file is too new for this version of Krita:" << syntaxVersion;
        m_doc->setErrorMessage(toPkString(i18n("The file is too new for this version of Krita (%1).", syntaxVersion)));
        return ImportExportCodes::FormatFeaturesUnsupported;
    }

    if (!root.hasChildNodes()) {
        errUI << "The file has no layers.";
        m_doc->setErrorMessage(toPkString(i18n("The file has no layers.")));
        return ImportExportCodes::FileFormatIncorrect;
    }

    PkString kritaVersionTag = root.attribute("kritaVersion", "6.0");
    PkVersionNumber kritaVersionNumber = PkVersionNumber::fromString(kritaVersionTag);
    if (kritaVersionNumber.isNull()) {
        kritaVersionNumber = PkVersionNumber::fromString(KritaVersionWrapper::versionString(false));
    }

    m_kraLoader = new KisKraLoader(m_doc, syntaxVersion, kritaVersionNumber, m_feedbackInterface);

    // reset the old image before loading the next one
    m_doc->setCurrentImage(0, false);

    for (node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        if (node.isElement()) {
            if (node.nodeName() == "IMAGE") {
                PkXmlElement elem = node.toElement();
                if (!(m_image = m_kraLoader->loadXML(elem))) {

                    if (m_kraLoader->errorMessages().isEmpty()) {
                        errUI << "Unknown error while opening the .kra file.";
                        m_doc->setErrorMessage(toPkString(i18n("Unknown error.")));
                    }
                    else {
                        m_doc->setErrorMessage(m_kraLoader->errorMessages().join("\n"));
                        errUI << m_kraLoader->errorMessages().join("\n");
                    }
                    return ImportExportCodes::Failure;
                }

                // HACK ALERT!
                m_doc->hackPreliminarySetImage(m_image);

                return ImportExportCodes::OK;
            }
            else {
                if (m_kraLoader->errorMessages().isEmpty()) {
                    m_doc->setErrorMessage(toPkString(i18n("The file does not contain an image.")));
                }
                return ImportExportCodes::FileFormatIncorrect;
            }
        }
    }
    return ImportExportCodes::Failure;
}

bool KraConverter::completeLoading(KoStore* store)
{
    if (!m_image) {
        if (m_kraLoader->errorMessages().isEmpty()) {
           m_doc->setErrorMessage(toPkString(i18n("Unknown error.")));
        }
        else {
           m_doc->setErrorMessage(m_kraLoader->errorMessages().join("\n"));
        }
        return false;
    }

    m_image->disableDirtyRequests();

    PkString layerPathName = m_kraLoader->imageName();
    if (!m_store->hasDirectory(layerPathName)) {
        // We might be hitting an encoding problem. Get the only folder in the toplevel
        for (const PkString &entry : m_store->directoryList()) {
            if (entry.contains("/layers/")) {
                const std::string entryUtf8 = entry.PkToUtf8();
                const std::size_t layersPos = entryUtf8.find("/layers/");
                layerPathName = PkString::PkFromUtf8(entryUtf8.c_str(), static_cast<int>(layersPos));
                m_store->setSubstitution(m_kraLoader->imageName(), layerPathName);
                break;
            }
        }
    }

    m_kraLoader->loadResources(store, m_doc);
    m_kraLoader->loadBinaryData(store, m_image, m_doc->localFilePath(), true);
    m_kraLoader->loadStoryboards(store, m_doc);
    m_kraLoader->loadAnimationMetadata(store, m_image);
    m_kraLoader->loadAudio(store, m_doc);

    if (!m_kraLoader->errorMessages().isEmpty()) {
        m_doc->setErrorMessage(m_kraLoader->errorMessages().join("\n"));
        return false;
    }

    m_image->enableDirtyRequests();

    if (!m_kraLoader->warningMessages().isEmpty()) {
        // warnings do not interrupt loading process, so we do not return here
        m_doc->setWarningMessage(m_kraLoader->warningMessages().join("\n"));
    }

    m_activeNodes = m_kraLoader->selectedNodes();
    m_assistants = m_kraLoader->assistants();
    m_storyboardItemList = m_kraLoader->storyboardItemList();
    m_storyboardCommentList = m_kraLoader->storyboardCommentList();

    return true;
}

void KraConverter::cancel()
{
    m_stop = true;
}

void KraConverter::setProgress(int progress)
{
    if (m_updater) {
        m_updater->setProgress(progress);
    }
}
