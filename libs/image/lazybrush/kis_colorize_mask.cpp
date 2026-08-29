/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_colorize_mask.cpp 阻塞登记（S-06 Task 6）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * 协变返回断裂：传递 include 到 kis_psd_layer_style.h（未剥），其覆盖
//     KoResource 虚函数用 Qt 列表容器，与 KoResource.h 被剥成的 PkVector 不一致
//   * kismpl 未声明：libs/global/KisMpl.h 未 include，两处 kismpl 调用（805/1077）
//   * mid()：include krita_utils.h 的模板用 Qt 序列容器 mid()
// 关闭条件：KoResource.h 的 旧列表容器→PkVector 误映射改回 PkList + 补 KisMpl.h
// include + 给 PkVector 补 mid()（或改 krita_utils.h 模板）后解除。
// 当前状态：Qt 仅经未剥依赖头传递进入，不参与薄壳构建。
// ===========================================================================


#include "kis_colorize_mask.h"

#include <PkStack.h>
#include <string>

#include <KoColorSpaceRegistry.h>
#include <KoProperties.h>
#include <kundo2command.h>
#include "kis_pixel_selection.h"

#include "kis_node_visitor.h"
#include "kis_processing_visitor.h"
#include "kis_painter.h"
#include "kis_fill_painter.h"
#include "kis_lazy_fill_tools.h"
#include "kis_cached_paint_device.h"
#include "kis_paint_device_debug_utils.h"
#include "kis_layer_properties_icons.h"
#include "kis_thread_safe_signal_compressor.h"

#include "kis_colorize_stroke_strategy.h"
#include "kis_multiway_cut.h"
#include "kis_image.h"
#include "kis_layer.h"
#include "kis_paint_layer.h"
#include "kis_undo_adapter.h"
#include "commands/kis_image_layer_add_command.h"
#include "kis_macro_based_undo_store.h"
#include "kis_post_execution_undo_adapter.h"
#include "kis_command_utils.h"
#include "kis_processing_applicator.h"
#include "krita_utils.h"
#include <KisFakeRunnableStrokeJobsExecutor.h>
#include <KisRunnableStrokeJobData.h>
#include <KisRunnableStrokeJobUtils.h>
#include <kis_pointer_utils.h>


using namespace KisLazyFillTools;

namespace
{
struct ColorizeMaskPosition
{
    KisNodeSP parent;
    KisNodeSP above;
    KisPaintLayerSP fallbackLayer;
    KisNodeSP fallbackParent;
};

bool resolveColorizeMaskPosition(KisImageSP image,
                                 KisNodeSP mask,
                                 KisNodeSP activeNode,
                                 bool avoidActiveNode,
                                 ColorizeMaskPosition &position)
{
    if (!avoidActiveNode && activeNode->allowAsChild(mask)) {
        position.parent = activeNode;
        position.above = activeNode->lastChild();
        return true;
    }

    if (activeNode->parent() && activeNode->parent()->allowAsChild(mask) &&
        activeNode->parent()->parent()) {
        position.parent = activeNode->parent();
        position.above = activeNode;
        return true;
    }

    KisNodeSP sibling = activeNode;
    while ((sibling = sibling->nextSibling())) {
        if (sibling->allowAsChild(mask)) {
            position.parent = sibling;
            position.above = sibling->lastChild();
            return true;
        }
    }

    sibling = activeNode;
    while ((sibling = sibling->prevSibling())) {
        if (sibling->allowAsChild(mask)) {
            position.parent = sibling;
            position.above = sibling->lastChild();
            return true;
        }
    }

    if (activeNode->parent()) {
        return resolveColorizeMaskPosition(image,
                                           mask,
                                           activeNode->parent(),
                                           true,
                                           position);
    }

    KisPaintLayerSP layer = new KisPaintLayer(image.data(),
                                              image->nextLayerName(),
                                              OPACITY_OPAQUE_U8,
                                              image->colorSpace());
    if (!activeNode->allowAsChild(layer)) {
        return false;
    }

    position.parent = layer;
    position.above = {};
    position.fallbackLayer = layer;
    position.fallbackParent = activeNode;
    return true;
}
}

KisColorizeMaskSP KisColorizeMaskUtils::createColorizeMask(KisImageSP image,
                                                           KisNodeSP activeNode)
{
    if (!image || !activeNode || !activeNode->isEditable(false)) {
        return {};
    }

    KisColorizeMaskSP mask = new KisColorizeMask(image, PkString());
    ColorizeMaskPosition position;
    if (!resolveColorizeMaskPosition(image, mask, activeNode, false, position)) {
        return {};
    }

    KisLayerSP parentLayer = dynamic_cast<KisLayer *>(position.parent.data());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(parentLayer, KisColorizeMaskSP());

    const int number = parentLayer->childNodes(PkStringList{PkString("KisColorizeMask")},
                                                KoProperties()).count() + 1;
    mask->setName(PkString("Colorize Mask") + PkString(" ") + PkString(std::to_string(number).c_str()));

    KisUndoAdapter *undoAdapter = image->undoAdapter();
    const KisImageLayerAddCommand::Flags updateFlags =
        KisImageLayerAddCommand::DoRedoUpdates |
        KisImageLayerAddCommand::DoUndoUpdates;

    undoAdapter->beginMacro(kundo2_text("Add Colorize Mask"));
    if (position.fallbackLayer) {
        undoAdapter->addCommand(new KisImageLayerAddCommand(image,
                                                            position.fallbackLayer,
                                                            position.fallbackParent,
                                                            KisNodeSP(),
                                                            updateFlags));
    }
    undoAdapter->addCommand(new KisImageLayerAddCommand(image,
                                                        mask,
                                                        parentLayer,
                                                        position.above,
                                                        updateFlags));
    undoAdapter->endMacro();

    mask->initializeCompositeOp();
    delete mask->setColorSpace(parentLayer->colorSpace());

    return mask;
}

