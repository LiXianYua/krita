/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_transform_utils.h"

#include <cmath>
#include <PkPainterPath.h>
#include <PkTransform.h>
#include <PkVectorND.h>
#include <KoUnit.h>
#include "tool_transform_args.h"
#include "kis_paint_device.h"
#include "kis_algebra_2d.h"
#include "transform_transaction_properties.h"
#include "kis_painter.h"

#include <kis_transform_worker.h>
#include <kis_perspectivetransform_worker.h>
#include <kis_warptransform_worker.h>
#include <kis_cage_transform_worker.h>
#include <kis_liquify_transform_worker.h>

#include "commands_new/kis_saved_commands.h"
#include "kis_transform_mask.h"
#include "kis_transform_mask_adapter.h"
#include "krita_container_utils.h"
#include "kis_selection.h"
#include "kis_image.h"
#include "kis_image_animation_interface.h"
#include "kis_layer_utils.h"
#include "krita_utils.h"
#include "kundo2commandextradata.h"

const int KisTransformUtils::rotationHandleVisualRadius = 12;
const int KisTransformUtils::rotationHandleRadius = 8;
const int KisTransformUtils::handleVisualRadius = 12;
const int KisTransformUtils::handleRadius = 8;


PkTransform KisTransformUtils::imageToFlakeTransform(const KisCoordinatesConverter *converter)
{
    return converter->imageToDocumentTransform() * converter->documentToFlakeTransform();
}

qreal KisTransformUtils::effectiveHandleGrabRadius(const KisCoordinatesConverter *converter)
{
    PkPointF handleRadiusPt = flakeToImage(converter, PkPointF(handleRadius, handleRadius));
    return (handleRadiusPt.x() > handleRadiusPt.y()) ? handleRadiusPt.x() : handleRadiusPt.y();
}

qreal KisTransformUtils::effectiveRotationHandleGrabRadius(const KisCoordinatesConverter *converter)
{
    PkPointF handleRadiusPt = flakeToImage(converter, PkPointF(rotationHandleRadius, rotationHandleRadius));
    return (handleRadiusPt.x() > handleRadiusPt.y()) ? handleRadiusPt.x() : handleRadiusPt.y();
}

qreal KisTransformUtils::scaleFromAffineMatrix(const PkTransform &t) {
    return KoUnit::approxTransformScale(t);
}

qreal KisTransformUtils::scaleFromPerspectiveMatrixX(const PkTransform &t, const PkPointF &basePt) {
    const PkPointF pt = basePt + PkPointF(1.0, 0);
    return kisDistance(t.map(pt), t.map(basePt));
}

qreal KisTransformUtils::scaleFromPerspectiveMatrixY(const PkTransform &t, const PkPointF &basePt) {
    const PkPointF pt = basePt + PkPointF(0, 1.0);
    return kisDistance(t.map(pt), t.map(basePt));
}

qreal KisTransformUtils::effectiveSize(const PkRectF &rc) {
    return 0.5 * (rc.width() + rc.height());
}

bool KisTransformUtils::thumbnailTooSmall(const PkTransform &resultThumbTransform, const PkRect &originalImageRect)
{
    return KisAlgebra2D::minDimension(resultThumbTransform.mapRect(originalImageRect)) < 32;
}

PkRectF handleRectImpl(qreal radius, const PkTransform &t, const PkRectF &limitingRect, const PkPointF &basePoint, qreal *dOutX, qreal *dOutY) {
    const qreal handlesExtraScaleX =
        KisTransformUtils::scaleFromPerspectiveMatrixX(t, basePoint);
    const qreal handlesExtraScaleY =
        KisTransformUtils::scaleFromPerspectiveMatrixY(t, basePoint);

    const qreal maxD = 0.2 * KisTransformUtils::effectiveSize(limitingRect);
    const qreal dX = qMin(maxD, radius / handlesExtraScaleX);
    const qreal dY = qMin(maxD, radius / handlesExtraScaleY);

    PkRectF handleRect(-0.5 * dX, -0.5 * dY, dX, dY);

    if (dOutX) {
        *dOutX = dX;
    }

    if (dOutY) {
        *dOutY = dY;
    }

    return handleRect;

}

