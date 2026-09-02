/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_kra_savexml_visitor.h"
#include "kis_kra_tags.h"
#include "kis_kra_utils.h"
#include "kis_layer_properties_icons.h"

#include <filesystem>

#include <PkNodeId.h>
#include <PkFlakeBridge.h>
#include <KoProperties.h>
#include <KoColorSpace.h>
#include <KoCompositeOp.h>
#include <KoColorProfile.h>

#include <kis_debug.h>
#include <filter/kis_filter_configuration.h>
#include <generator/kis_generator_layer.h>
#include <kis_adjustment_layer.h>
#include <kis_clone_layer.h>
#include <kis_filter_mask.h>
#include <kis_transform_mask.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
#include <kis_selection_mask.h>
#include <kis_shape_layer.h>
#include <kis_transparency_mask.h>
#include <lazybrush/kis_colorize_mask.h>
#include <kis_file_layer.h>
#include <kis_psd_layer_style.h>
#include <KisReferenceImage.h>
#include <KisReferenceImagesLayer.h>
#include "kis_keyframe_channel.h"
#include "kis_dom_utils.h"

using namespace KRA;

KisSaveXmlVisitor::KisSaveXmlVisitor(PkXmlDocument doc, const PkXmlElement & element, quint32 &count, const PkString &url, bool root)
    : KisNodeVisitor()
    , m_doc(doc)
    , m_count(count)
    , m_url(url)
    , m_root(root)
{
    Q_ASSERT(!element.isNull());
    m_elem = element;
}

void KisSaveXmlVisitor::setSelectedNodes(vKisNodeSP selectedNodes)
{
    m_selectedNodes = selectedNodes;
}

PkStringList KisSaveXmlVisitor::errorMessages() const
{
    return m_errorMessages;
}

bool KisSaveXmlVisitor::visit(KisExternalLayer * layer)
{
    if (layer->inherits("KisReferenceImagesLayer")) {
        return saveReferenceImagesLayer(layer);
    } else if (layer->inherits("KisShapeLayer")) {
        PkXmlElement layerElement = m_doc.createElement(LAYER);
        saveLayer(layerElement, SHAPE_LAYER, layer);
        m_elem.appendChild(layerElement);
        m_count++;
        return saveMasks(layer, layerElement);
    }
    else if (layer->inherits("KisFileLayer")) {
        PkXmlElement layerElement = m_doc.createElement(LAYER);
        saveLayer(layerElement, FILE_LAYER, layer);

        KisFileLayer *fileLayer = dynamic_cast<KisFileLayer*>(layer);
        KIS_ASSERT(fileLayer);

        PkString path = toPkString(fileLayer->path());

#ifndef Q_OS_ANDROID
        // 对拍 relativeFilePath：计算 source 相对 .kra 所在目录的相对路径。
        // std::filesystem::relative 无法相对（跨盘/异根）时返回规范化后的原路径，
        // 与尽量短相对路径的行为一致。
        const std::string sourcePath =
            std::filesystem::relative(std::filesystem::path(path.PkToUtf8()),
                                      std::filesystem::absolute(m_url.PkToUtf8()).parent_path())
                .string();
        layerElement.setAttribute("source", PkString::PkFromUtf8(sourcePath.c_str(), static_cast<int>(sourcePath.size())));
#else
        layerElement.setAttribute("source", path);
#endif

        if (fileLayer->scalingMethod() == KisFileLayer::ToImagePPI) {
            layerElement.setAttribute("scale", "true");
        }
        else {
            layerElement.setAttribute("scale", "false");
        }
        layerElement.setAttribute("scalingmethod", PkString("%1").arg((int)fileLayer->scalingMethod()));
        layerElement.setAttribute(COLORSPACE_NAME, layer->original()->colorSpace()->id());
        layerElement.setAttribute("scalingfilter", toPkString(fileLayer->scalingFilter()));

        m_elem.appendChild(layerElement);
        m_count++;
        return saveMasks(layer, layerElement);
    }
    return false;
}

