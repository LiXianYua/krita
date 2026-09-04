/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2006 C. Boemann Rasmussen <cbo@boemann.dk>
   SPDX-FileCopyrightText: 2006-2010 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2006-2010 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2007-2009, 2011 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <limits>

#include "KisMpl.h"
#include "KoShape.h"
#include "KoShape_p.h"
#include "KoShapeContainer.h"
#include "KoShapeLayer.h"
#include "KoShapeContainerModel.h"
#include "KoSelection.h"
#include "KoPointerEvent.h"
#include "KoInsets.h"
#include "KoShapeStrokeModel.h"
#include "KoShapeBackground.h"
#include "KoColorBackground.h"
#include "KoHatchBackground.h"
#include "KoGradientBackground.h"
#include "KoPatternBackground.h"
#include "KoShapeManager.h"
#include "KoShapeUserData.h"
#include "KoShapeSavingContext.h"
#include "KoShapeLoadingContext.h"
#include "KoViewConverter.h"
#include "KoShapeStroke.h"
#include "KoClipPath.h"
#include "KoPathShape.h"
#include <KoSnapData.h>

#include <KoXmlWriter.h>
#include <KoXmlNS.h>
#include <KoUnit.h>

#include <QPainter>
#include <PkVariant.h>
#include <PkPainterPath.h>
#include <PkList.h>
#include <PkMap.h>
#include <FlakeDebug.h>

#include "kis_assert.h"

#include <KisHandlePainterHelper.h>

// KoShape::Private

KoShape::SharedData::SharedData()
    : QSharedData()
    , size(50, 50)
    , transparency(0.0)
    , zIndex(0)
    , visible(true)
    , printable(true)
    , geometryProtected(false)
    , keepAspect(false)
    , selectable(true)
    , protectContent(false)
    , paintOrder(defaultPaintOrder())
    , inheritPaintOrder(true)
{ }

KoShape::SharedData::SharedData(const SharedData &rhs)
    : QSharedData()
    , size(rhs.size)
    , shapeId(rhs.shapeId)
    , name(rhs.name)
    , localMatrix(rhs.localMatrix)
    , userData(rhs.userData ? rhs.userData->clone() : 0)
    , stroke(rhs.stroke)
    , fill(rhs.fill)
    , inheritBackground(rhs.inheritBackground)
    , inheritStroke(rhs.inheritStroke)
    , clipPath(rhs.clipPath ? rhs.clipPath->clone() : 0)
    , clipMask(rhs.clipMask ? rhs.clipMask->clone() : 0)
    , additionalAttributes(rhs.additionalAttributes)
    , additionalStyleAttributes(rhs.additionalStyleAttributes)
    , transparency(rhs.transparency)
    , hyperLink(rhs.hyperLink)

    , zIndex(rhs.zIndex)
    , visible(rhs.visible)
    , printable(rhs.visible)
    , geometryProtected(rhs.geometryProtected)
    , keepAspect(rhs.keepAspect)
    , selectable(rhs.selectable)
    , protectContent(rhs.protectContent)

    , paintOrder(rhs.paintOrder)
    , inheritPaintOrder(rhs.inheritPaintOrder)
{
}

KoShape::SharedData::~SharedData()
{
}

void KoShape::shapeChangedPriv(KoShape::ChangeType type)
{
    if (d->parent)
        d->parent->model()->childChanged(this, type);

    this->shapeChanged(type);

    Q_FOREACH (KoShape * shape, d->dependees) {
        shape->shapeChanged(type, this);
    }

    Q_FOREACH (KoShape::ShapeChangeListener *listener, d->listeners) {
        listener->notifyShapeChangedImpl(type, this);
    }
}

void KoShape::addShapeManager(KoShapeManager *manager)
{
    d->shapeManagers.insert(manager);
}

void KoShape::removeShapeManager(KoShapeManager *manager)
{
    d->shapeManagers.remove(manager);
}

// ======== KoShape

const qint16 KoShape::maxZIndex = std::numeric_limits<qint16>::max();
const qint16 KoShape::minZIndex = std::numeric_limits<qint16>::min();

