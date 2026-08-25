/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISTYPES_H_
#define KISTYPES_H_

#include <PkContainerAlgo.h>
#include <PkPointer.h>
#include <PkPoint.h>

#include "kritaimage_export.h"

template<class T>
class KisWeakSharedPtr;
template<class T>
class KisSharedPtr;

template<class T> class PkSharedPointer;
template<class T> class PkWeakPointer;

template <class T>
uint qHash(KisSharedPtr<T> ptr) {
    return qHash(ptr.data());
}

template <class T>
uint qHash(KisWeakSharedPtr<T> ptr) {
    return qHash(ptr.data());
}

namespace std {
template <class T>
struct hash<KisSharedPtr<T>>
{
    size_t operator()(const KisSharedPtr<T> &ptr) const {
        return hash<const T*>()(ptr.data());
    }
};

template <class T>
struct hash<KisWeakSharedPtr<T>>
{
    size_t operator()(const KisWeakSharedPtr<T> &ptr) const {
        return hash<const T*>()(ptr.data());
    }
};

}


/**
 * Define lots of shared pointer versions of Krita classes.
 * Shared pointer classes have the advantage of near automatic
 * memory management (but beware of circular references)
 * These types should never be passed by reference,
 * because that will mess up their reference counter.
 *
 * An example of the naming pattern used:
 *
 * KisPaintDeviceSP is a KisSharedPtr of KisPaintDevice
 * KisPaintDeviceWSP is a KisWeakSharedPtr of KisPaintDevice
 * vKisPaintDeviceSP is a PkVector of KisPaintDeviceSP
 * vKisPaintDeviceSP_it is an iterator of vKisPaintDeviceSP
 *
 */
class KisImage;
typedef KisSharedPtr<KisImage> KisImageSP;
typedef KisWeakSharedPtr<KisImage> KisImageWSP;

class KisPaintDevice;
typedef KisSharedPtr<KisPaintDevice> KisPaintDeviceSP;
typedef KisWeakSharedPtr<KisPaintDevice> KisPaintDeviceWSP;
typedef PkVector<KisPaintDeviceSP> vKisPaintDeviceSP;
typedef vKisPaintDeviceSP::iterator vKisPaintDeviceSP_it;

class KisFixedPaintDevice;
typedef KisSharedPtr<KisFixedPaintDevice> KisFixedPaintDeviceSP;

class KisMask;
typedef KisSharedPtr<KisMask> KisMaskSP;
typedef KisWeakSharedPtr<KisMask> KisMaskWSP;

class KisNode;
typedef KisSharedPtr<KisNode> KisNodeSP;
typedef KisWeakSharedPtr<KisNode> KisNodeWSP;
typedef PkVector<KisNodeSP> vKisNodeSP;
typedef vKisNodeSP::iterator vKisNodeSP_it;
typedef vKisNodeSP::const_iterator vKisNodeSP_cit;

class KisBaseNode;
typedef KisSharedPtr<KisBaseNode> KisBaseNodeSP;
typedef KisWeakSharedPtr<KisBaseNode> KisBaseNodeWSP;

class KisEffectMask;
typedef KisSharedPtr<KisEffectMask> KisEffectMaskSP;
typedef KisWeakSharedPtr<KisEffectMask> KisEffectMaskWSP;

class KisFilterMask;
typedef KisSharedPtr<KisFilterMask> KisFilterMaskSP;
typedef KisWeakSharedPtr<KisFilterMask> KisFilterMaskWSP;

class KisTransformMask;
typedef KisSharedPtr<KisTransformMask> KisTransformMaskSP;
typedef KisWeakSharedPtr<KisTransformMask> KisTransformMaskWSP;

class KisTransformMaskParamsInterface;
typedef PkSharedPointer<KisTransformMaskParamsInterface> KisTransformMaskParamsInterfaceSP;
typedef PkWeakPointer<KisTransformMaskParamsInterface> KisTransformMaskParamsInterfaceWSP;

class KisTransparencyMask;
typedef KisSharedPtr<KisTransparencyMask> KisTransparencyMaskSP;
typedef KisWeakSharedPtr<KisTransparencyMask> KisTransparencyMaskWSP;

class KisColorizeMask;
typedef KisSharedPtr<KisColorizeMask> KisColorizeMaskSP;
typedef KisWeakSharedPtr<KisColorizeMask> KisColorizeMaskWSP;

class KisLayer;
typedef KisSharedPtr<KisLayer> KisLayerSP;
typedef KisWeakSharedPtr<KisLayer> KisLayerWSP;

class KisShapeLayer;
typedef KisSharedPtr<KisShapeLayer> KisShapeLayerSP;

class KisPaintLayer;
typedef KisSharedPtr<KisPaintLayer> KisPaintLayerSP;

