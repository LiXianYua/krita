/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_layer_properties_icons.h"

#include <pk/container/PkMap.h>

Q_GLOBAL_STATIC(KisLayerPropertiesIcons, s_instance)

#include <KoColorSpace.h>
#include <KoColorProfile.h>

#include <kis_node.h>
#include <commands/kis_node_property_list_command.h>
#include "kis_image.h"


const KoID KisLayerPropertiesIcons::locked("locked", PkString("Locked"));
const KoID KisLayerPropertiesIcons::visible("visible", PkString("Visible"));
const KoID KisLayerPropertiesIcons::layerStyle("layer-style", PkString("Layer Style"));
const KoID KisLayerPropertiesIcons::inheritAlpha("inherit-alpha", PkString("Inherit Alpha"));
const KoID KisLayerPropertiesIcons::alphaLocked("alpha-locked", PkString("Alpha Locked"));
const KoID KisLayerPropertiesIcons::onionSkins("onion-skins", PkString("Onion Skins"));
const KoID KisLayerPropertiesIcons::passThrough("pass-through", PkString("Pass Through"));
const KoID KisLayerPropertiesIcons::selectionActive("selection-active", PkString("Active"));
const KoID KisLayerPropertiesIcons::colorLabelIndex("color-label", PkString("Color Label"));
const KoID KisLayerPropertiesIcons::colorOverlay("color-overlay", PkString("Color Overlay"));
const KoID KisLayerPropertiesIcons::colorizeNeedsUpdate("colorize-needs-update", PkString("Update Result"));
const KoID KisLayerPropertiesIcons::colorizeEditKeyStrokes("colorize-show-key-strokes", PkString("Edit Key Strokes"));
const KoID KisLayerPropertiesIcons::colorizeShowColoring("colorize-show-coloring", PkString("Show Coloring"));
const KoID KisLayerPropertiesIcons::openFileLayerFile("open-file-layer-file", PkString("Open File"));
const KoID KisLayerPropertiesIcons::layerError("layer-error", PkString("Error"));
const KoID KisLayerPropertiesIcons::layerColorSpaceMismatch("layer-color-space-mismatch", PkString("Layer Color Space Mismatch"));
const KoID KisLayerPropertiesIcons::antialiased("antialiased", PkString("Anti-aliasing"));

struct IconsPair {
    IconsPair() {}
    IconsPair(const PkIcon &_on, const PkIcon &_off) : on(_on), off(_off) {}

    PkIcon on;
    PkIcon off;

    const PkIcon& getIcon(bool state) {
        return state ? on : off;
    }
};

struct KisLayerPropertiesIcons::Private
{
    PkMap<PkString, IconsPair> icons;
};

namespace {
/**
 * Read-only lookup into the shared icon map.
 *
 * Must not use PkMap::operator[]: KisLayerPropertiesIcons is a Q_GLOBAL_STATIC and
 * getProperty() is reached from image stroke/update jobs as well as from the GUI
 * thread, so an inserting lookup would be a concurrent write to shared state.
 * Missing ids resolve to a pair of null icons.
 */
IconsPair lookupIconsPair(const PkMap<PkString, IconsPair> &icons, const PkString &id)
{
    return icons.value(id);
}
}

KisLayerPropertiesIcons::KisLayerPropertiesIcons()
    : m_d(new Private)
{
    updateIcons();
}

KisLayerPropertiesIcons::~KisLayerPropertiesIcons()
{
}

KisLayerPropertiesIcons *KisLayerPropertiesIcons::instance()
{
    return s_instance;
}

void KisLayerPropertiesIcons::updateIcons()
{
    // Intentionally empty: icons are a UI concern and libs/image must not depend on
    // kritawidgetutils, where the icon loader lives. Property ids, names and states
    // stay here; every property therefore reports a pair of null icons.
    m_d->icons.clear();
}

KisBaseNode::Property KisLayerPropertiesIcons::getProperty(const KoID &id, bool state)
{
    const IconsPair pair = lookupIconsPair(instance()->m_d->icons, id.id());
    return KisBaseNode::Property(id,
                                 pair.on, pair.off, state);
}

KisBaseNode::Property KisLayerPropertiesIcons::getProperty(const KoID &id, bool state,
                                                                       bool isInStasis, bool stateInStasis)
{
    const IconsPair pair = lookupIconsPair(instance()->m_d->icons, id.id());
    return KisBaseNode::Property(id,
                                 pair.on, pair.off, state,
                                 isInStasis, stateInStasis);
}

KisBaseNode::Property KisLayerPropertiesIcons::getErrorProperty(const PkString &message)
{
    const IconsPair pair = lookupIconsPair(instance()->m_d->icons, layerError.id());

    KisBaseNode::Property prop;
    prop.id = layerError.id();
    prop.name =  layerError.name();
    prop.state = message;
    prop.onIcon = pair.on;
    prop.offIcon = pair.off;

    return prop;
}

KisBaseNode::Property KisLayerPropertiesIcons::getColorSpaceMismatchProperty(const KoColorSpace *cs)
{
    const PkString message = PkString(
        "Layer color space is different from the image color space:\n%1 [%2],\noperations may be slow")
        .arg(cs->name(), cs->profile() ? cs->profile()->name() : PkString(""));

    const IconsPair pair = lookupIconsPair(instance()->m_d->icons, layerColorSpaceMismatch.id());

    KisBaseNode::Property prop;
    prop.id = layerColorSpaceMismatch.id();
    prop.name =  layerColorSpaceMismatch.name();
    prop.state = message;
    prop.onIcon = pair.on;
    prop.offIcon = pair.off;

    return prop;
}

void KisLayerPropertiesIcons::setNodePropertyAutoUndo(KisNodeSP node, const KoID &id, const PkVariant &value, KisImageSP image)
{
    KisBaseNode::PropertyList props = node->sectionModelProperties();
    setNodeProperty(&props, id, value);
    KisNodePropertyListCommand::setNodePropertiesAutoUndo(node, image, props);
}

void KisLayerPropertiesIcons::setNodeProperty(KisBaseNode::PropertyList *props, const KoID &id, const PkVariant &value)
{
    KisBaseNode::PropertyList::iterator it = props->begin();
    KisBaseNode::PropertyList::iterator end = props->end();
    for (; it != end; ++it) {
        if (it->id == id.id()) {
            it->state = value;
            break;
        }
    }
}

PkVariant KisLayerPropertiesIcons::nodeProperty(KisNodeSP node, const KoID &id, const PkVariant &defaultValue)
{
    KisBaseNode::PropertyList props = node->sectionModelProperties();

    KisBaseNode::PropertyList::const_iterator it = props.constBegin();
    KisBaseNode::PropertyList::const_iterator end = props.constEnd();
    for (; it != end; ++it) {
        if (it->id == id.id()) {
            return it->state;
        }
    }

    return defaultValue;
}

bool KisLayerPropertiesIcons::isStatelessProperty(const PkString &id)
{
    return
        id == colorizeNeedsUpdate.id() ||
        id == openFileLayerFile.id() ||
        id == layerError.id() ||
        id == layerColorSpaceMismatch.id() ||
        id == colorOverlay.id();
}
