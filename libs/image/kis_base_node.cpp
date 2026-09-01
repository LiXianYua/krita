/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_base_node.h"

#include <kis_image.h>
#include <KoProperties.h>
#include <KisAnimatedOpacityProperty.h>
#include <KoColorSpace.h>
#include <KoCompositeOpRegistry.h>

#include "kis_pointer_utils.h"

#include "kis_paint_device.h"
#include "kis_layer_properties_icons.h"
#include "kis_default_bounds_node_wrapper.h"

#include "kis_scalar_keyframe_channel.h"

struct KisBaseNode::Private
{
    PkString compositeOp;
    KoProperties properties;
    KisBaseNode::Property hack_visible; //HACK
    PkNodeId id;
    PkMap<PkString, KisKeyframeChannel*> keyframeChannels;
    KisAnimatedOpacityProperty opacityProperty;

    bool collapsed {false};
    bool supportsLodMoves {false};
    bool animated {false};
    bool pinnedToTimeline {false};
    KisImageWSP image;
    KisThumbnailBoundsMode thumbnailBoundsMode { KisThumbnailBoundsMode::Precise };

    Private(KisImageWSP p_image)
        : id(PkNodeId())
        , opacityProperty(new KisDefaultBounds(p_image), &properties, OPACITY_OPAQUE_U8)
        , image(p_image)
    {
    }

    Private(const Private &rhs)
        : compositeOp(rhs.compositeOp),
          id(PkNodeId()),
          opacityProperty(new KisDefaultBounds(rhs.image), &properties, OPACITY_OPAQUE_U8),
          collapsed(rhs.collapsed),
          supportsLodMoves(rhs.supportsLodMoves),
          animated(rhs.animated),
          pinnedToTimeline(rhs.pinnedToTimeline),
          image(rhs.image)
    {
        PkMapIterator<PkString, PkVariant> iter = rhs.properties.propertyIterator();
        while (iter.hasNext()) {
            iter.next();
            properties.setProperty(iter.key(), iter.value());
        }
    }
};

KisBaseNode::KisBaseNode(KisImageWSP image)
    : m_d(new Private(image))
{
    /**
     * Be cautious! These two calls are vital to warm-up KoProperties.
     * We use it and its PkMap in a threaded environment. This is not
     * officially supported by Qt, but our environment guarantees, that
     * there will be the only writer and several readers. Whilst the
     * value of the PkMap is boolean and there are no implicit-sharing
     * calls provocated, it is safe to work with it in such an
     * environment.
     */
    setVisible(true, true);
    setUserLocked(false);
    setCollapsed(false);
    setSupportsLodMoves(true);

    m_d->compositeOp = COMPOSITE_OVER;

    PkObject::connect(&m_d->opacityProperty, &KisAnimatedOpacityProperty::changed, this, &KisBaseNode::opacityChanged);
}


KisBaseNode::KisBaseNode(const KisBaseNode & rhs)
    : PkShellObject()
    , KisShared()
    , m_d(new Private(*rhs.m_d))
{
    if (rhs.m_d->opacityProperty.hasChannel()) {
        m_d->opacityProperty.transferKeyframeData(rhs.m_d->opacityProperty);
        m_d->keyframeChannels.insert(m_d->opacityProperty.channel()->id(), m_d->opacityProperty.channel());
    }

    PkObject::connect(&m_d->opacityProperty, &KisAnimatedOpacityProperty::changed, this, &KisBaseNode::opacityChanged);
}

KisBaseNode::~KisBaseNode()
{
    delete m_d;
}

KisPaintDeviceSP KisBaseNode::colorSampleSourceDevice() const
{
    return projection();
}

quint8 KisBaseNode::opacity() const
{
    return m_d->opacityProperty.get();
}

void KisBaseNode::setOpacity(quint8 val)
{
    m_d->opacityProperty.set(val);
    baseNodeChangedCallback();
}

quint8 KisBaseNode::percentOpacity() const
{
    return int(float(opacity() * 100) / 255 + 0.5);
}

void KisBaseNode::setPercentOpacity(quint8 val)
{
    setOpacity(int(float(val * 255) / 100 + 0.5));
}

const PkString& KisBaseNode::compositeOpId() const
{
    return m_d->compositeOp;
}

void KisBaseNode::setCompositeOpId(const PkString& compositeOp)
{
    if (m_d->compositeOp == compositeOp) return;

    m_d->compositeOp = compositeOp;
    baseNodeChangedCallback();
    baseNodeInvalidateAllFramesCallback();
}

KisBaseNode::PropertyList KisBaseNode::sectionModelProperties() const
{
    KisBaseNode::PropertyList l;
    l << KisLayerPropertiesIcons::getProperty(KisLayerPropertiesIcons::visible, visible(), m_d->hack_visible.isInStasis, m_d->hack_visible.stateInStasis);
    l << KisLayerPropertiesIcons::getProperty(KisLayerPropertiesIcons::locked, userLocked());
    return l;
}