PkRectF KisTransformUtils::handleRect(qreal radius, const PkTransform &t, const PkRectF &limitingRect, qreal *dOutX, qreal *dOutY) {
    return handleRectImpl(radius, t, limitingRect, limitingRect.center(), dOutX, dOutY);
}

PkRectF KisTransformUtils::handleRect(qreal radius, const PkTransform &t, const PkRectF &limitingRect, const PkPointF &basePoint) {
    return handleRectImpl(radius, t, limitingRect, basePoint, 0, 0);
}

PkPointF KisTransformUtils::clipInRect(PkPointF p, PkRectF r)
{
    PkPointF center = r.center();
    PkPointF t = p - center;
    r.translate(- center);

    if (t.y() != 0) {
        if (t.x() != 0) {
            double slope = t.y() / t.x();

            if (t.x() < r.left()) {
                t.setY(r.left() * slope);
                t.setX(r.left());
            }
            else if (t.x() > r.right()) {
                t.setY(r.right() * slope);
                t.setX(r.right());
            }

            if (t.y() < r.top()) {
                t.setX(r.top() / slope);
                t.setY(r.top());
            }
            else if (t.y() > r.bottom()) {
                t.setX(r.bottom() / slope);
                t.setY(r.bottom());
            }
        }
        else {
            if (t.y() < r.top())
                t.setY(r.top());
            else if (t.y() > r.bottom())
                t.setY(r.bottom());
        }
    }
    else {
        if (t.x() < r.left())
            t.setX(r.left());
        else if (t.x() > r.right())
            t.setX(r.right());
    }

    t += center;

    return t;
}

KisTransformUtils::MatricesPack::MatricesPack(const ToolTransformArgs &args)
{
    TS = PkTransform::fromTranslate(-args.originalCenter().x(), -args.originalCenter().y());
    SC = PkTransform::fromScale(args.scaleX(), args.scaleY());
    S.shear(0, args.shearY()); S.shear(args.shearX(), 0);

    if (args.mode() == ToolTransformArgs::FREE_TRANSFORM) {
        BRI.rotate(180. * -args.boundsRotation() / M_PI);
        P.rotate(180. * normalizeAngle(args.aX()) / M_PI, PkVector3D(1, 0, 0));
        P.rotate(180. * normalizeAngle(args.aY()) / M_PI, PkVector3D(0, 1, 0));
        P.rotate(180. * normalizeAngle(args.aZ()) / M_PI, PkVector3D(0, 0, 1));
        projectedP = P.toTransform(args.cameraPos().z());
    } else if (args.mode() == ToolTransformArgs::PERSPECTIVE_4POINT) {
        // see a comment in KisPerspectiveTransformStrategy::Private::transformIntoArgs()
#if 0
        projectedP.rotate(kisRadiansToDegrees(args.aZ()));
        projectedP *= args.flattenedPerspectiveTransform();
#else
        projectedP = args.flattenedPerspectiveTransform();
#endif
        P = PkMatrix4x4(projectedP);
    }

    PkPointF translation = args.transformedCenter();
    T = PkTransform::fromTranslate(translation.x(), translation.y());
}

PkTransform KisTransformUtils::MatricesPack::finalTransform() const
{
    return TS * BRI * SC * S * projectedP * T;
}

bool KisTransformUtils::checkImageTooBig(const PkRectF &bounds, const MatricesPack &m, qreal cameraHeight)
{
    bool imageTooBig = false;

    PkMatrix4x4 unprojectedMatrix = PkMatrix4x4(m.T) * m.P * PkMatrix4x4(m.TS * m.SC * m.S);
    PkVector<PkPointF> points;
    points << bounds.topLeft();
    points << bounds.topRight();
    points << bounds.bottomRight();
    points << bounds.bottomLeft();

    for (const PkPointF &pt : points) {
        PkVector4D v(pt.x(), pt.y(), 0, 1);

        v = unprojectedMatrix * v;
        qreal z = v.z() / v.w();

        imageTooBig = z > 1.5 * cameraHeight;

        if (imageTooBig) {
            break;
        }
    }

    return imageTooBig;
}

