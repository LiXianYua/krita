/* This file is part of the Krita project
 *
 * SPDX-FileCopyrightText: 2014 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <KisMimeDatabase.h>

#include <KoColor.h>
#include <KoColorProfile.h>
#include <KoColorSpaceEngine.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoDocumentInfo.h>
#include <KoUnit.h>
#include <KoID.h>
#include <KoProgressProxy.h>
#include <KoProgressUpdater.h>
#include <KoShape.h>
#include <KoShapeController.h>
#include <KoStore.h>
#include <KoUpdater.h>
#include <KoXmlWriter.h>
#include <KoStoreDevice.h>
#include <KisImportExportErrorCode.h>
#include <KoMD5Generator.h>
#include <KisResourceStorage.h>
#include <KisResourceLocator.h>
#include <KisResourceTypes.h>
#include <KisGlobalResourcesInterface.h>
#include <KisResourceLoaderRegistry.h>
#include <KisResourceModel.h>
#include <KisResourceModelProvider.h>
#include <KisResourceCacheDb.h>
#include <KoEmbeddedResource.h>
#include <KisUsageLogger.h>
#include <kis_debug.h>
#include <kis_generator_layer.h>
#include <kis_generator_registry.h>
#include <KisBackup.h>

#include <PkString.h>
#include <PkStringList.h>
#include <PkAuxTypes.h>
#include <PkImage.h>
#include <PkSize.h>
#include <PkRect.h>
#include <PkColor.h>
#include <PkXmlDocument.h>
#include <PkObject.h>
#include <PkQueue.h>
#include <PkMutex.h>
#include <PkDateTime.h>
#include <PkEventLoop.h>
#include <PkTimer.h>
#include <PkFileStream.h>
#include <PkMemoryStream.h>
#include <PkSharedPointer.h>
#include <PkScopedPointer.h>
#include <PkPointer.h>
#include <PkDebug.h>

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <random>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

// Krita Image
#include <kis_image_animation_interface.h>
#include <flake/kis_shape_layer.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_name_server.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <kis_selection.h>
#include <kis_fill_painter.h>
#include <kis_document_undo_store.h>
#include <kis_idle_watcher.h>
#include <kis_signal_auto_connection.h>
#include "kis_layer_utils.h"
#include "kis_selection_mask.h"

// Local
#include "flake/kis_shape_controller.h"
#include "KisDocument.h"
#include "KisDocumentApplicationServices.h"
#include "KisImportExportManager.h"
#include "kis_grid_config.h"
#include "kis_guides_config.h"
#include "KisImageBarrierLock.h"
#include "KisReferenceImagesLayer.h"
#include <kis_painting_assistant.h>

#include <mutex>
#include "kis_config_notifier.h"
#include "KisCloneDocumentStroke.h"

#include <kis_algebra_2d.h>
#include <KisMirrorAxisConfig.h>
#include <KisDecorationsWrapperLayer.h>
#include "kis_simple_stroke_strategy.h"

// Define the protocol used here for embedded documents' URL
// This used to "store" but the URL parser didn't like it,
// so let's simply make it "tar" !
#define STORE_PROTOCOL "tar"
// The internal path is a hack to make the URL parser happy and for document children
#define INTERNAL_PROTOCOL "intern"
#define INTERNAL_PREFIX "intern:/"
// Warning, keep it sync in koStore.cc

#include <unistd.h>

using namespace std;

namespace {
constexpr int errorMessageTimeout = 5000;
constexpr int successMessageTimeout = 1000;
}

// ---- Pk helpers (S-03-e Task 5; replaces Qt's file/dir/string/url helpers) ----
static PkString pkNumber(int v) { char buf[32]; snprintf(buf, sizeof(buf), "%d", v); return PkString(buf); }
static PkString pkNumber(long long v) { char buf[32]; snprintf(buf, sizeof(buf), "%lld", v); return PkString(buf); }
static PkString pkNumber(long v) { char buf[32]; snprintf(buf, sizeof(buf), "%ld", v); return PkString(buf); }
static PkString pkNumber(double v) { char buf[64]; snprintf(buf, sizeof(buf), "%g", v); return PkString(buf); }
static PkString pkFileName(const PkString &path) {
    std::string p = path.PkToUtf8();
    std::size_t pos = p.find_last_of('/');
    if (pos == std::string::npos) return path;
    return PkString(p.substr(pos + 1).c_str());
}
static PkString pkAbsolutePath(const PkString &path) {
    std::string p = path.PkToUtf8();
    std::size_t pos = p.find_last_of('/');
    if (pos == std::string::npos) return PkString(".");
    if (pos == 0) return PkString("/");
    return PkString(p.substr(0, pos).c_str());
}
static bool pkFileExists(const PkString &path) {
    struct stat st;
    return ::stat(path.PkToUtf8().c_str(), &st) == 0;
}
static bool pkIsWritable(const PkString &path) {
    return ::access(path.PkToUtf8().c_str(), W_OK) == 0;
}
static bool pkRemoveFile(const PkString &path) {
    return ::remove(path.PkToUtf8().c_str()) == 0;
}
static int64_t pkFileSize(const PkString &path) {
    struct stat st;
    if (::stat(path.PkToUtf8().c_str(), &st) != 0) return 0;
    return (int64_t)st.st_size;
}
static PkString pkTempDir() {
    const char *t = ::getenv("TMPDIR");
    return PkString(t && *t ? t : "/tmp");
}
static PkString pkHomeDir() {
    const char *h = ::getenv("HOME");
    return PkString(h ? h : "");
}
static bool pkMkdir(const PkString &path) {
    if (::mkdir(path.PkToUtf8().c_str(), 0755) == 0) return true;
    return errno == EEXIST;
}
static PkStringList pkSplitSkipEmpty(const PkString &s, char16_t sep) {
    PkStringList out;
    auto parts = s.split(sep);
    for (const auto &p : parts) {
        if (!p.isEmpty()) out << p;
    }
    return out;
}
static PkStringList pkConcat(const PkStringList &a, const PkStringList &b) {
    PkStringList out = a;
    for (const auto &p : b) out << p;
    return out;
}
static PkString pkCreateUuidString() {
    static std::random_device rd;
    char buf[64];
    snprintf(buf, sizeof(buf), "{8%03x-0000-0000-0000-%012llx}",
             (unsigned)(rd() & 0xfff), (unsigned long long)rd());
    return PkString(buf);
}
static PkImage pkTileImage(const PkImage &tile, int w, int h) {
    PkImage out(w, h, tile.format());
    const int tw = tile.width(), th = tile.height();
    const int bpp = tw > 0 ? (tile.bytesPerLine() / tw) : 4;
    if (bpp <= 0) return out;
    for (int y = 0; y < h; ++y) {
        const uint8_t *src = tile.scanLine(y % th);
        uint8_t *dst = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const int sx = x % tw;
            for (int b = 0; b < bpp; ++b) {
                dst[x * bpp + b] = src[sx * bpp + b];
            }
        }
    }
    return out;
}
static PkString pkFromByteArray(const PkByteArray &ba) {
    return PkString::PkFromUtf8(ba.constData(), ba.size());
}
static PkByteArray pkToByteArray(const PkString &s) {
    std::string u = s.PkToUtf8();
    return PkByteArray(u.data(), (int)u.size());
}



/**********************************************************
 *
 * KisDocument
 *
 **********************************************************/

// NOTE: declared non-static in the header
PkString KisDocument::newObjectName()
{
    static int s_docIFNumber = 0;
    PkString name = PkString("document_") + pkNumber(s_docIFNumber++);
    return name;
}


class UndoStack : public KUndo2Stack
{
public:
    UndoStack(KisDocument *doc)
        : KUndo2Stack(doc),
          m_doc(doc)
    {
    }

    void setIndex(int idx) override {
        m_postponedJobs.append({PostponedJob::SetIndex, idx});
        processPostponedJobs();
    }

    void notifySetIndexChangedOneCommand() override {
        KisImageWSP image = this->image();
        image->unlock();

        /**
         * Some very weird commands may blocking signals to
         * the GUI (e.g. KisGuiContextCommand). Here is the best thing
         * we can do to avoid the deadlock
         */
        while(!image->tryBarrierLock()) {
            PkEventLoop::processEvents();
        }
    }

    void undo() override {
        m_postponedJobs.append({PostponedJob::Undo, 0});
        processPostponedJobs();
    }


    void redo() override {
        m_postponedJobs.append({PostponedJob::Redo, 0});
        processPostponedJobs();
    }

private:
    KisImageWSP image() {
        KisImageWSP currentImage = m_doc->image();
        KIS_SAFE_ASSERT_RECOVER_NOOP(currentImage);
        return currentImage;
    }

    void setIndexImpl(int idx) {
        KisImageWSP image = this->image();
        image->requestStrokeCancellation();
        if(image->tryBarrierLock()) {
            KUndo2Stack::setIndex(idx);
            image->unlock();
        }
    }

    void undoImpl() {
        KisImageWSP image = this->image();
        image->requestUndoDuringStroke();

        if (image->tryUndoUnfinishedLod0Stroke() == UNDO_OK) {
            return;
        }

        if(image->tryBarrierLock()) {
            KUndo2Stack::undo();
            image->unlock();
        }
    }

    void redoImpl() {
        KisImageWSP image = this->image();
        image->requestRedoDuringStroke();

        if(image->tryBarrierLock()) {
            KUndo2Stack::redo();
            image->unlock();
        }
    }

    void processPostponedJobs() {
        /**
         * Some undo commands may call the host application event loop,
         * see notifySetIndexChangedOneCommand(). That may cause
         * recursive calls to the undo stack methods when used from
         * the Undo History docker. Here we try to handle that gracefully
         * by accumulating all the requests and executing them at the
         * topmost level of recursion.
         */
        if (m_recursionCounter > 0) return;

        m_recursionCounter++;

        while (!m_postponedJobs.isEmpty()) {
            PostponedJob job = m_postponedJobs.dequeue();
            switch (job.type) {
            case PostponedJob::SetIndex:
                setIndexImpl(job.index);
                break;
            case PostponedJob::Redo:
                redoImpl();
                break;
            case PostponedJob::Undo:
                undoImpl();
                break;
            }
        }

        m_recursionCounter--;
    }

private:
    int m_recursionCounter = 0;

    struct PostponedJob {
        enum Type {
            Undo = 0,
            Redo,
            SetIndex
        };
        Type type = Undo;
        int index = 0;
    };
    PkQueue<PostponedJob> m_postponedJobs;

    KisDocument *m_doc;
};

class KisDocument::Private
{
public:
    Private(KisDocument *_q)
        : q(_q)
        , docInfo(new KoDocumentInfo(_q)) // explicitly deleted in ~KisDocument (no parent-child teardown)
        , importExportManager(new KisImportExportManager(_q)) // deleted manually
        , autoSaveTimer(new PkTimer())
        , undoStack(new UndoStack(_q)) // explicitly deleted in ~KisDocument (no parent-child teardown)
        , m_bAutoDetectedMime(false)
        , modified(false)
        , readwrite(true)
        , autoSaveActive(true)
        , firstMod(PkDateTime::currentDateTime())
        , lastMod(firstMod)
        , nserver(new KisNameServer(1))
        , imageIdleWatcher(2000 /*ms*/)
        , globalAssistantsColor(KisDocumentApplicationServices::instance()->defaultAssistantsColor())
        , securityBookmarksEnabled(KisDocumentApplicationServices::instance()->securityBookmarksEnabled())
        , batchMode(false)
    {
        if (false) {
            unit = KoUnit::Inch;
        } else {
            unit = KoUnit::Centimeter;
        }
        PkObject::connect(&imageIdleWatcher, &KisIdleWatcher::startedIdleMode, q, &KisDocument::slotPerformIdleRoutines);
    }

    Private(const Private &rhs, KisDocument *_q)
        : q(_q)
        , docInfo(new KoDocumentInfo(*rhs.docInfo, _q))
        , importExportManager(new KisImportExportManager(_q))
        , autoSaveTimer(new PkTimer())
        , undoStack(new UndoStack(_q))
        , nserver(new KisNameServer(*rhs.nserver))
        , preActivatedNode(0) // the node is from another hierarchy!
        , imageIdleWatcher(2000 /*ms*/)
        , colorHistory(rhs.colorHistory)
    {
        copyFromImpl(rhs, _q, CONSTRUCT);
        PkObject::connect(&imageIdleWatcher, &KisIdleWatcher::startedIdleMode, q, &KisDocument::slotPerformIdleRoutines);
    }

