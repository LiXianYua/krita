/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006-2008 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006-2010 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2009-2010 Jan Hambrecht <jaham@gmx.net>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoShapeManager.h"
#include "KoShapeManager_p.h"
#include "KoSelection.h"
#include "KoToolManager.h"
#include "KoPointerEvent.h"
#include "KoShape.h"
#include "KoShape_p.h"
#include "KoCanvasBase.h"
#include "KoShapeContainer.h"
#include "KoShapeStrokeModel.h"
#include "KoShapeGroup.h"
#include "KoToolProxy.h"
#include "KoShapeLayer.h"
#include "KoShapeBackground.h"
#include <KoRTree.h>
#include "KoClipPath.h"
#include "KoClipMaskPainter.h"
#include "KoViewConverter.h"
#include "KisQPainterStateSaver.h"
#include "KoSvgTextShape.h"
#include <QApplication>

#include <QPainter>
#include <PkPainterPath.h>
#include <PkThread.h>
#include <PkMutex.h>
#include <FlakeDebug.h>

#include "kis_painting_tweaks.h"
#include "kis_debug.h"
#include "KisForest.h"
#include <unordered_set>


namespace {

/**
 * Returns whether the shape should be added to the RTree for collision and ROI
 * detection.
 */
inline bool shapeUsedInRenderingTree(KoShape *shape)
{
    // FIXME: make more general!

    return !dynamic_cast<KoShapeGroup*>(shape) &&
            !dynamic_cast<KoShapeLayer*>(shape);
}

/**
 * Returns whether a shape should be added to the rendering tree because of
 * its clip mask/path or effects.
 */
inline bool shapeHasGroupEffects(KoShape *shape) {
    return shape->clipPath() ||
        shape->clipMask();
}

/**
 * Returns true if the shape is not fully transparent
 */
inline bool shapeIsVisible(KoShape *shape) {
    return shape->isVisible(false) && shape->transparency() < 1.0;
}


/**
 * Populate \p tree with the subtree of shapes pointed by a shape \p parentShape.
 * All new shapes are added as children of \p parentIt. Please take it into account
 * that \c *parentIt might be not the same as \c parentShape, because \c parentShape
 * may be hidden from rendering.
 */
void populateRenderSubtree(KoShape *parentShape,
                           KisForest<KoShape*>::child_iterator parentIt,
                           KisForest<KoShape*> &tree,
                           std::function<bool(KoShape*)> shouldIncludeNode,
                           std::function<bool(KoShape*)> shouldEnterSubtree)
{
    KoShapeContainer *parentContainer = dynamic_cast<KoShapeContainer*>(parentShape);
    if (!parentContainer) return;

    PkList<KoShape*> children = parentContainer->shapes();
    std::sort(children.begin(), children.end(), KoShape::compareShapeZIndex);

    for (auto it = children.constBegin(); it != children.constEnd(); ++it) {
        auto newParentIt = parentIt;

        if (shouldIncludeNode(*it)) {
            newParentIt = tree.insert(childEnd(parentIt), *it);
        }

        if (shouldEnterSubtree(*it)) {
            populateRenderSubtree(*it, newParentIt, tree, shouldIncludeNode, shouldEnterSubtree);
        }
    }

}

/**
 * Build a rendering tree for **leaf** nodes defined by \p leafNodes
 *
 * Sometimes we should render only a part of the layer (e.g. when we render
 * in patches). So we shouldn't render the whole graph. The problem is that
 * some of the shapes may have parents with clip paths/masks and/or effects.
 * In such a case, these parents should be also included into the rendering
 * process.
 *
 * \c buildRenderTree() builds a graph for such rendering. It includes the
 * leaf shapes themselves, and all parent shapes that have some effects affecting
 * these shapes.
 */
void buildRenderTree(PkList<KoShape*> leafShapes,
                     KisForest<KoShape*> &tree)
{
    PkList<KoShape*> sortedShapes = leafShapes;
    std::sort(sortedShapes.begin(), sortedShapes.end(), KoShape::compareShapeZIndex);

    std::unordered_set<KoShape*> includedShapes;

    Q_FOREACH (KoShape *shape, sortedShapes) {
        bool shouldSkipShape = !shapeIsVisible(shape);
        if (shouldSkipShape) continue;

        bool shapeIsPartOfIncludedSubtree = false;
        PkVector<KoShape*> hierarchy = {shape};

        while ((shape = shape->parent())) {
            if (!shapeIsVisible(shape)) {
                shouldSkipShape = true;
                break;
            }

            if (includedShapes.find(shape) != end(includedShapes)) {
                shapeIsPartOfIncludedSubtree = true;
                break;
            }

            if (shapeHasGroupEffects(shape)) {
                hierarchy << shape;
            }
        }

        if (shouldSkipShape) continue;

        if (!shapeIsPartOfIncludedSubtree &&
            includedShapes.find(hierarchy.last()) == end(includedShapes)) {

            tree.insert(childEnd(tree), hierarchy.last());
        }
        std::copy(hierarchy.begin(), hierarchy.end(),
                  std::inserter(includedShapes, end(includedShapes)));
    }

    auto shouldIncludeShape =
        [includedShapes] (KoShape *shape) {
            // included shapes are guaranteed to be visible
            return includedShapes.find(shape) != end(includedShapes);
        };

    for (auto it = childBegin(tree); it != childEnd(tree); ++it) {
        populateRenderSubtree(*it, it, tree, shouldIncludeShape, &shapeIsVisible);
    }
}

/**
 * Render the prebuilt rendering tree on \p painter
 */
void renderShapes(typename KisForest<KoShape*>::child_iterator beginIt,
                  typename KisForest<KoShape*>::child_iterator endIt,
                  QPainter &painter)
{
    for (auto it = beginIt; it != endIt; ++it) {
        KoShape *shape = *it;

        KisQPainterStateSaver saver(&painter);

        if (!isEnd(parent(it))) {
            painter.setTransform(toQTransform(shape->transformation()) * painter.transform());
        } else {
            painter.setTransform(toQTransform(shape->absoluteTransformation()) * painter.transform());
        }

        KoClipPath::applyClipping(shape, painter);

        qreal transparency = shape->transparency(true);
        if (transparency > 0.0) {
            painter.setOpacity(1.0-transparency);
        }

        PkScopedPointer<KoClipMaskPainter> clipMaskPainter;
        QPainter *shapePainter = &painter;

        KoClipMask *clipMask = shape->clipMask();
        if (clipMask) {
            /**
             * We should clip on both, the shape and the global clipping rect.
             * Otherwise filling huge shapes will go into almost infinite loop.
             */
            const PkRectF bounds = toPkRectF(painter.transform().mapRect(toQRectF(shape->outlineRect())) & painter.clipBoundingRect());

            clipMaskPainter.reset(new KoClipMaskPainter(&painter, bounds));
            shapePainter = clipMaskPainter->shapePainter();
        }

        /**
         * We expect the shape to save/restore the painter's state itself. Such design was not
         * not always here, so we need a period of sanity checks to ensure all the shapes are
         * ported correctly.
         */
        const PkTransform sanityCheckTransformSaved = toPkTransform(shapePainter->transform());

        renderShapes(childBegin(it), childEnd(it), *shapePainter);

        Q_FOREACH(const KoShape::PaintOrder p, shape->paintOrder()) {
            if (p == KoShape::Fill) {
                shape->paint(*shapePainter);
            } else if (p == KoShape::Stroke) {
                shape->paintStroke(*shapePainter);
            } else if (p == KoShape::Markers)  {
                shape->paintMarkers(*shapePainter);
            }
        }

        KIS_SAFE_ASSERT_RECOVER(shapePainter->transform() == toQTransform(sanityCheckTransformSaved)) {
            shapePainter->setTransform(toQTransform(sanityCheckTransformSaved));
        }

        if (clipMask) {
            clipMaskPainter->maskPainter()->save();

            shape->clipMask()->drawMask(clipMaskPainter->maskPainter(), shape);
            clipMaskPainter->renderOnGlobalPainter();

            clipMaskPainter->maskPainter()->restore();
        }
    }
}

}

