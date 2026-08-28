/*
 *  SPDX-FileCopyrightText: 2015 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ANIMATION_IMPORTER_H
#define KIS_ANIMATION_IMPORTER_H

#include <KoUpdater.h>

#include "kis_types.h"
#include "kritaanimation_export.h"
#include <KisImportExportErrorCode.h>
#include <PkPair.h>
#include <PkList.h>
#include <PkScopedPointer.h>
#include <PkStringList.h>

class KisDocument;

class KRITAANIMATION_EXPORT KisAnimationImporter
{
public:
    KisAnimationImporter(KisImageSP image,
                         KoUpdaterPtr updater = {},
                         bool trimFrames = false);
    ~KisAnimationImporter();

    KisImportExportErrorCode import(PkStringList files
                                    , int firstFrame
                                    , int step
                                    , bool autoAddHoldframes = false
                                    , bool startfrom0 = false
                                    , int isAscending = 0
                                    , bool assignDocumentProfile = false
                                    , PkList<int> optionalKeyframeTimeList = {});

private:
    PkPair<KisPaintLayerSP, class KisRasterKeyframeChannel*> initializePaintLayer(
        PkScopedPointer<KisDocument>& doc,
        class KisUndoAdapter* undoAdapter);

private:
    void cancel();

private:
    struct Private;
    PkScopedPointer<Private> m_d;
};

#endif
