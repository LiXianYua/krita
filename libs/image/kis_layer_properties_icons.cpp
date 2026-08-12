/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_layer_properties_icons.h"

#include <QMap>

#include <QGlobalStatic>
Q_GLOBAL_STATIC(KisLayerPropertiesIcons, s_instance)

#include <KoColorSpace.h>
#include <KoColorProfile.h>

#include <kis_node.h>
#include <commands/kis_node_property_list_command.h>
#include "kis_image.h"


const KoID KisLayerPropertiesIcons::locked("locked", ki18n("Locked"));
const KoID KisLayerPropertiesIcons::visible("visible", ki18n("Visible"));
const KoID KisLayerPropertiesIcons::layerStyle("layer-style", ki18n("Layer Style"));
const KoID KisLayerPropertiesIcons::inheritAlpha("inherit-alpha", ki18n("Inherit Alpha"));
const KoID KisLayerPropertiesIcons::alphaLocked("alpha-locked", ki18n("Alpha Locked"));
const KoID KisLayerPropertiesIcons::onionSkins("onion-skins", ki18n("Onion Skins"));
const KoID KisLayerPropertiesIcons::passThrough("pass-through", ki18n("Pass Through"));
const KoID KisLayerPropertiesIcons::selectionActive("selection-active", ki18n("Active"));
const KoID KisLayerPropertiesIcons::colorLabelIndex("color-label", ki18n("Color Label"));
const KoID KisLayerPropertiesIcons::colorOverlay("color-overlay", ki18n("Color Overlay"));
const KoID KisLayerPropertiesIcons::colorizeNeedsUpdate("colorize-needs-update", ki18n("Update Result"));
const KoID KisLayerPropertiesIcons::colorizeEditKeyStrokes("colorize-show-key-strokes", ki18n("Edit Key Strokes"));
const KoID KisLayerPropertiesIcons::colorizeShowColoring("colorize-show-coloring", ki18n("Show Coloring"));
const KoID KisLayerPropertiesIcons::openFileLayerFile("open-file-layer-file", ki18n("Open File"));
const KoID KisLayerPropertiesIcons::layerError("layer-error", ki18n("Error"));
const KoID KisLayerPropertiesIcons::layerColorSpaceMismatch("layer-color-space-mismatch", ki18n("Layer Color Space Mismatch"));
const KoID KisLayerPropertiesIcons::antialiased("antialiased", ki18n("Anti-aliasing"));

struct IconsPair {
    IconsPair() {}
    IconsPair(const QIcon &_on, const QIcon &_off) : on(_on), off(_off) {}

    QIcon on;
    QIcon off;

    const QIcon& getIcon(bool state) {
        return state ? on : off;
    }
};

struct KisLayerPropertiesIcons::Private
{
    QMap<QString, IconsPair> icons;
};

namespace {
/**
 * Read-only lookup into the shared icon map.
 *
 * Must not use QMap::operator[]: KisLayerPropertiesIcons is a Q_GLOBAL_STATIC and
 * getProperty() is reached from image stroke/update jobs as well as from the GUI
 * thread, so an inserting lookup would be a concurrent write to shared state.
 * Missing ids resolve to a pair of null icons.
 */
IconsPair lookupIconsPair(const QMap<QString, IconsPair> &icons, const QString &id)
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

KisBaseNode::Property KisLayerPropertiesIcons::getErrorProperty(const QString &message)
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
    const QString message =
        i18nc("a tooltip shown in when hovering layer's property",
              "Layer color space is different from the image color space:\n%1 [%2],\noperations may be slow",
              cs->name(),
              cs->profile() ? cs->profile()->name() : "");

    const IconsPair pair = lookupIconsPair(instance()->m_d->icons, layerColorSpaceMismatch.id());

    KisBaseNode::Property prop;
    prop.id = layerColorSpaceMismatch.id();
    prop.name =  layerColorSpaceMismatch.name();
    prop.state = message;
    prop.onIcon = pair.on;
    prop.offIcon = pair.off;

    return prop;
}

void KisLayerPropertiesIcons::setNodePropertyAutoUndo(KisNodeSP node, const KoID &id, const QVariant &value, KisImageSP image)
{
    KisBaseNode::PropertyList props = node->sectionModelProperties();
    setNodeProperty(&props, id, value);
    KisNodePropertyListCommand::setNodePropertiesAutoUndo(node, image, props);
}

void KisLayerPropertiesIcons::setNodeProperty(KisBaseNode::PropertyList *props, const KoID &id, const QVariant &value)
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

QVariant KisLayerPropertiesIcons::nodeProperty(KisNodeSP node, const KoID &id, const QVariant &defaultValue)
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

bool KisLayerPropertiesIcons::isStatelessProperty(const QString &id)
{
    return
        id == colorizeNeedsUpdate.id() ||
        id == openFileLayerFile.id() ||
        id == layerError.id() ||
        id == layerColorSpaceMismatch.id() ||
        id == colorOverlay.id();
}