KoShape::KoShape()
    : d(new Private()),
      s(new SharedData)
{
    notifyChanged();
}

KoShape::KoShape(const KoShape &rhs)
    : d(new Private()),
      s(rhs.s)
{
}

KoShape::~KoShape()
{
    shapeChangedPriv(Deleted);
    d->listeners.clear();
    /**
     * The shape must have already been detached from all the parents and
     * shape managers. Otherwise we might accidentally request some RTTI
     * information, which is not available anymore (we are in d-tor).
     *
     * TL;DR: fix the code that caused this destruction without unparenting
     *        instead of trying to remove these assert!
     */
    KIS_SAFE_ASSERT_RECOVER (!d->parent) {
        d->parent->removeShape(this);
    }

    KIS_SAFE_ASSERT_RECOVER (d->shapeManagers.isEmpty()) {
        Q_FOREACH (KoShapeManager *manager, d->shapeManagers) {
            manager->shapeInterface()->notifyShapeDestructed(this);
        }
        d->shapeManagers.clear();
    }
}

KoShape *KoShape::cloneShape() const
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(0 && "not implemented!");
    qWarning() << shapeId() << "cannot be cloned";
    return 0;
}

KoShape *KoShape::cloneShapeAndBakeAbsoluteTransform() const
{
    KoShape *clonedShape = this->cloneShape();

    /**
     * The shape is cloned without its parent's transformation, so we should
     * adjust it manually.
     */
    KoShape *oldParentShape = this->parent();
    if (oldParentShape && !oldParentShape->absoluteTransformation().isIdentity()) {
        clonedShape->applyAbsoluteTransformation(oldParentShape->absoluteTransformation());
    }

    return clonedShape;
}

void KoShape::paintStroke(QPainter &painter) const
{
    if (stroke()) {
        stroke()->paint(this, painter);
    }
}

void KoShape::paintMarkers(QPainter &painter) const
{
    if (stroke()) {
        stroke()->paintMarkers(this, painter);
    }
}

void KoShape::scale(qreal sx, qreal sy)
{
    PkPointF pos = position();
    PkTransform scaleMatrix;
    scaleMatrix.translate(pos.x(), pos.y());
    scaleMatrix.scale(sx, sy);
    scaleMatrix.translate(-pos.x(), -pos.y());
    s->localMatrix = s->localMatrix * scaleMatrix;

    notifyChanged();
    shapeChangedPriv(ScaleChanged);
}

void KoShape::rotate(qreal angle)
{
    PkPointF center = s->localMatrix.map(PkPointF(0.5 * size().width(), 0.5 * size().height()));
    PkTransform rotateMatrix;
    rotateMatrix.translate(center.x(), center.y());
    rotateMatrix.rotate(angle);
    rotateMatrix.translate(-center.x(), -center.y());
    s->localMatrix = s->localMatrix * rotateMatrix;

    notifyChanged();
    shapeChangedPriv(RotationChanged);
}

void KoShape::shear(qreal sx, qreal sy)
{
    PkPointF pos = position();
    PkTransform shearMatrix;
    shearMatrix.translate(pos.x(), pos.y());
    shearMatrix.shear(sx, sy);
    shearMatrix.translate(-pos.x(), -pos.y());
    s->localMatrix = s->localMatrix * shearMatrix;

    notifyChanged();
    shapeChangedPriv(ShearChanged);
}

void KoShape::setSize(const PkSizeF &newSize)
{
    PkSizeF oldSize(size());

    // always set size, as d->size and size() may vary
    setSizeImpl(newSize);

    if (oldSize == newSize)
        return;

    notifyChanged();
    shapeChangedPriv(SizeChanged);
}

void KoShape::setSizeImpl(const PkSizeF &size) const
{
    s->size = size;
}

void KoShape::setPosition(const PkPointF &newPosition)
{
    PkPointF currentPos = position();
    if (newPosition == currentPos)
        return;
    PkTransform translateMatrix;
    translateMatrix.translate(newPosition.x() - currentPos.x(), newPosition.y() - currentPos.y());
    s->localMatrix = s->localMatrix * translateMatrix;

    notifyChanged();
    shapeChangedPriv(PositionChanged);
}