    ~Private() {
        // Don't delete d->shapeController; it is owned by the shape controller hierarchy.
        delete nserver;
    }

    KisDocument *q = 0;
    KoDocumentInfo *docInfo = 0;

    KoUnit unit;

    KisImportExportManager *importExportManager = 0; // The filter-manager to use when loading/saving [for the options]

    PkByteArray mimeType; // The actual mimeType of the document
    PkByteArray outputMimeType; // The mimeType to use when saving

    PkTimer *autoSaveTimer;
    PkString lastErrorMessage; // see openFile()
    PkString lastWarningMessage;

    int autoSaveDelay = 300; // in seconds, 0 to disable.
    bool modifiedAfterAutosave = false;
    bool isAutosaving = false;
    bool disregardAutosaveFailure = false;
    int autoSaveFailureCount = 0;

    KUndo2Stack *undoStack = 0;

    KisGuidesConfig guidesConfig;
    KisMirrorAxisConfig mirrorAxisConfig;

    bool m_bAutoDetectedMime = false; // whether the mimeType in the arguments was detected by the part itself
    PkString m_path; // local url - the one displayed to the user.
    PkString m_file; // Local file - the only one the part implementation should deal with.

    PkMutex savingMutex;

    bool modified = false;
    bool readwrite = false;
    bool autoSaveActive = true;

    PkDateTime firstMod;
    PkDateTime lastMod;

    KisNameServer *nserver;

    KisImageSP image;
    KisImageSP savingImage;

    KisNodeWSP preActivatedNode;
    KisShapeController* shapeController = 0;
    KoShapeController* koShapeController = 0;
    KisIdleWatcher imageIdleWatcher;
    PkScopedPointer<KisSignalAutoConnection> imageIdleConnection;

    PkList<KisPaintingAssistantSP> assistants;

    StoryboardItemList m_storyboardItemList;
    PkVector<StoryboardComment> m_storyboardCommentList;

    PkVector<std::filesystem::path> audioTracks;
    qreal audioLevel = 1.0;

    PkColor globalAssistantsColor;
    bool securityBookmarksEnabled = false;
    PkList<KoColor> colorHistory;

    KisGridConfig gridConfig;

    bool imageModifiedWithoutUndo = false;
    bool modifiedWhileSaving = false;
    std::unique_ptr<KisDocument> backgroundSaveDocument;
    PkPointer<KoUpdater> savingUpdater;
    std::future<KisImportExportErrorCode> childSavingFuture;
    std::unique_ptr<PkTimer> savingWatchTimer;
    KritaUtils::ExportFileJob backgroundSaveJob;
    SavingCompletedCallback completeSavingCallback;
    KisSignalAutoConnectionsStore referenceLayerConnections;

    bool isRecovered = false;

    bool batchMode { false };
    bool decorationsSyncingDisabled = false;
    bool wasStorageAdded = false;
    bool documentIsClosing = false;

    // Resources saved in the .kra document
    PkString linkedResourcesStorageID;
    KisResourceStorageSP linkedResourceStorage;

    // Resources saved into other components of the kra file
    PkString embeddedResourcesStorageID;
    KisResourceStorageSP embeddedResourceStorage;

    void syncDecorationsWrapperLayerState();

    void setImageAndInitIdleWatcher(KisImageSP _image) {
        image = _image;

        imageIdleWatcher.setTrackedImage(image);
    }

    void copyFrom(const Private &rhs, KisDocument *q);
    void copyFromImpl(const Private &rhs, KisDocument *q, KisDocument::CopyPolicy policy);

    void uploadLinkedResourcesFromLayersToStorage();
    KisDocument* lockAndCloneImpl(bool fetchResourcesFromLayers);

    void updateDocumentMetadataOnSaving(const PkString &filePath, const PkByteArray &mimeType);

    /// clones the palette list oldList
    /// the ownership of the returned KoColorSet * belongs to the caller
    class StrippedSafeSavingLocker;
};