PkXmlElement KisSaveXmlVisitor::savePaintLayerAttributes(KisPaintLayer *layer, PkXmlDocument &doc, bool saveLayerOffset)
{
    PkXmlElement element = doc.createElement(LAYER);
    saveLayer(element, PAINT_LAYER, layer);
    element.setAttribute(CHANNEL_LOCK_FLAGS, flagsToString(layer->channelLockFlags()));
    element.setAttribute(COLORSPACE_NAME, layer->paintDevice()->colorSpace()->id());

    element.setAttribute(ONION_SKIN_ENABLED, PkString(layer->onionSkinEnabled() ? "1" : "0"));
    element.setAttribute(VISIBLE_IN_TIMELINE, PkString(layer->isPinnedToTimeline() ? "1" : "0"));

    if (!saveLayerOffset) {
        element.removeAttribute(X);
        element.removeAttribute(Y);
    }

    return element;
}

void KisSaveXmlVisitor::loadPaintLayerAttributes(const PkXmlElement &el, KisPaintLayer *layer, bool loadLayerOffset)
{
    PkXmlElement copy = el;

    if (!loadLayerOffset) {
        copy.removeAttribute(X);
        copy.removeAttribute(Y);
    }

    loadLayerAttributes(copy, layer);

    if (copy.hasAttribute(CHANNEL_LOCK_FLAGS)) {
        layer->setChannelLockFlags(stringToFlags(copy.attribute(CHANNEL_LOCK_FLAGS)));
    }
}

bool KisSaveXmlVisitor::visit(KisPaintLayer *layer)
{
    PkXmlElement layerElement = savePaintLayerAttributes(layer, m_doc, true);
    m_elem.appendChild(layerElement);
    m_count++;
    return saveMasks(layer, layerElement);
}

bool KisSaveXmlVisitor::visit(KisGroupLayer *layer)
{
    PkXmlElement layerElement;

    if (m_root) // if this is the root we fake so not to save it
        layerElement = m_elem;
    else {
        layerElement = m_doc.createElement(LAYER);
        saveLayer(layerElement, GROUP_LAYER, layer);
        layerElement.setAttribute(PASS_THROUGH_MODE, PkString(layer->passThroughMode() ? "1" : "0"));
        layerElement.setAttribute(COLORSPACE_NAME, layer->colorSpace()->id());
        layerElement.setAttribute(PROFILE, layer->colorSpace()->profile()->name());
        m_elem.appendChild(layerElement);
    }
    PkXmlElement elem = m_doc.createElement(LAYERS);
    Q_ASSERT(!layerElement.isNull());
    layerElement.appendChild(elem);
    KisSaveXmlVisitor visitor(m_doc, elem, m_count, m_url, false);
    visitor.setSelectedNodes(m_selectedNodes);
    m_count++;
    bool success = visitor.visitAllInverse(layer);

    m_errorMessages.append(visitor.errorMessages());
    if (!m_errorMessages.isEmpty()) {
        return false;
    }

    const PkMap<const KisNode*, PkString> nodeFileNames = visitor.nodeFileNames();
    for (auto it = nodeFileNames.cbegin(); it != nodeFileNames.cend(); ++it) {
        m_nodeFileNames[it.key()] = it.value();
    }

    const PkMap<const KisNode*, PkString> keyframeFileNames = visitor.keyframeFileNames();
    for (auto it = keyframeFileNames.cbegin(); it != keyframeFileNames.cend(); ++it) {
        m_keyframeFileNames[it.key()] = it.value();
    }

    return success;
}

bool KisSaveXmlVisitor::visit(KisAdjustmentLayer* layer)
{
    if (!layer->filter()) {
        return false;
    }
    PkXmlElement layerElement = m_doc.createElement(LAYER);
    saveLayer(layerElement, ADJUSTMENT_LAYER, layer);
    layerElement.setAttribute(FILTER_NAME, layer->filter()->name());
    layerElement.setAttribute(FILTER_VERSION, PkString("%1").arg(layer->filter()->version()));
    m_elem.appendChild(layerElement);

    m_count++;
    return saveMasks(layer, layerElement);
}

bool KisSaveXmlVisitor::visit(KisGeneratorLayer *layer)
{
    PkXmlElement layerElement = m_doc.createElement(LAYER);
    saveLayer(layerElement, GENERATOR_LAYER, layer);
    layerElement.setAttribute(GENERATOR_NAME, layer->filter()->name());
    layerElement.setAttribute(GENERATOR_VERSION, PkString("%1").arg(layer->filter()->version()));
    m_elem.appendChild(layerElement);

    m_count++;
    return saveMasks(layer, layerElement);
}

