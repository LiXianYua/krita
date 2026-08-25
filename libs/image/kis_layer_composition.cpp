/*
 *  SPDX-FileCopyrightText: 2012 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_layer_composition.h"
#include "kis_node_visitor.h"
#include "kis_group_layer.h"
#include "kis_adjustment_layer.h"
#include "kis_external_layer_iface.h"
#include "kis_paint_layer.h"
#include "generator/kis_generator_layer.h"
#include "kis_clone_layer.h"
#include "kis_filter_mask.h"
#include "kis_transform_mask.h"
#include "kis_transparency_mask.h"
#include "kis_selection_mask.h"
#include "lazybrush/kis_colorize_mask.h"
#include "kis_layer_utils.h"
#include "kis_node_query_path.h"

#include <PkXmlDocument.h>
#include <PkXmlElement.h>

class KisCompositionVisitor : public KisNodeVisitor {
public:
    enum Mode {
        STORE,
        APPLY
    };
    
    KisCompositionVisitor(KisLayerComposition* layerComposition, Mode mode)
        : m_layerComposition(layerComposition)
        , m_mode(mode)
    {        
    }

    bool visit(KisNode* node) override { return process(node); }
    bool visit(KisGroupLayer* layer) override
    { 
        bool result = visitAll(layer);
        if(layer == layer->image()->rootLayer()) {
            return result;
        }        
        return result && process(layer);
    }
    bool visit(KisAdjustmentLayer* layer) override { return process(layer); }
    bool visit(KisPaintLayer* layer) override { return process(layer); }
    bool visit(KisExternalLayer* layer) override { return process(layer); }
    bool visit(KisGeneratorLayer* layer) override { return process(layer); }
    bool visit(KisCloneLayer* layer) override { return process(layer); }
    bool visit(KisFilterMask* mask) override { return process(mask); }
    bool visit(KisTransformMask* mask) override { return process(mask); }
    bool visit(KisTransparencyMask* mask) override { return process(mask); }
    bool visit(KisSelectionMask* mask) override { return process(mask); }
    bool visit(KisColorizeMask* mask) override { return process(mask); }

    bool process(KisNode* node) {
        if (node->isFakeNode()) {
            dbgKrita << "Compositions: Skipping over Fake Node" << node->uuid() << node->name();
            return true;
        }

        bool result = visitAll(node);

        if(m_mode == STORE) {
            m_layerComposition->m_visibilityMap[node->uuid()] = node->visible();
            m_layerComposition->m_collapsedMap[node->uuid()] = node->collapsed();
        } else {
            bool newState = false;
            if(m_layerComposition->m_visibilityMap.contains(node->uuid())) {
                newState = m_layerComposition->m_visibilityMap[node->uuid()];
            }
            if(node->visible() != newState) {
                node->setVisible(m_layerComposition->m_visibilityMap[node->uuid()]);
                node->setDirty();
            }
            if(m_layerComposition->m_collapsedMap.contains(node->uuid())) {
                node->setCollapsed(m_layerComposition->m_collapsedMap[node->uuid()]);
            }
        }
        
        return result;
    }
private:
    KisLayerComposition* m_layerComposition;
    Mode m_mode;
};

KisLayerComposition::KisLayerComposition(KisImageWSP image, const PkString& name)
    : m_image(image)
    , m_name(name)
    , m_exportEnabled(true)
{

}

KisLayerComposition::~KisLayerComposition()
{

}

KisLayerComposition::KisLayerComposition(const KisLayerComposition &rhs, KisImageWSP otherImage)
    : m_image(otherImage ? otherImage : rhs.m_image),
      m_name(rhs.m_name),
      m_exportEnabled(rhs.m_exportEnabled)
{
    {
        auto it = rhs.m_visibilityMap.constBegin();
        for (; it != rhs.m_visibilityMap.constEnd(); ++it) {
            PkNodeId nodeUuid = it.key();
            KisNodeSP node = KisLayerUtils::findNodeByUuid(rhs.m_image->root(), nodeUuid);
            if (node) {
                KisNodeQueryPath path = KisNodeQueryPath::absolutePath(node);
                KisNodeSP newNode = path.queryUniqueNode(m_image);
                KIS_ASSERT_RECOVER(newNode) { continue; }

                m_visibilityMap.insert(newNode->uuid(), it.value());
            }
        }
    }

    {
        auto it = rhs.m_collapsedMap.constBegin();
        for (; it != rhs.m_collapsedMap.constEnd(); ++it) {
            PkNodeId nodeUuid = it.key();
            KisNodeSP node = KisLayerUtils::findNodeByUuid(rhs.m_image->root(), nodeUuid);
            if (node) {
                KisNodeQueryPath path = KisNodeQueryPath::absolutePath(node);
                KisNodeSP newNode = path.queryUniqueNode(m_image);
                KIS_ASSERT_RECOVER(newNode) { continue; }

                m_collapsedMap.insert(newNode->uuid(), it.value());
            }
        }
    }
}

void KisLayerComposition::setName(const PkString& name)
{
    m_name = name;
}

PkString KisLayerComposition::name()
{
    return m_name;
}

void KisLayerComposition::store()
{
    if(m_image.isNull()) {
        return;
    }
    KisCompositionVisitor visitor(this, KisCompositionVisitor::STORE);
    m_image->rootLayer()->accept(visitor);
}

void KisLayerComposition::apply()
{
    if (m_image.isNull()) {
        return;
    }
    KisCompositionVisitor visitor(this, KisCompositionVisitor::APPLY);
    m_image->rootLayer()->accept(visitor);
}

void KisLayerComposition::setExportEnabled ( bool enabled )
{
    m_exportEnabled = enabled;
}

bool KisLayerComposition::isExportEnabled()
{
    return m_exportEnabled;
}

void KisLayerComposition::setVisible(PkNodeId id, bool visible)
{
    m_visibilityMap[id] = visible;
}

void KisLayerComposition::setCollapsed ( PkNodeId id, bool collapsed )
{
    m_collapsedMap[id] = collapsed;
}

void KisLayerComposition::save(PkXmlDocument& doc, PkXmlElement& element)
{
    PkXmlElement compositionElement = doc.createElement("composition");
    compositionElement.setAttribute("name", m_name);
    compositionElement.setAttribute("exportEnabled", PkString(m_exportEnabled ? "1" : "0"));
    auto iter = m_visibilityMap.constBegin();
    while (iter != m_visibilityMap.constEnd()) {
        PkXmlElement valueElement = doc.createElement("value");
        dbgKrita << "uuid" << iter.key().toString() << "visible" <<  iter.value();
        valueElement.setAttribute("uuid", iter.key().toString());
        valueElement.setAttribute("visible", PkString(iter.value() ? "1" : "0"));
        dbgKrita << "contains" << m_collapsedMap.contains(iter.key());
        if (m_collapsedMap.contains(iter.key())) {
            dbgKrita << "collapsed :" << m_collapsedMap[iter.key()];
            valueElement.setAttribute("collapsed", PkString(m_collapsedMap[iter.key()] ? "1" : "0"));
        }
        compositionElement.appendChild(valueElement);
        ++iter;
    }
    element.appendChild(compositionElement);
}