struct KisColorizeMask::Private
{
    Private(KisColorizeMask *_q, KisImageWSP image)
        : q(_q),
          coloringProjection(new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8())),
          fakePaintDevice(new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8())),
          filteredSource(new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8())),
          needAddCurrentKeyStroke(false),
          showKeyStrokes(true),
          showColoring(true),
          needsUpdate(true),
          originalSequenceNumber(-1),
          updateCompressor(1000, KisSignalCompressor::FIRST_ACTIVE_POSTPONE_NEXT),
          dirtyParentUpdateCompressor(200, KisSignalCompressor::FIRST_ACTIVE_POSTPONE_NEXT),
          prefilterRecalculationCompressor(1000, KisSignalCompressor::POSTPONE),
          updateIsRunning(false),
          filteringOptions(false, 4.0, 15, 0.7),
          limitToDeviceBounds(false)
    {
        KisDefaultBoundsSP bounds(new KisDefaultBounds(image));

        coloringProjection->setDefaultBounds(bounds);
        fakePaintDevice->setDefaultBounds(bounds);
        filteredSource->setDefaultBounds(bounds);
    }

    Private(const Private &rhs, KisColorizeMask *_q)
        : q(_q),
          coloringProjection(new KisPaintDevice(*rhs.coloringProjection)),
          fakePaintDevice(new KisPaintDevice(*rhs.fakePaintDevice)),
          filteredSource(new KisPaintDevice(*rhs.filteredSource)),
          filteredDeviceBounds(rhs.filteredDeviceBounds),
          needAddCurrentKeyStroke(rhs.needAddCurrentKeyStroke),
          showKeyStrokes(rhs.showKeyStrokes),
          showColoring(rhs.showColoring),
          needsUpdate(false),
          originalSequenceNumber(-1),
          updateCompressor(1000, KisSignalCompressor::FIRST_ACTIVE_POSTPONE_NEXT),
          dirtyParentUpdateCompressor(200, KisSignalCompressor::FIRST_ACTIVE_POSTPONE_NEXT),
          prefilterRecalculationCompressor(1000, KisSignalCompressor::POSTPONE),
          offset(rhs.offset),
          updateIsRunning(false),
          filteringOptions(rhs.filteringOptions),
          limitToDeviceBounds(rhs.limitToDeviceBounds)
    {
        for (const KeyStroke &stroke : rhs.keyStrokes) {
            keyStrokes << KeyStroke(KisPaintDeviceSP(new KisPaintDevice(*stroke.dev)), stroke.color, stroke.isTransparent);
        }
    }

    KisColorizeMask *q = 0;

    PkList<KeyStroke> keyStrokes;
    KisPaintDeviceSP coloringProjection;
    KisPaintDeviceSP fakePaintDevice;
    KisPaintDeviceSP filteredSource;
    PkRect filteredDeviceBounds;

    KoColor currentColor;
    KisPaintDeviceSP currentKeyStrokeDevice;
    bool needAddCurrentKeyStroke;

    bool showKeyStrokes;
    bool showColoring;

    KisCachedSelection cachedSelection;

    bool needsUpdate;
    int originalSequenceNumber;

    KisThreadSafeSignalCompressor updateCompressor;
    KisThreadSafeSignalCompressor dirtyParentUpdateCompressor;
    KisThreadSafeSignalCompressor prefilterRecalculationCompressor;
    PkPoint offset;

    bool updateIsRunning;
    PkStack<PkRect> extentBeforeUpdateStart;

    FilteringOptions filteringOptions;
    bool filteringDirty = true;

    bool limitToDeviceBounds = false;

    bool filteredSourceValid(KisPaintDeviceSP parentDevice) {
        return !filteringDirty && originalSequenceNumber == parentDevice->sequenceNumber();
    }

    void setNeedsUpdateImpl(bool value, bool requestedByUser);

    bool shouldShowFilteredSource() const;
    bool shouldShowColoring() const;
};

KisColorizeMask::KisColorizeMask(KisImageWSP image, const PkString &name)
    : KisEffectMask(image, name)
    , m_d(new Private(this, image))
{
    PkObject::connect(&m_d->updateCompressor,
            &KisThreadSafeSignalCompressor::timeout, this,
            [this]() { slotUpdateRegenerateFilling(); });

    PkObject::connect(this, &KisColorizeMask::sigUpdateOnDirtyParent,
            &m_d->dirtyParentUpdateCompressor, &KisThreadSafeSignalCompressor::start);

    PkObject::connect(&m_d->dirtyParentUpdateCompressor,
            &KisThreadSafeSignalCompressor::timeout, this,
            &KisColorizeMask::slotUpdateOnDirtyParent);

    PkObject::connect(&m_d->prefilterRecalculationCompressor,
            &KisThreadSafeSignalCompressor::timeout, this,
            &KisColorizeMask::slotRecalculatePrefilteredImage);


    m_d->updateCompressor.moveToThread(PkThread::mainThreadId());
}

KisColorizeMask::~KisColorizeMask()
{
}