void KoShapeManager::Private::updateTree()
{
    bool selectionModified = false;
    bool anyModified = false;

    {
        PkMutexLocker l(&this->treeMutex);

        Q_FOREACH (KoShape *shape, aggregate4update) {
            selectionModified = selectionModified || selection->isSelected(shape);
            anyModified = true;
        }

        foreach (KoShape *shape, aggregate4update) {
            if (!shapeUsedInRenderingTree(shape)) continue;

            tree.remove(shape);
            PkRectF br(shape->boundingRect());
            tree.insert(br, shape);
        }

        aggregate4update.clear();
    }

    if (selectionModified) {
        Q_EMIT q->selectionContentChanged();
    }
    if (anyModified) {
        Q_EMIT q->contentChanged();
    }
}

void KoShapeManager::Private::forwardCompressedUpdate()
{
    bool shouldUpdateDecorations = false;
    PkRectF scheduledUpdate;

    {
        PkMutexLocker l(&shapesMutex);

        if (!compressedUpdate.isEmpty()) {
            scheduledUpdate = compressedUpdate;
            compressedUpdate = PkRect();
        }

        Q_FOREACH (const KoShape *shape, compressedUpdatedShapes) {
            if (selection->isSelected(shape)) {
                shouldUpdateDecorations = true;
                break;
            }
        }
        compressedUpdatedShapes.clear();
    }

    if (shouldUpdateDecorations && canvas->toolProxy()) {
        canvas->toolProxy()->repaintDecorations();
    }
    canvas->updateCanvas(scheduledUpdate);

}