void KisBaseNode::setSectionModelProperties(const KisBaseNode::PropertyList &properties)
{
    setVisible(properties.at(0).state.toBool());
    m_d->hack_visible = properties.at(0);
    setUserLocked(properties.at(1).state.toBool());
}

const KoProperties & KisBaseNode::nodeProperties() const
{
    return m_d->properties;
}

void KisBaseNode::setNodeProperty(const PkString & name, const PkVariant & value)
{
    m_d->properties.setProperty(name, value);
    baseNodeChangedCallback();
}

void KisBaseNode::mergeNodeProperties(const KoProperties & properties)
{
    PkMapIterator<PkString, PkVariant> iter = properties.propertyIterator();
    while (iter.hasNext()) {
        iter.next();
        m_d->properties.setProperty(iter.key(), iter.value());
    }
    baseNodeChangedCallback();
    baseNodeInvalidateAllFramesCallback();
}

bool KisBaseNode::check(const KoProperties & properties) const
{
    PkMapIterator<PkString, PkVariant> iter = properties.propertyIterator();
    while (iter.hasNext()) {
        iter.next();
        if (m_d->properties.contains(iter.key())) {
            if (m_d->properties.value(iter.key()) != iter.value())
                return false;
        }
    }
    return true;
}


PkImage KisBaseNode::createThumbnail(qint32 w, qint32 h, Qt::AspectRatioMode aspectRatioMode, KisThumbnailBoundsMode boundsMode)
{
    Q_UNUSED(aspectRatioMode);
    Q_UNUSED(boundsMode);

    try {
        PkImage image(w, h, PkImage::Format_ARGB32);
        image.fill(0);
        return image;
    } catch (const std::bad_alloc&) {
        return PkImage();
    }

}

PkImage KisBaseNode::createPreferredThumbnail(qint32 w, qint32 h, Qt::AspectRatioMode aspectRatioMode)
{
    return createThumbnail(w, h, aspectRatioMode, m_d->thumbnailBoundsMode);
}

KisThumbnailBoundsMode KisBaseNode::preferredThumbnailBoundsMode() const
{
    return m_d->thumbnailBoundsMode;
}

void KisBaseNode::setPreferredThumbnailBoundsMode(KisThumbnailBoundsMode value) const
{
    m_d->thumbnailBoundsMode = value;
}

int KisBaseNode::thumbnailSeqNo() const
{
    return -1;
}

PkImage KisBaseNode::createThumbnailForFrame(qint32 w, qint32 h, int time, Qt::AspectRatioMode aspectRatioMode, KisThumbnailBoundsMode boundsMode)
{
    Q_UNUSED(time);
    return createThumbnail(w, h, aspectRatioMode, boundsMode);
}

PkImage KisBaseNode::createPreferredThumbnailForFrame(qint32 w, qint32 h, int time, Qt::AspectRatioMode aspectRatioMode)
{
    return createThumbnailForFrame(w, h, time, aspectRatioMode, m_d->thumbnailBoundsMode);
}

bool KisBaseNode::visible(bool recursive) const
{
    bool isVisible = m_d->properties.boolProperty(KisLayerPropertiesIcons::visible.id(), true);
    KisBaseNodeSP parentNode = parentCallback();

    return recursive && isVisible && parentNode ?
        parentNode->visible(recursive) : isVisible;
}

void KisBaseNode::setVisible(bool visible, bool loading)
{
    const bool isVisible = m_d->properties.boolProperty(KisLayerPropertiesIcons::visible.id(), true);
    if (!loading && isVisible == visible) return;

    m_d->properties.setProperty(KisLayerPropertiesIcons::visible.id(), visible);
    notifyParentVisibilityChanged(visible);

    if (!loading) {
        baseNodeChangedCallback();
        baseNodeInvalidateAllFramesCallback();
    }
}

bool KisBaseNode::userLocked() const
{
    return m_d->properties.boolProperty(KisLayerPropertiesIcons::locked.id(), false);
}

bool KisBaseNode::belongsToIsolatedGroup() const
{
    if (!m_d->image) {
        return false;
    }

    const KisBaseNode* element = this;

    while (element) {
        if (element->isIsolatedRoot()) {
            return true;
        } else {
            element = element->parentCallback().data();
        }
    }

    return false;
}

bool KisBaseNode::isIsolatedRoot() const
{
    if (!m_d->image) {
        return false;
    }

    const KisBaseNode* isolatedRoot = m_d->image->isolationRootNode().data();

    return (this == isolatedRoot);
}

void KisBaseNode::setUserLocked(bool locked)
{
    const bool isLocked = m_d->properties.boolProperty(KisLayerPropertiesIcons::locked.id(), true);
    if (isLocked == locked) return;

    m_d->properties.setProperty(KisLayerPropertiesIcons::locked.id(), locked);
    baseNodeChangedCallback();
}