KisColorizeMask::KisColorizeMask(const KisColorizeMask& rhs)
    : KisEffectMask(rhs),
      m_d(new Private(*rhs.m_d, this))
{
    PkObject::connect(&m_d->updateCompressor,
            &KisThreadSafeSignalCompressor::timeout, this,
            [this]() { slotUpdateRegenerateFilling(); });

    PkObject::connect(this, &KisColorizeMask::sigUpdateOnDirtyParent,
            &m_d->dirtyParentUpdateCompressor, &KisThreadSafeSignalCompressor::start);

    PkObject::connect(&m_d->dirtyParentUpdateCompressor,
            &KisThreadSafeSignalCompressor::timeout, this,
            &KisColorizeMask::slotUpdateOnDirtyParent);

    m_d->updateCompressor.moveToThread(PkThread::mainThreadId());
}

void KisColorizeMask::initializeCompositeOp()
{
    KisLayerSP parentLayer(dynamic_cast<KisLayer*>(parent().data()));
    if (!parentLayer || !parentLayer->original()) return;

    KisImageSP image = parentLayer->image();
    if (!image) return;

    const qreal samplePortion = 0.1;
    const qreal alphaPortion =
        KritaUtils::estimatePortionOfTransparentPixels(parentLayer->original(),
                                                       image->bounds(),
                                                       samplePortion);

    setCompositeOpId(alphaPortion > 0.3 ? COMPOSITE_BEHIND : COMPOSITE_MULT);
}

const KoColorSpace* KisColorizeMask::colorSpace() const
{
    return m_d->fakePaintDevice->colorSpace();
}

struct SetKeyStrokesColorSpaceCommand : public KUndo2Command {
    SetKeyStrokesColorSpaceCommand(const KoColorSpace *dstCS,
                                   KoColorConversionTransformation::Intent renderingIntent,
                                   KoColorConversionTransformation::ConversionFlags conversionFlags,
                                   PkList<KeyStroke> *list,
                                   KisColorizeMaskSP node)
        : m_dstCS(dstCS),
          m_renderingIntent(renderingIntent),
          m_conversionFlags(conversionFlags),
          m_list(list),
          m_node(node) {}

    void undo() override {
        KIS_ASSERT_RECOVER_RETURN(m_list->size() == m_oldColors.size());

        for (int i = 0; i < m_list->size(); i++) {
            (*m_list)[i].color = m_oldColors[i];
        }

        m_node->setNeedsUpdate(true);
        m_node->sigKeyStrokesListChanged();
    }

    void redo() override {
        if (m_oldColors.isEmpty()) {
            for (const KeyStroke &stroke : *m_list) {
                m_oldColors << stroke.color;
                m_newColors << stroke.color;
                m_newColors.last().convertTo(m_dstCS, m_renderingIntent, m_conversionFlags);
            }
        }

        KIS_ASSERT_RECOVER_RETURN(m_list->size() == m_newColors.size());

        for (int i = 0; i < m_list->size(); i++) {
            (*m_list)[i].color = m_newColors[i];
        }

        m_node->setNeedsUpdate(true);
        m_node->sigKeyStrokesListChanged();
    }

private:
    PkVector<KoColor> m_oldColors;
    PkVector<KoColor> m_newColors;

    const KoColorSpace *m_dstCS;
    KoColorConversionTransformation::Intent m_renderingIntent;
    KoColorConversionTransformation::ConversionFlags m_conversionFlags;
    PkList<KeyStroke> *m_list;
    KisColorizeMaskSP m_node;
};


void KisColorizeMask::setProfile(const KoColorProfile *profile, KUndo2Command *parentCommand)
{
    m_d->fakePaintDevice->setProfile(profile, parentCommand);
    m_d->coloringProjection->setProfile(profile, parentCommand);

    for (auto & stroke : m_d->keyStrokes) {
        stroke.color.setProfile(profile);
    }
}

KUndo2Command *KisColorizeMask::setColorSpace(const KoColorSpace *dstColorSpace,
                                              KoColorConversionTransformation::Intent renderingIntent,
                                              KoColorConversionTransformation::ConversionFlags conversionFlags,
                                              KoUpdater *progressUpdater)
{
    using namespace KisCommandUtils;

    CompositeCommand *composite = new CompositeCommand();

    m_d->fakePaintDevice->convertTo(dstColorSpace, renderingIntent, conversionFlags, composite, progressUpdater);
    m_d->coloringProjection->convertTo(dstColorSpace, renderingIntent, conversionFlags, composite, progressUpdater);

    KUndo2Command *strokesConversionCommand =
        new SetKeyStrokesColorSpaceCommand(
            dstColorSpace, renderingIntent, conversionFlags,
            &m_d->keyStrokes, KisColorizeMaskSP(this));
    strokesConversionCommand->redo();

    composite->addCommand(new SkipFirstRedoWrapper(strokesConversionCommand));

    return composite;
}

bool KisColorizeMask::needsUpdate() const
{
    return m_d->needsUpdate;
}

void KisColorizeMask::setNeedsUpdate(bool value)
{
    m_d->setNeedsUpdateImpl(value, true);
}

void KisColorizeMask::Private::setNeedsUpdateImpl(bool value, bool requestedByUser)
{
    if (value != needsUpdate) {
        needsUpdate = value;
        q->baseNodeChangedCallback();

        if (!value && requestedByUser) {
            updateCompressor.start();
        }
    }
}