KoShapeManager::KoShapeManager(KoCanvasBase *canvas, const PkList<KoShape *> &shapes)
    : d(new Private(this, canvas))
{
    Q_ASSERT(d->canvas); // not optional.
    connect(d->selection, &KoSelection::selectionChanged, this, &KoShapeManager::selectionChanged);
    setShapes(shapes);

    /**
     * Shape manager uses uses queued signals, therefore it should belong
     * to the GUI thread.
     */
    this->moveToThread(qApp->thread());
    connect(this, &KoShapeManager::forwardUpdate, this, [this]() { d->forwardCompressedUpdate(); });
}

KoShapeManager::KoShapeManager(KoCanvasBase *canvas)
    : d(new Private(this, canvas))
{
    Q_ASSERT(d->canvas); // not optional.
    connect(d->selection, &KoSelection::selectionChanged, this, &KoShapeManager::selectionChanged);

    // see a comment in another constructor
    this->moveToThread(qApp->thread());
    connect(this, &KoShapeManager::forwardUpdate, this, [this]() { d->forwardCompressedUpdate(); });
}

void KoShapeManager::Private::unlinkFromShapesRecursively(const PkList<KoShape*> &shapes)
{
    Q_FOREACH (KoShape *shape, shapes) {
        shape->removeShapeManager(q);

        KoShapeContainer *container = dynamic_cast<KoShapeContainer*>(shape);
        if (container) {
            unlinkFromShapesRecursively(container->shapes());
        }
    }
}

KoShapeManager::~KoShapeManager()
{
    d->unlinkFromShapesRecursively(d->shapes);
    d->shapes.clear();

    delete d;
}

void KoShapeManager::setShapes(const PkList<KoShape *> &shapes, Repaint repaint)
{
    {
        PkMutexLocker l1(&d->shapesMutex);
        PkMutexLocker l2(&d->treeMutex);

        //clear selection
        d->selection->deselectAll();
        d->unlinkFromShapesRecursively(d->shapes);
        d->compressedUpdate = PkRect();
        d->compressedUpdatedShapes.clear();
        d->aggregate4update.clear();
        d->tree.clear();
        d->shapes.clear();
    }

    Q_FOREACH (KoShape *shape, shapes) {
        addShape(shape, repaint);
    }
}

