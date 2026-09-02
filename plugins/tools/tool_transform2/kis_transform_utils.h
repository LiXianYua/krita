/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_TRANSFORM_UTILS_H
#define __KIS_TRANSFORM_UTILS_H

#include <PkGlobal.h>

#include "kis_coordinates_converter.h"

#include <PkTransform.h>
#include <PkMatrix4x4.h>
#include <kis_processing_visitor.h>
#include <limits>

// for kisSquareDistance only
#include "kis_global.h"

#include "tool_transform_args.h"

class ToolTransformArgs;
class KisTransformWorker;
class TransformTransactionProperties;
class KisSavedMacroCommand;
class KisStrokeUndoFacade;
class KisStrokeJobData;

class KisTransformUtils
{
public:

    static const int rotationHandleVisualRadius;
    static const int handleVisualRadius;
    static const int handleRadius;
    static const int rotationHandleRadius;

    template <class T>
    static T flakeToImage(const KisCoordinatesConverter *converter, T object) {
        return converter->documentToImage(converter->flakeToDocument(object));
    }

    template <class T>
    static T imageToFlake(const KisCoordinatesConverter *converter, T object) {
        return converter->documentToFlake(converter->imageToDocument(object));
    }

    static PkTransform imageToFlakeTransform(const KisCoordinatesConverter *converter);
    static qreal effectiveHandleGrabRadius(const KisCoordinatesConverter *converter);

    static qreal effectiveRotationHandleGrabRadius(const KisCoordinatesConverter *converter);

    static qreal scaleFromAffineMatrix(const PkTransform &t);
    static qreal scaleFromPerspectiveMatrixX(const PkTransform &t, const PkPointF &basePt);
    static qreal scaleFromPerspectiveMatrixY(const PkTransform &t, const PkPointF &basePt);
    static qreal effectiveSize(const PkRectF &rc);
    static bool thumbnailTooSmall(const PkTransform &resultThumbTransform, const PkRect &originalImageRect);

    static PkRectF handleRect(qreal radius, const PkTransform &t, const PkRectF &limitingRect, qreal *dOutX, qreal *dOutY);
    static PkRectF handleRect(qreal radius, const PkTransform &t, const PkRectF &limitingRect, const PkPointF &basePoint);

    static PkPointF clipInRect(PkPointF p, PkRectF r);

    struct MatricesPack
    {
        MatricesPack(const ToolTransformArgs &args);

        PkTransform TS;
        PkTransform SC;
        PkTransform S;
        PkMatrix4x4 P;
        PkTransform projectedP;
        PkTransform T;

        PkTransform BRI;

        // the final transformation looks like
        // transform = TS * BRI * SC * S * projectedP * T
        PkTransform finalTransform() const;
    };

    static bool checkImageTooBig(const PkRectF &bounds, const MatricesPack &m, qreal cameraHeight);

    static KisTransformWorker createTransformWorker(const ToolTransformArgs &config,
                                                    KisPaintDeviceSP device,
                                                    KoUpdaterPtr updater);

    static void transformDevice(const ToolTransformArgs &config,
                                KisPaintDeviceSP device,
                                KisProcessingVisitor::ProgressHelper *helper);

    static void transformDevice(const ToolTransformArgs &config,
                                KisPaintDeviceSP srcDevice,
                                KisPaintDeviceSP dstDevice,
                                KisProcessingVisitor::ProgressHelper *helper);

    static void transformDeviceWithCroppedDst(const ToolTransformArgs &config,
                                              KisPaintDeviceSP srcDevice,
                                              KisPaintDeviceSP dstDevice,
                                              KisProcessingVisitor::ProgressHelper *helper,
                                              bool forceSubPixelTranslation);

    static PkRect needRect(const ToolTransformArgs &config,
                          const PkRect &rc,
                          const PkRect &srcBounds);

    static PkRect changeRect(const ToolTransformArgs &config,
                            const PkRect &rc);

    template<typename Function>
    class HandleChooser {
    public:
        HandleChooser(const PkPointF &cursorPos, Function defaultFunction)
            : m_cursorPos(cursorPos),
              m_minDistance(std::numeric_limits<qreal>::max()),
              m_function(defaultFunction)
        {
        }

        bool addFunction(const PkPointF &pt, qreal radius, Function function) {
            bool result = false;
            qreal distance = kisSquareDistance(pt, m_cursorPos);

            if (distance < pow2(radius) && distance < m_minDistance) {
                m_minDistance = distance;
                m_function = function;
                result = true;
            }

            return result;
        }

        Function function() const {
            return m_function;
        }

    private:
        PkPointF m_cursorPos;
        qreal m_minDistance;
        Function m_function;
    };

    /**
     * A special class that ensures that the view position of the anchor point of the
     * transformation is unchanged during the lifetime of the object. On destruction
     * of the keeper the position of the anchor point will be restored.
     */
    struct AnchorHolder {
        AnchorHolder(bool enabled, ToolTransformArgs *config);
        ~AnchorHolder();

    private:
        bool m_enabled;
        ToolTransformArgs *m_config;
        PkPointF m_staticPoint;
        PkPointF m_oldStaticPointInView;

    };

    static void setDefaultWarpPoints(int pointsPerLine,
                                     const TransformTransactionProperties *transaction,
                                     ToolTransformArgs *config);

    static ToolTransformArgs resetArgsForMode(ToolTransformArgs::TransformMode mode,
                                              const PkString &filterId,
                                              const TransformTransactionProperties &transaction, KisPaintDeviceSP externalSource);

    static bool shouldRestartStrokeOnModeChange(ToolTransformArgs::TransformMode oldMode,
                                                ToolTransformArgs::TransformMode newMode,
                                                KisNodeList processedNodes);

    static void transformAndMergeDevice(const ToolTransformArgs &config,
                                        KisPaintDeviceSP src,
                                        KisPaintDeviceSP dst,
                                        KisProcessingVisitor::ProgressHelper *helper);

    static void postProcessToplevelCommand(KUndo2Command *command,
                                           const ToolTransformArgs &args,
                                           KisNodeList rootNodes,
                                           KisNodeList processedNodes, int currentTime,
                                           const KisSavedMacroCommand *overriddenCommand);

    static bool fetchArgsFromCommand(const KUndo2Command *command,
                                     ToolTransformArgs *args,
                                     KisNodeList *rootNodes,
                                     KisNodeList *transformedNodes, int *oldTime);

    static KisNodeSP tryOverrideRootToTransformMask(KisNodeSP root);

    static int fetchCurrentImageTime(KisNodeList rootNodes);
    static PkList<KisNodeSP> fetchNodesList(ToolTransformArgs::TransformMode mode, KisNodeList rootNodes, bool isExternalSourcePresent, KisSelectionSP selection);
    static bool tryInitArgsFromNode(KisNodeList rootNodes, ToolTransformArgs *args);
    static bool tryFetchArgsFromCommandAndUndo(ToolTransformArgs *outArgs, ToolTransformArgs::TransformMode mode, KisNodeList currentNodes, KisNodeList selectedNodes, KisStrokeUndoFacade *undoFacade, int currentTime, PkVector<KisStrokeJobData *> *undoJobs, const KisSavedMacroCommand **overriddenCommand);

};

#endif /* __KIS_TRANSFORM_UTILS_H */