void KisColorizeMask::slotUpdateRegenerateFilling(bool prefilterOnly)
{
    KisPaintDeviceSP src = parent()->original();
    KIS_ASSERT_RECOVER_RETURN(src);

    const bool filteredSourceValid = m_d->filteredSourceValid(src);
    m_d->originalSequenceNumber = src->sequenceNumber();
    m_d->filteringDirty = false;

    if (!prefilterOnly) {
        m_d->coloringProjection->clear();
    }

    KisLayerSP parentLayer(dynamic_cast<KisLayer*>(parent().data()));
    if (!parentLayer) return;

    KisImageSP image = parentLayer->image();
    if (image) {
        m_d->updateIsRunning = true;

        PkRect fillBounds;

        if (m_d->limitToDeviceBounds) {
            fillBounds |= src->exactBounds();
            for (const KeyStroke &stroke : m_d->keyStrokes) {
                fillBounds |= stroke.dev->exactBounds();
            }
            fillBounds &= image->bounds();
        } else {
            fillBounds = image->bounds();
        }

        m_d->filteredDeviceBounds = fillBounds;

        KisColorizeStrokeStrategy *strategy =
            new KisColorizeStrokeStrategy(src,
                                          m_d->coloringProjection,
                                          m_d->filteredSource,
                                          filteredSourceValid,
                                          fillBounds,
                                          this,
                                          prefilterOnly);

        strategy->setFilteringOptions(m_d->filteringOptions);

        for (const KeyStroke &stroke : m_d->keyStrokes) {
            const KoColor color =
                !stroke.isTransparent ?
                stroke.color :
                KoColor::createTransparent(stroke.color.colorSpace());

            strategy->addKeyStroke(stroke.dev, color);
        }

        m_d->extentBeforeUpdateStart.push(extent());

        PkObject::connect(strategy, &KisColorizeStrokeStrategy::sigFinished,
                this, &KisColorizeMask::slotRegenerationFinished);
        PkObject::connect(strategy, &KisColorizeStrokeStrategy::sigCancelled,
                this, &KisColorizeMask::slotRegenerationCancelled);
        KisStrokeId id = image->startStroke(strategy);
        image->endStroke(id);
    }
}

void KisColorizeMask::slotUpdateOnDirtyParent()
{
    if (!parent()) {
        // When the colorize mask is being merged,
        // the update is performed for all the layers,
        // so the invisible areas around the canvas are included in the merged layer.
        // Colorize Mask gets the info that its parent is "dirty" (needs updating),
        // but when it arrives, the parent doesn't exist anymore and is set to null.
        // Colorize Mask doesn't work outside of the canvas anyway (at least in time of writing).
        return;
    }
    KisPaintDeviceSP src = parent()->original();
    KIS_ASSERT_RECOVER_RETURN(src);

    if (!m_d->filteredSourceValid(src)) {
        const PkRect &oldExtent = extent();

        m_d->setNeedsUpdateImpl(true, false);
        m_d->filteringDirty = true;

        setDirty(oldExtent | extent());
    }
}

void KisColorizeMask::slotRecalculatePrefilteredImage()
{
    slotUpdateRegenerateFilling(true);
}

void KisColorizeMask::slotRegenerationFinished(bool prefilterOnly)
{
    m_d->updateIsRunning = false;

    if (!prefilterOnly) {
        m_d->setNeedsUpdateImpl(false, false);
    }

    PkRect oldExtent;

    if (!m_d->extentBeforeUpdateStart.isEmpty()) {
        oldExtent = m_d->extentBeforeUpdateStart.pop();
    } else {
        KIS_SAFE_ASSERT_RECOVER_NOOP(!m_d->extentBeforeUpdateStart.isEmpty()); // always fail!
    }

    setDirty(oldExtent | extent());
}

void KisColorizeMask::slotRegenerationCancelled()
{
    slotRegenerationFinished(true);
    m_d->setNeedsUpdateImpl(true, false);
}

KisBaseNode::PropertyList KisColorizeMask::sectionModelProperties() const
{
    KisBaseNode::PropertyList l = KisMask::sectionModelProperties();
    l << KisLayerPropertiesIcons::getProperty(KisLayerPropertiesIcons::colorizeNeedsUpdate, needsUpdate());
    l << KisLayerPropertiesIcons::getProperty(KisLayerPropertiesIcons::colorizeEditKeyStrokes, showKeyStrokes());
    l << KisLayerPropertiesIcons::getProperty(KisLayerPropertiesIcons::colorizeShowColoring, showColoring());

    return l;
}

void KisColorizeMask::setSectionModelProperties(const KisBaseNode::PropertyList &properties)
{
    KisMask::setSectionModelProperties(properties);

    for (const KisBaseNode::Property &property : properties) {
        if (property.id == KisLayerPropertiesIcons::colorizeNeedsUpdate.id()) {
            if (m_d->needsUpdate && m_d->needsUpdate != property.state.toBool()) {
                setNeedsUpdate(property.state.toBool());
            }
        }
        if (property.id == KisLayerPropertiesIcons::colorizeEditKeyStrokes.id()) {
            if (m_d->showKeyStrokes != property.state.toBool()) {
                setShowKeyStrokes(property.state.toBool());
            }
        }
        if (property.id == KisLayerPropertiesIcons::colorizeShowColoring.id()) {
            if (m_d->showColoring != property.state.toBool()) {
                setShowColoring(property.state.toBool());
            }
        }
    }
}

KisPaintDeviceSP KisColorizeMask::paintDevice() const
{
    return m_d->showKeyStrokes && !m_d->updateIsRunning ? m_d->fakePaintDevice : KisPaintDeviceSP();
}

KisPaintDeviceSP KisColorizeMask::coloringProjection() const
{
    return m_d->coloringProjection;
}