KisTransformWorker KisTransformUtils::createTransformWorker(const ToolTransformArgs &config,
                                                            KisPaintDeviceSP device,
                                                            KoUpdaterPtr updater)
{
    double scaleX = config.scaleX();
    double scaleY = config.scaleY();
    double shearX = config.shearX();
    double shearY = config.shearY();
    double aZ = config.aZ();

    if (config.boundsRotation() != 0.0) {
        const KisTransformUtils::MatricesPack m(config);
        PkTransform Z; Z.rotateRadians(aZ);
        PkTransform desired = m.BRI * m.SC * m.S * Z;
        KisAlgebra2D::DecomposedMatrix dm(desired);
        if (dm.isValid()) {
            scaleX = dm.scaleX;
            scaleY = dm.scaleY;
            shearX = dm.shearXY;
            shearY = 0;
            aZ = kisDegreesToRadians(dm.angle);
        }
    }

    PkPointF transformedCenter;
    {
        KisTransformWorker t(0,
                             scaleX, scaleY,
                             shearX, shearY,
                             aZ,
                             0, // set X and Y translation
                             0, // to null for calculation
                             0,
                             config.filter());

        transformedCenter = t.transform().map(config.originalCenter());
    }

    PkPointF translation = config.transformedCenter() - transformedCenter;

    KisTransformWorker transformWorker(device,
                                       scaleX, scaleY,
                                       shearX, shearY,
                                       normalizeAngle(aZ),
                                       translation.x(),
                                       translation.y(),
                                       updater,
                                       config.filter());

    // MatricesPack m(config);
    // ENTER_FUNCTION() << ppVar(m.finalTransform() - transformWorker.transform());

    return transformWorker;
}

void KisTransformUtils::transformDevice(const ToolTransformArgs &config,
                                        KisPaintDeviceSP device,
                                        KisProcessingVisitor::ProgressHelper *helper)
{
    KisPaintDeviceSP tmp = new KisPaintDevice(*device);
    transformDevice(config, tmp, device, helper);
}


namespace {

void transformDeviceImpl(const ToolTransformArgs &config,
                         KisPaintDeviceSP srcDevice,
                         KisPaintDeviceSP dstDevice,
                         KisProcessingVisitor::ProgressHelper *helper,
                         bool cropDst,
                         bool forceSubPixelTranslation)
{
    if (config.mode() == ToolTransformArgs::WARP) {
        KoUpdaterPtr updater = helper->updater();

        KisWarpTransformWorker worker(config.warpType(),
                                      config.origPoints(),
                                      config.transfPoints(),
                                      config.alpha(),
                                      updater);
        worker.run(srcDevice, dstDevice);
    } else if (config.mode() == ToolTransformArgs::CAGE) {
        KoUpdaterPtr updater = helper->updater();

        dstDevice->makeCloneFromRough(srcDevice, srcDevice->extent());

        KisCageTransformWorker worker(srcDevice->region().boundingRect(),
                                      config.origPoints(),
                                      updater,
                                      config.pixelPrecision());

        worker.prepareTransform();
        worker.setTransformedCage(config.transfPoints());
        worker.run(srcDevice, dstDevice);
    } else if (config.mode() == ToolTransformArgs::LIQUIFY && config.liquifyWorker()) {
        KoUpdaterPtr updater = helper->updater();
        //FIXME:
        (void)updater;

        config.liquifyWorker()->run(srcDevice, dstDevice);
    } else if (config.mode() == ToolTransformArgs::MESH) {
        KoUpdaterPtr updater = helper->updater();
        //FIXME:
        (void)updater;

        dstDevice->clear();
        config.meshTransform()->transformMesh(srcDevice, dstDevice);

    } else {
        KoUpdaterPtr updater1 = helper->updater();
        KoUpdaterPtr updater2 = helper->updater();

        dstDevice->makeCloneFromRough(srcDevice, srcDevice->extent());

        KisTransformWorker transformWorker =
            KisTransformUtils::createTransformWorker(config, dstDevice, updater1);

        transformWorker.setForceSubPixelTranslation(forceSubPixelTranslation);
        transformWorker.run();

        KisPerspectiveTransformWorker::SampleType sampleType =
            config.filterId() == "NearestNeighbor" ?
            KisPerspectiveTransformWorker::NearestNeighbour :
            KisPerspectiveTransformWorker::Bilinear;

        if (config.mode() == ToolTransformArgs::FREE_TRANSFORM) {
            KisPerspectiveTransformWorker perspectiveWorker(dstDevice,
                                                            config.transformedCenter(),
                                                            config.aX(),
                                                            config.aY(),
                                                            config.cameraPos().z(),
                                                            cropDst,
                                                            updater2);
            perspectiveWorker.setForceSubPixelTranslation(forceSubPixelTranslation);
            perspectiveWorker.run(sampleType);
        } else if (config.mode() == ToolTransformArgs::PERSPECTIVE_4POINT) {
            PkTransform T =
                PkTransform::fromTranslate(config.transformedCenter().x(),
                                          config.transformedCenter().y());

            KisPerspectiveTransformWorker perspectiveWorker(dstDevice,
                                                            T.inverted() * config.flattenedPerspectiveTransform() * T,
                                                            cropDst,
                                                            updater2);
            perspectiveWorker.setForceSubPixelTranslation(forceSubPixelTranslation);
            perspectiveWorker.run(sampleType);
        }
    }
}

}

