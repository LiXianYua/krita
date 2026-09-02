/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_KRA_SAVEXML_VISITOR_H_
#define KIS_KRA_SAVEXML_VISITOR_H_

#include <QtCore/qnamespace.h>
#include <QtGlobal>
#include <QtCore/qalgorithms.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qpair.h>

#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkStringList.h>
#include <PkMap.h>

#include "kis_node_visitor.h"
#include "kis_types.h"
#include "kritalibkra_export.h"

class KRITALIBKRA_EXPORT KisSaveXmlVisitor : public KisNodeVisitor
{
public:
    KisSaveXmlVisitor(PkXmlDocument doc, const PkXmlElement & element, quint32 &count, const PkString &url, bool root);

    void setSelectedNodes(vKisNodeSP selectedNodes);

    using KisNodeVisitor::visit;

    PkStringList errorMessages() const;

public:

    bool visit(KisNode*) override {
        return true;
    }
    bool visit(KisExternalLayer *) override;
    bool visit(KisPaintLayer *layer) override;
    bool visit(KisGroupLayer *layer) override;
    bool visit(KisAdjustmentLayer* layer) override;
    bool visit(KisGeneratorLayer *layer) override;
    bool visit(KisCloneLayer *layer) override;
    bool visit(KisFilterMask *mask) override;
    bool visit(KisTransformMask *mask) override;
    bool visit(KisTransparencyMask *mask) override;
    bool visit(KisSelectionMask *mask) override;
    bool visit(KisColorizeMask *mask) override;

    PkMap<const KisNode*, PkString> nodeFileNames() {
        return m_nodeFileNames;
    }

    PkMap<const KisNode*, PkString> keyframeFileNames() {
        return m_keyframeFileNames;
    }

public:
    PkXmlElement savePaintLayerAttributes(KisPaintLayer *layer, PkXmlDocument &doc, bool saveLayerOffset);

    // used by EXR to save properties of Krita layers inside .exr
    static void loadPaintLayerAttributes(const PkXmlElement &el, KisPaintLayer *layer, bool loadLayerOffset);

private:
    static void loadLayerAttributes(const PkXmlElement &el, KisLayer *layer);

private:

    void saveLayer(PkXmlElement & el, const PkString & layerType, const KisLayer * layer);
    void saveMask(PkXmlElement & el, const PkString & maskType, const KisMaskSP mask);
    bool saveMasks(KisNode * node, PkXmlElement & layerElement);
    void saveNodeKeyframes(const KisNode *node, PkString filename, PkXmlElement& el);

    friend class KisKraSaveXmlVisitorTest;

    vKisNodeSP m_selectedNodes;
    PkMap<const KisNode*,  PkString> m_nodeFileNames;
    PkMap<const KisNode*,  PkString> m_keyframeFileNames;
    PkXmlDocument m_doc;
    PkXmlElement m_elem;
    quint32 &m_count;
    PkString m_url;
    bool m_root;
    PkStringList m_errorMessages;

    bool saveReferenceImagesLayer(KisExternalLayer *layer);
};

#endif