KisPaintDeviceSP KisColorizeMask::colorSampleSourceDevice() const
{
    return
        m_d->shouldShowColoring() && !m_d->coloringProjection->extent().isEmpty() ?
            m_d->coloringProjection : projection();
}

bool KisColorizeMask::accept(KisNodeVisitor &v)
{
    return v.visit(this);
}

void KisColorizeMask::accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter)
{
    return visitor.visit(this, undoAdapter);
}

bool KisColorizeMask::Private::shouldShowFilteredSource() const
{
    return !updateIsRunning &&
            showKeyStrokes &&
            !filteringDirty &&
            filteredSource &&
            !filteredSource->extent().isEmpty();
}

bool KisColorizeMask::Private::shouldShowColoring() const
{
    return !updateIsRunning &&
            showColoring &&
            coloringProjection;
}

PkRect KisColorizeMask::decorateRect(KisPaintDeviceSP &src,
                                    KisPaintDeviceSP &dst,
                                    const PkRect &rect,
                                    PositionToFilthy maskPos,
                                    KisRenderPassFlags flags) const
{
    Q_UNUSED(maskPos);
    Q_UNUSED(flags);

    if (maskPos == N_ABOVE_FILTHY) {
        // the source layer has changed, we should update the filtered cache!

        if (!m_d->filteringDirty) {
            const_cast<KisColorizeMask *>(this)->sigUpdateOnDirtyParent();
        }
    }

    KIS_ASSERT(dst != src);

    // Draw the filling and the original layer
    {
        KisPainter gc(dst);

        if (m_d->shouldShowFilteredSource()) {
            const PkRect drawRect = m_d->limitToDeviceBounds ? rect & m_d->filteredDeviceBounds : rect;

            gc.setOpacityF(0.5);
            gc.bitBlt(drawRect.topLeft(), m_d->filteredSource, drawRect);
        } else {
            gc.setOpacityToUnit();
            gc.bitBlt(rect.topLeft(), src, rect);
        }

        if (m_d->shouldShowColoring()) {

            gc.setOpacityU8(opacity());
            gc.setCompositeOpId(compositeOpId());
            gc.bitBlt(rect.topLeft(), m_d->coloringProjection, rect);
        }
    }

    // Draw the key strokes
    if (m_d->showKeyStrokes) {
        KisIndirectPaintingSupport::ReadLocker locker(this);

        KisCachedSelection::Guard s1(m_d->cachedSelection);
        KisCachedSelection::Guard s2(m_d->cachedSelection);

        KisSelectionSP selection = s1.selection();
        KisPixelSelectionSP tempSelection = s2.selection()->pixelSelection();

        KisPaintDeviceSP temporaryTarget = this->temporaryTarget();
        const bool isTemporaryTargetErasing = temporaryCompositeOp() == COMPOSITE_ERASE;
        const PkRect temporaryExtent = temporaryTarget ? temporaryTarget->extent() : PkRect(0, 0, 0, 0);


        KisFillPainter gc(dst);

        PkList<KeyStroke> extendedStrokes = m_d->keyStrokes;

        if (m_d->currentKeyStrokeDevice &&
            m_d->needAddCurrentKeyStroke &&
            !isTemporaryTargetErasing) {

            extendedStrokes << KeyStroke(m_d->currentKeyStrokeDevice, m_d->currentColor);
        }

        for (const KeyStroke &stroke : extendedStrokes) {
            selection->pixelSelection()->makeCloneFromRough(stroke.dev, rect);
            gc.setSelection(selection);

            if (stroke.color == m_d->currentColor ||
                (isTemporaryTargetErasing &&
                 temporaryExtent.intersects(selection->pixelSelection()->selectedRect()))) {

                if (temporaryTarget) {
                    tempSelection->copyAlphaFrom(temporaryTarget, rect);

                    KisPainter selectionPainter(selection->pixelSelection());
                    setupTemporaryPainter(&selectionPainter);
                    selectionPainter.bitBlt(rect.topLeft(), tempSelection, rect);
                }
            }

            gc.fillSelection(rect, stroke.color);
        }
    }

    return rect;
}

struct DeviceExtentPolicy
{
    inline PkRect operator() (const KisPaintDevice *dev) {
        return dev->extent();
    }
};

struct DeviceExactBoundsPolicy
{
    inline PkRect operator() (const KisPaintDevice *dev) {
        return dev->exactBounds();
    }
};

template <class DeviceMetricPolicy>
PkRect KisColorizeMask::calculateMaskBounds(DeviceMetricPolicy boundsPolicy) const
{
    PkRect rc;

    if (m_d->shouldShowFilteredSource()) {
        rc |= boundsPolicy(m_d->filteredSource);
    }

    if (m_d->shouldShowColoring()) {
        rc |= boundsPolicy(m_d->coloringProjection);
    }

    if (m_d->showKeyStrokes) {
        for (const KeyStroke &stroke : m_d->keyStrokes) {
            rc |= boundsPolicy(stroke.dev);
        }

        KisIndirectPaintingSupport::ReadLocker locker(this);

        KisPaintDeviceSP temporaryTarget = this->temporaryTarget();
        if (temporaryTarget) {
            rc |= boundsPolicy(temporaryTarget);
        }
    }

    return rc;
}


PkRect KisColorizeMask::extent() const
{
    return calculateMaskBounds(DeviceExtentPolicy());
}

PkRect KisColorizeMask::exactBounds() const
{
    return calculateMaskBounds(DeviceExactBoundsPolicy());
}