bool KisBaseNode::isEditable(bool checkVisibility) const
{
    bool editable = true;
    if (checkVisibility) {
        editable = ((visible(false) || belongsToIsolatedGroup()) && !userLocked());
    }
    else {
        editable = (!userLocked());
    }

    if (editable) {
        KisBaseNodeSP parentNode = parentCallback();
        if (parentNode && parentNode != this) {
            editable = parentNode->isEditable(checkVisibility);
        }
    }
    return editable;
}

bool KisBaseNode::hasEditablePaintDevice() const
{
    return paintDevice() && isEditable();
}

void KisBaseNode::setCollapsed(bool collapsed)
{
    const bool oldCollapsed = m_d->collapsed;

    m_d->collapsed = collapsed;

    if (oldCollapsed != collapsed) {
        baseNodeCollapsedChangedCallback();
    }
}

bool KisBaseNode::collapsed() const
{
    return m_d->collapsed;
}

void KisBaseNode::setColorLabelIndex(int index)
{
    const int currentLabel = colorLabelIndex();

    if (currentLabel == index) return;

    m_d->properties.setProperty(KisLayerPropertiesIcons::colorLabelIndex.id(), index);
    baseNodeChangedCallback();
}

int KisBaseNode::colorLabelIndex() const
{
    return m_d->properties.intProperty(KisLayerPropertiesIcons::colorLabelIndex.id(), 0);
}

PkNodeId KisBaseNode::uuid() const
{
    return m_d->id;
}

void KisBaseNode::setUuid(const PkNodeId& id)
{
    m_d->id = id;
    baseNodeChangedCallback();
}

bool KisBaseNode::supportsLodMoves() const
{
    return m_d->supportsLodMoves;
}

bool KisBaseNode::supportsLodPainting() const
{
    return true;
}

void KisBaseNode::setImage(KisImageWSP image)
{
    m_d->image = image;
    m_d->opacityProperty.updateDefaultBounds(new KisDefaultBounds(image));
}

KisImageWSP KisBaseNode::image() const
{
    return m_d->image;
}

bool KisBaseNode::isFakeNode() const
{
    return false;
}

void KisBaseNode::setSupportsLodMoves(bool value)
{
    m_d->supportsLodMoves = value;
}


PkMap<PkString, KisKeyframeChannel*> KisBaseNode::keyframeChannels() const
{
    return m_d->keyframeChannels;
}

KisKeyframeChannel * KisBaseNode::getKeyframeChannel(const PkString &id) const
{
    PkMap<PkString, KisKeyframeChannel*>::const_iterator i = m_d->keyframeChannels.constFind(id);
    if (i == m_d->keyframeChannels.constEnd()) {
        return 0;
    }
    return i.value();
}

bool KisBaseNode::isPinnedToTimeline() const
{
    return m_d->pinnedToTimeline;
}

void KisBaseNode::setPinnedToTimeline(bool pinned)
{
   if (pinned == m_d->pinnedToTimeline) return;

   m_d->pinnedToTimeline = pinned;
   baseNodeChangedCallback();
}

KisKeyframeChannel * KisBaseNode::getKeyframeChannel(const PkString &id, bool create)
{
    KisKeyframeChannel *channel = getKeyframeChannel(id);

    if (!channel && create) {
        channel = requestKeyframeChannel(id);

        if (channel) {
            addKeyframeChannel(channel);
        }
    }

    return channel;
}

bool KisBaseNode::isAnimated() const
{
    return m_d->animated;
}

void KisBaseNode::enableAnimation()
{
    m_d->animated = true;
    baseNodeChangedCallback();
}

void KisBaseNode::addKeyframeChannel(KisKeyframeChannel *channel)
{
    m_d->keyframeChannels.insert(channel->id(), channel);
    keyframeChannelAdded(channel);
}

KisKeyframeChannel *KisBaseNode::requestKeyframeChannel(const PkString &id)
{
    if (id == KisKeyframeChannel::Opacity.id()) {
        Q_ASSERT(!m_d->opacityProperty.hasChannel());

        KisPaintDeviceSP device = original();
        KisNode* node = dynamic_cast<KisNode*>(this);

        if (device && node) {
            m_d->opacityProperty.makeAnimated(node);
            return m_d->opacityProperty.channel();
        }
    }

    return 0;
}

bool KisBaseNode::supportsKeyframeChannel(const PkString &id)
{
    if (id == KisKeyframeChannel::Opacity.id() && original()) {
        return true;
    }

    return false;
}

PkDebug operator<<(PkDebug dbg, const KisBaseNode::Property &prop)
{
    dbg.nospace() << "Property(" << prop.id << ", " << prop.state.toString();

    if (prop.isInStasis) {
        dbg.nospace() << ", in-stasis";
    }

    dbg.nospace() << ")";

    return dbg.space();
}