bool KoShape::hitTest(const PkPointF &position) const
{
    if (d->parent && d->parent->isClipped(this) && !d->parent->hitTest(position))
        return false;

    PkPointF point = absoluteTransformation().inverted().map(position);
    PkRectF bb = outlineRect();

    if (s->stroke) {
        KoInsets insets;
        s->stroke->strokeInsets(this, insets);
        bb.adjust(-insets.left, -insets.top, insets.right, insets.bottom);
    }
    if (bb.contains(point))
        return true;

    return false;
}

PkRectF KoShape::boundingRect() const
{
    PkTransform transform = absoluteTransformation();
    PkRectF bb = outlineRect();
    if (s->stroke) {
        KoInsets insets;
        s->stroke->strokeInsets(this, insets);
        bb.adjust(-insets.left, -insets.top, insets.right, insets.bottom);
    }
    bb = transform.mapRect(bb);
    return bb;
}

PkRectF KoShape::boundingRect(const PkList<KoShape *> &shapes)
{
    return std::accumulate(shapes.begin(), shapes.end(), PkRectF(),
                           kismpl::mem_bit_or(&KoShape::boundingRect));
}

PkRectF KoShape::absoluteOutlineRect() const
{
    return absoluteTransformation().map(outline()).boundingRect();
}

PkRectF KoShape::absoluteOutlineRect(const PkList<KoShape *> &shapes)
{
    return std::accumulate(shapes.begin(), shapes.end(), PkRectF(),
                           kismpl::mem_bit_or(&KoShape::absoluteOutlineRect));
}

PkTransform KoShape::absoluteTransformation() const
{
    PkTransform matrix;
    // apply parents matrix to inherit any transformations done there.
    KoShapeContainer * container = d->parent;
    if (container) {
        if (container->inheritsTransform(this)) {
            matrix = container->absoluteTransformation();
        } else {
            PkSizeF containerSize = container->size();
            PkPointF containerPos = container->absolutePosition() - PkPointF(0.5 * containerSize.width(), 0.5 * containerSize.height());
            matrix.translate(containerPos.x(), containerPos.y());
        }
    }

    return s->localMatrix * matrix;
}

void KoShape::applyAbsoluteTransformation(const PkTransform &matrix)
{
    PkTransform globalMatrix = absoluteTransformation();
    // the transformation is relative to the global coordinate system
    // but we want to change the local matrix, so convert the matrix
    // to be relative to the local coordinate system
    PkTransform transformMatrix = globalMatrix * matrix * globalMatrix.inverted();
    applyTransformation(transformMatrix);
}

void KoShape::applyTransformation(const PkTransform &matrix)
{
    const PkTransform newLocalMatrix = matrix * s->localMatrix;

    if (s->localMatrix != newLocalMatrix) {
        s->localMatrix = newLocalMatrix;
        notifyChanged();
        shapeChangedPriv(GenericMatrixChange);
    }
}

void KoShape::setTransformation(const PkTransform &matrix)
{
    if (s->localMatrix != matrix) {
        s->localMatrix = matrix;
        notifyChanged();
        shapeChangedPriv(GenericMatrixChange);
    }
}

PkTransform KoShape::transformation() const
{
    return s->localMatrix;
}

KoShape::ChildZOrderPolicy KoShape::childZOrderPolicy()
{
    return ChildZDefault;
}