PkRect KisColorizeMask::nonDependentExtent() const
{
    return extent();
}

void KisColorizeMask::setImage(KisImageWSP image)
{
    KisDefaultBoundsSP bounds(new KisDefaultBounds(image));

    auto it = m_d->keyStrokes.begin();
    for(; it != m_d->keyStrokes.end(); ++it) {
        it->dev->setDefaultBounds(bounds);
    }

    m_d->coloringProjection->setDefaultBounds(bounds);
    m_d->fakePaintDevice->setDefaultBounds(bounds);
    m_d->filteredSource->setDefaultBounds(bounds);
}

void KisColorizeMask::setCurrentColor(const KoColor &_color)
{
    KoColor color = _color;
    color.convertTo(colorSpace());

    WriteLocker locker(this);

    m_d->setNeedsUpdateImpl(true, false);

    PkList<KeyStroke>::const_iterator it =
        std::find_if(m_d->keyStrokes.constBegin(),
                     m_d->keyStrokes.constEnd(),
                     kismpl::mem_equal_to(&KeyStroke::color, color));

    KisPaintDeviceSP activeDevice;
    bool newKeyStroke = false;

    if (it == m_d->keyStrokes.constEnd()) {
        activeDevice = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
        activeDevice->setParentNode(this);
        activeDevice->setDefaultBounds(KisDefaultBoundsBaseSP(new KisDefaultBounds(image())));
        newKeyStroke = true;
    } else {
        activeDevice = it->dev;
    }

    m_d->currentColor = color;
    m_d->currentKeyStrokeDevice = activeDevice;
    m_d->needAddCurrentKeyStroke = newKeyStroke;
}



struct KeyStrokeAddRemoveCommand : public KisCommandUtils::FlipFlopCommand {
    KeyStrokeAddRemoveCommand(bool add, int index, KeyStroke stroke, PkList<KeyStroke> *list, KisColorizeMaskSP node, KUndo2Command *parentCommand = nullptr)
        : FlipFlopCommand(!add, parentCommand),
          m_index(index), m_stroke(stroke),
          m_list(list), m_node(node) {}

    void partA() override {
        m_list->insert(m_index, m_stroke);
        m_node->setNeedsUpdate(true);
        m_node->sigKeyStrokesListChanged();
    }

    void partB() override {
        KIS_ASSERT_RECOVER_RETURN((*m_list)[m_index] == m_stroke);
        m_list->removeAt(m_index);
        m_node->setNeedsUpdate(true);
        m_node->sigKeyStrokesListChanged();
    }

private:
    int m_index;
    KeyStroke m_stroke;
    PkList<KeyStroke> *m_list;
    KisColorizeMaskSP m_node;
};

void KisColorizeMask::mergeToLayerThreaded(KisNodeSP layer, KUndo2Command *parentCommand, const KUndo2MagicString &transactionText,int timedID, PkVector<KisRunnableStrokeJobData*> *jobs)
{
    // Just fake threaded merging. It is not supported for the colorize mask.

    KritaUtils::addJobSequential(*jobs,
        [=] () {
            this->mergeToLayerUnthreaded(layer, parentCommand, transactionText, timedID);
        }
    );
}

void KisColorizeMask::mergeToLayerUnthreaded(KisNodeSP layer, KUndo2Command *parentCommand, const KUndo2MagicString &transactionText, int timedID)
{
    Q_UNUSED(layer);

    auto executeAndAdd = [parentCommand] (KUndo2Command *cmd) {
        cmd->redo();
        new KisCommandUtils::SkipFirstRedoWrapper(cmd, parentCommand);
    };

    WriteLockerSP sharedWriteLock(new WriteLocker(this));

    KisPaintDeviceSP temporaryTarget = this->temporaryTarget();
    const bool isTemporaryTargetErasing = temporaryCompositeOp() == COMPOSITE_ERASE;
    const PkRect temporaryExtent = temporaryTarget ? temporaryTarget->extent() : PkRect(0, 0, 0, 0);

    /**
     * Add a new key stroke plane
     */
    if (m_d->needAddCurrentKeyStroke && !isTemporaryTargetErasing) {
        KeyStroke key(m_d->currentKeyStrokeDevice, m_d->currentColor);
        executeAndAdd(new KeyStrokeAddRemoveCommand(
            true, m_d->keyStrokes.size(), key, &m_d->keyStrokes, KisColorizeMaskSP(this), nullptr));
    }

    PkVector<KisRunnableStrokeJobData*> jobs;

    /**
     * When erasing, the brush affects all the key strokes, not only
     * the current one.
     */
    if (!isTemporaryTargetErasing) {
        mergeToLayerImpl(m_d->currentKeyStrokeDevice, parentCommand, transactionText, timedID, false, sharedWriteLock, &jobs);
    } else {
        for (const KeyStroke &stroke : m_d->keyStrokes) {
            if (temporaryExtent.intersects(stroke.dev->extent())) {
                mergeToLayerImpl(stroke.dev, parentCommand, transactionText, timedID, false, sharedWriteLock, &jobs);
            }
        }
    }

    mergeToLayerImpl(m_d->fakePaintDevice, parentCommand, transactionText, timedID, false, sharedWriteLock, &jobs);

    /**
     * When merging, we use barrier jobs only for ensuring that the merge jobs
     * are not split by the update jobs. Merge jobs hold the shared lock, so
     * forcing them out of CPU will basically cause a deadlock. When running in
     * the fake executor, the jobs cannot be split anyway, so there is no danger
     * in that.
     */
    KisFakeRunnableStrokeJobsExecutor fakeExecutor(KisFakeRunnableStrokeJobsExecutor::AllowBarrierJobs);
    fakeExecutor.addRunnableJobs(implicitCastList<KisRunnableStrokeJobDataBase*>(jobs));

    m_d->currentKeyStrokeDevice = 0;
    m_d->currentColor = KoColor();
    releaseResources();

    /**
     * Try removing the key strokes that has been completely erased
     */
    if (isTemporaryTargetErasing) {
        for (int index = 0; index < m_d->keyStrokes.size(); /*noop*/) {
            const KeyStroke &stroke = m_d->keyStrokes[index];

            if (stroke.dev->exactBounds().isEmpty()) {
                executeAndAdd(new KeyStrokeAddRemoveCommand(
                    false, index, stroke, &m_d->keyStrokes, KisColorizeMaskSP(this), nullptr));
            } else {
                index++;
            }
        }
    }
}