class KisAdjustmentLayer;
typedef KisSharedPtr<KisAdjustmentLayer> KisAdjustmentLayerSP;

class KisGeneratorLayer;
typedef KisSharedPtr<KisGeneratorLayer> KisGeneratorLayerSP;

class KisCloneLayer;
typedef KisSharedPtr<KisCloneLayer> KisCloneLayerSP;
typedef KisWeakSharedPtr<KisCloneLayer> KisCloneLayerWSP;

class KisGroupLayer;
typedef KisSharedPtr<KisGroupLayer> KisGroupLayerSP;
typedef KisWeakSharedPtr<KisGroupLayer> KisGroupLayerWSP;

class KisFileLayer;
typedef KisSharedPtr<KisFileLayer> KisFileLayerSP;
typedef KisWeakSharedPtr<KisFileLayer> KisFileLayerWSP;

class KisSelection;
typedef KisSharedPtr<KisSelection> KisSelectionSP;
typedef KisWeakSharedPtr<KisSelection> KisSelectionWSP;

class KisSelectionComponent;
typedef KisSharedPtr<KisSelectionComponent> KisSelectionComponentSP;

class KisSelectionMask;
typedef KisSharedPtr<KisSelectionMask> KisSelectionMaskSP;

class KisPixelSelection;
typedef KisSharedPtr<KisPixelSelection> KisPixelSelectionSP;

class KisHistogram;
typedef KisSharedPtr<KisHistogram> KisHistogramSP;

typedef PkVector<PkPoint> vKisSegments;

class KisFilter;
typedef KisSharedPtr<KisFilter> KisFilterSP;

class KisLayerStyleFilter;
typedef KisSharedPtr<KisLayerStyleFilter> KisLayerStyleFilterSP;

class KisGenerator;
typedef KisSharedPtr<KisGenerator> KisGeneratorSP;

class KisConvolutionKernel;
typedef KisSharedPtr<KisConvolutionKernel> KisConvolutionKernelSP;

class KisAnnotation;
typedef KisSharedPtr<KisAnnotation> KisAnnotationSP;
typedef PkVector<KisAnnotationSP> vKisAnnotationSP;
typedef vKisAnnotationSP::iterator vKisAnnotationSP_it;
typedef vKisAnnotationSP::const_iterator vKisAnnotationSP_cit;

class KisAnimationFrameCache;
typedef KisSharedPtr<KisAnimationFrameCache> KisAnimationFrameCacheSP;
typedef KisWeakSharedPtr<KisAnimationFrameCache> KisAnimationFrameCacheWSP;

class KisPaintingAssistant;
typedef PkSharedPointer<KisPaintingAssistant> KisPaintingAssistantSP;
typedef PkWeakPointer<KisPaintingAssistant> KisPaintingAssistantWSP;

class KisReferenceImage;
typedef PkSharedPointer<KisReferenceImage> KisReferenceImageSP;
typedef PkWeakPointer<KisReferenceImage> KisReferenceImageWSP;

// Repeat iterators
class KisHLineIterator2;
template<class T> class KisRepeatHLineIteratorPixelBase;
typedef KisRepeatHLineIteratorPixelBase< KisHLineIterator2 > KisRepeatHLineConstIteratorNG;
typedef KisSharedPtr<KisRepeatHLineConstIteratorNG> KisRepeatHLineConstIteratorSP;

class KisVLineIterator2;
template<class T> class KisRepeatVLineIteratorPixelBase;
typedef KisRepeatVLineIteratorPixelBase< KisVLineIterator2 > KisRepeatVLineConstIteratorNG;
typedef KisSharedPtr<KisRepeatVLineConstIteratorNG> KisRepeatVLineConstIteratorSP;


// NG Iterators
class KisHLineIteratorNG;
typedef KisSharedPtr<KisHLineIteratorNG> KisHLineIteratorSP;

class KisHLineConstIteratorNG;
typedef KisSharedPtr<KisHLineConstIteratorNG> KisHLineConstIteratorSP;

class KisVLineIteratorNG;
typedef KisSharedPtr<KisVLineIteratorNG> KisVLineIteratorSP;

class KisVLineConstIteratorNG;
typedef KisSharedPtr<KisVLineConstIteratorNG> KisVLineConstIteratorSP;

class KisRandomConstAccessorNG;
typedef KisSharedPtr<KisRandomConstAccessorNG> KisRandomConstAccessorSP;

class KisRandomAccessorNG;
typedef KisSharedPtr<KisRandomAccessorNG> KisRandomAccessorSP;

class KisRandomSubAccessor;
typedef KisSharedPtr<KisRandomSubAccessor> KisRandomSubAccessorSP;

// Things

typedef PkVector<PkPointF> vQPointF;

