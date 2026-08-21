/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkXmlCompat.h>

#include "KoCompositeColorTransformation.h"

#include <PkVector.h>


struct KoCompositeColorTransformation::Private
{
    ~Private() {
        for (KoColorTransformation* t : transformations) {
            delete t;
        }
    }

    PkVector<KoColorTransformation*> transformations;
};


KoCompositeColorTransformation::KoCompositeColorTransformation(Mode mode)
    : m_d(new Private)
{
    Q_ASSERT(mode == INPLACE);
    Q_UNUSED(mode);
}

KoCompositeColorTransformation::~KoCompositeColorTransformation()
{
}

void KoCompositeColorTransformation::appendTransform(KoColorTransformation *transform)
{
    if (transform) {
        m_d->transformations.append(transform);
    }
}

void KoCompositeColorTransformation::transform(const quint8 *src, quint8 *dst, qint32 nPixels) const
{
    PkVector<KoColorTransformation*>::const_iterator begin = m_d->transformations.constBegin();
    PkVector<KoColorTransformation*>::const_iterator it = begin;
    PkVector<KoColorTransformation*>::const_iterator end = m_d->transformations.constEnd();

    for (; it != end; ++it) {
        if (it == begin) {
            (*it)->transform(src, dst, nPixels);
        } else {
            (*it)->transform(dst, dst, nPixels);
        }
    }
}

KoColorTransformation* KoCompositeColorTransformation::createOptimizedCompositeTransform(const PkVector<KoColorTransformation*> transforms)
{
    KoColorTransformation *finalTransform = 0;

    int numValidTransforms = 0;
    for (KoColorTransformation *t : transforms) {
        numValidTransforms += bool(t);
    }

    if (numValidTransforms > 1) {
        KoCompositeColorTransformation *compositeTransform =
            new KoCompositeColorTransformation(
                KoCompositeColorTransformation::INPLACE);

        for (KoColorTransformation *t : transforms) {
            if (t) {
                compositeTransform->appendTransform(t);
            }
        }

        finalTransform = compositeTransform;

    } else if (numValidTransforms == 1) {
        for (KoColorTransformation *t : transforms) {
            if (t) {
                finalTransform = t;
                break;
            }
        }
    }

    return finalTransform;
}