void KisColorizeMask::writeMergeData(KisPainter *painter, KisPaintDeviceSP src, const PkRect &rc)
{
    const KoColorSpace *alpha8 = KoColorSpaceRegistry::instance()->alpha8();
    const bool nonAlphaDst = !(*painter->device()->colorSpace() == *alpha8);

    if (nonAlphaDst) {
        painter->bitBlt(rc.topLeft(), src, rc);
    } else {
        KisCachedSelection::Guard s1(m_d->cachedSelection);
        KisPixelSelectionSP tempSelection = s1.selection()->pixelSelection();

        tempSelection->copyAlphaFrom(src, rc);
        painter->bitBlt(rc.topLeft(), tempSelection, rc);
    }
}

bool KisColorizeMask::supportsNonIndirectPainting() const
{
    return false;
}

bool KisColorizeMask::showColoring() const
{
    return m_d->showColoring;
}

void KisColorizeMask::setShowColoring(bool value)
{
    PkRect savedExtent;
    if (m_d->showColoring && !value) {
        savedExtent = extent();
    }

    m_d->showColoring = value;
    baseNodeChangedCallback();

    if (!savedExtent.isEmpty()) {
        setDirty(savedExtent);
    }
}

bool KisColorizeMask::showKeyStrokes() const
{
    return m_d->showKeyStrokes;
}

void KisColorizeMask::setShowKeyStrokes(bool value)
{
    PkRect savedExtent;
    if (m_d->showKeyStrokes && !value) {
        savedExtent = extent();
    }

    m_d->showKeyStrokes = value;
    baseNodeChangedCallback();

    if (!savedExtent.isEmpty()) {
        setDirty(savedExtent);
    }

    regeneratePrefilteredDeviceIfNeeded();
}

KisColorizeMask::KeyStrokeColors KisColorizeMask::keyStrokesColors() const
{
    KeyStrokeColors colors;

    // TODO: thread safety!
    for (int i = 0; i < m_d->keyStrokes.size(); i++) {
        colors.colors << m_d->keyStrokes[i].color;

        if (m_d->keyStrokes[i].isTransparent) {
            colors.transparentIndex = i;
        }
    }

    return colors;
}

struct SetKeyStrokeColorsCommand : public KUndo2Command {
    SetKeyStrokeColorsCommand(const PkList<KeyStroke> newList, PkList<KeyStroke> *list, KisColorizeMaskSP node)
        : m_newList(newList),
          m_oldList(*list),
          m_list(list),
          m_node(node) {}

    void redo() override {
        *m_list = m_newList;

        m_node->setNeedsUpdate(true);
        m_node->sigKeyStrokesListChanged();
        m_node->setDirty();
    }

    void undo() override {
        *m_list = m_oldList;

        m_node->setNeedsUpdate(true);
        m_node->sigKeyStrokesListChanged();
        m_node->setDirty();
    }

private:
    PkList<KeyStroke> m_newList;
    PkList<KeyStroke> m_oldList;
    PkList<KeyStroke> *m_list;
    KisColorizeMaskSP m_node;
};

void KisColorizeMask::setKeyStrokesColors(KeyStrokeColors colors)
{
    KIS_ASSERT_RECOVER_RETURN(colors.colors.size() == m_d->keyStrokes.size());

    PkList<KeyStroke> newList = m_d->keyStrokes;

    for (int i = 0; i < newList.size(); i++) {
        newList[i].color = colors.colors[i];
        newList[i].color.convertTo(colorSpace());
        newList[i].isTransparent = colors.transparentIndex == i;
    }

    KisProcessingApplicator applicator(image(), KisNodeSP(this),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_text("Change Key Stroke Color"));
    applicator.applyCommand(
        new SetKeyStrokeColorsCommand(
            newList, &m_d->keyStrokes, KisColorizeMaskSP(this)));

    applicator.end();
}

void KisColorizeMask::removeKeyStroke(const KoColor &_color)
{
    KoColor color = _color;
    color.convertTo(colorSpace());

    PkList<KeyStroke>::iterator it =
        std::find_if(m_d->keyStrokes.begin(),
                     m_d->keyStrokes.end(),
                     kismpl::mem_equal_to(&KeyStroke::color, color));

    KIS_SAFE_ASSERT_RECOVER_RETURN(it != m_d->keyStrokes.end());

    const int index = it - m_d->keyStrokes.begin();

    KisProcessingApplicator applicator(image(), KisNodeSP(this),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_text("Remove Key Stroke"));
    applicator.applyCommand(
        new KeyStrokeAddRemoveCommand(
            false, index, *it, &m_d->keyStrokes, KisColorizeMaskSP(this)));

    applicator.end();
}