void KisTransformUtils::transformDevice(const ToolTransformArgs &config,
                                        KisPaintDeviceSP srcDevice,
                                        KisPaintDeviceSP dstDevice,
                                        KisProcessingVisitor::ProgressHelper *helper)
{
    transformDeviceImpl(config, srcDevice, dstDevice, helper, false, false);
}

void KisTransformUtils::transformDeviceWithCroppedDst(const ToolTransformArgs &config, KisPaintDeviceSP srcDevice, KisPaintDeviceSP dstDevice, KisProcessingVisitor::ProgressHelper *helper, bool forceSubPixelTranslation)
{
    transformDeviceImpl(config, srcDevice, dstDevice, helper, true, forceSubPixelTranslation);
}

PkRect KisTransformUtils::needRect(const ToolTransformArgs &config,
                                  const PkRect &rc,
                                  const PkRect &srcBounds)
{
    PkRect result = rc;

    if (config.mode() == ToolTransformArgs::WARP) {
        KisWarpTransformWorker worker(config.warpType(),
                                      config.origPoints(),
                                      config.transfPoints(),
                                      config.alpha(),
                                      0);

        result = worker.approxNeedRect(rc, srcBounds);

    } else if (config.mode() == ToolTransformArgs::CAGE) {
        KisCageTransformWorker worker(srcBounds,
                                      config.origPoints(),
                                      0,
                                      config.pixelPrecision());
        worker.setTransformedCage(config.transfPoints());
        result = worker.approxNeedRect(rc, srcBounds);
    } else if (config.mode() == ToolTransformArgs::LIQUIFY) {
        result = config.liquifyWorker() ?
            config.liquifyWorker()->approxNeedRect(rc, srcBounds) : rc;
    } else if (config.mode() == ToolTransformArgs::MESH) {
        result = config.meshTransform()->approxNeedRect(rc);
    } else {
        KIS_ASSERT_RECOVER_NOOP(0 && "this works for non-affine transformations only!");
    }

    return result;
}

PkRect KisTransformUtils::changeRect(const ToolTransformArgs &config,
                                    const PkRect &rc)
{
    PkRect result = rc;

    if (config.mode() == ToolTransformArgs::WARP) {
        KisWarpTransformWorker worker(config.warpType(),
                                      config.origPoints(),
                                      config.transfPoints(),
                                      config.alpha(),
                                      0);

        result = worker.approxChangeRect(rc);

    } else if (config.mode() == ToolTransformArgs::CAGE) {
        KisCageTransformWorker worker(rc,
                                      config.origPoints(),
                                      0,
                                      config.pixelPrecision());

        worker.setTransformedCage(config.transfPoints());
        result = worker.approxChangeRect(rc);
    } else if (config.mode() == ToolTransformArgs::LIQUIFY) {
        result = config.liquifyWorker() ?
            config.liquifyWorker()->approxChangeRect(rc) : rc;
    } else if (config.mode() == ToolTransformArgs::MESH) {
        result = config.meshTransform()->approxChangeRect(rc);

    } else {
        KIS_ASSERT_RECOVER_NOOP(0 && "this works for non-affine transformations only!");
    }

    return result;
}

