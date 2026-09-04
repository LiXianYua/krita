/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KOSHAPEPRIVATE_H
#define KOSHAPEPRIVATE_H

#include "KoShape.h"

#include <PkPoint.h>
#include <QPaintDevice>
#include <PkTransform.h>
#include <PkScopedPointer.h>
#include <QSharedData>

#include <KoClipMask.h>

class KoShapeManager;


class KoShape::SharedData : public QSharedData
{
public:
    explicit SharedData();
    virtual ~SharedData();

    explicit SharedData(const SharedData &rhs);


public:
    // Members

    mutable PkSizeF size; // size in pt
    PkString shapeId;
    PkString name; ///< the shapes names

    PkTransform localMatrix; ///< the shapes local transformation matrix

    PkScopedPointer<KoShapeUserData> userData;
    PkSharedPointer<KoShapeStrokeModel> stroke; ///< points to a stroke, or 0 if there is no stroke
    PkSharedPointer<KoShapeBackground> fill; ///< Stands for the background color / fill etc.
    bool inheritBackground = false;
    bool inheritStroke = false;
    // XXX: change this to instance instead of pointer
    PkScopedPointer<KoClipPath> clipPath; ///< the current clip path
    PkScopedPointer<KoClipMask> clipMask; ///< the current clip mask
    PkMap<PkString, PkString> additionalAttributes;
    PkMap<PkByteArray, PkString> additionalStyleAttributes;
    qreal transparency; ///< the shapes transparency
    PkString hyperLink; //hyperlink for this shape

    int zIndex : 16; // keep maxZIndex in sync!
    int visible : 1;
    int printable : 1;
    int geometryProtected : 1;
    int keepAspect : 1;
    int selectable : 1;
    int protectContent : 1;

    PkVector<PaintOrder> paintOrder {Fill, Stroke, Markers};
    bool inheritPaintOrder = true;
};

class KoShape::Private
{
private:
    /**
     * A special listener class to track the lifetime of the dependees
     * and remove them from the notification list when they are deleted
     */
    struct DependeesLifetimeListener : KoShape::ShapeChangeListener
    {
        DependeesLifetimeListener(PkList<KoShape*> &dependees)
            : m_dependees(dependees)
        {
        }

        void notifyShapeChanged(KoShape::ChangeType type, KoShape *shape) override {
            if (type == KoShape::Deleted && m_dependees.contains(shape)) {
                m_dependees.removeOne(shape);
                // the listener will be unlinked on destruction automatically
            }
        }

    private:
        PkList<KoShape*> &m_dependees;
    };

public:
    Private() : dependeesLifetimeListener(dependees) {}
    Private(const Private &rhs) = delete;

    KoShapeContainer *parent = nullptr;
    PkSet<KoShapeManager *> shapeManagers;
    PkSet<KoShape *> toolDelegates;
    PkList<KoShape*> dependees; ///< list of shape dependent on this shape
    PkList<KoShape::ShapeChangeListener*> listeners;
    DependeesLifetimeListener dependeesLifetimeListener;
};

#endif