bool KisSaveXmlVisitor::visit(KisCloneLayer *layer)
{
    PkXmlElement layerElement = m_doc.createElement(LAYER);
    saveLayer(layerElement, CLONE_LAYER, layer);
    layerElement.setAttribute(CLONE_FROM, layer->copyFromInfo().name());
    layerElement.setAttribute(CLONE_FROM_UUID, layer->copyFromInfo().uuid().toString());
    layerElement.setAttribute(CLONE_TYPE, PkString("%1").arg((int)layer->copyType()));
    m_elem.appendChild(layerElement);

    m_count++;
    return saveMasks(layer, layerElement);
}

bool KisSaveXmlVisitor::visit(KisFilterMask *mask)
{
    Q_ASSERT(mask);
    if (!mask->filter()) {
        return false;
    }
    PkXmlElement el = m_doc.createElement(MASK);
    saveMask(el, FILTER_MASK, mask);
    el.setAttribute(FILTER_NAME, mask->filter()->name());
    el.setAttribute(FILTER_VERSION, PkString("%1").arg(mask->filter()->version()));

    m_elem.appendChild(el);

    m_count++;
    return true;
}

bool KisSaveXmlVisitor::visit(KisTransformMask *mask)
{
    Q_ASSERT(mask);

    PkXmlElement el = m_doc.createElement(MASK);
    saveMask(el, TRANSFORM_MASK, mask);

    m_elem.appendChild(el);

    m_count++;
    return true;
}

bool KisSaveXmlVisitor::visit(KisTransparencyMask *mask)
{
    Q_ASSERT(mask);
    PkXmlElement el = m_doc.createElement(MASK);
    saveMask(el, TRANSPARENCY_MASK, mask);
    m_elem.appendChild(el);
    m_count++;
    return true;
}

bool KisSaveXmlVisitor::visit(KisColorizeMask *mask)
{
    Q_ASSERT(mask);
    PkXmlElement el = m_doc.createElement(MASK);
    saveMask(el, COLORIZE_MASK, mask);
    m_elem.appendChild(el);
    m_count++;
    return true;
}

bool KisSaveXmlVisitor::visit(KisSelectionMask *mask)
{
    Q_ASSERT(mask);

    PkXmlElement el = m_doc.createElement(MASK);
    saveMask(el, SELECTION_MASK, mask);
    m_elem.appendChild(el);
    m_count++;
    return true;
}


void KisSaveXmlVisitor::loadLayerAttributes(const PkXmlElement &el, KisLayer *layer)
{
    if (el.hasAttribute(NAME)) {
        PkString layerName = el.attribute(NAME);
        if (layerName != layer->name()) {
            // Make the EXR layername leading in case of conflicts
            layer->setName(layerName);
        }
    }

    if (el.hasAttribute(CHANNEL_FLAGS)) {
        layer->setChannelFlags(stringToFlags(el.attribute(CHANNEL_FLAGS)));
    }

    if (el.hasAttribute(OPACITY)) {
        layer->setOpacity(el.attribute(OPACITY).toInt());
    }

    if (el.hasAttribute(COMPOSITE_OP)) {
        layer->setCompositeOpId(el.attribute(COMPOSITE_OP));
    }

    if (el.hasAttribute(VISIBLE)) {
        layer->setVisible(el.attribute(VISIBLE).toInt());
    }

    if (el.hasAttribute(LOCKED)) {
        layer->setUserLocked(el.attribute(LOCKED).toInt());
    }

    if (el.hasAttribute(X)) {
        layer->setX(el.attribute(X).toInt());
    }

    if (el.hasAttribute(Y)) {
        layer->setY(el.attribute(Y).toInt());
    }

    if (el.hasAttribute(UUID)) {
        layer->setUuid(PkNodeId::fromString(el.attribute(UUID)));
    }

    if (el.hasAttribute(COLLAPSED)) {
        layer->setCollapsed(el.attribute(COLLAPSED).toInt());
    }

    if (el.hasAttribute(COLOR_LABEL)) {
        layer->setColorLabelIndex(el.attribute(COLOR_LABEL).toInt());
    }

    if (el.hasAttribute(VISIBLE_IN_TIMELINE)) {
        layer->setPinnedToTimeline(el.attribute(VISIBLE_IN_TIMELINE).toInt());
    }

    if (el.hasAttribute(LAYER_STYLE_UUID)) {
        PkString uuidString = el.attribute(LAYER_STYLE_UUID);
        PkNodeId uuid = PkNodeId::fromString(uuidString);
        if (!uuid.isNull()) {
            KisPSDLayerStyleSP dumbLayerStyle(new KisPSDLayerStyle());
            dumbLayerStyle->setUuid(uuid);
            layer->setLayerStyle(dumbLayerStyle);
        } else {
            warnKrita << "WARNING: Layer style for layer" << layer->name() << "contains invalid UUID" << uuidString;
        }
    }

    if (layer->inherits("KisShapeLayer") && el.hasAttribute(ANTIALIASED)) {
        KisShapeLayer *shapeLayer = static_cast<KisShapeLayer*>(layer);
        shapeLayer->setAntialiased(el.attribute(ANTIALIASED).toInt());
    }
}