bool KoShape::compareShapeZIndex(KoShape *s1, KoShape *s2)
{
    /**
     * WARNING: Our definition of zIndex is not yet compatible with SVG2's
     *          definition. In SVG stacking context of groups with the same
     *          zIndex are **merged**, while in Krita the contents of groups
     *          is never merged. One group will always below than the other.
     *          Therefore, when zIndex of two groups inside the same parent
     *          coincide, the resulting painting order in Krita is
     *          **UNDEFINED**.
     *
     *          To avoid this trouble we use  KoShapeReorderCommand::mergeInShape()
     *          inside KoShapeCreateCommand.
     */

    /**
     * The algorithm below doesn't correctly handle the case when the two pointers actually
     * point to the same shape. So just check it in advance to guarantee strict weak ordering
     * relation requirement
     */
    if (s1 == s2) return false;

    KoShape *parentShapeS1 = s1->parent();
    KoShape *parentShapeS2 = s2->parent();

    // We basically walk up through the parents until we find a common base parent
    // To do that we need two loops where the inner loop walks up through the parents
    // of s2 every time we step up one parent level on s1
    //
    // We don't update the index value until after we have seen that it's not a common base
    // That way we ensure that two children of a common base are sorted according to their respective
    // z value
    bool foundCommonParent = false;
    int index1 = s1->zIndex();
    int index2 = s2->zIndex();
    parentShapeS1 = s1;
    parentShapeS2 = s2;
    while (parentShapeS1 && !foundCommonParent) {
        parentShapeS2 = s2;
        index2 = parentShapeS2->zIndex();
        while (parentShapeS2) {
            if (parentShapeS2 == parentShapeS1) {
                foundCommonParent = true;
                break;
            }
            if (parentShapeS2->childZOrderPolicy() == KoShape::ChildZParentChild) {
                index2 = parentShapeS2->zIndex();
            }
            parentShapeS2 = parentShapeS2->parent();
        }

        if (!foundCommonParent) {
            if (parentShapeS1->childZOrderPolicy() == KoShape::ChildZParentChild) {
                index1 = parentShapeS1->zIndex();
            }
            parentShapeS1 = parentShapeS1->parent();
        }
    }

    // If the one shape is a parent/child of the other then sort so.
    if (s1 == parentShapeS2) {
        return true;
    }
    if (s2 == parentShapeS1) {
        return false;
    }

    // If we went that far then the z-Index is used for sorting.
    return index1 < index2;
}

void KoShape::setParent(KoShapeContainer *parent)
{

    if (d->parent == parent) {
        return;
    }

    if (d->parent) {
        d->parent->shapeInterface()->removeShape(this);
        d->parent = 0;
    }


    KIS_SAFE_ASSERT_RECOVER_NOOP(parent != this);

    if (parent && parent != this) {
        d->parent = parent;
        parent->shapeInterface()->addShape(this);
    }

    notifyChanged();
    shapeChangedPriv(ParentChanged);
}

bool KoShape::inheritsTransformFromAny(const PkList<KoShape *> ancestorsInQuestion) const
{
    bool result = false;

    KoShape *shape = const_cast<KoShape*>(this);
    while (shape) {
        KoShapeContainer *parent = shape->parent();
        if (parent && !parent->inheritsTransform(shape)) {
            break;
        }

        if (ancestorsInQuestion.contains(shape)) {
            result = true;
            break;
        }

        shape = parent;
    }

    return result;
}

bool KoShape::hasCommonParent(const KoShape *shape) const
{
    const KoShape *thisShape = this;
    while (thisShape) {

        const KoShape *otherShape = shape;
        while (otherShape) {
            if (thisShape == otherShape) {
                return true;
            }
            otherShape = otherShape->parent();
        }

        thisShape = thisShape->parent();
    }

    return false;
}

qint16 KoShape::zIndex() const
{
    return s->zIndex;
}

void KoShape::update() const
{

    if (!d->shapeManagers.isEmpty()) {
        const PkRectF rect(boundingRect());
        Q_FOREACH (KoShapeManager * manager, d->shapeManagers) {
            manager->update(rect, this, true);
        }
    }
}

void KoShape::updateAbsolute(const PkRectF &rect) const
{
    if (rect.isEmpty() && !rect.isNull()) {
        return;
    }


    if (!d->shapeManagers.isEmpty() && isVisible()) {
        Q_FOREACH (KoShapeManager *manager, d->shapeManagers) {
            manager->update(rect);
        }
    }
}

PkPainterPath KoShape::outline() const
{
    PkPainterPath path;
    path.addRect(outlineRect());
    return path;
}