KisTransformUtils::AnchorHolder::AnchorHolder(bool enabled, ToolTransformArgs *config)
    : m_enabled(enabled),
      m_config(config)
{
    if (!m_enabled) return;

    m_staticPoint = m_config->originalCenter() + m_config->rotationCenterOffset();

    const KisTransformUtils::MatricesPack m(*m_config);
    m_oldStaticPointInView = m.finalTransform().map(m_staticPoint);
}

KisTransformUtils::AnchorHolder::~AnchorHolder() {
    if (!m_enabled) return;

    const KisTransformUtils::MatricesPack m(*m_config);
    const PkPointF newStaticPointInView = m.finalTransform().map(m_staticPoint);

    const PkPointF diff = m_oldStaticPointInView - newStaticPointInView;

    m_config->setTransformedCenter(m_config->transformedCenter() + diff);
}

void KisTransformUtils::setDefaultWarpPoints(int pointsPerLine,
                                             const TransformTransactionProperties *transaction,
                                             ToolTransformArgs *config)
{
    static const int DEFAULT_POINTS_PER_LINE = 3;

    if (pointsPerLine < 0) {
        pointsPerLine = DEFAULT_POINTS_PER_LINE;
    }

    int nbPoints = pointsPerLine * pointsPerLine;
    PkVector<PkPointF> origPoints(nbPoints);
    PkVector<PkPointF> transfPoints(nbPoints);
    qreal gridSpaceX, gridSpaceY;

    if (nbPoints == 1) {
        //there is actually no grid
        origPoints[0] = transaction->originalCenterGeometric();
        transfPoints[0] = transaction->originalCenterGeometric();
    }
    else if (nbPoints > 1) {
        gridSpaceX = transaction->originalRect().width() / (pointsPerLine - 1);
        gridSpaceY = transaction->originalRect().height() / (pointsPerLine - 1);
        double y = transaction->originalRect().top();
        for (int i = 0; i < pointsPerLine; ++i) {
            double x = transaction->originalRect().left();
            for (int j = 0 ; j < pointsPerLine; ++j) {
                origPoints[i * pointsPerLine + j] = PkPointF(x, y);
                transfPoints[i * pointsPerLine + j] = PkPointF(x, y);
                x += gridSpaceX;
            }
            y += gridSpaceY;
        }
    }

    config->setDefaultPoints(nbPoints > 0);
    config->setPoints(origPoints, transfPoints);
}

ToolTransformArgs KisTransformUtils::resetArgsForMode(ToolTransformArgs::TransformMode mode,
                                                      const PkString &filterId,
                                                      const TransformTransactionProperties &transaction,
                                                      KisPaintDeviceSP externalSource)
{
    ToolTransformArgs args;

    args.setOriginalCenter(transaction.originalCenterGeometric());
    args.setTransformedCenter(transaction.originalCenterGeometric());
    args.setFilterId(filterId);
    args.setExternalSource(externalSource);

    if (mode == ToolTransformArgs::FREE_TRANSFORM) {
        args.setMode(ToolTransformArgs::FREE_TRANSFORM);
    } else if (mode == ToolTransformArgs::WARP) {
        args.setMode(ToolTransformArgs::WARP);
        KisTransformUtils::setDefaultWarpPoints(-1, &transaction, &args);
        args.setEditingTransformPoints(false);
    } else if (mode == ToolTransformArgs::CAGE) {
        args.setMode(ToolTransformArgs::CAGE);
        args.setEditingTransformPoints(true);
    } else if (mode == ToolTransformArgs::LIQUIFY) {
        args.setMode(ToolTransformArgs::LIQUIFY);
        const PkRect srcRect = transaction.originalRect().toAlignedRect();
        if (!srcRect.isEmpty()) {
            args.initLiquifyTransformMode(srcRect);
        }
    } else if (mode == ToolTransformArgs::MESH) {
        args.setMode(ToolTransformArgs::MESH);
        const PkRect srcRect = transaction.originalRect().toAlignedRect();
        if (!srcRect.isEmpty()) {
            *args.meshTransform() = KisBezierTransformMesh(PkRectF(srcRect));
        }
    } else if (mode == ToolTransformArgs::PERSPECTIVE_4POINT) {
        args.setMode(ToolTransformArgs::PERSPECTIVE_4POINT);
    }

    return args;
}