void KoShapeManager::addShape(KoShape *shape, Repaint repaint)
{
    {
        PkMutexLocker l1(&d->shapesMutex);

        if (d->shapes.contains(shape))
            return;
        shape->addShapeManager(this);
        d->shapes.append(shape);

        if (shapeUsedInRenderingTree(shape)) {
            PkMutexLocker l2(&d->treeMutex);

            PkRectF br(shape->boundingRect());
            d->tree.insert(br, shape);
        }
    }

    if (repaint == PaintShapeOnAdd) {
        shape->update();
    }

    // add the children of a KoShapeContainer
    KoShapeContainer *container = dynamic_cast<KoShapeContainer*>(shape);

    if (container) {
        foreach (KoShape *containerShape, container->shapes()) {
            addShape(containerShape, repaint);
        }
    }
}

void KoShapeManager::remove(KoShape *shape)
{
    PkRectF dirtyRect;
    {
        PkMutexLocker l1(&d->shapesMutex);
        PkMutexLocker l2(&d->treeMutex);

        dirtyRect = shape->boundingRect();

        shape->removeShapeManager(this);
        d->selection->deselect(shape);
        d->aggregate4update.remove(shape);
        d->compressedUpdatedShapes.remove(shape);

        if (shapeUsedInRenderingTree(shape)) {
            d->tree.remove(shape);
        }
        d->shapes.removeAll(shape);
    }

    if (!dirtyRect.isEmpty()) {
        d->canvas->updateCanvas(dirtyRect);
    }

    // remove the children of a KoShapeContainer
    KoShapeContainer *container = dynamic_cast<KoShapeContainer*>(shape);
    if (container) {
        foreach (KoShape *containerShape, container->shapes()) {
            remove(containerShape);
        }
    }
}

KoShapeManager::ShapeInterface::ShapeInterface(KoShapeManager *_q)
    : q(_q)
{
}

void KoShapeManager::ShapeInterface::notifyShapeDestructed(KoShape *shape)
{
    PkMutexLocker l1(&q->d->shapesMutex);
    PkMutexLocker l2(&q->d->treeMutex);

    q->d->selection->deselect(shape);
    q->d->aggregate4update.remove(shape);
    q->d->compressedUpdatedShapes.remove(shape);

    // we cannot access RTTI of the semi-destructed shape, so just
    // unlink it lazily
    if (q->d->tree.contains(shape)) {
        q->d->tree.remove(shape);
    }

    q->d->shapes.removeAll(shape);
}


KoShapeManager::ShapeInterface *KoShapeManager::shapeInterface()
{
    return &d->shapeInterface;
}

