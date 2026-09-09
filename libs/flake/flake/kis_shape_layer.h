/*
 *  SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SHAPE_LAYER_H_
#define KIS_SHAPE_LAYER_H_

#include <KoShapeLayer.h>

#include <kis_types.h>
#include <kis_external_layer_iface.h>
#include <kritashapemodel_export.h>
#include <KisDelayedUpdateNodeInterface.h>
#include <KisCroppedOriginalLayerInterface.h>
#include <PkStream.h>

class PkRect;
class QIcon;
class PkRect;
class PkString;
class KoShapeManager;
class KoStore;
class KoViewConverter;
class KoShapeControllerBase;
class KoDocumentResourceManager;
class KisShapeLayerCanvasBase;
class KoSelectedShapesProxy;

const PkString KIS_SHAPE_LAYER_ID = "KisShapeLayer";
/**
   A KisShapeLayer contains any number of non-krita flakes, such as
   path shapes, text shapes and anything else people come up with.

   The KisShapeLayer has a shapemanager and a canvas of its own. The
   canvas paints onto the projection, and the projection is what we
   render in Krita. This means that no matter how many views you have,
   you cannot have a different view on your shapes per view.

   XXX: what about removing shapes?
*/
class KRITASHAPEMODEL_EXPORT KisShapeLayer
        : public KisExternalLayer,
        public KoShapeLayer,
        public KisDelayedUpdateNodeInterface,
        public KisCroppedOriginalLayerInterface
{
public:

    KisShapeLayer(KoShapeControllerBase* shapeController, KisImageWSP image, const PkString &name, quint8 opacity);
    KisShapeLayer(const KisShapeLayer& _rhs);
    KisShapeLayer(const KisShapeLayer& _rhs, KoShapeControllerBase* controller);
    KisShapeLayer(const KisShapeLayer& _rhs, KoShapeControllerBase* controller, std::function<KisShapeLayerCanvasBase*()> canvasFactory);

    /**
     * Merge constructor
     *
     * Creates a new layer as a merge of two existing layers. The shapes of \p baseTemplate
     * are **not** used. You need to add them into \p newShapes to be used in the final layer.
     *
     * \p newShapes are the shapes used in the new shape layer, they should be z-order sorted
     * homogenized externally before calling this constructor
     */
    KisShapeLayer(const KisShapeLayer& baseTemplate, const PkList<KoShape*> &newShapes);

    ~KisShapeLayer() override;

    KisBaseNode::PropertyList sectionModelProperties() const override;
    void setSectionModelProperties(const KisBaseNode::PropertyList &properties) override;

protected:
    KisShapeLayer(KoShapeControllerBase* shapeController, KisImageWSP image, const PkString &name, quint8 opacity, std::function<KisShapeLayerCanvasBase *()> canvasFactory);
private:
    void initShapeLayerImpl(KoShapeControllerBase* controller, KisShapeLayerCanvasBase *overrideCanvas);
public:
    KisNodeSP clone() const override {
        return new KisShapeLayer(*this);
    }
    bool allowAsChild(KisNodeSP) const override;


    void setImage(KisImageWSP image) override;

    KisLayerSP tryCreateInternallyMergedLayerFromMutipleLayers(PkList<KisLayerSP> layers) override;

    KisLayerSP createMergedLayerTemplate(KisLayerSP prevLayer) override;
    void fillMergedLayerTemplate(KisLayerSP dstLayer, KisLayerSP prevLayer, bool skipPaintingThisLayer) override;
public:

    // KoShape overrides
    bool isSelectable() const {
        return false;
    }

    void setParent(KoShapeContainer *parent);

    // KisExternalLayer implementation
    QIcon icon() const;
    void resetCache(const KoColorSpace *colorSpace) override;

    KisPaintDeviceSP original() const override;
    KisPaintDeviceSP paintDevice() const override;

    PkRect theoreticalBoundingRect() const override;

    qint32 x() const override;
    qint32 y() const override;
    void setX(qint32) override;
    void setY(qint32) override;

    bool accept(KisNodeVisitor&) override;
    void accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter) override;

    KoShapeManager *shapeManager() const;

    static bool saveShapesToStore(KoStore *store, PkList<KoShape*> shapes, const PkSizeF &sizeInPt);

    static PkList<KoShape *> createShapesFromSvg(PkStream *device,
                                                const PkString &baseXmlDir,
                                                const PkRectF &rectInPixels,
                                                qreal resolutionPPI,
                                                KoDocumentResourceManager *resourceManager,
                                                bool loadingFromKra,
                                                PkSizeF *fragmentSize,
                                                PkStringList *warnings = 0,
                                                PkStringList *errors = 0);

    bool saveLayer(KoStore * store) const;
    bool loadLayer(KoStore* store, PkStringList *warnings = 0);

    KUndo2Command* crop(const PkRect & rect) override;
    KUndo2Command* transform(const PkTransform &transform) override;
    KUndo2Command* setProfile(const KoColorProfile *profile) override;
    KUndo2Command* convertTo(const KoColorSpace * dstColorSpace,
                                 KoColorConversionTransformation::Intent renderingIntent = KoColorConversionTransformation::internalRenderingIntent(),
                                 KoColorConversionTransformation::ConversionFlags conversionFlags = KoColorConversionTransformation::internalConversionFlags()) override;


    bool visible(bool recursive = false) const override;
    void setVisible(bool visible, bool isLoading = false) override;

    void setUserLocked(bool value) override;

    bool isShapeEditable(bool recursive) const override;

    /**
     * Forces a repaint of a shape layer without waiting for an event loop
     * calling a delayed timer update. If you want to see the result of the shape
     * layer right here and right now, you should do:
     *
     * shapeLayer->setDirty();
     * shapeLayer->image()->waitForDone();
     * shapeLayer->forceUpdateTimedNode();
     * shapeLayer->image()->waitForDone();
     *
     */
    void forceUpdateTimedNode() override;

    /**
     * \return true if there are any pending updates in the delayed queue
     */
    bool hasPendingTimedUpdates() const override;

    void forceUpdateHiddenAreaOnOriginal() override;

    /**
     * @brief selectedShapesProxy
     * @return returns the selectedShapesProxy of the KoCanvasBase of this layer,
     * used for certain undo commands.
     */
    KoSelectedShapesProxy* selectedShapesProxy();

    bool antialiased() const;
    void setAntialiased(const bool antialiased);

protected:
    using KoShape::isVisible;

    bool loadSvg(PkStream *device, const PkString &baseXmlDir, PkStringList *warnings = 0);


    friend class ShapeLayerContainerModel;
    const KoViewConverter *converter() const;

    KoShapeControllerBase *shapeController() const;
    KisShapeLayerCanvasBase *canvas() const;

    friend class TransformShapeLayerDeferred;

public:
    /**
     * These signals are forwarded from the local shape manager
     * This is done because we switch KoShapeManager and therefore
     * KoSelection in KisCanvas2, so we need to connect local managers
     * to the UI as well.
     *
     * \see comment in the constructor of KisCanvas2
     */
    void selectionChanged();
    void currentLayerChanged(const KoShapeLayer *layer);

public:
    /**
     * A signal + slot to synchronize UI and image
     * threads. Image thread emits the signal, UI
     * thread performs the action
     */
    void sigMoveShapes(const PkPointF &diff);

private:
    void slotMoveShapes(const PkPointF &diff);
    void slotTransformShapes(const PkTransform &transform);
    void slotImageResolutionChanged();

private:
    PkList<KoShape*> shapesToBeTransformed();

private:
    struct Private;
    Private * const m_d;
};

#endif