bool KisTransformUtils::shouldRestartStrokeOnModeChange(ToolTransformArgs::TransformMode oldMode, ToolTransformArgs::TransformMode newMode, KisNodeList processedNodes)
{
    bool hasExternalLayers = false;
    for (KisNodeSP node : processedNodes) {
        if (node->inherits("KisShapeLayer")) {
            hasExternalLayers = true;
            break;
        }
    }

    bool result = false;

    if (hasExternalLayers) {
        result =
            (oldMode == ToolTransformArgs::FREE_TRANSFORM) !=
            (newMode == ToolTransformArgs::FREE_TRANSFORM);
    }

    return result;
}

void KisTransformUtils::transformAndMergeDevice(const ToolTransformArgs &config,
                                                KisPaintDeviceSP src,
                                                KisPaintDeviceSP dst,
                                                KisProcessingVisitor::ProgressHelper *helper)
{
    KoUpdaterPtr mergeUpdater = helper->updater();

    KisPaintDeviceSP tmp = new KisPaintDevice(src->colorSpace());
    tmp->prepareClone(src);

    KisTransformUtils::transformDevice(config, src, tmp, helper);

    PkRect mergeRect = tmp->extent();
    KisPainter painter(dst);
    painter.setProgress(mergeUpdater);
    painter.bitBlt(mergeRect.topLeft(), tmp, mergeRect);
    painter.end();
}

struct TransformExtraData : public KUndo2CommandExtraData
{
    ToolTransformArgs savedTransformArgs;
    KisNodeList rootNodes;
    KisNodeList transformedNodes;
    int transformedTime = -1;

    KUndo2CommandExtraData* clone() const override {
        return new TransformExtraData(*this);
    }
};

void KisTransformUtils::postProcessToplevelCommand(KUndo2Command *command, const ToolTransformArgs &args, KisNodeList rootNodes, KisNodeList processedNodes, int currentTime, const KisSavedMacroCommand *overriddenCommand)
{
    TransformExtraData *data = new TransformExtraData();
    data->savedTransformArgs = args;
    data->rootNodes = rootNodes;
    data->transformedNodes = processedNodes;
    data->transformedTime = currentTime;

    command->setExtraData(data);

    KisSavedMacroCommand *macroCommand = dynamic_cast<KisSavedMacroCommand*>(command);
    KIS_SAFE_ASSERT_RECOVER_NOOP(macroCommand);

    if (overriddenCommand && macroCommand) {
        macroCommand->setOverrideInfo(overriddenCommand, {});
    }
}

bool KisTransformUtils::fetchArgsFromCommand(const KUndo2Command *command, ToolTransformArgs *args, KisNodeList *rootNodes, KisNodeList *transformedNodes, int *oldTime)
{
    const TransformExtraData *data = dynamic_cast<const TransformExtraData*>(command->extraData());

    if (data) {
        *args = data->savedTransformArgs;
        *rootNodes = data->rootNodes;
        *transformedNodes = data->transformedNodes;
        *oldTime = data->transformedTime;
    }

    return bool(data);
}

KisNodeSP KisTransformUtils::tryOverrideRootToTransformMask(KisNodeSP root)
{
    // we search for masks only at the first level of hierarchy,
    // all other masks are just ignored.

    KisNodeSP node = root->firstChild();

    while (node) {
        if (node->inherits("KisTransformMask") && node->isEditable()) {
            root = node;
            break;
        }

        node = node->nextSibling();
    }

    return root;
}