void KoShapeManager::preparePaintJobs(PaintJobsOrder &jobsOrder,
                                      KoShape *excludeRoot)
{
    d->updateTree();

    PkMutexLocker l1(&d->shapesMutex);

    PkSet<KoShape*> rootShapesSet;
    Q_FOREACH (KoShape *shape, d->shapes) {
        while (shape->parent() && shape->parent() != excludeRoot) {
            shape = shape->parent();
        }

        if (!rootShapesSet.contains(shape) && shape != excludeRoot) {
            rootShapesSet.insert(shape);
        }
    }
    PkList<KoShape*> rootShapes;
        for (KoShape *rs : rootShapesSet) rootShapes.append(rs);
    PkList<KoShape*> newRootShapes;

    Q_FOREACH (KoShape *srcShape, rootShapes) {
        KIS_SAFE_ASSERT_RECOVER(srcShape->parent() == excludeRoot
                                || !srcShape->parent()) {
            continue;
        }

        KoShape *clonedShape = srcShape->cloneShape();

        KoShapeContainer *parentShape = srcShape->parent();

        if (parentShape && !parentShape->transformation().isIdentity()) {
            clonedShape->applyAbsoluteTransformation(parentShape->transformation());
        }

        newRootShapes << clonedShape;
    }

    PaintJobsOrder result;

    PaintJob::SharedSafeStorage shapesStorage = std::make_shared<PaintJob::ShapesStorage>();
    Q_FOREACH (KoShape *shape, newRootShapes) {
        shapesStorage->emplace_back(std::unique_ptr<KoShape>(shape));
    }

    const PkList<KoShape*> originalShapes = KoShape::linearizeSubtreeSorted(rootShapes);
    const PkList<KoShape*> clonedShapes = KoShape::linearizeSubtreeSorted(newRootShapes);
    KIS_SAFE_ASSERT_RECOVER_RETURN(clonedShapes.size() == originalShapes.size());

    PkHash<KoShape*, KoShape*> clonedFromOriginal;
    for (int i = 0; i < originalShapes.size(); i++) {
        clonedFromOriginal[originalShapes[i]] = clonedShapes[i];
    }


    for (auto it = std::begin(jobsOrder.jobs); it != std::end(jobsOrder.jobs); ++it) {
        PkMutexLocker l(&d->treeMutex);
        PkList<KoShape*> unsortedOriginalShapes = d->tree.intersects(it->docUpdateRect);

        it->allClonedShapes = shapesStorage;

        Q_FOREACH (KoShape *shape, unsortedOriginalShapes) {
            KIS_SAFE_ASSERT_RECOVER(shapeUsedInRenderingTree(shape)) { continue; }
            it->shapes << clonedFromOriginal[shape];
        }
    }
}

void KoShapeManager::paintJob(QPainter &painter, const KoShapeManager::PaintJob &job)
{
    painter.setPen(Qt::NoPen);  // painters by default have a black stroke, lets turn that off.
    painter.setBrush(Qt::NoBrush);

    KisForest<KoShape*> renderTree;
    buildRenderTree(job.shapes, renderTree);

    renderShapes(childBegin(renderTree), childEnd(renderTree), painter);
}

void KoShapeManager::paint(QPainter &painter)
{
    d->updateTree();

    PkMutexLocker l1(&d->shapesMutex);

    painter.setPen(Qt::NoPen);  // painters by default have a black stroke, lets turn that off.
    painter.setBrush(Qt::NoBrush);

    PkList<KoShape*> unsortedShapes;
    if (painter.hasClipping()) {
        PkMutexLocker l(&d->treeMutex);

        PkRectF rect = KisPaintingTweaks::safeClipBoundingRect(painter);
        unsortedShapes = d->tree.intersects(rect);
    } else {
        unsortedShapes = d->shapes;
        warnFlake << "KoShapeManager::paint  Painting with a painter that has no clipping will lead to too much being painted!";
    }

    KisForest<KoShape*> renderTree;
    buildRenderTree(unsortedShapes, renderTree);
    renderShapes(childBegin(renderTree), childEnd(renderTree), painter);
}

void KoShapeManager::renderSingleShape(KoShape *shape, QPainter &painter)
{
    KisForest<KoShape*> renderTree;

    KoViewConverter converter;

    auto root = renderTree.insert(childBegin(renderTree), shape);
    populateRenderSubtree(shape, root, renderTree, &shapeIsVisible, &shapeIsVisible);
    renderShapes(childBegin(renderTree), childEnd(renderTree), painter);
}