PkVector<KisPaintDeviceSP> KisColorizeMask::allPaintDevices() const
{
    PkVector<KisPaintDeviceSP> devices;

    for (const KeyStroke &stroke : m_d->keyStrokes) {
        devices << stroke.dev;
    }

    devices << m_d->coloringProjection;
    devices << m_d->fakePaintDevice;

    return devices;
}

void KisColorizeMask::resetCache()
{
    m_d->filteredSource->clear();
    m_d->originalSequenceNumber = -1;
    m_d->filteringDirty = true;

    rerenderFakePaintDevice();
    slotUpdateRegenerateFilling(true);
}

void KisColorizeMask::setUseEdgeDetection(bool value)
{
    m_d->filteringOptions.useEdgeDetection = value;
    m_d->filteringDirty = true;
    setNeedsUpdate(true);
}

bool KisColorizeMask::useEdgeDetection() const
{
    return m_d->filteringOptions.useEdgeDetection;
}

void KisColorizeMask::setEdgeDetectionSize(qreal value)
{
    m_d->filteringOptions.edgeDetectionSize = value;
    m_d->filteringDirty = true;
    setNeedsUpdate(true);
}

qreal KisColorizeMask::edgeDetectionSize() const
{
    return m_d->filteringOptions.edgeDetectionSize;
}

void KisColorizeMask::setFuzzyRadius(qreal value)
{
    m_d->filteringOptions.fuzzyRadius = value;
    m_d->filteringDirty = true;
    setNeedsUpdate(true);
}

qreal KisColorizeMask::fuzzyRadius() const
{
    return m_d->filteringOptions.fuzzyRadius;
}

void KisColorizeMask::setCleanUpAmount(qreal value)
{
    m_d->filteringOptions.cleanUpAmount = value;
    setNeedsUpdate(true);
}

qreal KisColorizeMask::cleanUpAmount() const
{
    return m_d->filteringOptions.cleanUpAmount;
}

void KisColorizeMask::setLimitToDeviceBounds(bool value)
{
    m_d->limitToDeviceBounds = value;
    m_d->filteringDirty = true;
    setNeedsUpdate(true);
}

bool KisColorizeMask::limitToDeviceBounds() const
{
    return m_d->limitToDeviceBounds;
}

void KisColorizeMask::rerenderFakePaintDevice()
{
    m_d->fakePaintDevice->clear();
    KisFillPainter gc(m_d->fakePaintDevice);

    KisCachedSelection::Guard s1(m_d->cachedSelection);
    KisSelectionSP selection = s1.selection();

    for (const KeyStroke &stroke : m_d->keyStrokes) {
        const PkRect rect = stroke.dev->extent();

        selection->pixelSelection()->makeCloneFromRough(stroke.dev, rect);
        gc.setSelection(selection);
        gc.fillSelection(rect, stroke.color);
    }
}

void KisColorizeMask::testingAddKeyStroke(KisPaintDeviceSP dev, const KoColor &color, bool isTransparent)
{
    m_d->keyStrokes << KeyStroke(dev, color, isTransparent);
}

void KisColorizeMask::forceRegenerateMask()
{
    slotUpdateRegenerateFilling();
    m_d->updateIsRunning = false;
}

KisPaintDeviceSP KisColorizeMask::testingFilteredSource() const
{
    return m_d->filteredSource;
}

PkList<KeyStroke> KisColorizeMask::fetchKeyStrokesDirect() const
{
    return m_d->keyStrokes;
}

void KisColorizeMask::setKeyStrokesDirect(const PkList<KisLazyFillTools::KeyStroke> &strokes)
{
    m_d->keyStrokes = strokes;

    for (auto it = m_d->keyStrokes.begin(); it != m_d->keyStrokes.end(); ++it) {
        it->dev->setParentNode(this);
    }

    KIS_SAFE_ASSERT_RECOVER(image())
    {};
}

qint32 KisColorizeMask::x() const
{
    return m_d->offset.x();
}

qint32 KisColorizeMask::y() const
{
    return m_d->offset.y();
}

void KisColorizeMask::setX(qint32 x)
{
    const PkPoint oldOffset = m_d->offset;
    m_d->offset.rx() = x;
    moveAllInternalDevices(m_d->offset - oldOffset);
}

void KisColorizeMask::setY(qint32 y)
{
    const PkPoint oldOffset = m_d->offset;
    m_d->offset.ry() = y;
    moveAllInternalDevices(m_d->offset - oldOffset);
}

KisPaintDeviceList KisColorizeMask::getLodCapableDevices() const
{
    KisPaintDeviceList list;

    auto it = m_d->keyStrokes.begin();
    for(; it != m_d->keyStrokes.end(); ++it) {
        list << it->dev;
    }

    list << m_d->coloringProjection;
    list << m_d->fakePaintDevice;
    list << m_d->filteredSource;

    return list;
}

void KisColorizeMask::regeneratePrefilteredDeviceIfNeeded()
{
    if (!parent()) return;

    KisPaintDeviceSP src = parent()->original();
    KIS_ASSERT_RECOVER_RETURN(src);

    if (!m_d->filteredSourceValid(src)) {
        // update the prefiltered source if needed
        slotUpdateRegenerateFilling(true);
    }
}

void KisColorizeMask::moveAllInternalDevices(const PkPoint &diff)
{
    PkVector<KisPaintDeviceSP> devices = allPaintDevices();

    for (KisPaintDeviceSP dev : devices) {
        dev->moveTo(dev->offset() + diff);
    }
}