PkRectF KoShape::outlineRect() const
{
    const PkSizeF s = size();
    return PkRectF(PkPointF(0, 0), PkSizeF(qMax(s.width(),  qreal(0.0001)),
                                        qMax(s.height(), qreal(0.0001))));
}

PkPointF KoShape::absolutePosition(KoFlake::AnchorPosition anchor) const
{
    const PkRectF rc = outlineRect();

    PkPointF point = rc.topLeft();

    bool valid = false;
    PkPointF anchoredPoint = KoFlake::anchorToPoint(anchor, rc, &valid);
    if (valid) {
        point = anchoredPoint;
    }

    return absoluteTransformation().map(point);
}

void KoShape::setAbsolutePosition(const PkPointF &newPosition, KoFlake::AnchorPosition anchor)
{
    PkPointF currentAbsPosition = absolutePosition(anchor);
    PkPointF translate = newPosition - currentAbsPosition;
    PkTransform translateMatrix;
    translateMatrix.translate(translate.x(), translate.y());
    applyAbsoluteTransformation(translateMatrix);
    notifyChanged();
    shapeChangedPriv(PositionChanged);
}

void KoShape::copySettings(const KoShape *shape)
{
    s->size = shape->size();
    s->zIndex = shape->zIndex();
    s->visible = shape->isVisible(false);

    // Ensure printable is true by default
    if (!s->visible)
        s->printable = true;
    else
        s->printable = shape->isPrintable();

    s->geometryProtected = shape->isGeometryProtected();
    s->protectContent = shape->isContentProtected();
    s->selectable = shape->isSelectable();
    s->keepAspect = shape->keepAspectRatio();
    s->localMatrix = shape->s->localMatrix;
}

void KoShape::notifyChanged()
{
    Q_FOREACH (KoShapeManager * manager, d->shapeManagers) {
        manager->notifyShapeChanged(this);
    }
}

void KoShape::setUserData(KoShapeUserData *userData)
{
    s->userData.reset(userData);
}

KoShapeUserData *KoShape::userData() const
{
    return s->userData.data();
}

bool KoShape::hasTransparency() const
{
    PkSharedPointer<KoShapeBackground> bg = background();

    return !bg || bg->hasTransparency() || s->transparency > 0.0;
}

void KoShape::setTransparency(qreal transparency)
{
    s->transparency = qBound<qreal>(0.0, transparency, 1.0);

    shapeChangedPriv(TransparencyChanged);
    notifyChanged();
}

qreal KoShape::transparency(bool recursive) const
{
    if (!recursive || !parent()) {
        return s->transparency;
    } else {
        const qreal parentOpacity = 1.0-parent()->transparency(recursive);
        const qreal childOpacity = 1.0-s->transparency;
        return 1.0-(parentOpacity*childOpacity);
    }
}

KoInsets KoShape::strokeInsets() const
{
    KoInsets answer;
    if (s->stroke)
        s->stroke->strokeInsets(this, answer);
    return answer;
}

void KoShape::setPaintOrder(KoShape::PaintOrder first, KoShape::PaintOrder second)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(first != second);
    PkVector<PaintOrder> order = defaultPaintOrder();

    if (first != Fill) {
        if (order.at(1) == first) {
            order[1] = order[0];
            order[0] = first;
        } else if (order.at(2) == first) {
            order[2] = order[0];
            order[0] = first;
        }
    }
    if (second != first && second != Stroke) {
        if (order.at(2) == second) {
            order[2] = order[1];
            order[1] = second;
        }
    }
    s->inheritPaintOrder = false;
    s->paintOrder = order;
}

PkVector<KoShape::PaintOrder> KoShape::paintOrder() const
{
    PkVector<PaintOrder> order = defaultPaintOrder();
    if (!s->inheritPaintOrder) {
        order = s->paintOrder;
    } else if (parent()) {
        order = parent()->paintOrder();
    }
    return order;
}

PkVector<KoShape::PaintOrder> KoShape::defaultPaintOrder()
{
    static PkVector<PaintOrder> order = {Fill, Stroke, Markers};
    return order;
}