KoShape *KoShapeManager::shapeAt(const PkPointF &position, KoFlake::ShapeSelection selection, bool omitHiddenShapes)
{
    d->updateTree();

    PkMutexLocker l(&d->shapesMutex);

    PkList<KoShape*> sortedShapes;

    {
        PkMutexLocker l(&d->treeMutex);
        sortedShapes = d->tree.contains(position);
    }

    std::sort(sortedShapes.begin(), sortedShapes.end(), KoShape::compareShapeZIndex);
    KoShape *firstUnselectedShape = 0;
    for (int count = sortedShapes.count() - 1; count >= 0; count--) {
        KoShape *shape = sortedShapes.at(count);
        if (omitHiddenShapes && ! shape->isVisible())
            continue;
        if (! shape->hitTest(position))
            continue;

        switch (selection) {
        case KoFlake::ShapeOnTop:
            if (shape->isSelectable())
                return shape;
            break;
        case KoFlake::Selected:
            if (d->selection->isSelected(shape))
                return shape;
            break;
        case KoFlake::Unselected:
            if (! d->selection->isSelected(shape))
                return shape;
            break;
        case KoFlake::NextUnselected:
            // we want an unselected shape
            if (d->selection->isSelected(shape))
                continue;
            // memorize the first unselected shape
            if (! firstUnselectedShape)
                firstUnselectedShape = shape;
            // check if the shape above is selected
            if (count + 1 < sortedShapes.count() && d->selection->isSelected(sortedShapes.at(count + 1)))
                return shape;
            break;
        }
    }
    // if we want the next unselected below a selected but there was none selected,
    // return the first found unselected shape
    if (selection == KoFlake::NextUnselected && firstUnselectedShape)
        return firstUnselectedShape;

    if (d->selection->hitTest(position))
        return d->selection;

    return 0; // missed everything
}

PkList<KoShape *> KoShapeManager::shapesAt(const PkRectF &rect, bool omitHiddenShapes, bool containedMode)
{
    d->updateTree();
    PkList<KoShape*> shapes;

    {
        PkMutexLocker l(&d->treeMutex);
        shapes = containedMode ? d->tree.contained(rect) : d->tree.intersects(rect);
    }

    for (int count = shapes.count() - 1; count >= 0; count--) {

        KoShape *shape = shapes.at(count);

        if (omitHiddenShapes && !shape->isVisible()) {
            shapes.removeAt(count);
        } else {
            const PkPainterPath outline = shape->absoluteTransformation().map(shape->outline());

            if (!containedMode && !outline.intersects(rect) && !outline.contains(rect)) {
                shapes.removeAt(count);

            } else if (containedMode) {

                PkPainterPath containingPath;
                containingPath.addRect(rect);

                if (!containingPath.contains(outline)) {
                    shapes.removeAt(count);
                }
            }
        }
    }

    return shapes;
}

void KoShapeManager::update(const PkRectF &rect, const KoShape *shape, bool selectionHandles)
{
    if (d->updatesBlocked) return;

    {
        PkMutexLocker l(&d->shapesMutex);

        d->compressedUpdate |= rect;

        if (selectionHandles) {
            d->compressedUpdatedShapes.insert(shape);
        }
    }

    emit(forwardUpdate());
}

void KoShapeManager::setUpdatesBlocked(bool value)
{
    d->updatesBlocked = value;
}

bool KoShapeManager::updatesBlocked() const
{
    return d->updatesBlocked;
}
void KoShapeManager::notifyShapeChanged(KoShape *shape)
{
    {
        PkMutexLocker l(&d->treeMutex);

        Q_ASSERT(shape);
        if (d->aggregate4update.contains(shape)) {
            return;
        }

        d->aggregate4update.insert(shape);
    }

    KoShapeContainer *container = dynamic_cast<KoShapeContainer*>(shape);
    if (container) {
        Q_FOREACH (KoShape *child, container->shapes())
            notifyShapeChanged(child);
    }
}

PkList<KoShape*> KoShapeManager::shapes() const
{
    PkMutexLocker l(&d->shapesMutex);

    return d->shapes;
}

PkList<KoShape*> KoShapeManager::topLevelShapes() const
{
    PkMutexLocker l(&d->shapesMutex);

    PkList<KoShape*> shapes;
    // get all toplevel shapes
    Q_FOREACH (KoShape *shape, d->shapes) {
        if (!shape->parent() || dynamic_cast<KoShapeLayer*>(shape->parent())) {
            shapes.append(shape);
        }
    }
    return shapes;
}

KoSelection *KoShapeManager::selection() const
{
    return d->selection;
}

void KoShapeManager::explicitlyIssueShapeChangedSignals()
{
    d->updateTree();
}

KoCanvasBase *KoShapeManager::canvas()
{
    return d->canvas;
}

//have to include this because of Q_PRIVATE_SLOT
