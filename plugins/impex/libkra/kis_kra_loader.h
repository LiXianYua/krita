/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KIS_KRA_LOADER_H
#define KIS_KRA_LOADER_H


#include <PkStringList.h>
#include <PkString.h>
#include "PkXmlDocument.h"

class KoStore;

class KisDocument;
class KisImportUserFeedbackInterface;
class KoColorSpace;
class KisPaintingAssistant;
class StoryboardComment;
class PkVersionNumber;

#include <kis_types.h>
#include "kritalibkra_export.h"
#include "KoColor.h"

/**
 * Load old-style 1.x .kra files. Updated for 2.0, let's try to stay
 * compatible. But 2.0 won't be able to save 1.x .kra files unless we
 * implement an export filter.
 */
class KRITALIBKRA_EXPORT KisKraLoader
{

public:

    KisKraLoader(KisDocument * document, int syntaxVersion, const PkVersionNumber &kritaVersion, KisImportUserFeedbackInterface *feedbackInterface = nullptr);

    ~KisKraLoader();

    /**
     * Loading is done in two steps: first all xml is loaded, then, in finishLoading,
     * the actual layer data is loaded.
     */
    KisImageSP loadXML(const PkXmlElement& imageElement);

    void loadBinaryData(KoStore* store, KisImageSP image, const PkString & uri, bool external);

    void loadResources(KoStore *store, KisDocument *doc);
    void loadStoryboards(KoStore *store, KisDocument *doc);
    void loadAnimationMetadata(KoStore *store, KisImageSP image);
    void loadAudio(KoStore *store, KisDocument *kisDoc);
    Q_DECL_DEPRECATED void backCompat_loadAudio(const PkXmlElement &elem, KisDocument *document);

    vKisNodeSP selectedNodes() const;

    // it's neater to follow the same design as with selectedNodes, so let's have a getter here
    PkList<KisPaintingAssistantSP> assistants() const;

    StoryboardItemList storyboardItemList() const;

    StoryboardCommentList storyboardCommentList() const;

    /// if empty, loading didn't fail...
    PkStringList errorMessages() const;

    /// if not empty, loading didn't fail, but there are problems
    PkStringList warningMessages() const;

    /// Returns the name of the image as defined in maindoc.xml. This might
    /// be different from the name of the image as used in the path to the
    /// layers, because before Krita 4.2, under some circumstances, this
    /// string is in utf8, but the paths were stored in a different encoding.
    PkString imageName() const;

private:

    // this needs to be private, for neatness sake
    void loadAssistants(KoStore* store, const PkString & uri, bool external);

    void loadAnimationMetadataFromXML(const PkXmlElement& element, KisImageSP image);

    KisNodeSP loadNodes(const PkXmlElement& element, KisImageSP image, KisNodeSP parent);

    KisNodeSP loadNode(const PkXmlElement& elem, KisImageSP image);

    KisNodeSP loadPaintLayer(const PkXmlElement& elem, KisImageSP image, const PkString& name, const KoColorSpace* cs, quint32 opacity);

    KisNodeSP loadGroupLayer(const PkXmlElement& elem, KisImageSP image, const PkString& name, const KoColorSpace* cs, quint32 opacity);

    KisNodeSP loadAdjustmentLayer(const PkXmlElement& elem, KisImageSP image, const PkString& name, const KoColorSpace* cs, quint32 opacity);

    KisNodeSP loadShapeLayer(const PkXmlElement& elem, KisImageSP image, const PkString& name, const KoColorSpace* cs, quint32 opacity);

    KisNodeSP loadGeneratorLayer(const PkXmlElement& elem, KisImageSP image, const PkString& name, const KoColorSpace* cs, quint32 opacity);

    KisNodeSP loadCloneLayer(const PkXmlElement& elem, KisImageSP image, const PkString& name, const KoColorSpace* cs, quint32 opacity);

    KisNodeSP loadFilterMask(KisImageSP image, const PkXmlElement& elem);

    KisNodeSP loadTransformMask(KisImageSP image, const PkXmlElement& elem);

    KisNodeSP loadTransparencyMask(KisImageSP image, const PkXmlElement& elem);

    KisNodeSP loadSelectionMask(KisImageSP image, const PkXmlElement& elem);

    KisNodeSP loadColorizeMask(KisImageSP image, const PkXmlElement& elem, const KoColorSpace *colorSpace);

    KisNodeSP loadFileLayer(const PkXmlElement& elem, KisImageSP image, const PkString& name, quint32 opacity, const KoColorSpace *fallbackColorSpace);

    KisNodeSP loadReferenceImagesLayer(const PkXmlElement& elem, KisImageSP image);

    void loadNodeKeyframes(KoStore *store, const PkString &location, KisNodeSP node);

    void loadCompositions(const PkXmlElement& elem, KisImageSP image);

    void loadAssistantsList(const PkXmlElement& elem);
    void loadGrid(const PkXmlElement& elem);
    void loadGuides(const PkXmlElement& elem);
    void loadMirrorAxis(const PkXmlElement& elem);
    void loadStoryboardItemList(const PkXmlElement& elem);
    void loadStoryboardCommentList(const PkXmlElement& elem);
    void loadAudioXML(PkXmlDocument& xmlDoc, PkXmlElement &xmlElement, KisDocument* kisDoc);
    PkList<KoColor> loadKoColors(const PkXmlElement& elem) const;
private:

    struct Private;
    Private * const m_d;

};

#endif
