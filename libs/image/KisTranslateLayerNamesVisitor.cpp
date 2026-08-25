/*
 *  SPDX-FileCopyrightText: 2020 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisTranslateLayerNamesVisitor.h"

#include "kis_node.h"
#include "kis_paint_layer.h"
#include "kis_group_layer.h"
#include "kis_adjustment_layer.h"
#include "kis_external_layer_iface.h"
#include "kis_clone_layer.h"
#include "kis_filter_mask.h"
#include "kis_transform_mask.h"
#include "kis_transparency_mask.h"
#include "kis_selection_mask.h"
#include "lazybrush/kis_colorize_mask.h"
#include "generator/kis_generator_layer.h"

KisTranslateLayerNamesVisitor::KisTranslateLayerNamesVisitor(PkMap<PkString, PkString> dictionary)
    : m_dictionary(dictionary)
{
    PkMap<PkString, PkString> d = defaultDictionary();
    PkMap<PkString, PkString>::const_iterator i = d.constBegin();
    while (i != d.constEnd()) {
        if (!dictionary.contains(i.key())) {
            dictionary[i.key()] = i.value();
        }
        ++i;
    }
    m_dictionary = dictionary;
}

bool KisTranslateLayerNamesVisitor::translate(KisNode *node)
{
    if (m_dictionary.contains(node->name())) {
        node->setName(m_dictionary[node->name()]);
    }
    // 壳内翻译宏为恒等映射，"Layer"/"layer" 后缀替换无实际效果（PkString 无 replace()），
    // 模板层名翻译走上方 dictionary 查找。
    return true;
}

bool KisTranslateLayerNamesVisitor::visit(KisNode* node) {
    return translate(node);
}

PkMap<PkString, PkString> KisTranslateLayerNamesVisitor::defaultDictionary()
{
    PkMap<PkString, PkString> dictionary;

    dictionary["Background"] = PkString("Background");
    dictionary["Group"] = PkString("Group");
    dictionary["Margins"] = PkString("Margins");
    dictionary["Bleed"] = PkString("Bleed");
    dictionary["Lines"] = PkString("Lines");
    dictionary["Colors"] = PkString("Colors");
    dictionary["Sketch"] = PkString("Sketch");
    dictionary["Shade"] = PkString("Shade");
    dictionary["Filter"] = PkString("Filter");
    dictionary["Mask"] = PkString("Mask");
    dictionary["Layer"] = PkString("Layer");
    dictionary["Indirect light"] = PkString("Indirect light");
    dictionary["Highlight"] = PkString("Highlight");
    dictionary["Flat"] = PkString("Flat");
    dictionary["Panel"] = PkString("Panel");
    dictionary["Text"] = PkString("Text");
    dictionary["Effect"] = PkString("Effect");
    dictionary["Tones"] = PkString("Tones");
    dictionary["Textures"] = PkString("Textures");
    dictionary["Guides"] = PkString("Guides");
    dictionary["Balloons"] = PkString("Balloons");
    dictionary["Clone"] = PkString("Clone");
    dictionary["In Betweening"] = PkString("In Betweening");
    dictionary["Layout"] = PkString("Layout");

    return dictionary;
}

bool KisTranslateLayerNamesVisitor::visit(KisPaintLayer *layer) {
    return translate(layer);
}

bool KisTranslateLayerNamesVisitor::visit(KisGroupLayer *layer) {
    return translate(layer);
}


bool KisTranslateLayerNamesVisitor::visit(KisAdjustmentLayer *layer) {
    return translate(layer);
}


bool KisTranslateLayerNamesVisitor::visit(KisExternalLayer *layer) {
    return translate(layer);
}


bool KisTranslateLayerNamesVisitor::visit(KisCloneLayer *layer) {
    return translate(layer);
}


bool KisTranslateLayerNamesVisitor::visit(KisFilterMask *mask) {
    return translate(mask);
}

bool KisTranslateLayerNamesVisitor::visit(KisTransformMask *mask) {
    return translate(mask);
}

bool KisTranslateLayerNamesVisitor::visit(KisTransparencyMask *mask) {
    return translate(mask);
}


bool KisTranslateLayerNamesVisitor::visit(KisGeneratorLayer * layer) {
    return translate(layer);
}

bool KisTranslateLayerNamesVisitor::visit(KisSelectionMask* mask) {
    return translate(mask);
}

bool KisTranslateLayerNamesVisitor::visit(KisColorizeMask* mask) {
    return translate(mask);
}