void KisDocument::Private::syncDecorationsWrapperLayerState()
{
    if (!this->image || this->decorationsSyncingDisabled) return;

    KisImageSP image = this->image;
    KisDecorationsWrapperLayerSP decorationsLayer =
            KisLayerUtils::findNodeByType<KisDecorationsWrapperLayer>(image->root());

    const bool needsDecorationsWrapper =
            gridConfig.showGrid() || (guidesConfig.showGuides() && guidesConfig.hasGuides()) || !assistants.isEmpty();

    struct SyncDecorationsWrapperStroke : public KisSimpleStrokeStrategy {
        SyncDecorationsWrapperStroke(KisDocument *document, bool needsDecorationsWrapper)
            : KisSimpleStrokeStrategy(PkString("sync-decorations-wrapper"),
                                      kundo2_text_raw("start-isolated-mode")),
              m_document(document),
              m_needsDecorationsWrapper(needsDecorationsWrapper)
        {
            this->enableJob(JOB_INIT, true, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
            setClearsRedoOnStart(false);
            setRequestsOtherStrokesToEnd(false);
        }

        void initStrokeCallback() override {
            KisDecorationsWrapperLayerSP decorationsLayer =
                    KisLayerUtils::findNodeByType<KisDecorationsWrapperLayer>(m_document->image()->root());

            if (m_needsDecorationsWrapper && !decorationsLayer) {
                m_document->image()->addNode(new KisDecorationsWrapperLayer(m_document));
            } else if (!m_needsDecorationsWrapper && decorationsLayer) {
                m_document->image()->removeNode(decorationsLayer);
            }
        }

    private:
        KisDocument *m_document = 0;
        bool m_needsDecorationsWrapper = false;
    };

    KisStrokeId id = image->startStroke(new SyncDecorationsWrapperStroke(q, needsDecorationsWrapper));
    image->endStroke(id);
}

void KisDocument::Private::copyFrom(const Private &rhs, KisDocument *q)
{
    copyFromImpl(rhs, q, KisDocument::REPLACE);
}

void KisDocument::Private::copyFromImpl(const Private &rhs, KisDocument *q, KisDocument::CopyPolicy policy)
{
    if (policy == REPLACE) {
        delete docInfo;
    }
    docInfo = (new KoDocumentInfo(*rhs.docInfo, q));
    unit = rhs.unit;
    mimeType = rhs.mimeType;
    outputMimeType = rhs.outputMimeType;

    if (policy == REPLACE) {
        q->setGuidesConfig(rhs.guidesConfig);
        q->setMirrorAxisConfig(rhs.mirrorAxisConfig);
        q->setModified(rhs.modified);
        q->setAssistants(KisPaintingAssistant::cloneAssistantList(rhs.assistants));
        q->setStoryboardItemList(StoryboardItem::cloneStoryboardItemList(rhs.m_storyboardItemList));
        q->setStoryboardCommentList(rhs.m_storyboardCommentList);
        q->setAudioTracks(rhs.audioTracks);
        q->setAudioVolume(rhs.audioLevel);
        q->setGridConfig(rhs.gridConfig);
    } else {
        // in CONSTRUCT mode, we cannot use the functions of KisDocument
        // because KisDocument does not yet have a pointer to us.
        guidesConfig = rhs.guidesConfig;
        mirrorAxisConfig = rhs.mirrorAxisConfig;
        modified = rhs.modified;
        assistants = KisPaintingAssistant::cloneAssistantList(rhs.assistants);
        m_storyboardItemList = StoryboardItem::cloneStoryboardItemList(rhs.m_storyboardItemList);
        m_storyboardCommentList = rhs.m_storyboardCommentList;
        audioTracks = rhs.audioTracks;
        audioLevel = rhs.audioLevel;
        gridConfig = rhs.gridConfig;
    }
    imageModifiedWithoutUndo = rhs.imageModifiedWithoutUndo;
    m_bAutoDetectedMime = rhs.m_bAutoDetectedMime;
    m_path = rhs.m_path;
    m_file = rhs.m_file;
    readwrite = rhs.readwrite;
    autoSaveActive = rhs.autoSaveActive;
    firstMod = rhs.firstMod;
    lastMod = rhs.lastMod;
    // XXX: the display properties will be shared between different snapshots
    globalAssistantsColor = rhs.globalAssistantsColor;
    securityBookmarksEnabled = rhs.securityBookmarksEnabled;
    batchMode = rhs.batchMode;


    if (rhs.linkedResourceStorage) {
        linkedResourceStorage = rhs.linkedResourceStorage->clone();
    }

    if (rhs.embeddedResourceStorage) {
        embeddedResourceStorage = rhs.embeddedResourceStorage->clone();
    }

}

class KisDocument::Private::StrippedSafeSavingLocker {
public:
    StrippedSafeSavingLocker(PkMutex *savingMutex, KisImageSP image)
        : m_locked(false)
        , m_image(image)
        , m_savingLock(savingMutex)
        , m_imageLock(image, std::defer_lock)

    {
        /**
         * Initial try to lock both objects. Locking the image guards
         * us from any image composition threads running in the
         * background, while savingMutex guards us from entering the
         * saving code twice by autosave and main threads.
         *
         * Since we are trying to lock multiple objects, so we should
         * do it in a safe manner.
         */
        m_locked = std::try_lock(m_imageLock, *m_savingLock) < 0;

        if (!m_locked) {
            m_image->requestStrokeEnd();
            PkEventLoop::processEvents();

            // one more try...
            m_locked = std::try_lock(m_imageLock, *m_savingLock) < 0;
        }
    }

    ~StrippedSafeSavingLocker() {
        if (m_locked) {
            m_imageLock.unlock();
            m_savingLock->unlock();
        }
    }

    bool successfullyLocked() const {
        return m_locked;
    }

private:
    StrippedSafeSavingLocker(const StrippedSafeSavingLocker &) = delete;
    StrippedSafeSavingLocker &operator=(const StrippedSafeSavingLocker &) = delete;
    StrippedSafeSavingLocker(StrippedSafeSavingLocker &&) = delete;
    StrippedSafeSavingLocker &operator=(StrippedSafeSavingLocker &&) = delete;

    bool m_locked;
    KisImageSP m_image;
    PkMutex *m_savingLock;
    KisImageReadOnlyBarrierLock m_imageLock;
};

KisDocument::KisDocument(bool addStorage)
    : d(new Private(this))
{
    PkObject::connect(KisConfigNotifier::instance(), &KisConfigNotifier::configChanged, this, &KisDocument::slotConfigChanged);
    PkObject::connect(d->undoStack, &KUndo2Stack::cleanChanged, this, &KisDocument::slotUndoStackCleanChanged);
    setObjectName(newObjectName());

    if (addStorage) {
        d->linkedResourcesStorageID = pkCreateUuidString();
        d->linkedResourceStorage.reset(new KisResourceStorage(d->linkedResourcesStorageID));
        KisResourceLocator::instance()->addStorage(d->linkedResourcesStorageID, d->linkedResourceStorage);

        d->embeddedResourcesStorageID = pkCreateUuidString();
        d->embeddedResourceStorage.reset(new KisResourceStorage(d->embeddedResourcesStorageID));
        KisResourceLocator::instance()->addStorage(d->embeddedResourcesStorageID, d->embeddedResourceStorage);

        d->wasStorageAdded = true;
    }

    d->shapeController = new KisShapeController(d->nserver, d->undoStack, this);
    d->koShapeController = new KoShapeController(0, d->shapeController);

    slotConfigChanged();
}

KisDocument::KisDocument(const KisDocument &rhs, bool addStorage)
    : PkObject(),
      d(new Private(*rhs.d, this))
{
    copyFromDocumentImpl(rhs, CONSTRUCT);

    if (addStorage) {
        KisResourceLocator::instance()->addStorage(d->linkedResourcesStorageID, d->linkedResourceStorage);
        KisResourceLocator::instance()->addStorage(d->embeddedResourcesStorageID, d->embeddedResourceStorage);
        d->wasStorageAdded = true;
    }
}

KisDocument::~KisDocument()
{
    d->documentIsClosing = true;

    // wait until all the pending operations are in progress
    waitForSavingToComplete();
    d->imageIdleWatcher.setTrackedImage(0);

    /**
     * Push a timebomb, which will try to release the memory after
     * the document has been deleted
     */
    KisPaintDevice::createMemoryReleaseObject()->deleteLater();

    d->autoSaveTimer->stop();
    d->autoSaveTimer->stop();

    delete d->importExportManager;

    // Despite being PkObject they need to be deleted before the image
    delete d->shapeController;

    delete d->koShapeController;

    if (d->image) {
        d->image->animationInterface()->blockBackgroundFrameGeneration();

        d->image->notifyAboutToBeDeleted();

        /**
         * WARNING: We should wait for all the internal image jobs to
         * finish before entering KisImage's destructor. The problem is,
         * while execution of KisImage::~KisImage, all the weak shared
         * pointers pointing to the image enter an inconsistent
         * state(!). The shared counter is already zero and destruction
         * has started, but the weak reference doesn't know about it,
         * because KisShared::~KisShared hasn't been executed yet. So all
         * the threads running in background and having weak pointers will
         * enter the KisImage's destructor as well.
         */

        d->image->requestStrokeCancellation();
        d->image->waitForDone();

        // clear undo commands that can still point to the image
        d->undoStack->clear();
        d->image->waitForDone();

        KisImageWSP sanityCheckPointer = d->image;
        (void)sanityCheckPointer;;

        // The following line trigger the deletion of the image
        d->image.clear();

        // check if the image has actually been deleted
        KIS_SAFE_ASSERT_RECOVER_NOOP(!sanityCheckPointer.isValid());
    }

    if (d->wasStorageAdded) {
        if (KisResourceLocator::instance()->hasStorage(d->linkedResourcesStorageID)) {
            KisResourceLocator::instance()->removeStorage(d->linkedResourcesStorageID);
        }
        if (KisResourceLocator::instance()->hasStorage(d->embeddedResourcesStorageID)) {
            KisResourceLocator::instance()->removeStorage(d->embeddedResourcesStorageID);
        }
    }

    delete d;
}

PkString KisDocument::embeddedResourcesStorageId() const
{
    return d->embeddedResourcesStorageID;
}

PkString KisDocument::linkedResourcesStorageId() const
{
    return d->linkedResourcesStorageID;
}

KisDocument *KisDocument::clone(bool addStorage)
{
    return new KisDocument(*this, addStorage);
}

bool KisDocument::exportDocumentImpl(const KritaUtils::ExportFileJob &job, KisPropertiesConfigurationSP exportConfiguration, bool isAdvancedExporting)
{
    // ANDROID NOTES (other comments in this file reference this one!)
    //
    // The Android file system doesn't work like on a real operating system.
    // Instead of normal file paths, we get to deal with "content URIs", which
    // can have various kinds of storage providers behind them. For example,
    // there's a "normal" storage provider, a slightly less normal documents
    // provider, a Google Drive provider and various kinds of third-party
    // providers that are mostly just good at losing the data you give them.
    // For example, we have reports of compression programs that provide a
    // storage provider to write to ZIP or RAR archives or something. Except
    // that they don't seem to work at all, they just accept the data with no
    // error and throw it on the floor.
    //
    // The providers are particularly unreliable with regards to permissions,
    // so calling "isWritable" will just always return false on some of them.
    // This includes the default provider on some devices, which means checking
    // whether a file is writable will mean that the user can't save anything!
    // So, all the Android code skips over the writability check and assumes the
    // files are writable. If they're not, we'll notice later anyway, by the
    // fact that writing to them fails.
    //
    // Another issue is that it's only possible to overwrite files using File >
    // Save. Using File > Save As or File > Export can't replace existing
    // files. The reason for this is that we have to go through the operating
    // system to request access to a file and the only things you can ask for
    // is to open an existing file or to create a new file. Out of necessity,
    // Save As and Export use the latter. If the user selects an existing file,
    // the operating system "helpfully" appends a number to the path, *after*
    // the file extension of course, because it hates the living. So that's
    // another reason we can't go with the "safer" option of assuming that files
    // aren't writable in some cases, since that would prevent the user from
    // saving them normally and end up with a lot of "kiki.kra (2)".
    //
    // Also, when you request a file from the operating system, it always
    // creates an empty file. That means checking whether a file exists will
    // pretty much always succeed, so to know whether a file actually exists
    // you have to check whether it isn't empty. Of course providers may fail to
    // implement this correctly, but I'm not aware of any of them botching it
    // that hard. Well, at least none of the ones that actually save files, as
    // mentioned above some of them just seem to lose whatever you give them.

    PkString filePathInfo = job.filePath;
    bool fileExists = pkFileExists(filePathInfo);
#ifdef Q_OS_ANDROID
    if (fileExists) {
        fileExists = pkFileSize(filePathInfo) > 0;
    }
#else
    if (fileExists && !pkIsWritable(filePathInfo)) {
        slotCompleteSavingDocument(job, ImportExportCodes::NoAccessToWrite,
                                   PkString("%1 cannot be written to. Please save under a different name.").arg(job.filePath),
                                   "");
        return false;
    }
#endif

    KisDocumentApplicationServices *services = KisDocumentApplicationServices::instance();
    if (services->backupFileEnabled() && fileExists) {

        PkString backupDir;

        switch (services->backupFileLocation()) {
        case 1:
            backupDir = pkHomeDir();
            break;
        case 2:
            backupDir = pkTempDir();
            break;
        default:
#ifdef Q_OS_ANDROID
            // We deal with URIs, there may or may not be a "directory"
            backupDir = services->autoSaveLocation();
            pkMkdir(backupDir);
#endif

#ifdef Q_OS_MACOS
            if (services->securityBookmarksEnabled()) {
                // If the user does not have directory permission force backup
                // files to be inside Container tmp
                PkString fileUrl = job.filePath;
                if (!services->parentDirectoryHasPermissions(fileUrl)) {
                    backupDir = pkTempDir();
                }
            }
#endif

            // Do nothing: the empty string is user file location
            break;
        }

        const int numOfBackupsKept = services->numberOfBackupFiles();
        const PkString suffix = services->backupFileSuffix();

        if (numOfBackupsKept == 1) {
            if (!KisBackup::simpleBackupFile(job.filePath, backupDir, suffix)) {
                qWarning() << "Failed to create simple backup file!" << job.filePath << backupDir << suffix;
                KisUsageLogger::log(PkString("Failed to create a simple backup for %1 in %2.")
                                        .arg(job.filePath, backupDir.isEmpty()
                                                               ? "the same location as the file"
                                                               : backupDir));
                slotCompleteSavingDocument(job, ImportExportCodes::ErrorWhileWriting, PkString("Failed to create a backup file"), "");
                return false;
            }
            else {
                KisUsageLogger::log(PkString("Create a simple backup for %1 in %2.")
                                        .arg(job.filePath, backupDir.isEmpty()
                                                               ? "the same location as the file"
                                                               : backupDir));
            }
        }
        else if (numOfBackupsKept > 1) {
            if (!KisBackup::numberedBackupFile(job.filePath, backupDir, suffix, numOfBackupsKept)) {
                qWarning() << "Failed to create numbered backup file!" << job.filePath << backupDir << suffix;
                KisUsageLogger::log(PkString("Failed to create a numbered backup for %2.")
                                        .arg(job.filePath, backupDir.isEmpty()
                                                               ? "the same location as the file"
                                                               : backupDir));
                slotCompleteSavingDocument(job, ImportExportCodes::ErrorWhileWriting, PkString("Failed to create a numbered backup file"), "");
                return false;
            }
            else {
                KisUsageLogger::log(PkString("Create a simple backup for %1 in %2.")
                                        .arg(job.filePath, backupDir.isEmpty()
                                                               ? "the same location as the file"
                                                               : backupDir));
            }
        }
    }

    //KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!job.mimeType.isEmpty(), false);
    if (job.mimeType.isEmpty()) {
        KisImportExportErrorCode error = ImportExportCodes::FileFormatNotSupported;
        slotCompleteSavingDocument(job, error, error.errorMessage(), "");
        return false;

    }

    const PkString actionName =
            job.flags & KritaUtils::SaveIsExporting ?
                PkString("Exporting Document...") :
                PkString("Saving Document...");

    KritaUtils::BackgroudSavingStartResult result =
            initiateSavingInBackground(actionName,
                                       [this](const KritaUtils::ExportFileJob &job, KisImportExportErrorCode status, const PkString &errorMessage, const PkString &warningMessage) { slotCompleteSavingDocument(job, status, errorMessage, warningMessage); },
                                       job, exportConfiguration, isAdvancedExporting);

    if (result != KritaUtils::BackgroudSavingStartResult::Success) {
        PkString errorShortLog;
        PkString errorMessage;
        ImportExportCodes::ErrorCodeID errorCode = ImportExportCodes::Failure;

        switch (result) {
        case KritaUtils::BackgroudSavingStartResult::AnotherSavingInProgress:
            errorShortLog = "another save operation is in progress";
            errorMessage = PkString("Could not start saving %1. Wait until the current save operation has finished.").arg(job.filePath);
            errorCode = ImportExportCodes::Failure;
            break;
        case KritaUtils::BackgroudSavingStartResult::ImageLockFailure:
            errorShortLog = "failed to lock and clone the image";
            errorMessage = PkString("Could not start saving %1. Image is busy").arg(job.filePath);
            errorCode = ImportExportCodes::Busy;
            break;
        case KritaUtils::BackgroudSavingStartResult::Failure:
            errorShortLog = "failed to start background saving";
            errorMessage = PkString("Could not start saving %1. Unknown failure has happened").arg(job.filePath);
            errorCode = ImportExportCodes::Failure;
            break;
        case KritaUtils::BackgroudSavingStartResult::Cancelled:
            errorCode = ImportExportCodes::Cancelled;
            break;
        case KritaUtils::BackgroudSavingStartResult::Success:
            // noop, not possible
            break;
        }

        KisUsageLogger::log(PkString("Failed to initiate saving %1 in background: %2").arg(job.filePath).arg(errorShortLog));

        slotCompleteSavingDocument(job, errorCode,
                                   errorMessage,
                                   "");
        return false;
    }

    return (result == KritaUtils::BackgroudSavingStartResult::Success);
}

bool KisDocument::exportDocument(const PkString &path, const PkByteArray &mimeType, bool isAdvancedExporting, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration)
{
    using namespace KritaUtils;

    SaveFlags flags = SaveIsExporting;
    if (showWarnings) {
        flags |= SaveShowWarnings;
    }

    KisUsageLogger::log(PkString("Exporting Document: %1 as %2. %3 * %4 pixels, %5 layers, %6 frames, %7 "
                                "framerate. Export configuration: %8")
                            .arg(path)
                            .arg(pkFromByteArray(mimeType))
                            .arg(pkNumber(d->image->width()))
                            .arg(pkNumber(d->image->height()))
                            .arg(pkNumber(d->image->nlayers()))
                            .arg(pkNumber(d->image->animationInterface()->totalLength()))
                            .arg(pkNumber(d->image->animationInterface()->framerate()))
                            .arg(exportConfiguration ? exportConfiguration->toXML() : PkString("No configuration")));

    return exportDocumentImpl(KritaUtils::ExportFileJob(path,
                                                        mimeType,
                                                        flags),
                              exportConfiguration, isAdvancedExporting);
}

bool KisDocument::saveAs(const PkString &_path, const PkByteArray &mimeType, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration)
{
    using namespace KritaUtils;

    KisUsageLogger::log(PkString("Saving Document %9 as %1 (mime: %2). %3 * %4 pixels, %5 layers.  %6 frames, "
                                "%7 framerate. Export configuration: %8")
                            .arg(_path)
                            .arg(pkFromByteArray(mimeType))
                            .arg(pkNumber(d->image->width()))
                            .arg(pkNumber(d->image->height()))
                            .arg(pkNumber(d->image->nlayers()))
                            .arg(pkNumber(d->image->animationInterface()->totalLength()))
                            .arg(pkNumber(d->image->animationInterface()->framerate()))
                            .arg(exportConfiguration ? exportConfiguration->toXML() : PkString("No configuration"))
                            .arg(path()));

    // Check whether it's an existing resource were are saving to
    if (resourceSavingFilter(_path, mimeType, exportConfiguration)) {
        return true;
    }

    return exportDocumentImpl(ExportFileJob(_path,
                                            mimeType,
                                            showWarnings ? SaveShowWarnings : SaveNone),
                              exportConfiguration);
}

bool KisDocument::save(bool showWarnings, KisPropertiesConfigurationSP exportConfiguration)
{
    return saveAs(path(), mimeType(), showWarnings, exportConfiguration);
}

PkByteArray KisDocument::serializeToNativeByteArray()
{
    PkMemoryStream buffer;

    PkScopedPointer<KisImportExportFilter> filter(KisImportExportManager::filterForMimeType(pkFromByteArray(nativeFormatMimeType()), KisImportExportManager::Export));
    filter->setBatchMode(true);
    filter->setMimeType(pkFromByteArray(nativeFormatMimeType()));

    Private::StrippedSafeSavingLocker locker(&d->savingMutex, d->image);
    if (!locker.successfullyLocked()) {
        return PkByteArray(buffer.data(), (int)buffer.size());
    }

    d->savingImage = d->image;

    if (!filter->convert(this, &buffer).isOk()) {
        qWarning() << "serializeToByteArray():: Could not export to our native format";
    }

    return PkByteArray(buffer.data(), (int)buffer.size());
}

void KisDocument::slotCompleteSavingDocument(const KritaUtils::ExportFileJob &job, KisImportExportErrorCode status, const PkString &errorMessage, const PkString &warningMessage)
{
    if (status.isCancelled())
        return;

    const PkString fileName = pkFileName(job.filePath);

    if (!status.isOk()) {
        statusBarMessage(PkString("Error during saving %1: %2").arg(fileName).arg(errorMessage), errorMessageTimeout);


        if (!fileBatchMode()) {
            KisDocumentApplicationServices::instance()->showDocumentMessage({
                PkString("Krita"),
                PkString("Could not save %1.").arg(job.filePath),
                pkConcat(pkSplitSkipEmpty(errorMessage, u'\n'), pkSplitSkipEmpty(warningMessage, u'\n')),
                status.errorMessage()});
        }
    }
    else {
        if (!fileBatchMode() && !warningMessage.isEmpty()) {

            PkStringList reasons = pkSplitSkipEmpty(warningMessage, u'\n');

            KisDocumentApplicationServices::instance()->showDocumentMessage({
                PkString("Krita"),
                PkString("%1 has been saved but is incomplete.").arg(job.filePath),
                reasons,
                reasons.isEmpty()
                    ? PkString()
                    : PkString("Some problems were encountered when saving.")});
        }


        if (!(job.flags & KritaUtils::SaveIsExporting)) {
            const PkString existingAutoSaveBaseName = localFilePath();
            const bool wasRecovered = isRecovered();

            d->updateDocumentMetadataOnSaving(job.filePath, job.mimeType);

            removeAutoSaveFiles(existingAutoSaveBaseName, wasRecovered);
        }

        completed();
        sigSavingFinished(job.filePath);

        KisDocumentApplicationServices *services = KisDocumentApplicationServices::instance();
        if (d->securityBookmarksEnabled) {
            services->createSavedFileBookmark(job.filePath);
        }

        statusBarMessage(PkString("Finished saving %1").arg(fileName), successMessageTimeout);
    }
}

void KisDocument::Private::updateDocumentMetadataOnSaving(const PkString &filePath, const PkByteArray &mimeType)
{
    q->setPath(filePath);
    q->setLocalFilePath(filePath);
    q->setMimeType(mimeType);
    q->updateEditingTime(true);

#ifdef Q_OS_ANDROID
    // See the comment titled "ANDROID NOTES" in this file for an explanation of
    // what this is about. (This is not that comment.)
    q->setReadWrite(true);
#else
    PkString fi = filePath;
    q->setReadWrite(pkIsWritable(fi));
#endif

    if (!modifiedWhileSaving) {
        /**
         * If undo stack is already clean/empty, it doesn't any
         * signals, so we might forget update document modified state
         * (which was set, e.g. while recovering an autosave file)
         */

        if (undoStack->isClean()) {
            q->setModified(false);
        } else {
            imageModifiedWithoutUndo = false;
            undoStack->setClean();
        }
    }
    q->setRecovered(false);
}

PkByteArray KisDocument::mimeType() const
{
    return d->mimeType;
}

void KisDocument::setMimeType(const PkByteArray & mimeType)
{
    d->mimeType = mimeType;
}

bool KisDocument::fileBatchMode() const
{
    return d->batchMode;
}

void KisDocument::setFileBatchMode(const bool batchMode)
{
    d->batchMode = batchMode;
}

void KisDocument::Private::uploadLinkedResourcesFromLayersToStorage()
{
    /// Fetch resources from KisAdjustmentLayer, KisFilterMask and
    /// KisGeneratorLayer and put them into the cloned storage. This must be
    /// done in the context of the GUI thread, otherwise we will not be able to
    /// access resources database

    KisDocument *doc = q;

    KisLayerUtils::recursiveApplyNodes(doc->image()->root(),
        [doc] (KisNodeSP node) {
            if (KisNodeFilterInterface *layer = dynamic_cast<KisNodeFilterInterface*>(node.data())) {
                KisFilterConfigurationSP filterConfig = layer->filter();
                if (!filterConfig) return;

                PkList<KoResourceLoadResult> linkedResources = filterConfig->linkedResources(KisGlobalResourcesInterface::instance());

                for (const KoResourceLoadResult &result : linkedResources) {
                    KIS_SAFE_ASSERT_RECOVER(result.type() != KoResourceLoadResult::EmbeddedResource) { continue; }

                    KoResourceSP resource = result.resource();

                    if (!resource) {
                        qWarning() << "WARNING: KisDocument::lockAndCloneForSaving failed to fetch a resource" << result.signature();
                        continue;
                    }

                    PkMemoryStream buf;
                    buf.open(PkStream::WriteOnly);

                    KisResourceModel model(resource->resourceType().first);
                    bool res = model.exportResource(resource, &buf);

                    buf.close();

                    if (!res) {
                        qWarning() << "WARNING: KisDocument::lockAndCloneForSaving failed to export resource" << result.signature();
                        continue;
                    }

                    buf.open(PkStream::ReadOnly);

                    res = doc->d->linkedResourceStorage->importResource(resource->resourceType().first + "/" + resource->filename(), &buf);

                    buf.close();

                    if (!res) {
                        qWarning() << "WARNING: KisDocument::lockAndCloneForSaving failed to import resource" << result.signature();
                        continue;
                    }
                }

            }
    });
}

KisDocument *KisDocument::Private::lockAndCloneImpl(bool fetchResourcesFromLayers)
{
    // force update of all the asynchronous nodes before cloning
    PkEventLoop::processEvents();
    KisLayerUtils::forceAllDelayedNodesUpdate(image->root());

    if (!KisDocumentApplicationServices::instance()->waitForImage(
            image, KisDocumentApplicationServices::WaitMode::Cancellable)) {
        return nullptr;
    }

    Private::StrippedSafeSavingLocker locker(&savingMutex, image);
    if (!locker.successfullyLocked()) {
        return 0;
    }

    KisDocument *doc = new KisDocument(*this->q, false);

    if (fetchResourcesFromLayers) {
        doc->d->uploadLinkedResourcesFromLayersToStorage();
    }

    return doc;
}

KisDocument* KisDocument::lockAndCloneForSaving()
{
    return d->lockAndCloneImpl(true);
}

KisDocument *KisDocument::lockAndCreateSnapshot()
{
    return d->lockAndCloneImpl(false);
}

void KisDocument::copyFromDocument(const KisDocument &rhs)
{
    copyFromDocumentImpl(rhs, REPLACE);
}

void KisDocument::copyFromDocumentImpl(const KisDocument &rhs, CopyPolicy policy)
{
    if (policy == REPLACE) {
        d->decorationsSyncingDisabled = true;
        d->copyFrom(*(rhs.d), this);
        d->decorationsSyncingDisabled = false;

        d->undoStack->clear();
    } else {
        // in CONSTRUCT mode, d should be already initialized
        PkObject::connect(KisConfigNotifier::instance(), &KisConfigNotifier::configChanged, this, &KisDocument::slotConfigChanged);
        PkObject::connect(d->undoStack, &KUndo2Stack::cleanChanged, this, &KisDocument::slotUndoStackCleanChanged);
    
        d->shapeController = new KisShapeController(d->nserver, d->undoStack, this);
        d->koShapeController = new KoShapeController(0, d->shapeController);
    }

    setObjectName(rhs.objectName());

    slotConfigChanged();

    if (rhs.d->image) {
        if (policy == REPLACE) {
            d->image->barrierLock(/* readOnly = */ false);
            rhs.d->image->barrierLock(/* readOnly = */ true);
            d->image->copyFromImage(*(rhs.d->image));
            d->image->unlock();
            rhs.d->image->unlock();

            setCurrentImage(d->image, /* forceInitialUpdate = */ true);
        } else {
            // clone the image with keeping the GUIDs of the layers intact
            // NOTE: we expect the image to be locked!
            setCurrentImage(rhs.image()->clone(/* exactCopy = */ true), /* forceInitialUpdate = */ false);
        }
    }

    if (policy == REPLACE) {
        d->syncDecorationsWrapperLayerState();
    }

    if (rhs.d->preActivatedNode) {
        PkQueue<KisNodeSP> linearizedNodes;
        KisLayerUtils::recursiveApplyNodes(rhs.d->image->root(),
                                           [&linearizedNodes](KisNodeSP node) {
            linearizedNodes.enqueue(node);
        });
        KisLayerUtils::recursiveApplyNodes(d->image->root(),
                                           [&linearizedNodes, &rhs, this](KisNodeSP node) {
            KisNodeSP refNode = linearizedNodes.dequeue();
            if (rhs.d->preActivatedNode.data() == refNode.data()) {
                d->preActivatedNode = node;
            }
        });
    }

    // reinitialize references' signal connection
    KisReferenceImagesLayerSP referencesLayer = this->referenceImagesLayer();
    if (referencesLayer) {
        d->referenceLayerConnections.clear();
        d->referenceLayerConnections.addConnection(
                    referencesLayer.data(), &KisReferenceImagesLayer::sigUpdateCanvas,
                    this, &KisDocument::sigReferenceImagesChanged);

        sigReferenceImagesLayerChanged(referencesLayer);
        sigReferenceImagesChanged();
    }

    KisDecorationsWrapperLayerSP decorationsLayer =
            KisLayerUtils::findNodeByType<KisDecorationsWrapperLayer>(d->image->root());
    if (decorationsLayer) {
        decorationsLayer->setDocument(this);
    }


    if (policy == REPLACE) {
        setModified(true);
    }
}

bool KisDocument::exportDocumentSync(const PkString &path, const PkByteArray &mimeType, KisPropertiesConfigurationSP exportConfiguration)
{
    {
        /**
         * The caller guarantees that no one else uses the document (usually,
         * it is a temporary document created specifically for exporting), so
         * we don't need to copy or lock the document. Instead we should just
         * ensure the barrier lock is synced and then released.
         */
        Private::StrippedSafeSavingLocker locker(&d->savingMutex, d->image);
        if (!locker.successfullyLocked()) {
            return false;
        }
    }

    d->savingImage = d->image;

    KisImportExportErrorCode status =
            d->importExportManager->
            exportDocument(path, path, mimeType, false, exportConfiguration);

    d->savingImage = 0;

    return status.isOk();
}


KritaUtils::BackgroudSavingStartResult KisDocument::initiateSavingInBackground(const PkString actionName,
                                             SavingCompletedCallback completedCallback,
                                             const KritaUtils::ExportFileJob &job,
                                             KisPropertiesConfigurationSP exportConfiguration,bool isAdvancedExporting)
{
    return initiateSavingInBackground(actionName, std::move(completedCallback),
                                      job, exportConfiguration, std::unique_ptr<KisDocument>(), isAdvancedExporting);
}

KritaUtils::BackgroudSavingStartResult KisDocument::initiateSavingInBackground(const PkString actionName,
                                             SavingCompletedCallback completedCallback,
                                             const KritaUtils::ExportFileJob &job,
                                             KisPropertiesConfigurationSP exportConfiguration,
                                             std::unique_ptr<KisDocument> &&optionalClonedDocument,bool isAdvancedExporting)
{
    KIS_ASSERT_RECOVER_RETURN_VALUE(job.isValid(), KritaUtils::BackgroudSavingStartResult::Failure);

    std::unique_ptr<KisDocument> clonedDocument;

    if (!optionalClonedDocument) {
        clonedDocument.reset(lockAndCloneForSaving());
    } else {
        clonedDocument.reset(optionalClonedDocument.release());
    }

    if (!d->savingMutex.tryLock()){
        return KritaUtils::BackgroudSavingStartResult::AnotherSavingInProgress;
    }

    /**
     * This lock will later release() when we start the background thread,
     * it means that the ownership is transferred to the background thread
     */
    std::unique_lock<PkMutex> savingMutexLock(d->savingMutex, std::adopt_lock);

    if (!clonedDocument) {
        return KritaUtils::BackgroudSavingStartResult::ImageLockFailure;
    }

    auto waitForImage = [] (KisImageSP image) {
        KisDocumentApplicationServices::instance()->waitForImage(
            image, KisDocumentApplicationServices::WaitMode::Forced);
    };

    {
        KisNodeSP newRoot = clonedDocument->image()->root();
        KIS_SAFE_ASSERT_RECOVER(!KisLayerUtils::hasDelayedNodeWithUpdates(newRoot)) {
            KisLayerUtils::forceAllDelayedNodesUpdate(newRoot);
            waitForImage(clonedDocument->image());
        }
    }

    if (clonedDocument->image()->hasOverlaySelectionMask()) {
        clonedDocument->image()->setOverlaySelectionMask(0);
        waitForImage(clonedDocument->image());
    }

    if (KisDocumentApplicationServices::instance()->trimKra()) {
        clonedDocument->image()->cropImage(clonedDocument->image()->bounds());
        clonedDocument->image()->purgeUnusedData(false);
        waitForImage(clonedDocument->image());
    }

    KIS_SAFE_ASSERT_RECOVER(clonedDocument->image()->isIdle()) {
        waitForImage(clonedDocument->image());
    }

    KIS_ASSERT_RECOVER_RETURN_VALUE(!d->backgroundSaveDocument, KritaUtils::BackgroudSavingStartResult::Failure);
    KIS_ASSERT_RECOVER_RETURN_VALUE(!d->backgroundSaveJob.isValid(), KritaUtils::BackgroudSavingStartResult::Failure);

    /**
     * From now on **no** return statements are allowed, even inside
     * asserts, since the ownership over the saving mutex has already
     * been passed to the background thread.
     *
     * The cancellation process should go through
     * slotChildCompletedSavingInBackground(), which will unlock the
     * mutex itself.
     */
    savingMutexLock.release();

    d->backgroundSaveDocument.reset(clonedDocument.release());
    d->backgroundSaveJob = job;
    d->modifiedWhileSaving = false;

    if (d->backgroundSaveJob.flags & KritaUtils::SaveInAutosaveMode) {
        d->backgroundSaveDocument->d->isAutosaving = true;
    }

    PkObject::connect(d->backgroundSaveDocument.get(),
                      &KisDocument::sigBackgroundSavingFinished,
                      this,
                      &KisDocument::slotChildCompletedSavingInBackground);


    d->completeSavingCallback = std::move(completedCallback);

    KisImportExportErrorCode error =
            d->backgroundSaveDocument->startExportInBackground(actionName,
                                                               job.filePath,
                                                               job.filePath,
                                                               job.mimeType,
                                                               job.flags & KritaUtils::SaveShowWarnings,
                                                               exportConfiguration, isAdvancedExporting);
    if (!error.isOk()) {
        // the state should have been deinitialized in slotChildCompletedSavingInBackground()
        KIS_SAFE_ASSERT_RECOVER (!d->backgroundSaveDocument && !d->backgroundSaveJob.isValid()) {
            d->backgroundSaveDocument.release()->deleteLater();
            d->savingMutex.unlock();
            d->backgroundSaveJob = KritaUtils::ExportFileJob();
        }
        if (error.isCancelled()) {
            return KritaUtils::BackgroudSavingStartResult::Cancelled;
        }
        return KritaUtils::BackgroudSavingStartResult::Failure;
    }

    return KritaUtils::BackgroudSavingStartResult::Success;
}


void KisDocument::slotChildCompletedSavingInBackground(KisImportExportErrorCode status, const PkString &errorMessage, const PkString &warningMessage)
{
    KIS_ASSERT_RECOVER_RETURN(isSaving());

    /**
     * Take back the ownership of the saving mutex and make sure it
     * well be released whatever the result of executing this function
     * will be, even if it asserts.
     */
    std::unique_lock<PkMutex> savingMutexLock(d->savingMutex, std::adopt_lock);

    KIS_ASSERT_RECOVER_RETURN(d->backgroundSaveDocument);

    if (d->backgroundSaveJob.flags & KritaUtils::SaveInAutosaveMode) {
        d->backgroundSaveDocument->d->isAutosaving = false;
    }

    d->backgroundSaveDocument.release()->deleteLater();

    KIS_ASSERT_RECOVER_RETURN(d->backgroundSaveJob.isValid());

    const KritaUtils::ExportFileJob job = d->backgroundSaveJob;
    d->backgroundSaveJob = KritaUtils::ExportFileJob();

    // unlock at the very end
    savingMutexLock.unlock();

    PkString fi = job.filePath;
    KisUsageLogger::log(PkString("Completed saving %1 (mime: %2). Result: %3. Warning: %4. Size: %5")
                            .arg(job.filePath).arg(pkFromByteArray(job.mimeType))
                            .arg(!status.isOk() ? errorMessage : PkString("OK"))
                            .arg(warningMessage).arg(pkNumber(pkFileSize(fi))));

    sigCompleteBackgroundSaving(job, status, errorMessage, warningMessage);
    // One-shot: invoke the completion callback (stored at initiate time) and clear it.
    if (d->completeSavingCallback) {
        auto cb = std::move(d->completeSavingCallback);
        d->completeSavingCallback = SavingCompletedCallback();
        cb(job, status, errorMessage, warningMessage);
    }
}

void KisDocument::slotAutoSaveImpl(std::unique_ptr<KisDocument> &&optionalClonedDocument)
{
    if (!d->modified || !d->modifiedAfterAutosave) return;
    const PkString autoSaveFileName = generateAutoSaveFileName(localFilePath());

    statusBarMessage(PkString("Autosaving... %1").arg(autoSaveFileName), successMessageTimeout);

    KisUsageLogger::log(PkString("Autosaving: %1").arg(autoSaveFileName));

    const bool hadClonedDocument = bool(optionalClonedDocument);
    KritaUtils::BackgroudSavingStartResult result = KritaUtils::BackgroudSavingStartResult::Failure;

    if (d->image->isIdle() || hadClonedDocument) {
        result = initiateSavingInBackground(PkString("Autosaving..."),
                                             [this](const KritaUtils::ExportFileJob &job, KisImportExportErrorCode status, const PkString &errorMessage, const PkString &warningMessage) { slotCompleteAutoSaving(job, status, errorMessage, warningMessage); },
                                             KritaUtils::ExportFileJob(autoSaveFileName, nativeFormatMimeType(), KritaUtils::SaveFlags(KritaUtils::SaveIsExporting) | KritaUtils::SaveInAutosaveMode),
                                             0,
                                             std::move(optionalClonedDocument));
    } else {
        statusBarMessage(PkString("Autosaving postponed: document is busy..."), errorMessageTimeout);
    }

    if (result != KritaUtils::BackgroudSavingStartResult::Success && !hadClonedDocument && d->autoSaveFailureCount >= 3) {
        KisCloneDocumentStroke *stroke = new KisCloneDocumentStroke(this);
        PkObject::connect(stroke, &KisCloneDocumentStroke::sigDocumentCloned,
                this, &KisDocument::slotInitiateAsyncAutosaving,
                PkConnectionType::BlockingQueued);
        PkObject::connect(stroke, &KisCloneDocumentStroke::sigCloningCancelled,
                this, &KisDocument::slotDocumentCloningCancelled,
                PkConnectionType::BlockingQueued);

        KisStrokeId strokeId = d->image->startStroke(stroke);
        d->image->endStroke(strokeId);

        setInfiniteAutoSaveInterval();

    } else if (result != KritaUtils::BackgroudSavingStartResult::Success) {
        setEmergencyAutoSaveInterval();
    } else {
        d->modifiedAfterAutosave = false;
    }
}

bool KisDocument::resourceSavingFilter(const PkString &path, const PkByteArray &mimeType, KisPropertiesConfigurationSP exportConfiguration)
{
    if (pkAbsolutePath(path).startsWith(KisResourceLocator::instance()->resourceLocationBase())) {

        PkStringList pathParts = pkSplitSkipEmpty(pkAbsolutePath(path), u'/');
        if (pathParts.size() > 0) {
            PkString resourceType = pathParts.last();
            if (KisResourceLoaderRegistry::instance()->resourceTypes().contains(resourceType)) {

                KisResourceModel model(resourceType);
                model.setResourceFilter(KisResourceModel::ShowAllResources);

                PkString tempFileName = pkTempDir() + "/" + pkFileName(path);

                if (pkFileExists(path)) {

                    int outResourceId;
                    KoResourceSP res;
                    if (KisResourceCacheDb::getResourceIdFromVersionedFilename(pkFileName(path), resourceType, "", outResourceId)) {
                        res = model.resourceForId(outResourceId);
                    }

                    if (res) {
                        d->modifiedWhileSaving = false;

                        if (!exportConfiguration) {
                            PkScopedPointer<KisImportExportFilter> filter(
                                KisImportExportManager::filterForMimeType(pkFromByteArray(mimeType), KisImportExportManager::Export));
                            if (filter) {
                                exportConfiguration = filter->defaultConfiguration(nativeFormatMimeType(), mimeType);
                            }
                        }

                        if (exportConfiguration) {
                            // make sure the name of the resource doesn't change
                            exportConfiguration->setProperty("name", res->name());
                        }

                        if (exportDocumentSync(tempFileName, mimeType, exportConfiguration)) {
                            PkFileStream f2(tempFileName);
                            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f2.open(PkStream::ReadOnly), false);

                            PkByteArray ba = f2.readAll();

                            PkMemoryStream buf;
                            buf.open(PkStream::ReadOnly);



                            if (res->loadFromDevice(&buf, KisGlobalResourcesInterface::instance())) {
                                if (model.updateResource(res)) {
                                    const PkString filePath =
                                        KisResourceLocator::instance()->filePathForResource(res);

                                    d->updateDocumentMetadataOnSaving(filePath, mimeType);

                                    return true;
                                }
                            }
                        }
                    }
                }
                else {
                    d->modifiedWhileSaving = false;
                    if (exportDocumentSync(tempFileName, mimeType, exportConfiguration)) {
                        KoResourceSP res = model.importResourceFile(tempFileName, false);
                        if (res) {
                            const PkString filePath =
                                KisResourceLocator::instance()->filePathForResource(res);

                            d->updateDocumentMetadataOnSaving(filePath, mimeType);

                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void KisDocument::slotAutoSave()
{
    slotAutoSaveImpl(std::unique_ptr<KisDocument>());
}

void KisDocument::slotInitiateAsyncAutosaving(KisDocument *clonedDocument)
{
    slotAutoSaveImpl(std::unique_ptr<KisDocument>(clonedDocument));
}

void KisDocument::slotDocumentCloningCancelled()
{
    setEmergencyAutoSaveInterval();
}

void KisDocument::slotPerformIdleRoutines()
{
    d->image->explicitRegenerateLevelOfDetail();


    /// TODO: automatic purging is disabled for now: it modifies
    ///       data managers without creating a transaction, which breaks
    ///       undo.

    // d->image->purgeUnusedData(true);
}

void KisDocument::slotCompleteAutoSaving(const KritaUtils::ExportFileJob &job, KisImportExportErrorCode status, const PkString &errorMessage, const PkString &warningMessage)
{
    (void)job;;
    (void)warningMessage;;

    const PkString fileName = pkFileName(job.filePath);

    if (!status.isOk()) {
        setEmergencyAutoSaveInterval();
        statusBarMessage(PkString("Error during autosaving %1: %2").arg(fileName).arg(exportErrorToUserMessage(status, errorMessage)), errorMessageTimeout);
    } else {
        d->autoSaveDelay = KisDocumentApplicationServices::instance()->autoSaveInterval();

        if (!d->modifiedWhileSaving) {
            d->autoSaveTimer->stop(); // until the next change
            d->autoSaveFailureCount = 0;
        } else {
            setNormalAutoSaveInterval();
        }

        statusBarMessage(PkString("Finished autosaving %1").arg(fileName), successMessageTimeout);
    }
}

KisImportExportErrorCode KisDocument::startExportInBackground(const PkString &actionName,
                                          const PkString &location,
                                          const PkString &realLocation,
                                          const PkByteArray &mimeType,
                                          bool showWarnings,
                                          KisPropertiesConfigurationSP exportConfiguration, bool isAdvancedExporting)
{
    d->savingImage = d->image;

    d->savingUpdater = KisDocumentApplicationServices::instance()->createUpdater(
        actionName, KisDocumentApplicationServices::UpdaterMode::SaveThreaded);
    if (d->savingUpdater) {
        d->importExportManager->setUpdater(d->savingUpdater);
    }

    KisImportExportErrorCode initializationStatus(ImportExportCodes::OK);
    d->childSavingFuture =
            d->importExportManager->exportDocumentAsync(location,
                                                       realLocation,
                                                       mimeType,
                                                       initializationStatus,
                                                       showWarnings,
                                                       exportConfiguration,
                                                       isAdvancedExporting);

    if (!initializationStatus.isOk()) {
        if (d->savingUpdater) {
            d->savingUpdater->cancel();
        }
        d->savingImage.clear();
        sigBackgroundSavingFinished(initializationStatus, initializationStatus.errorMessage(), "");
        return initializationStatus;
    }

    // 后台保存完成的 std::future 通知：PkTimer 每 50ms 轮询 future 是否就绪，
    // 就绪即停表并收尾（UI 线程事件循环泵，R-30）。原 watcher->deleteLater() 的
    // 堆对象清理由 d->savingWatchTimer 持有替代。
    if (!d->savingWatchTimer) {
        d->savingWatchTimer.reset(new PkTimer());
    }
    d->savingWatchTimer->start(std::chrono::milliseconds(50), [this]() {
        if (d->childSavingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            d->savingWatchTimer->stop();
            finishExportInBackground();
        }
    });

    return initializationStatus;
}

void KisDocument::finishExportInBackground()
{
    KIS_SAFE_ASSERT_RECOVER(d->childSavingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        sigBackgroundSavingFinished(ImportExportCodes::InternalError, "", "");
        return;
    }

    KisImportExportErrorCode status = d->childSavingFuture.get();
    PkString errorMessage = status.errorMessage();
    PkString warningMessage = d->lastWarningMessage;

    if (!d->lastErrorMessage.isEmpty()) {
        if (status == ImportExportCodes::InternalError || status == ImportExportCodes::Failure) {
            errorMessage = d->lastErrorMessage;
        } else {
            errorMessage += PkString("\n") + d->lastErrorMessage;
        }
    }

    d->savingImage.clear();
    d->childSavingFuture = std::future<KisImportExportErrorCode>();
    d->lastErrorMessage = PkString();
    d->lastWarningMessage = PkString();

    if (d->savingUpdater) {
        d->savingUpdater->setProgress(100);
    }

    sigBackgroundSavingFinished(status, errorMessage, warningMessage);
}

void KisDocument::setReadWrite(bool readwrite)
{
    const bool changed = readwrite != d->readwrite;

    d->readwrite = readwrite;

    if (changed) {
        sigReadWriteChanged(readwrite);
    }
}

void KisDocument::setAutoSaveActive(bool autoSaveActive)
{
    const bool changed = autoSaveActive != d->autoSaveActive;

    if (changed) {
        d->autoSaveActive = autoSaveActive;
        setNormalAutoSaveInterval();
    }
}

void KisDocument::setAutoSaveDelay(int delay)
{
    if (isReadWrite() && delay > 0 && d->autoSaveActive) {
        d->autoSaveTimer->start(std::chrono::milliseconds(delay * 1000), [this]() { slotAutoSave(); });
    } else {
        d->autoSaveTimer->stop();
    }
}

void KisDocument::setNormalAutoSaveInterval()
{
    setAutoSaveDelay(d->autoSaveDelay);
    d->autoSaveFailureCount = 0;
}

void KisDocument::setEmergencyAutoSaveInterval()
{
    const int emergencyAutoSaveInterval = 10; /* sec */
    setAutoSaveDelay(emergencyAutoSaveInterval);
    d->autoSaveFailureCount++;
}

void KisDocument::setInfiniteAutoSaveInterval()
{
    setAutoSaveDelay(-1);
}

bool KisDocument::isAutoSaveActive()
{
    return d->autoSaveActive;
}

KoDocumentInfo *KisDocument::documentInfo() const
{
    return d->docInfo;
}

bool KisDocument::isModified() const
{
    return d->modified;
}

PkImage KisDocument::generatePreview(const PkSize& size)
{
    KisImageSP image = d->image;
    if (d->savingImage) image = d->savingImage;

    if (image) {
        PkRect bounds = image->bounds();
        PkSize originalSize = bounds.size();
        // PkSize may round down one dimension to zero on extreme aspect rations, so ensure 1px minimum
        PkSize newSize = originalSize.scaled(size, Qt::KeepAspectRatio).expandedTo({1, 1});

        bool pixelArt = false;
        // determine if the image is pixel art or not
        if (originalSize.width() < size.width() && originalSize.height() < size.height()) {
            // the image must be smaller than the requested preview
            // the scale must be integer
            if (newSize.height()%originalSize.height() == 0 && newSize.width()%originalSize.width() == 0) {
                pixelArt = true;
            }
        }

        PkImage px;
        if (pixelArt) {
            // do not scale while converting (because it uses Bicubic)
            PkImage original = image->convertToQImage(originalSize, 0);
            // scale using FastTransformation, which is probably Nearest neighbour, suitable for pixel art
            px = original.scaled(newSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        } else {
            px = image->convertToQImage(newSize, 0);
        }
        if (px.size() == PkSize(0,0)) {
            PkImage checkTile =
                KisDocumentApplicationServices::instance()->previewCheckerboard(newSize.width() / 5);
            px = pkTileImage(checkTile, newSize.width(), newSize.height());
        }
        return px;
    }
    return PkImage(size, PkImage::Format_ARGB32);
}

PkString KisDocument::generateAutoSaveFileName(const PkString & path) const
{
    PkString retval;

    // Using the extension allows to avoid relying on the mime magic when opening
    const PkString extension (".kra");
    KisDocumentApplicationServices *services = KisDocumentApplicationServices::instance();
    PkString prefix = services->autoSaveFilesHidden() ? PkString(".") : PkString();
    std::regex autosavePattern1("^\\..+-autosave.kra$");
    std::regex autosavePattern2("^.+-autosave.kra$");

    PkString fi = path;
    PkString dir = pkAbsolutePath(fi);

#ifdef Q_OS_ANDROID
    // URIs may or may not have a directory backing them, so we save to our default autosave location
    if (path.startsWith("content://")) {
        dir = services->autoSaveLocation();
        pkMkdir(dir);
    }
#endif

    PkString filename = pkFileName(fi);

    const std::string autosaveName = filename.PkToUtf8();
    const bool isAutosaveName = std::regex_search(autosaveName, autosavePattern1) || std::regex_search(autosaveName, autosavePattern2);
    if (path.isEmpty() || isAutosaveName || !pkIsWritable(fi)) {
        // Never saved?
        retval = PkString("%1%2%3%4-%5-%6-autosave%7")
                     .arg(services->autoSaveLocation())
                     .arg('/')
                     .arg(prefix)
                     .arg("krita")
                     .arg(getpid())
                     .arg(objectName())
                     .arg(extension);
    } else {
        // Beware: don't reorder arguments
        //   otherwise in case of filename = '1-file.kra' it will become '.-file.kra-autosave.kra' instead of '.1-file.kra-autosave.kra'
        retval = PkString("%1%2%3%4-autosave%5").arg(dir).arg('/').arg(prefix).arg(filename).arg(extension);
    }

    //qDebug() << "generateAutoSaveFileName() for path" << path << ":" << retval;
    return retval;
}

bool KisDocument::importDocument(const PkString &_path)
{
    bool ret;

    dbgUI << "path=" << _path;

    // open...
    ret = openPath(_path);

    // reset url & m_file (kindly? set by KisParts::openUrl()) to simulate a
    // File --> Import
    if (ret) {
        dbgUI << "success, resetting url";
        resetPath();
    }

    return ret;
}


bool KisDocument::openPath(const PkString &_path, OpenFlags flags)
{
    dbgUI << "path=" << _path;
    d->lastErrorMessage = PkString();

    // Reimplemented, to add a check for autosave files and to improve error reporting
    if (_path.isEmpty()) {
        d->lastErrorMessage = PkString("Malformed Path\n%1").arg(_path);  // ## used anywhere ?
        return false;
    }

    PkString path = _path;
    PkString original  = "";
    bool autosaveOpened = false;
    if (!fileBatchMode()) {
        PkString file = path;
        PkString asf = generateAutoSaveFileName(file);
        if (pkFileExists(asf)) {
            switch (KisDocumentApplicationServices::instance()->chooseNamedAutosave(file, asf)) {
            case KisDocumentApplicationServices::RecoveryChoice::OpenAutosave:
                original = file;
                path = asf;
                autosaveOpened = true;
                break;
            case KisDocumentApplicationServices::RecoveryChoice::OpenMainFile:
                KisUsageLogger::log(PkString("Removing autosave file: %1").arg(asf));
                pkRemoveFile(asf);
                break;
            case KisDocumentApplicationServices::RecoveryChoice::Cancel:
                return false;
            }
        }
    }

    bool ret = openPathInternal(path);

    if (autosaveOpened || flags & RecoveryFile) {
        setReadWrite(true); // enable save button
        setModified(true);
        setRecovered(true);

        setPath(original); // since it was an autosave, it will be a local file
        setLocalFilePath(original);
    }
    else {
        if (ret) {

            if (!(flags & DontAddToRecent)) {
                KisDocumentApplicationServices::instance()->addRecentFile(_path);
            }

#ifdef Q_OS_ANDROID
            // See the comment titled "ANDROID NOTES" in this file for an
            // explanation of what this is about. (This is not that comment.)
            setReadWrite(true);
#else
            PkString fi = _path;
            setReadWrite(pkIsWritable(fi));
#endif
        }

        setRecovered(false);
    }

    return ret;
}

bool KisDocument::openFile()
{
    //dbgUI <<"for" << localFilePath();
    if (!pkFileExists(localFilePath()) && !fileBatchMode()) {
        KisDocumentApplicationServices::instance()->showDocumentMessage({
            PkString("Krita"),
            PkString("File %1 does not exist.").arg(localFilePath()),
            {},
            {},
            KisDocumentApplicationServices::MessageType::Critical});
        return false;
    }

    PkString filename = localFilePath();
    PkString typeName = pkFromByteArray(mimeType());

    if (typeName.isEmpty()) {
        typeName = KisMimeDatabase::mimeTypeForFile(filename);
    }

    // Allow to open backup files, don't keep the mimeType application/x-trash.
    if (typeName == "application/x-trash") {
        PkString path = filename;
        while (path.size() > 0) {
            path = path.left(path.size() - 1);
            typeName = KisMimeDatabase::mimeTypeForFile(path);
            //qDebug() << "\t" << path << typeName;
            if (!typeName.isEmpty()) {
                break;
            }
        }
        //qDebug() << "chopped" << filename  << "to" << path << "Was trash, is" << typeName;
    }
    dbgUI << localFilePath() << "type:" << typeName;

    KoUpdaterPtr updater = KisDocumentApplicationServices::instance()->createUpdater(
        PkString("Opening document"),
        KisDocumentApplicationServices::UpdaterMode::LoadUnthreaded);
    if (updater) {
        d->importExportManager->setUpdater(updater);
    }

    KisImportExportErrorCode status = d->importExportManager->importDocument(localFilePath(), typeName);

    if (!status.isOk()) {
        if (updater) {
            updater->cancel();
        }
        PkString msg = status.errorMessage();
        KisUsageLogger::log(PkString("Loading %1 failed: %2").arg(prettyPath(), msg));

        if (!msg.isEmpty() && !fileBatchMode()) {
            KisDocumentApplicationServices::instance()->showDocumentMessage({
                PkString("Krita"),
                PkString("Could not open %1.").arg(prettyPath()),
                pkConcat(pkSplitSkipEmpty(errorMessage(), u'\n'), pkSplitSkipEmpty(warningMessage(), u'\n')),
                msg});
        }
        return false;
    }
    else if (!warningMessage().isEmpty() && !fileBatchMode()) {
        KisDocumentApplicationServices::instance()->showDocumentMessage({
            PkString("Krita"),
            PkString("There were problems opening %1.").arg(prettyPath()),
            pkSplitSkipEmpty(warningMessage(), u'\n'),
            {}});
        setPath(PkString());
    }

    setMimeTypeAfterLoading(typeName);
    d->syncDecorationsWrapperLayerState();
    sigLoadingFinished();

    undoStack()->clear();

    return true;
}

void KisDocument::autoSaveOnPause()
{
    if (!d->modified || !d->modifiedAfterAutosave)
        return;

    const PkString autoSaveFileName = generateAutoSaveFileName(localFilePath());

    bool started = exportDocumentSync(autoSaveFileName, nativeFormatMimeType());

    if (started)
    {
        d->modifiedAfterAutosave = false;
        dbgAndroid << "autoSaveOnPause successful";
    }
    else
    {
        qWarning() << "Could not auto-save when paused";
    }
}

// shared between openFile and koMainWindow's "create new empty document" code
void KisDocument::setMimeTypeAfterLoading(const PkString& mimeType)
{
    d->mimeType = pkToByteArray(mimeType);
    d->outputMimeType = d->mimeType;
}


bool KisDocument::loadNativeFormat(const PkString & file_)
{
    return openPath(file_);
}

void KisDocument::setModified(bool mod)
{
    if (mod) {
        updateEditingTime(false);
    }

    /// 1) Ignore setModified calls due to autosaving
    /// 2) When closing a document, the undo stack emits a lot of
    ///    modified signals, when clearing itself, so we should
    ///    ignore all of them.
    if (d->isAutosaving || d->documentIsClosing)
        return;

    //dbgUI<<" url:" << url.path();
    //dbgUI<<" mod="<<mod<<" MParts mod="<<KisParts::ReadWritePart::isModified()<<" isModified="<<isModified();

    if (mod && !d->autoSaveTimer->isActive()) {
        // First change since last autosave -> start the autosave timer
        setNormalAutoSaveInterval();
    }
    d->modifiedAfterAutosave = mod;
    d->modifiedWhileSaving = mod;

    if (!mod) {
        d->imageModifiedWithoutUndo = mod;
    }

    if (mod == isModified())
        return;

    d->modified = mod;

    if (mod) {
        documentInfo()->updateParameters(isModified());
    }

    modified(mod);
}

void KisDocument::setRecovered(bool value)
{
    const bool changed = value != d->isRecovered;

    d->isRecovered = value;

    if (changed) {
        sigRecoveredChanged(value);
    }
}

bool KisDocument::isRecovered() const
{
    return d->isRecovered;
}

void KisDocument::updateEditingTime(bool forceStoreElapsed)
{
    PkDateTime now = PkDateTime::currentDateTime();
    int firstModDelta = d->firstMod.secsTo(now);
    int lastModDelta = d->lastMod.secsTo(now);

    if (lastModDelta > 30) {
        d->docInfo->setAboutInfo("editing-time", pkNumber(d->docInfo->aboutInfo("editing-time").toInt() + d->firstMod.secsTo(d->lastMod)));
        d->firstMod = now;
    } else if (firstModDelta > 60 || forceStoreElapsed) {
        d->docInfo->setAboutInfo("editing-time", pkNumber(d->docInfo->aboutInfo("editing-time").toInt() + firstModDelta));
        d->firstMod = now;
    }

    d->lastMod = now;
}

PkString KisDocument::prettyPath() const
{
    PkString _url(path());
#ifdef Q_OS_WIN
    _url = _url.toLower();
#endif
    return _url;
}

// Get caption from document info (title(), in about page)
PkString KisDocument::caption() const
{
    PkString c;
    const PkString _url(pkFileName(path()));

    // if URL is empty...it is probably an unsaved file
    if (_url.isEmpty()) {
        c = PkString(" [") + PkString("Not Saved") + PkString("] ");
    } else {
        c = _url; // Fall back to document URL
    }

    return c;
}

PkXmlDocument KisDocument::createDomDocument(const PkString& tagName, const PkString& version) const
{
    return createDomDocument("krita", tagName, version);
}

//static
PkXmlDocument KisDocument::createDomDocument(const PkString& appName, const PkString& tagName, const PkString& version)
{
    PkString url = PkString("http://www.calligra.org/DTD/%1-%2.dtd").arg(appName).arg(version);
    // The namespace URN doesn't need to include the version number.
    PkString namespaceURN = PkString("http://www.calligra.org/DTD/%1").arg(appName);

    // 原文档创建 API（createDocument + createProcessingInstruction）的组合效果：
    // 文档含 XML 声明、DTD（publicId/systemId）与带命名空间的根元素。
    // PkXmlDocument 无 createDocumentType/createProcessingInstruction API，
    // 直接构造 XML 文本交给 pugixml 解析（setContent 默认 parse_doctype 保留 DTD）。
    PkString xml = PkString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                            "<!DOCTYPE %1 PUBLIC \"-//KDE//DTD %2 %3//EN\" \"%4\">\n"
                            "<%1 xmlns=\"%5\"/>\n")
            .arg(tagName)
            .arg(appName)
            .arg(version)
            .arg(url)
            .arg(namespaceURN);

    PkXmlDocument doc;
    PkString errorMsg;
    int errorLine = 0;
    if (!doc.setContent(xml, &errorMsg, &errorLine)) {
        doc = PkXmlDocument();
    }
    return doc;
}

bool KisDocument::isNativeFormat(const PkByteArray& mimeType) const
{
    if (mimeType == nativeFormatMimeType())
        return true;
    return extraNativeMimeTypes().contains(pkFromByteArray(mimeType));
}

void KisDocument::setErrorMessage(const PkString& errMsg)
{
    d->lastErrorMessage = errMsg;
}

PkString KisDocument::errorMessage() const
{
    return d->lastErrorMessage;
}

void KisDocument::setWarningMessage(const PkString& warningMsg)
{
    d->lastWarningMessage = warningMsg;
}

PkString KisDocument::warningMessage() const
{
    return d->lastWarningMessage;
}


void KisDocument::removeAutoSaveFiles(const PkString &autosaveBaseName, bool wasRecovered)
{
    // Eliminate any auto-save file
    PkString asf = generateAutoSaveFileName(autosaveBaseName);   // the one in the current dir
    if (pkFileExists(asf)) {
        KisUsageLogger::log(PkString("Removing autosave file: %1").arg(asf));
        pkRemoveFile(asf);
    }
    asf = generateAutoSaveFileName(PkString());   // and the one in $HOME

    if (pkFileExists(asf)) {
        KisUsageLogger::log(PkString("Removing autosave file: %1").arg(asf));
        pkRemoveFile(asf);
    }

    std::vector<std::regex> expressions;

    expressions.push_back(std::regex("^\\..+-autosave.kra$"));
    expressions.push_back(std::regex("^.+-autosave.kra$"));

    for (const std::regex &rex : expressions) {
        if (wasRecovered &&
                !autosaveBaseName.isEmpty() &&
                std::regex_search(pkFileName(autosaveBaseName).PkToUtf8(), rex) &&
                pkFileExists(autosaveBaseName)) {

            KisUsageLogger::log(PkString("Removing autosave file: %1").arg(autosaveBaseName));
            pkRemoveFile(autosaveBaseName);
        }
    }
}

KoUnit KisDocument::unit() const
{
    return d->unit;
}

void KisDocument::setUnit(const KoUnit &unit)
{
    if (d->unit != unit) {
        d->unit = unit;
        unitChanged(unit);
    }
}

KUndo2Stack *KisDocument::undoStack()
{
    return d->undoStack;
}

KisImportExportManager *KisDocument::importExportManager() const
{
    return d->importExportManager;
}

void KisDocument::slotUndoStackCleanChanged(bool value)
{
    setModified(!value || d->imageModifiedWithoutUndo);
}

void KisDocument::slotConfigChanged()
{
    KisDocumentApplicationServices *services = KisDocumentApplicationServices::instance();

    if (d->undoStack->undoLimit() != services->undoStackLimit()) {
        if (!d->undoStack->isClean()) {
            d->undoStack->clear();
            // we set this because the document *has* changed, even though the
            // undo history was purged.
            setImageModifiedWithoutUndo();
        }
        d->undoStack->setUndoLimit(services->undoStackLimit());
    }
    d->undoStack->setUseCumulativeUndoRedo(services->useCumulativeUndoRedo());
    d->undoStack->setCumulativeUndoData(services->cumulativeUndoData());

    d->autoSaveDelay = services->autoSaveInterval();
    setNormalAutoSaveInterval();
}

void KisDocument::slotImageRootChanged()
{
    d->syncDecorationsWrapperLayerState();
}

void KisDocument::clearUndoHistory()
{
    d->undoStack->clear();
}

KisGridConfig KisDocument::gridConfig() const
{
    return d->gridConfig;
}

void KisDocument::setGridConfig(const KisGridConfig &config)
{
    if (d->gridConfig != config) {
        d->gridConfig = config;
        d->syncDecorationsWrapperLayerState();
        sigGridConfigChanged(config);

        // Store last assigned value as future default...
        KisDocumentApplicationServices::instance()->setDefaultGridSpacing(config.spacing());
    }
}

PkList<KoResourceLoadResult> KisDocument::linkedDocumentResources()
{
    PkList<KoResourceLoadResult> result;
    if (!d->linkedResourceStorage) {
        return result;
    }

    for (const PkString &resourceType : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        PkSharedPointer<KisResourceStorage::ResourceIterator> iter = d->linkedResourceStorage->resources(resourceType);
        while (iter->hasNext()) {
            iter->next();

            PkMemoryStream buf;
            buf.open(PkStream::WriteOnly);
            bool exportSuccessful =
                d->linkedResourceStorage->exportResource(iter->url(), &buf);

            KoResourceSP resource = d->linkedResourceStorage->resource(iter->url());
            exportSuccessful &= bool(resource);

            const PkString name = resource ? resource->name() : PkString();
            const PkString fileName = pkFileName(iter->url());
            const KoResourceSignature signature(resourceType,
                                                KoMD5Generator::generateHash(buf.data()),
                                                fileName, name);

            if (exportSuccessful) {
                result << KoEmbeddedResource(signature, PkByteArray(buf.data(), (int)buf.size()));
            } else {
                result << signature;
            }
        }
    }

    return result;
}

void KisDocument::setPaletteList(const PkList<KoColorSetSP > &paletteList, bool emitSignal)
{
    PkList<KoColorSetSP> oldPaletteList;
    if (d->linkedResourceStorage) {
        PkSharedPointer<KisResourceStorage::ResourceIterator> iter = d->linkedResourceStorage->resources(ResourceType::Palettes);
        while (iter->hasNext()) {
            iter->next();
            KoResourceSP resource = iter->resource();
            if (resource && resource->valid()) {
                oldPaletteList << resource.dynamicCast<KoColorSet>();
            }
        }
        if (oldPaletteList != paletteList) {
            KisResourceModel resourceModel(ResourceType::Palettes);
            for (KoColorSetSP palette : oldPaletteList) {
                if (!paletteList.contains(palette)) {
                    resourceModel.setResourceInactive(palette);
                }
            }
            for (KoColorSetSP palette : paletteList) {
                if (!oldPaletteList.contains(palette)) {
                    resourceModel.addResource(palette, d->linkedResourcesStorageID);
                }
                else {
                    palette->setStorageLocation(d->linkedResourcesStorageID);
                    resourceModel.updateResource(palette);
                }
            }
            if (emitSignal) {
                sigPaletteListChanged(oldPaletteList, paletteList);
            }
        }
    }
}

StoryboardItemList KisDocument::getStoryboardItemList()
{
    return d->m_storyboardItemList;
}

void KisDocument::setStoryboardItemList(const StoryboardItemList &storyboardItemList, bool emitSignal)
{
    d->m_storyboardItemList = storyboardItemList;
    if (emitSignal) {
        sigStoryboardItemListChanged();
    }
}

PkVector<StoryboardComment> KisDocument::getStoryboardCommentsList()
{
    return d->m_storyboardCommentList;
}

void KisDocument::setStoryboardCommentList(const PkVector<StoryboardComment> &storyboardCommentList, bool emitSignal)
{
    d->m_storyboardCommentList = storyboardCommentList;
    if (emitSignal) {
        sigStoryboardCommentListChanged();
    }
}

PkVector<std::filesystem::path> KisDocument::getAudioTracks() const {
    return d->audioTracks;
}

void KisDocument::setAudioTracks(PkVector<std::filesystem::path> f)
{
    d->audioTracks = f;
    sigAudioTracksChanged();
}

void KisDocument::setAudioVolume(qreal level)
{
    d->audioLevel = level;
    sigAudioLevelChanged(level);
}

qreal KisDocument::getAudioLevel()
{
    return d->audioLevel;
}

const KisGuidesConfig& KisDocument::guidesConfig() const
{
    return d->guidesConfig;
}

void KisDocument::setGuidesConfig(const KisGuidesConfig &data)
{
    if (d->guidesConfig == data) return;

    d->guidesConfig = data;
    d->syncDecorationsWrapperLayerState();
    sigGuidesConfigChanged(d->guidesConfig);
}


const KisMirrorAxisConfig& KisDocument::mirrorAxisConfig() const
{
    return d->mirrorAxisConfig;
}

void KisDocument::setMirrorAxisConfig(const KisMirrorAxisConfig &config)
{
    if (d->mirrorAxisConfig == config) {
        return;
    }

    d->mirrorAxisConfig = config;
    if (d->image) {
        d->image->setMirrorAxesCenter(KisAlgebra2D::absoluteToRelative(d->mirrorAxisConfig.axisPosition(),
                                                                       d->image->bounds()));
    }
    setModified(true);

    sigMirrorAxisConfigChanged();
}

void KisDocument::resetPath() {
    setPath(PkString());
    setLocalFilePath(PkString());
}

bool KisDocument::isReadWrite() const
{
    return d->readwrite;
}

PkString KisDocument::path() const
{
    return d->m_path;
}

bool KisDocument::closePath(bool promptToSave)
{
    if (promptToSave) {
        if ( isReadWrite() && isModified()) {
            if (!KisDocumentApplicationServices::instance()->queryClose(this)) {
                return false;
            }
        }
    }
    // Not modified => ok and delete temp file.
    d->mimeType = PkByteArray();

    // It always succeeds for a read-only part,
    // but the return value exists for reimplementations
    // (e.g. pressing cancel for a modified read-write part)
    return true;
}



void KisDocument::setPath(const PkString &path)
{
    const bool changed = path != d->m_path;

    d->m_path = path;

    if (changed) {
        sigPathChanged(path);
    }
}

PkString KisDocument::localFilePath() const
{
    return d->m_file;
}


void KisDocument::setLocalFilePath( const PkString &localFilePath )
{
    d->m_file = localFilePath;
}

bool KisDocument::openPathInternal(const PkString &path)
{
    if ( path.isEmpty() ) {
        return false;
    }

    if (d->m_bAutoDetectedMime) {
        d->mimeType = PkByteArray();
        d->m_bAutoDetectedMime = false;
    }

    PkByteArray mimeType = d->mimeType;

    if ( !closePath() ) {
        return false;
    }

    d->mimeType = mimeType;
    setPath(path);

    d->m_file = PkString();

    d->m_file = d->m_path;

    bool ret = false;
    // set the mimeType only if it was not already set (for example, by the host application)
    if (d->mimeType.isEmpty()) {
        // get the mimeType of the file
        // using findByUrl() to avoid another string -> url conversion
        PkString mime = KisMimeDatabase::mimeTypeForFile(d->m_path);
        d->mimeType = pkToByteArray(mime);
        d->m_bAutoDetectedMime = true;
    }

    setPath(d->m_path);
    ret = openFile();

    if (ret) {
        completed();
    }
    else {
        canceled(PkString());
    }
    return ret;
}

bool KisDocument::newImage(const PkString& name,
                           qint32 width, qint32 height,
                           const KoColorSpace* cs,
                           const KoColor &bgColor, NewImageBackgroundStyle bgStyle,
                           int numberOfLayers,
                           const PkString &description, const double imageResolution)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(cs);

    KisImageSP image;

    if (!cs) return false;

    std::unique_ptr<KisDocumentBusyCursor> busyCursor =
        KisDocumentApplicationServices::instance()->createBusyCursor();

    image = new KisImage(createUndoStore(), width, height, cs, name);

    KIS_SAFE_ASSERT_RECOVER_NOOP(image);

    PkObject::connect(image, &KisImage::sigImageModified, this, &KisDocument::setImageModified);
    PkObject::connect(image, &KisImage::sigImageModifiedWithoutUndo, this, &KisDocument::setImageModifiedWithoutUndo);
    image->setResolution(imageResolution, imageResolution);

    image->assignImageProfile(cs->profile());
    image->waitForDone();

    documentInfo()->setAboutInfo("title", name);
    documentInfo()->setAboutInfo("abstract", description);

    KisDocumentApplicationServices *services = KisDocumentApplicationServices::instance();
    services->storeNewImageDefaults(width,
                                    height,
                                    imageResolution,
                                    image->colorSpace()->colorModelId().id(),
                                    image->colorSpace()->colorDepthId().id(),
                                    image->colorSpace()->profile()->name());

    const bool autopin = services->autoPinLayersToTimeline();

    KisLayerSP bgLayer;
    if (bgStyle == NewImageBackgroundStyle::RasterLayer ||
        bgStyle == NewImageBackgroundStyle::FillLayer) {
        KoColor strippedAlpha = bgColor;
        strippedAlpha.setOpacity(OPACITY_OPAQUE_U8);

        if (bgStyle == NewImageBackgroundStyle::RasterLayer) {
            bgLayer = new KisPaintLayer(image.data(), PkString("Background"), OPACITY_OPAQUE_U8, cs);
            bgLayer->paintDevice()->setDefaultPixel(strippedAlpha);
            bgLayer->setPinnedToTimeline(autopin);
        } else if (bgStyle == NewImageBackgroundStyle::FillLayer) {
            KisFilterConfigurationSP filter_config = KisGeneratorRegistry::instance()->get("color")->defaultConfiguration(KisGlobalResourcesInterface::instance());
            filter_config->setProperty("color", PkVariant::fromValue(strippedAlpha.toQColor()));
            filter_config->createLocalResourcesSnapshot();
            bgLayer = new KisGeneratorLayer(image.data(), PkString("Background Fill"), filter_config, image->globalSelection());
        }

        bgLayer->setOpacity(bgColor.opacityU8());

        if (numberOfLayers > 1) {
            //Lock bg layer if others are present.
            bgLayer->setUserLocked(true);
        }
    }
    else { // NewImageBackgroundStyle::CanvasColor (needs an unlocked starting layer).
        image->setDefaultProjectionColor(bgColor);
        bgLayer = new KisPaintLayer(image.data(), image->nextLayerName(), OPACITY_OPAQUE_U8, cs);
    }

    KIS_SAFE_ASSERT_RECOVER_NOOP(bgLayer);
    image->addNode(bgLayer.data(), image->rootLayer().data());
    bgLayer->setDirty(PkRect(0, 0, width, height));

    // reset mirror axis to default:
    d->mirrorAxisConfig.setAxisPosition(PkRectF(image->bounds()).center());
    setCurrentImage(image);

    for(int i = 1; i < numberOfLayers; ++i) {
        KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), OPACITY_OPAQUE_U8, cs);
        layer->setPinnedToTimeline(autopin);
        image->addNode(layer, image->root(), i);
        layer->setDirty(PkRect(0, 0, width, height));
    }

    if (const auto history = services->activeColorHistory()) {
        setColorHistory(*history);
    }

    KisUsageLogger::log(
        PkString("Created image \"%1\", %2 * %3 pixels, %4 dpi. Color model: %6 %5 (%7). Layers: %8")
            .arg(name)
            .arg(pkNumber(width))
            .arg(pkNumber(height))
            .arg(pkNumber(imageResolution * 72.0))
            .arg(image->colorSpace()->colorModelId().name())
            .arg(image->colorSpace()->colorDepthId().name())
            .arg(image->colorSpace()->profile()->name())
            .arg(pkNumber(numberOfLayers)));

    return true;
}

bool KisDocument::isSaving() const
{
    const bool result = d->savingMutex.tryLock();
    if (result) {
        d->savingMutex.unlock();
    }
    return !result;
}

void KisDocument::waitForSavingToComplete()
{
    if (isSaving()) {
        KisDocumentApplicationServices::instance()->waitForMutexWithFeedback(
            d->savingMutex,
            PkString("Waiting for saving to complete..."));
    }
}

KoShapeControllerBase *KisDocument::shapeController() const
{
    return d->shapeController;
}

KoShapeLayer* KisDocument::shapeForNode(KisNodeSP layer) const
{
    return d->shapeController->shapeForNode(layer);
}

PkList<KisPaintingAssistantSP> KisDocument::assistants() const
{
    return d->assistants;
}

void KisDocument::setAssistants(const PkList<KisPaintingAssistantSP> &value)
{
    if (d->assistants != value) {
        d->assistants = value;
        d->syncDecorationsWrapperLayerState();
        sigAssistantsChanged();
    }
}

KisReferenceImagesLayerSP KisDocument::referenceImagesLayer() const
{
    if (!d->image) return KisReferenceImagesLayerSP();

    KisReferenceImagesLayerSP referencesLayer =
            KisLayerUtils::findNodeByType<KisReferenceImagesLayer>(d->image->root());

    return referencesLayer;
}

void KisDocument::setReferenceImagesLayer(KisSharedPtr<KisReferenceImagesLayer> layer, bool updateImage)
{
    KisReferenceImagesLayerSP currentReferenceLayer = referenceImagesLayer();

    // updateImage=false inherently means we are not changing the
    // reference images layer, but just would like to update its signals.
    if (currentReferenceLayer == layer && updateImage) {
        return;
    }

    d->referenceLayerConnections.clear();

    if (updateImage) {
        if (currentReferenceLayer) {
            d->image->removeNode(currentReferenceLayer);
        }

        if (layer) {
            d->image->addNode(layer);
        }
    }

    currentReferenceLayer = layer;

    if (currentReferenceLayer) {
        d->referenceLayerConnections.addConnection(
                    currentReferenceLayer.data(), &KisReferenceImagesLayer::sigUpdateCanvas,
                    this, &KisDocument::sigReferenceImagesChanged);
    }

    sigReferenceImagesLayerChanged(layer);
    sigReferenceImagesChanged();
}

void KisDocument::setPreActivatedNode(KisNodeSP activatedNode)
{
    d->preActivatedNode = activatedNode;
}

KisNodeSP KisDocument::preActivatedNode() const
{
    return d->preActivatedNode;
}

KisImageWSP KisDocument::image() const
{
    return d->image;
}

KisImageSP KisDocument::savingImage() const
{
    return d->savingImage;
}


void KisDocument::setCurrentImage(KisImageSP image, bool forceInitialUpdate, KisNodeSP preActivatedNode)
{
    if (d->image) {
        // Disconnect existing sig/slot connections
        d->image->setUndoStore(new KisDumbUndoStore());
        d->image->disconnect();
        d->shapeController->setImage(0);
        d->image = 0;
    }

    if (!image) return;

    if (d->linkedResourceStorage){
        d->linkedResourceStorage->setMetaData(KisResourceStorage::s_meta_name, image->objectName());
    }

    d->setImageAndInitIdleWatcher(image);
    d->image->setUndoStore(new KisDocumentUndoStore(this));
    d->shapeController->setImage(image, preActivatedNode);
    d->image->setMirrorAxesCenter(KisAlgebra2D::absoluteToRelative(d->mirrorAxisConfig.axisPosition(), image->bounds()));
    setModified(false);
    PkObject::connect(d->image, &KisImage::sigImageModified, this, &KisDocument::setImageModified);
    PkObject::connect(d->image, &KisImage::sigImageModifiedWithoutUndo, this, &KisDocument::setImageModifiedWithoutUndo);
    PkObject::connect(d->image, &KisImage::sigLayersChangedAsync, this, &KisDocument::slotImageRootChanged);

    if (forceInitialUpdate) {
        d->image->initialRefreshGraph();
    }
}

void KisDocument::hackPreliminarySetImage(KisImageSP image)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!d->image);

    // we set image without connecting idle-watcher, because loading
    // hasn't been finished yet
    d->image = image;
    d->shapeController->setImage(image);
}

void KisDocument::setImageModified()
{
    // we only set as modified if undo stack is not at clean state
    setModified(d->imageModifiedWithoutUndo || !d->undoStack->isClean());
}

void KisDocument::setImageModifiedWithoutUndo()
{
    d->imageModifiedWithoutUndo = true;
    setImageModified();
}


KisUndoStore* KisDocument::createUndoStore()
{
    return new KisDocumentUndoStore(this);
}

bool KisDocument::isAutosaving() const
{
    return d->isAutosaving;
}

PkString KisDocument::exportErrorToUserMessage(KisImportExportErrorCode status, const PkString &errorMessage)
{
    return errorMessage.isEmpty() ? status.errorMessage() : errorMessage;
}

void KisDocument::setAssistantsGlobalColor(PkColor color)
{
    d->globalAssistantsColor = color;
}

PkColor KisDocument::assistantsGlobalColor()
{
    return d->globalAssistantsColor;
}

PkList<KoColor> KisDocument::colorHistory()
{
    return d->colorHistory;
}

PkRectF KisDocument::documentBounds() const
{
    PkRectF bounds = d->image->bounds();

    KisReferenceImagesLayerSP referenceImagesLayer = this->referenceImagesLayer();

    if (referenceImagesLayer) {
        bounds |= referenceImagesLayer->boundingImageRect();
    }

    return bounds;
}

void KisDocument::setColorHistory(const PkList<KoColor> &colors)
{
    d->colorHistory = colors;
}
