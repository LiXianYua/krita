/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KRA_CONVERTER_H_
#define _KRA_CONVERTER_H_

#include <QtCore/qnamespace.h>
#include <QtGlobal>
#include <QtCore/qalgorithms.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qpair.h>

#include <PkXmlDocument.h>
#include <PkObject.h>
#include <PkPointer.h>

#include <KisImportExportErrorCode.h>
#include <KoProgressUpdater.h>
#include <KoStore.h>
#include <KoUpdater.h>
#include <kis_kra_loader.h>
#include <kis_kra_saver.h>
#include <kis_types.h>

#include "kritalibkra_export.h"

class KisDocument;
class KisImportUserFeedbackInterface;

class KRITALIBKRA_EXPORT KraConverter : public PkObject
{
public:

    KraConverter(KisDocument *doc);
    KraConverter(KisDocument *doc, PkPointer<KoUpdater> updater, KisImportUserFeedbackInterface *feedbackInterface = nullptr);
    ~KraConverter() override;

    KisImportExportErrorCode buildImage(PkStream *io);
    KisImportExportErrorCode buildFile(PkStream *io, const PkString &filename, bool addMergedImage = true);
    /**
     * Retrieve the constructed image
     */
    KisImageSP image();
    vKisNodeSP activeNodes();
    PkList<KisPaintingAssistantSP> assistants();
    StoryboardItemList storyboardItemList();
    StoryboardCommentList storyboardCommentList();

    virtual void cancel();

private:

    KisImportExportErrorCode saveRootDocuments(KoStore *store);
    bool saveToStream(PkStream *dev);
    PkXmlDocument createDomDocument();
    KisImportExportErrorCode savePreview(KoStore *store);
    KisImportExportErrorCode oldLoadAndParse(KoStore *store, const PkString &filename, PkXmlDocument &xmldoc);
    KisImportExportErrorCode loadXML(const PkXmlDocument &doc, KoStore *store);
    bool completeLoading(KoStore *store);

    void setProgress(int progress);

    KisDocument *m_doc {0};
    KisImageSP m_image;

    vKisNodeSP m_activeNodes;
    PkList<KisPaintingAssistantSP> m_assistants;
    StoryboardItemList m_storyboardItemList;
    StoryboardCommentList m_storyboardCommentList;
    bool m_stop {false};

    KoStore *m_store {0};
    KisKraSaver *m_kraSaver {0};
    KisKraLoader *m_kraLoader {0};
    PkPointer<KoUpdater> m_updater;
    KisImportUserFeedbackInterface *m_feedbackInterface {nullptr};
};

#endif