int KisTransformUtils::fetchCurrentImageTime(KisNodeList rootNodes)
{
    for (KisNodeSP node : rootNodes) {
        /**
         * We cannot just use projection's default bounds, because masks don't have
         * any projection
         */
        if (node && node->image()) {
            return node->image()->animationInterface()->currentTime();
        }
    }
    return -1;
}

PkList<KisNodeSP> KisTransformUtils::fetchNodesList(ToolTransformArgs::TransformMode mode, KisNodeList rootNodes, bool isExternalSourcePresent, KisSelectionSP selection)
{
    PkList<KisNodeSP> result;

    for (KisNodeSP root : rootNodes) {
        bool hasTransformMaskDescendant =
            KisLayerUtils::recursiveFindNode(root, [root] (KisNodeSP node) {
                return node != root && node->visible() && node->inherits("KisTransformMask");
            });

        /// Cannot transform nodes with visible transform masks inside,
        /// this situation should have been caught either in
        /// tryOverrideRootToTransformMask or in the transform tool
        /// stroke initialization routine.
        KIS_SAFE_ASSERT_RECOVER_NOOP(!hasTransformMaskDescendant);

        KisNodeSP selectionNode = selection ? selection->parentNode() : nullptr;

        auto fetchFunc =
            [&result, mode, root, selectionNode] (KisNodeSP node) {
            if (node->isEditable(node == root) &&
                    (!node->inherits("KisShapeLayer") || mode == ToolTransformArgs::FREE_TRANSFORM) &&
                    !node->inherits("KisFileLayer") &&
                    !node->inherits("KisColorizeMask") &&
                    (!node->inherits("KisTransformMask") || node == root) &&
                    (!selectionNode || node != selectionNode)) {

                    result << node;
                }
        };

        if (isExternalSourcePresent) {
            fetchFunc(root);
        } else {
            KisLayerUtils::recursiveApplyNodes(root, fetchFunc);
        }
    }

    result = KisLayerUtils::sortMergeableInternalNodes(result);
    return result;
}

bool KisTransformUtils::tryInitArgsFromNode(KisNodeList rootNodes, ToolTransformArgs *args)
{
    bool result = false;

    for (KisNodeSP node : rootNodes) {
        if (KisTransformMaskSP mask =
            dynamic_cast<KisTransformMask*>(node.data())) {

            KisTransformMaskParamsInterfaceSP savedParams =
                mask->transformParams();

            KisTransformMaskAdapter *adapter =
                dynamic_cast<KisTransformMaskAdapter*>(savedParams.data());

            if (adapter && adapter->isInitialized()) {
                *args = *adapter->transformArgs();
                result = true;
            }
        }
    }

    return result;
}

bool KisTransformUtils::tryFetchArgsFromCommandAndUndo(ToolTransformArgs *outArgs,
                                                       ToolTransformArgs::TransformMode mode,
                                                       KisNodeList currentNodes,
                                                       KisNodeList selectedNodes,
                                                       KisStrokeUndoFacade *undoFacade,
                                                       int currentTime,
                                                       PkVector<KisStrokeJobData *> *undoJobs,
                                                       const KisSavedMacroCommand **overriddenCommand)
{
    bool result = false;

    const KUndo2Command *lastCommand = undoFacade->lastExecutedCommand();
    KisNodeList oldRootNodes;
    KisNodeList oldTransformedNodes;
    int oldTime = -1;

    ToolTransformArgs args;

    if (lastCommand &&
        KisTransformUtils::fetchArgsFromCommand(lastCommand, &args, &oldRootNodes, &oldTransformedNodes, &oldTime) &&
        args.mode() == mode &&
        oldRootNodes == currentNodes &&
        oldTime == currentTime) {

        if (KritaUtils::compareListsUnordered(oldTransformedNodes, selectedNodes)) {
            args.saveContinuedState();

            *outArgs = args;

            const KisSavedMacroCommand *command = dynamic_cast<const KisSavedMacroCommand*>(lastCommand);
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(command, false);

            // the jobs are fetched as !shouldGoToHistory,
            // so there is no need to put them into
            // m_s->skippedWhileMergeCommands
            command->getCommandExecutionJobs(undoJobs, true, false);
            *overriddenCommand = command;

            result = true;
        }
    }

    return result;
}