void KoShape::setInheritPaintOrder(bool value)
{
    s->inheritPaintOrder = value;
}

bool KoShape::inheritPaintOrder() const
{
    return s->inheritPaintOrder;
}

qreal KoShape::rotation() const
{
    // try to extract the rotation angle out of the local matrix
    // if it is a pure rotation matrix

    // check if the matrix has shearing mixed in
    if (fabs(fabs(s->localMatrix.m12()) - fabs(s->localMatrix.m21())) > 1e-10)
        return std::numeric_limits<qreal>::quiet_NaN();
    // check if the matrix has scaling mixed in
    if (fabs(s->localMatrix.m11() - s->localMatrix.m22()) > 1e-10)
        return std::numeric_limits<qreal>::quiet_NaN();

    // calculate the angle from the matrix elements
    qreal angle = atan2(-s->localMatrix.m21(), s->localMatrix.m11()) * 180.0 / M_PI;
    if (angle < 0.0)
        angle += 360.0;

    return angle;
}

PkSizeF KoShape::size() const
{
    return s->size;
}

PkPointF KoShape::position() const
{
    PkPointF center = outlineRect().center();
    return s->localMatrix.map(center) - center;
}

void KoShape::setBackground(PkSharedPointer<KoShapeBackground> fill)
{
    s->inheritBackground = false;
    s->fill = fill;
    shapeChangedPriv(BackgroundChanged);
    notifyChanged();
}

PkSharedPointer<KoShapeBackground> KoShape::background() const
{

    PkSharedPointer<KoShapeBackground> bg;

    if (!s->inheritBackground) {
        bg = s->fill;
    } else if (parent()) {
        bg = parent()->background();
    }

    return bg;
}

void KoShape::setInheritBackground(bool value)
{

    s->inheritBackground = value;
    if (s->inheritBackground) {
        s->fill.clear();
    }
}

bool KoShape::inheritBackground() const
{
    return s->inheritBackground;
}

void KoShape::setZIndex(qint16 zIndex)
{
    if (s->zIndex == zIndex)
        return;
    s->zIndex = zIndex;
    notifyChanged();
}

void KoShape::setVisible(bool on)
{
    int _on = (on ? 1 : 0);
    if (s->visible == _on) return;
    s->visible = _on;
}

bool KoShape::isVisible(bool recursive) const
{
    if (!recursive)
        return s->visible;

    if (!s->visible)
        return false;

    KoShapeContainer * parentShape = parent();

    if (parentShape) {
        return parentShape->isVisible(true);
    }

    return true;
}

void KoShape::setPrintable(bool on)
{
    s->printable = on;
}

bool KoShape::isPrintable() const
{
    if (s->visible)
        return s->printable;
    else
        return false;
}

void KoShape::setSelectable(bool selectable)
{
    s->selectable = selectable;
}

bool KoShape::isSelectable() const
{
    return s->selectable;
}

void KoShape::setGeometryProtected(bool on)
{
    s->geometryProtected = on;
}

bool KoShape::isGeometryProtected() const
{
    return s->geometryProtected;
}

void KoShape::setContentProtected(bool protect)
{
    s->protectContent = protect;
}

bool KoShape::isContentProtected() const
{
    return s->protectContent;
}

KoShapeContainer *KoShape::parent() const
{
    return d->parent;
}

void KoShape::setKeepAspectRatio(bool keepAspect)
{
    s->keepAspect = keepAspect;

    shapeChangedPriv(KeepAspectRatioChange);
    notifyChanged();
}

bool KoShape::keepAspectRatio() const
{
    return s->keepAspect;
}

PkString KoShape::shapeId() const
{
    return s->shapeId;
}

void KoShape::setShapeId(const PkString &id)
{
    s->shapeId = id;
}

KoShapeStrokeModelSP KoShape::stroke() const
{

    KoShapeStrokeModelSP stroke;

    if (!s->inheritStroke) {
        stroke = s->stroke;
    } else if (parent()) {
        stroke = parent()->stroke();
    }

    return stroke;
}

