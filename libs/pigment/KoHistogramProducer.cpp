/*
 *  SPDX-FileCopyrightText: 2005 Bart Coppens <kde@bartcoppens.be>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <PkXmlCompat.h>

#include "KoHistogramProducer.h"

#include <KoID.h>

#include "KoBasicHistogramProducers.h"

#include "KoColorSpace.h"

KoHistogramProducerFactoryRegistry::KoHistogramProducerFactoryRegistry()
{
}

KoHistogramProducerFactoryRegistry::~KoHistogramProducerFactoryRegistry()
{
    for (auto *entry : values()) {
        delete entry;
    }
}

KoHistogramProducerFactoryRegistry* KoHistogramProducerFactoryRegistry::instance()
{
    static KoHistogramProducerFactoryRegistry s_instance;
    return &s_instance;
}

PkList<PkString> KoHistogramProducerFactoryRegistry::keysCompatibleWith(const KoColorSpace* colorSpace, bool isStrict) const
{
    PkList<PkString> list;
    PkList<float> preferredList;
    for (const PkString &id : keys()) {
        KoHistogramProducerFactory *f = value(id);

        if (f->isCompatibleWith(colorSpace, isStrict)) {
            float preferred = f->preferrednessLevelWith(colorSpace);
            PkList<float>::iterator pit = preferredList.begin();
            PkList<float>::iterator pend = preferredList.end();
            PkList<PkString>::iterator lit = list.begin();

            while (pit != pend && preferred <= *pit) {
                ++pit;
                ++lit;
            }

            list.insert(lit, id);
            preferredList.insert(pit, preferred);
        }
    }
    return list;
}
