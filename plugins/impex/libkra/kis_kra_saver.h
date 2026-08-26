/* This file is part of the KDE project
 * Copyright 2008 (C) Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KIS_KRA_SAVER
#define KIS_KRA_SAVER

#include <kis_types.h>

#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkStringList.h>
#include <PkString.h>

class KisDocument;
class KoStore;


#include "kritalibkra_export.h"
#include "KoColor.h"

class KRITALIBKRA_EXPORT KisKraSaver
{
public:

    KisKraSaver(KisDocument* document, const PkString &filename, bool addMergedImage = true);

    ~KisKraSaver();

    PkXmlElement saveXML(PkXmlDocument& doc,  KisImageSP image);

    bool saveKeyframes(KoStore *store, const PkString &uri, bool external);

    bool saveBinaryData(KoStore* store, KisImageSP image, const PkString & uri, bool external, bool addMergedImage);

    bool saveResources(KoStore *store, KisImageSP image, const PkString &uri);

    bool saveStoryboard(KoStore *store, KisImageSP image, const PkString &uri);

    bool saveAnimationMetadata(KoStore *store, KisImageSP image, const PkString &uri);

    bool saveAudio(KoStore *store);

    /// @return a list with everything that went wrong while saving
    PkStringList errorMessages() const;

    /// @return a list with non-critical issues that happened while saving
    PkStringList warningMessages() const;

private:
    void saveBackgroundColor(PkXmlDocument& doc, PkXmlElement& element, KisImageSP image);
    void saveAssistantsGlobalColor(PkXmlDocument& doc, PkXmlElement& element);
    void saveWarningColor(PkXmlDocument& doc, PkXmlElement& element, KisImageSP image);
    void saveCompositions(PkXmlDocument& doc, PkXmlElement& element, KisImageSP image);
    bool saveAssistants(KoStore *store,PkString uri, bool external);
    bool saveAssistantsList(PkXmlDocument& doc, PkXmlElement& element);
    bool saveGrid(PkXmlDocument& doc, PkXmlElement& element);
    bool saveGuides(PkXmlDocument& doc, PkXmlElement& element);
    bool saveMirrorAxis(PkXmlDocument& doc, PkXmlElement& element);
    bool saveAudioXML(PkXmlDocument& doc, PkXmlElement& element);
    bool saveNodeKeyframes(KoStore *store, PkString location, const KisNode *node);
    void saveResourcesToXML(PkXmlDocument& doc, PkXmlElement &element);
    void saveStoryboardToXML(PkXmlDocument& doc, PkXmlElement &element);
    void saveAnimationMetadataToXML(PkXmlDocument& doc, PkXmlElement &element, KisImageSP image);
    void saveColorHistory(PkXmlDocument &doc, PkXmlElement &element);

    bool saveKoColors(PkXmlDocument& doc, PkXmlElement &element, const PkList<KoColor> &colors) const;

    struct Private;
    Private * const m_d;
};

#endif