class KisPaintOpPreset;
typedef PkSharedPointer<KisPaintOpPreset> KisPaintOpPresetSP;
typedef PkWeakPointer<KisPaintOpPreset> KisPaintOpPresetWSP;

template <typename T>
class KisPinnedSharedPtr;

class KisPaintOpSettings;
typedef KisPinnedSharedPtr<KisPaintOpSettings> KisPaintOpSettingsSP;

template <typename T>
class KisRestrictedSharedPtr;
typedef KisRestrictedSharedPtr<KisPaintOpSettings> KisPaintOpSettingsRestrictedSP;

class KisPaintOp;
typedef KisSharedPtr<KisPaintOp> KisPaintOpSP;

class KoID;
typedef PkList<KoID> KoIDList;

class KoUpdater;
template<class T> class PkPointer;
typedef PkPointer<KoUpdater> KoUpdaterPtr;

class KisProcessingVisitor;
typedef KisSharedPtr<KisProcessingVisitor> KisProcessingVisitorSP;

class KUndo2Command;
typedef PkSharedPointer<KUndo2Command> KUndo2CommandSP;

typedef PkList<KisNodeSP> KisNodeList;
typedef PkSharedPointer<KisNodeList> KisNodeListSP;

typedef PkList<KisPaintDeviceSP> KisPaintDeviceList;

class KisStroke;
typedef PkSharedPointer<KisStroke> KisStrokeSP;
typedef PkWeakPointer<KisStroke> KisStrokeWSP;
typedef KisStrokeWSP KisStrokeId;

class KisFilterConfiguration;
typedef KisPinnedSharedPtr<KisFilterConfiguration> KisFilterConfigurationSP;

class KisPropertiesConfiguration;
typedef KisPinnedSharedPtr<KisPropertiesConfiguration> KisPropertiesConfigurationSP;

class KisLockedProperties;
typedef KisSharedPtr<KisLockedProperties> KisLockedPropertiesSP;

class KisProjectionUpdatesFilter;
typedef PkSharedPointer<KisProjectionUpdatesFilter> KisProjectionUpdatesFilterSP;
using KisProjectionUpdatesFilterCookie = void*;

class KisAbstractProjectionPlane;
typedef PkSharedPointer<KisAbstractProjectionPlane> KisAbstractProjectionPlaneSP;
typedef PkWeakPointer<KisAbstractProjectionPlane> KisAbstractProjectionPlaneWSP;

class KisProjectionLeaf;
typedef PkSharedPointer<KisProjectionLeaf> KisProjectionLeafSP;
typedef PkWeakPointer<KisProjectionLeaf> KisProjectionLeafWSP;

class KisKeyframe;
typedef PkSharedPointer<KisKeyframe> KisKeyframeSP;
typedef PkWeakPointer<KisKeyframe> KisKeyframeWSP;

class KisScalarKeyframe;
typedef PkSharedPointer<KisScalarKeyframe> KisScalarKeyframeSP;
typedef PkWeakPointer<KisScalarKeyframe> KisScalarKeyframeWSP;

class KisRasterKeyframe;
typedef PkSharedPointer<KisRasterKeyframe> KisRasterKeyframeSP;
typedef PkWeakPointer<KisRasterKeyframe> KisRasterKeyframeWSP;

class KisFilterChain;
typedef KisSharedPtr<KisFilterChain> KisFilterChainSP;

class KisProofingConfiguration;
typedef PkSharedPointer<KisProofingConfiguration> KisProofingConfigurationSP;
typedef PkWeakPointer<KisProofingConfiguration> KisProofingConfigurationWSP;

class KisLayerComposition;
typedef PkSharedPointer<KisLayerComposition> KisLayerCompositionSP;
typedef PkWeakPointer<KisLayerComposition> KisLayerCompositionWSP;

class KisMirrorAxis;
typedef KisSharedPtr<KisMirrorAxis> KisMirrorAxisSP;
typedef KisWeakSharedPtr<KisMirrorAxis> KisMirrorAxisWSP;

class StoryboardItem;
typedef PkSharedPointer<StoryboardItem> StoryboardItemSP;
typedef PkVector<StoryboardItemSP> StoryboardItemList;

class StoryboardComment;
typedef PkVector<StoryboardComment> StoryboardCommentList;

class KisImageResolutionProxy;
using KisImageResolutionProxySP = PkSharedPointer<KisImageResolutionProxy>;

/**
 * Thumbnail generation mode, 'Precise' means to use exactBounds() and
 * 'Coarse' means to use extent() for the thumbnail generation.
 */
enum class KisThumbnailBoundsMode {
    Coarse,
    Precise
};

#include <PkSharedPointer.h>
#include <kis_shared_ptr.h>
#include <kis_restricted_shared_ptr.h>
#include <kis_pinned_shared_ptr.h>

#endif // KISTYPES_H_

