/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_SHAPE_CONTROLLER
#define KIS_SHAPE_CONTROLLER

#include "kis_dummies_facade_base.h"
#include "kritashapemodel_export.h"
#include <KoShapeControllerBase.h>


class KisNodeDummy;
class KoShapeLayer;

class KisNameServer;
class KUndo2Stack;

/**
 * KisShapeController keeps track of new layers, shapes, masks and
 * selections -- everything that needs to be wrapped as a shape for
 * the tools to work on.
 */
class KRITASHAPEMODEL_EXPORT KisShapeController : public KisDummiesFacadeBase, public KoShapeControllerBase
{

    Q_OBJECT

public:

    KisShapeController(KisNameServer *nameServer, KUndo2Stack *undoStack, QObject *parent = 0);
    ~KisShapeController() override;

    bool hasDummyForNode(KisNodeSP node) const override;
    KisNodeDummy* dummyForNode(KisNodeSP layer) const override;

    /**
     * Return the root dummy of the graph. Since the call to
     * KisDummiesFacadeBase::setImage() causes and **asynchronous** update of
     * the dummies graph, it may cause rootDummy() to be null at some moments,
     * which is a valid state.
     */
    KisNodeDummy* rootDummy() const override;
    int dummiesCount() const override;

    KoShapeLayer* shapeForNode(KisNodeSP layer) const;
    KisImageSP currentImage() const;

    using KisDummiesFacadeBase::setImage;
    void setImage(KisImageWSP image, KisNodeSP activeNode) override;


private:
    void addNodeImpl(KisNodeSP node, KisNodeSP parent, KisNodeSP aboveThis) override;
    void removeNodeImpl(KisNodeSP node) override;

private Q_SLOTS:
    void slotUpdateDocumentResolution();
    void slotUpdateDocumentSize();

Q_SIGNALS:
    /**
     * These three signals are forwarded from each KisShapeLayer's local
     * shape manager. Consumers may switch between managers, so they need a
     * stable controller-level notification source.
     */
    void selectionChanged();
    void selectionContentChanged();
    void currentLayerChanged(const KoShapeLayer*);

public:
    KoShapeContainer *createParentForShapes(const QList<KoShape *> shapes, bool forceNewLayer, KUndo2Command *parentCommand) override;

    QRectF documentRectInPixels() const override;
    qreal pixelsPerInch() const override;

private:
    struct Private;
    Private * const m_d;
};

#endif