void KoShape::setStroke(KoShapeStrokeModelSP stroke)
{

    s->inheritStroke = false;
    s->stroke = stroke;
    shapeChangedPriv(StrokeChanged);
    notifyChanged();
}

void KoShape::setInheritStroke(bool value)
{
    s->inheritStroke = value;
    if (s->inheritStroke) {
        s->stroke.clear();
    }
}

bool KoShape::inheritStroke() const
{
    return s->inheritStroke;
}

void KoShape::setClipPath(KoClipPath *clipPath)
{
    s->clipPath.reset(clipPath);
    shapeChangedPriv(ClipPathChanged);
    notifyChanged();
}

KoClipPath * KoShape::clipPath() const
{
    return s->clipPath.data();
}

void KoShape::setClipMask(KoClipMask *clipMask)
{
    s->clipMask.reset(clipMask);
    shapeChangedPriv(ClipMaskChanged);
    notifyChanged();
}

KoClipMask* KoShape::clipMask() const
{
    return s->clipMask.data();
}

PkTransform KoShape::transform() const
{
    return s->localMatrix;
}

PkString KoShape::name() const
{
    return s->name;
}

void KoShape::setName(const PkString &name)
{
    s->name = name;
}

void KoShape::waitUntilReady(bool asynchronous) const
{
    Q_UNUSED(asynchronous);
}

bool KoShape::isShapeEditable(bool recursive) const
{
    if (!s->visible || s->geometryProtected)
        return false;

    if (recursive && d->parent) {
        return d->parent->isShapeEditable(true);
    }

    return true;
}

KisHandlePainterHelper KoShape::createHandlePainterHelperView(QPainter *painter, KoShape *shape, const KoViewConverter &converter, qreal handleRadius, int decorationThickness)
{
    const PkTransform originalPainterTransform = toPkTransform(painter->transform());

    painter->setTransform(toQTransform(shape->absoluteTransformation()) *
                          toQTransform(converter.documentToView()) *
                          painter->transform());

    // move c-tor
    return KisHandlePainterHelper(painter, originalPainterTransform, handleRadius, decorationThickness);
}

KisHandlePainterHelper KoShape::createHandlePainterHelperDocument(QPainter *painter, KoShape *shape, qreal handleRadius, int decorationThickness)
{
    const PkTransform originalPainterTransform = toPkTransform(painter->transform());

    painter->setTransform(toQTransform(shape->absoluteTransformation()) *
                          painter->transform());

    // move c-tor
    return KisHandlePainterHelper(painter, originalPainterTransform, handleRadius, decorationThickness);
}


PkPointF KoShape::shapeToDocument(const PkPointF &point) const
{
    return absoluteTransformation().map(point);
}

PkRectF KoShape::shapeToDocument(const PkRectF &rect) const
{
    return absoluteTransformation().mapRect(rect);
}

PkPointF KoShape::documentToShape(const PkPointF &point) const
{
    return absoluteTransformation().inverted().map(point);
}

PkRectF KoShape::documentToShape(const PkRectF &rect) const
{
    return absoluteTransformation().inverted().mapRect(rect);
}

bool KoShape::addDependee(KoShape *shape)
{
    if (! shape)
        return false;

    // refuse to establish a circular dependency
    if (shape->hasDependee(this))
        return false;

    if (! d->dependees.contains(shape)) {
        d->dependees.append(shape);
        shape->addShapeChangeListener(&d->dependeesLifetimeListener);
    }

    return true;
}

void KoShape::removeDependee(KoShape *shape)
{
    d->dependees.removeOne(shape);
    shape->removeShapeChangeListener(&d->dependeesLifetimeListener);
}

bool KoShape::hasDependee(KoShape *shape) const
{
    return d->dependees.contains(shape);
}

PkList<KoShape*> KoShape::dependees() const
{
    return d->dependees;
}

void KoShape::shapeChanged(ChangeType type, KoShape *shape)
{
    Q_UNUSED(type);
    Q_UNUSED(shape);

    /**
     * NOTE: we don't need to handle Deleted type for the shapes
     * in d->dependees list here, becasue this function is called
     * on the other side of the relation, i.e. in the actual
     * "dependee". We handle that in Private::DependeesLifetimeListener.
     */
}