void KisSaveXmlVisitor::saveNodeKeyframes(const KisNode* node, PkString nodeFilename, PkXmlElement& nodeElement)
{
    if (node->isAnimated()) {
        PkString keyframeFile = nodeFilename + ".keyframes.xml";
        m_keyframeFileNames[node] = keyframeFile;
        nodeElement.setAttribute(KEYFRAME_FILE, keyframeFile);
    }
}

void KisSaveXmlVisitor::saveLayer(PkXmlElement & el, const PkString & layerType, const KisLayer * layer)
{
    PkString filename = LAYER + PkString("%1").arg((int)m_count);

    el.setAttribute(CHANNEL_FLAGS, flagsToString(layer->channelFlags()));
    el.setAttribute(NAME, layer->name());
    el.setAttribute(OPACITY, PkString("%1").arg((int)layer->opacity()));
    el.setAttribute(COMPOSITE_OP, layer->compositeOp()->id());
    el.setAttribute(VISIBLE, PkString(layer->visible() ? "1" : "0"));
    el.setAttribute(LOCKED, PkString(layer->userLocked() ? "1" : "0"));
    el.setAttribute(NODE_TYPE, layerType);
    el.setAttribute(FILE_NAME, filename);
    el.setAttribute(X, PkString("%1").arg(layer->x()));
    el.setAttribute(Y, PkString("%1").arg(layer->y()));
    el.setAttribute(UUID, layer->uuid().toString());
    el.setAttribute(COLLAPSED, PkString(layer->collapsed() ? "1" : "0"));
    el.setAttribute(COLOR_LABEL, PkString("%1").arg(layer->colorLabelIndex()));
    el.setAttribute(VISIBLE_IN_TIMELINE, PkString(layer->isPinnedToTimeline() ? "1" : "0"));

    if(layerType == SHAPE_LAYER) {
        const KisShapeLayer *shapeLayer = static_cast<const KisShapeLayer*>(layer);
        el.setAttribute(ANTIALIASED, PkString(shapeLayer->antialiased() ? "1" : "0"));
    }

    if (layer->layerStyle()) {
        el.setAttribute(LAYER_STYLE_UUID, layer->layerStyle()->uuid().toString());
    }

    Q_FOREACH (KisNodeSP node, m_selectedNodes) {
        if (node.data() == layer) {
            el.setAttribute("selected", "true");
            break;
        }
    }

    saveNodeKeyframes(layer, filename, el);

    m_nodeFileNames[layer] = filename;

    dbgFile << "Saved layer "
            << layer->name()
            << " of type " << layerType
            << " with filename " << LAYER + PkString("%1").arg((int)m_count);
}

