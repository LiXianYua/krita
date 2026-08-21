/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoColorProfileStorage.h"

#include <cmath>

#include <PkAuxTypes.h>
#include <PkHash.h>
#include <PkList.h>
#include <PkReadWriteLock.h>
#include <PkString.h>
#include <PkVector.h>

#include "DebugPigment.h"
#include "KoColorSpaceFactory.h"
#include "KoColorProfile.h"
#include "kis_assert.h"


// PkByteArray 的 qHash：pk 树未提供该重载，本 target 首个以 PkByteArray 为
// PkHash 键的调用点（profileUniqueIdMap）。按 PkHashFunctions.h 的 ADL 机制补在
// 全局命名空间。哈希值不要求与 Qt 逐位相同（该头已声明此约定）。
unsigned int qHash(const PkByteArray &key, unsigned int seed = 0)
{
    unsigned int h = seed;
    const char *data = key.constData();
    for (int i = 0; i < key.size(); ++i) {
        h = (h ^ static_cast<unsigned char>(data[i])) * 16777619u; // FNV-1a
    }
    return h;
}

struct KoColorProfileStorage::Private {
    PkHash<PkString, KoColorProfile * > profileMap;
    PkHash<PkByteArray, KoColorProfile * > profileUniqueIdMap;
    PkList<KoColorProfile *> duplicates;
    PkHash<PkString, PkString> profileAlias;
    PkReadWriteLock lock;

    void populateUniqueIdMap();

    ~Private()
    {
        for (KoColorProfile *p : profileMap) {
            profileUniqueIdMap.remove(p->uniqueId());
            duplicates.removeAll(p);
            delete p;
        }
        profileMap.clear();
        for (KoColorProfile *p : profileUniqueIdMap) {
            duplicates.removeAll(p);
            delete p;
        }
        profileUniqueIdMap.clear();
        for (KoColorProfile *p : duplicates) {
            delete p;
        }
        duplicates.clear();
    }
};

KoColorProfileStorage::KoColorProfileStorage()
    : d(new Private)
{

}

KoColorProfileStorage::~KoColorProfileStorage()
{
}

void KoColorProfileStorage::addProfile(KoColorProfile *profile)
{
    PkWriteLocker locker(&d->lock);

    if (profile->valid()) {
        d->profileMap[profile->name()] = profile;
        if (d->profileUniqueIdMap.contains(profile->uniqueId())) {
            //warnPigment << "Duplicated profile" << profile->name() << profile->fileName() << d->profileUniqueIdMap[profile->uniqueId()]->fileName();
            d->duplicates.append(d->profileUniqueIdMap[profile->uniqueId()]);
        }
        d->profileUniqueIdMap.insert(profile->uniqueId(), profile);
    }
}

void KoColorProfileStorage::removeProfile(KoColorProfile *profile)
{
    PkWriteLocker locker(&d->lock);

    d->profileMap.remove(profile->name());
    d->profileUniqueIdMap.remove(profile->uniqueId());
    d->duplicates.removeAll(profile);
}

bool KoColorProfileStorage::containsProfile(const KoColorProfile *profile)
{
    PkReadLocker l(&d->lock);
    return d->profileMap.contains(profile->name());
}

void KoColorProfileStorage::addProfileAlias(const PkString &name, const PkString &to)
{
    PkWriteLocker l(&d->lock);
    d->profileAlias[name] = to;
}

PkString KoColorProfileStorage::profileAlias(const PkString &name) const
{
    PkReadLocker l(&d->lock);
    return d->profileAlias.value(name, name);
}

const KoColorProfile *KoColorProfileStorage::profileByName(const PkString &name) const
{
    PkReadLocker l(&d->lock);
    return d->profileMap.value(d->profileAlias.value(name, name), 0);
}

void KoColorProfileStorage::Private::populateUniqueIdMap()
{
    PkWriteLocker l(&lock);
    profileUniqueIdMap.clear();

    for (auto it = profileMap.constBegin();
         it != profileMap.constEnd();
         ++it) {

        KoColorProfile *profile = it.value();
        PkByteArray id = profile->uniqueId();

        if (!id.isEmpty()) {
            profileUniqueIdMap.insert(id, profile);
        }
    }
}


const KoColorProfile *KoColorProfileStorage::profileByUniqueId(const PkByteArray &id) const
{
    PkReadLocker l(&d->lock);
    if (d->profileUniqueIdMap.isEmpty()) {
        l.unlock();
        d->populateUniqueIdMap();
        l.relock();
    }
    return d->profileUniqueIdMap.value(id, 0);

}

PkList<const KoColorProfile *> KoColorProfileStorage::profilesFor(const KoColorSpaceFactory *csf) const
{
    PkList<const KoColorProfile *>  profiles;
    if (!csf) return profiles;

    PkReadLocker l(&d->lock);

    PkHash<PkString, KoColorProfile * >::ConstIterator it;
    for (it = d->profileMap.constBegin(); it != d->profileMap.constEnd(); ++it) {
        KoColorProfile *  profile = it.value();
        if (csf->profileIsCompatible(profile)) {
            Q_ASSERT(profile);
            //         if (profile->colorSpaceSignature() == csf->colorSpaceSignature()) {
            profiles.push_back(profile);
        }
    }
    return profiles;
}

PkList<const KoColorProfile *> KoColorProfileStorage::profilesFor(const PkVector<double> &colorants, ColorPrimaries colorantType, TransferCharacteristics transferType, double error)
{
    PkList<const KoColorProfile *> profiles;

    if (colorants.isEmpty() && colorantType == PRIMARIES_UNSPECIFIED && transferType == TRC_UNSPECIFIED) {
        return profiles;
    }

    PkReadLocker l(&d->lock);
    for (const KoColorProfile* profile : d->profileMap) {
        bool colorantMatch = (colorants.isEmpty() || colorantType != PRIMARIES_UNSPECIFIED);
        bool colorantTypeMatch = (colorantType == PRIMARIES_UNSPECIFIED);
        bool transferMatch = (transferType == 2);
        if (colorantType != PRIMARIES_UNSPECIFIED) {
            if (int(profile->getColorPrimaries()) == colorantType) {
                colorantTypeMatch = true;
            }
        }
        if (transferType != TRC_UNSPECIFIED) {
            if (int(profile->getTransferCharacteristics()) == transferType) {
                transferMatch = true;
            }
        }

        if (!colorants.isEmpty() && colorantType == PRIMARIES_UNSPECIFIED) {
            PkVector<qreal> wp = profile->getWhitePointxyY();
            if (profile->hasColorants() && colorants.size() == 8) {
                PkVector<qreal> col = profile->getColorantsxyY();
                if (col.size() < 8 || wp.size() < 2) {
                    // too few colorants, skip.
                    continue;
                }
                PkVector<double> compare = {wp[0], wp[1], col[0], col[1], col[3], col[4], col[6], col[7]};

                for (int i = 0; i < compare.size(); i++) {
                    colorantMatch = std::fabs(compare[i] - colorants[i]) < error;
                    if (!colorantMatch) {
                        break;
                    }
                }
            } else {
                if (wp.size() < 2 || colorants.size() < 2) {
                    // too few colorants, skip.
                    continue;
                }
                if (std::fabs(wp[0] - colorants[0]) < error && std::fabs(wp[1] - colorants[1]) < error) {
                    colorantMatch = true;
                }
            }
        }

        if (transferMatch && colorantMatch && colorantTypeMatch) {
            profiles.push_back(profile);
        }
    }

    return profiles;
}