KoSnapData KoShape::snapData() const
{
    return KoSnapData();
}

void KoShape::setAdditionalAttribute(const PkString &name, const PkString &value)
{
    s->additionalAttributes.insert(name, value);
}

void KoShape::removeAdditionalAttribute(const PkString &name)
{
    s->additionalAttributes.remove(name);
}

bool KoShape::hasAdditionalAttribute(const PkString &name) const
{
    return s->additionalAttributes.contains(name);
}

PkString KoShape::additionalAttribute(const PkString &name) const
{
    return s->additionalAttributes.value(name);
}

void KoShape::setAdditionalStyleAttribute(const char *name, const PkString &value)
{
    s->additionalStyleAttributes.insert(name, value);
}

void KoShape::removeAdditionalStyleAttribute(const char *name)
{
    s->additionalStyleAttributes.remove(name);
}

PkSet<KoShape*> KoShape::toolDelegates() const
{
    return d->toolDelegates;
}

void KoShape::setToolDelegates(const PkSet<KoShape*> &delegates)
{
    d->toolDelegates = delegates;
}

PkString KoShape::hyperLink () const
{
    return s->hyperLink;
}

void KoShape::setHyperLink(const PkString &hyperLink)
{
    s->hyperLink = hyperLink;
}

KoShape::ShapeChangeListener::~ShapeChangeListener()
{
    Q_FOREACH(KoShape *shape, m_registeredShapes) {
        shape->removeShapeChangeListener(this);
    }
}

void KoShape::ShapeChangeListener::registerShape(KoShape *shape)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_registeredShapes.contains(shape));
    m_registeredShapes.append(shape);
}

void KoShape::ShapeChangeListener::unregisterShape(KoShape *shape)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_registeredShapes.contains(shape));
    m_registeredShapes.removeAll(shape);
}

void KoShape::ShapeChangeListener::notifyShapeChangedImpl(KoShape::ChangeType type, KoShape *shape)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_registeredShapes.contains(shape));

    notifyShapeChanged(type, shape);

    if (type == KoShape::Deleted) {
        unregisterShape(shape);
    }
}

void KoShape::addShapeChangeListener(KoShape::ShapeChangeListener *listener)
{

    KIS_SAFE_ASSERT_RECOVER_RETURN(!d->listeners.contains(listener));
    listener->registerShape(this);
    d->listeners.append(listener);
}

void KoShape::removeShapeChangeListener(KoShape::ShapeChangeListener *listener)
{

    KIS_SAFE_ASSERT_RECOVER_RETURN(d->listeners.contains(listener));
    d->listeners.removeAll(listener);
    listener->unregisterShape(this);
}

PkList<KoShape::ShapeChangeListener *> KoShape::listeners() const
{
    return d->listeners;
}

PkList<KoShape *> KoShape::linearizeSubtree(const PkList<KoShape *> &shapes)
{
    PkList<KoShape *> result;

    Q_FOREACH (KoShape *shape, shapes) {
        result << shape;

        KoShapeContainer *container = dynamic_cast<KoShapeContainer*>(shape);
        if (container) {
            result << linearizeSubtree(container->shapes());
        }
    }

    return result;
}

PkList<KoShape *> KoShape::linearizeSubtreeSorted(const PkList<KoShape *> &shapes)
{
    PkList<KoShape*> sortedShapes = shapes;
    std::sort(sortedShapes.begin(), sortedShapes.end(), KoShape::compareShapeZIndex);

    PkList<KoShape *> result;

    Q_FOREACH (KoShape *shape, sortedShapes) {
        result << shape;

        KoShapeContainer *container = dynamic_cast<KoShapeContainer*>(shape);
        if (container) {
            result << linearizeSubtreeSorted(container->shapes());
        }
    }

    return result;
}

void KoShape::setResolution(qreal /*xRes*/, qreal /*yRes*/)
{
}