void KisSaveXmlVisitor::saveMask(PkXmlElement & el, const PkString & maskType, const KisMaskSP mask)
{
    PkString filename = MASK + PkString("%1").arg((int)m_count);

    el.setAttribute(NAME, mask->name());
    el.setAttribute(VISIBLE, PkString(mask->visible() ? "1" : "0"));
    el.setAttribute(LOCKED, PkString(mask->userLocked() ? "1" : "0"));
    el.setAttribute(NODE_TYPE, maskType);
    el.setAttribute(FILE_NAME, filename);
    el.setAttribute(X, PkString("%1").arg(mask->x()));
    el.setAttribute(Y, PkString("%1").arg(mask->y()));
    el.setAttribute(UUID, mask->uuid().toString());
    el.setAttribute(COLOR_LABEL, PkString("%1").arg(mask->colorLabelIndex()));
    el.setAttribute(VISIBLE_IN_TIMELINE, PkString(mask->isPinnedToTimeline() ? "1" : "0"));

    if (maskType == SELECTION_MASK) {
        el.setAttribute(ACTIVE, PkString(mask->nodeProperties().boolProperty("active") ? "1" : "0"));
    } else if (maskType == COLORIZE_MASK) {
        el.setAttribute(COLORSPACE_NAME, mask->colorSpace()->id());
        el.setAttribute(COMPOSITE_OP, mask->compositeOpId());
        el.setAttribute(COLORIZE_EDIT_KEYSTROKES, PkString(KisLayerPropertiesIcons::nodeProperty(mask, KisLayerPropertiesIcons::colorizeEditKeyStrokes, true).toBool() ? "1" : "0"));
        el.setAttribute(COLORIZE_SHOW_COLORING, PkString(KisLayerPropertiesIcons::nodeProperty(mask, KisLayerPropertiesIcons::colorizeShowColoring, true).toBool() ? "1" : "0"));

        const KisColorizeMask *colorizeMask = dynamic_cast<const KisColorizeMask*>(mask.data());
        KIS_SAFE_ASSERT_RECOVER_NOOP(colorizeMask);

        if (colorizeMask) {
            el.setAttribute(COLORIZE_USE_EDGE_DETECTION, PkString(colorizeMask->useEdgeDetection() ? "1" : "0"));
            el.setAttribute(COLORIZE_EDGE_DETECTION_SIZE, KisDomUtils::toString(colorizeMask->edgeDetectionSize()));
            el.setAttribute(COLORIZE_FUZZY_RADIUS, KisDomUtils::toString(colorizeMask->fuzzyRadius()));
            el.setAttribute(COLORIZE_CLEANUP, PkString("%1").arg(int(100 * colorizeMask->cleanUpAmount())));
            el.setAttribute(COLORIZE_LIMIT_TO_DEVICE, PkString(colorizeMask->limitToDeviceBounds() ? "1" : "0"));
        }
    }

    saveNodeKeyframes(mask, filename, el);

    m_nodeFileNames[mask] = filename;

    dbgFile << "Saved mask "
            << mask->name()
            << " of type " << maskType
            << " with filename " << filename;
}

bool KisSaveXmlVisitor::saveMasks(KisNode * node, PkXmlElement & layerElement)
{
    if (node->childCount() > 0) {
        PkXmlElement elem = m_doc.createElement(MASKS);
        Q_ASSERT(!layerElement.isNull());
        layerElement.appendChild(elem);
        KisSaveXmlVisitor visitor(m_doc, elem, m_count, m_url, false);
        visitor.setSelectedNodes(m_selectedNodes);
        bool success = visitor.visitAllInverse(node);
        m_errorMessages.append(visitor.errorMessages());
        if (!m_errorMessages.isEmpty()) {
            return false;
        }

        const PkMap<const KisNode*, PkString> nodeFileNames = visitor.nodeFileNames();
        for (auto it = nodeFileNames.cbegin(); it != nodeFileNames.cend(); ++it) {
            m_nodeFileNames[it.key()] = it.value();
        }

        const PkMap<const KisNode*, PkString> keyframeFileNames = visitor.keyframeFileNames();
        for (auto it = keyframeFileNames.cbegin(); it != keyframeFileNames.cend(); ++it) {
            m_keyframeFileNames[it.key()] = it.value();
        }

        return success;
    }
    return true;
}

bool KisSaveXmlVisitor::saveReferenceImagesLayer(KisExternalLayer *layer)
{
    auto *referencesLayer = dynamic_cast<KisReferenceImagesLayer*>(layer);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(referencesLayer, false);

    QDomDocument qtDocument;
    QDomElement qtLayerElement = qtDocument.createElement(toQString(LAYER));
    qtLayerElement.setAttribute(toQString(NODE_TYPE), toQString(REFERENCE_IMAGES_LAYER));
    qtDocument.appendChild(qtLayerElement);

    int nextId = 0;
    Q_FOREACH(KoShape *shape, referencesLayer->shapes()) {
        auto *reference = dynamic_cast<KisReferenceImage*>(shape);
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(reference, false);
        reference->saveXml(qtDocument, qtLayerElement, nextId);
        nextId++;
    }

    const PkXmlElement convertedLayerElement = toPkXmlElement(qtLayerElement);
    PkXmlElement layerElement = m_doc.importNode(convertedLayerElement, true).toElement();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!layerElement.isNull(), false);

    m_elem.appendChild(layerElement);
    m_count++;
    return true;
}

