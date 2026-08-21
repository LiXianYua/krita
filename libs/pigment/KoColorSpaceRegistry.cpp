/*
 *  SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004, 2010 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <PkXmlCompat.h>

#include "KoColorSpaceRegistry.h"

#include <PkHash.h>

#include <PkReadWriteLock.h>

#include "KoGenericRegistry.h"
#include "DebugPigment.h"
#include "KoBasicHistogramProducers.h"
#include "KoColorSpace.h"
#include "KoColorProfile.h"
#include "KoColorConversionCache.h"
#include "KoColorConversionSystem.h"

#include "colorspaces/KoAlphaColorSpace.h"
#include "colorspaces/KoLabColorSpace.h"
#include "colorspaces/KoRgbU16ColorSpace.h"
#include "colorspaces/KoRgbU8ColorSpace.h"
#include "colorspaces/KoSimpleColorSpaceEngine.h"
#include "KoColorSpace_p.h"

#include "kis_assert.h"
#include "KoColorProfileStorage.h"
#include <KisReadWriteLockPolicy.h>
#include <PkScopedPointer.h>

#include <KoColorModelStandardIds.h>


struct Q_DECL_HIDDEN KoColorSpaceRegistry::Private {

    // interface for KoColorSpaceFactory
    struct ProfileRegistrationInterface;
    // interface for KoColorConversionSystem
    struct ConversionSystemInterface;


    Private(KoColorSpaceRegistry *_q) : q(_q) {}

    KoColorSpaceRegistry *q {nullptr};

    KoGenericRegistry<KoColorSpaceFactory *> colorSpaceFactoryRegistry;
    KoColorProfileStorage profileStorage;
    PkHash<PkString, const KoColorSpace *> csMap;
    PkScopedPointer<ConversionSystemInterface> conversionSystemInterface;
    KoColorConversionSystem *colorConversionSystem {nullptr};
    KoColorConversionCache* colorConversionCache {nullptr};
    const KoColorSpace *rgbU8sRGB {nullptr};
    const KoColorSpace *lab16sLAB {nullptr};
    const KoColorSpace *alphaCs {nullptr};
    const KoColorSpace *alphaU16Cs {nullptr};
#ifdef HAVE_OPENEXR
    const KoColorSpace *alphaF16Cs {nullptr};
#endif
    const KoColorSpace *alphaF32Cs {nullptr};
    PkReadWriteLock registrylock;

    /**
     * The function checks if a colorspace with a certain id and profile name can be found in the cache
     * NOTE: the function doesn't take any lock but it needs to be called inside a d->registryLock
     * locked either in read or write.
     * @param csId The colorspace id
     * @param profileName The colorspace profile name
     * @retval KoColorSpace The matching colorspace
     * @retval 0 Null pointer if not match
     */
    const KoColorSpace* getCachedColorSpaceImpl(const PkString & csId, const PkString & profileName) const;

    PkString idsToCacheName(const PkString & csId, const PkString & profileName) const;
    PkString defaultProfileForCsIdImpl(const PkString &csID);
    const KoColorProfile * profileForCsIdWithFallbackImpl(const PkString &csID, const PkString &profileName);
    PkString colorSpaceIdImpl(const PkString & colorModelId, const PkString & colorDepthId) const;

    const KoColorSpace *lazyCreateColorSpaceImpl(const PkString &csID, const KoColorProfile *profile);

    /**
     * Return a colorspace that works with the parameter profile.
     * @param profileName the name of the KoColorProfile to be combined with the colorspace
     * @return the wanted colorspace, or 0 when the cs and profile can not be combined.
     */
    template<class LockPolicy = NormalLockPolicy>
    const KoColorSpace * colorSpace1(const PkString &colorSpaceId, const PkString &pName = PkString());

    /**
     * Return a colorspace that works with the parameter profile.
     * @param colorSpaceId the ID string of the colorspace that you want to have returned
     * @param profile the profile be combined with the colorspace
     * @return the wanted colorspace, or 0 when the cs and profile can not be combined.
     */
    const KoColorSpace * colorSpace1(const PkString &colorSpaceId, const KoColorProfile *profile);
};

struct KoColorSpaceRegistry::Private::ConversionSystemInterface : public KoColorConversionSystem::RegistryInterface
{
    ConversionSystemInterface(KoColorSpaceRegistry *parentRegistry)
        : q(parentRegistry)
    {
    }

    const KoColorSpace * colorSpace(const PkString & colorModelId, const PkString & colorDepthId, const PkString &profileName) override {
        return q->d->colorSpace1<NoLockPolicy>(q->d->colorSpaceIdImpl(colorModelId, colorDepthId), profileName);
    }

    const KoColorSpaceFactory* colorSpaceFactory(const PkString &colorModelId, const PkString &colorDepthId) const override {
        return q->d->colorSpaceFactoryRegistry.get(q->d->colorSpaceIdImpl(colorModelId, colorDepthId));
    }

    PkList<const KoColorProfile *>  profilesFor(const KoColorSpaceFactory * csf) const override {
        return q->d->profileStorage.profilesFor(csf);
    }

    PkList<const KoColorSpaceFactory*> colorSpacesFor(const KoColorProfile* profile) const override {
        PkList<const KoColorSpaceFactory*> csfs;
        for (KoColorSpaceFactory* csf : q->d->colorSpaceFactoryRegistry.values()) {
            if (csf->profileIsCompatible(profile)) {
                csfs.push_back(csf);
            }
        }
        return csfs;
    }

private:
    KoColorSpaceRegistry *q {nullptr};
};

KoColorSpaceRegistry* KoColorSpaceRegistry::instance()
{
    // 原 Q_GLOBAL_STATIC 语义：对象在 init() 之前完成构造（exists() 即为真），
    // 因此 init → add → 工厂 ctor → instance() 的递归重入时能直接返回部分初始化
    // 的单例。C++ 函数局部 static 的 __cxa_guard 不允许递归重入（会抛
    // recursive_init_error），不能把 init 放进 static 初始化器；改用先置位 bool
    // 旗标再 init()，递归重入时看到 s_initDone 为真即返回。测试路径单线程，
    // 与 Q_GLOBAL_STATIC 的并发首调用保护语义有偏差（可接受的薄壳差异）。
    static KoColorSpaceRegistry s_instance;
    static bool s_initDone = false;
    if (!s_initDone) {
        s_initDone = true;
        s_instance.init();
    }
    return &s_instance;
}


void KoColorSpaceRegistry::init()
{
    d->rgbU8sRGB = 0;
    d->lab16sLAB = 0;
    d->alphaCs = 0;
    d->alphaU16Cs = 0;
#ifdef HAVE_OPENEXR
    d->alphaF16Cs = 0;
#endif
    d->alphaF32Cs = 0;

    d->conversionSystemInterface.reset(new Private::ConversionSystemInterface(this));
    d->colorConversionSystem = new KoColorConversionSystem(d->conversionSystemInterface.data());
    d->colorConversionCache = new KoColorConversionCache;

    KoColorSpaceEngineRegistry::instance()->add(new KoSimpleColorSpaceEngine());

    addProfile(new KoDummyColorProfile);

    // Create the built-in colorspaces
    PkList<KoColorSpaceFactory *> localFactories;
    localFactories
            << new KoAlphaColorSpaceFactory()
            << new KoAlphaU16ColorSpaceFactory()
           #ifdef HAVE_OPENEXR
            << new KoAlphaF16ColorSpaceFactory()
           #endif
            << new KoAlphaF32ColorSpaceFactory()
            << new KoLabColorSpaceFactory()
            << new KoRgbU8ColorSpaceFactory()
            << new KoRgbU16ColorSpaceFactory();

    for (KoColorSpaceFactory *factory : localFactories) {
        add(factory);
    }

    dbgPigment << "Loaded the following colorspaces:";
    for (const KoID& id : listKeys()) {
        dbgPigment << "\t" << id.id() << "," << id.name();
    }
}

KoColorSpaceRegistry::KoColorSpaceRegistry() : d(new Private(this))
{
    d->colorConversionSystem = nullptr;
    d->colorConversionCache = nullptr;
}

KoColorSpaceRegistry::~KoColorSpaceRegistry()
{
    delete d->colorConversionSystem;
    d->colorConversionSystem = nullptr;

    for (const KoColorSpace * cs : d->csMap) {
        cs->d->deletability = OwnedByRegistryRegistryDeletes;
        delete cs;
    }
    d->csMap.clear();

    // deleting colorspaces calls a function in the cache
    delete d->colorConversionCache;
    d->colorConversionCache = nullptr;

    // Delete the colorspace factories
    for (KoColorSpaceFactory *f : d->colorSpaceFactoryRegistry.values()) {
        d->colorSpaceFactoryRegistry.remove(f->id());
        delete f;
    }
    for (KoColorSpaceFactory *f : d->colorSpaceFactoryRegistry.doubleEntries()) {
        delete f;
    }

    delete d;
}

void KoColorSpaceRegistry::add(KoColorSpaceFactory* item)
{
    PkWriteLocker l(&d->registrylock);
    d->colorSpaceFactoryRegistry.add(item);
    d->colorConversionSystem->insertColorSpace(item);
}

void KoColorSpaceRegistry::remove(KoColorSpaceFactory* item)
{
    PkWriteLocker l(&d->registrylock);

    PkList<PkString> toremove;
    for (const KoColorSpace * cs : d->csMap) {
        if (cs->id() == item->id()) {
            toremove.push_back(d->idsToCacheName(cs->id(), cs->profile()->name()));
            cs->d->deletability = OwnedByRegistryRegistryDeletes;
        }
    }

    for (const PkString& id : toremove) {
        d->csMap.remove(id);
        // TODO: should not it delete the color space when removing it from the map ?
    }
    d->colorSpaceFactoryRegistry.remove(item->id());
}

void KoColorSpaceRegistry::addProfileAlias(const PkString& name, const PkString& to)
{
    d->profileStorage.addProfileAlias(name, to);
}

PkString KoColorSpaceRegistry::profileAlias(const PkString& name) const
{
    return d->profileStorage.profileAlias(name);
}

const KoColorProfile*  KoColorSpaceRegistry::profileByName(const PkString &name) const
{
    return d->profileStorage.profileByName(name);
}

const KoColorProfile *  KoColorSpaceRegistry::profileByUniqueId(const PkByteArray &id) const
{
    return d->profileStorage.profileByUniqueId(id);
}

PkList<const KoColorProfile *>  KoColorSpaceRegistry::profilesFor(const PkString &csID) const
{
    PkReadLocker l(&d->registrylock);
    return d->profileStorage.profilesFor(d->colorSpaceFactoryRegistry.value(csID));
}

const KoColorSpace * KoColorSpaceRegistry::colorSpace(const PkString & colorModelId, const PkString & colorDepthId, const KoColorProfile *profile)
{
    return d->colorSpace1(colorSpaceId(colorModelId, colorDepthId), profile);
}

const KoColorSpace * KoColorSpaceRegistry::colorSpace(const PkString & colorModelId, const PkString & colorDepthId, const PkString &profileName)
{
    return d->colorSpace1(colorSpaceId(colorModelId, colorDepthId), profileName);
}

const KoColorSpace * KoColorSpaceRegistry::colorSpace(const PkString & colorModelId, const PkString & colorDepthId)
{
    return d->colorSpace1(colorSpaceId(colorModelId, colorDepthId));
}

bool KoColorSpaceRegistry::profileIsCompatible(const KoColorProfile *profile, const PkString &colorSpaceId)
{
    PkReadLocker l(&d->registrylock);
    KoColorSpaceFactory *csf = d->colorSpaceFactoryRegistry.value(colorSpaceId);

    return csf ? csf->profileIsCompatible(profile) : false;
}

void KoColorSpaceRegistry::addProfileToMap(KoColorProfile *p)
{
    d->profileStorage.addProfile(p);
}

void KoColorSpaceRegistry::addProfile(KoColorProfile *p)
{
    if (!p->valid()) return;

    PkWriteLocker locker(&d->registrylock);
    if (p->valid()) {
        addProfileToMap(p);
        d->colorConversionSystem->insertColorProfile(p);
    }
}

void KoColorSpaceRegistry::addProfile(const KoColorProfile* profile)
{
    addProfile(profile->clone());
}

void KoColorSpaceRegistry::removeProfile(KoColorProfile* profile)
{
    d->profileStorage.removeProfile(profile);
    // FIXME: how about removing it from conversion system?
}

const KoColorSpace* KoColorSpaceRegistry::Private::getCachedColorSpaceImpl(const PkString & csID, const PkString & profileName) const
{
    auto it = csMap.find(idsToCacheName(csID, profileName));

    if (it != csMap.end()) {
        return it.value();
    }

    return 0;
}

PkString KoColorSpaceRegistry::Private::idsToCacheName(const PkString & csID, const PkString & profileName) const
{
    return csID + "<comb>" + profileName;
}

PkString KoColorSpaceRegistry::defaultProfileForColorSpace(const PkString &colorSpaceId) const
{
    PkReadLocker l(&d->registrylock);
    return d->defaultProfileForCsIdImpl(colorSpaceId);
}

KoColorConversionTransformation *KoColorSpaceRegistry::createColorConverter(const KoColorSpace *srcColorSpace, const KoColorSpace *dstColorSpace, KoColorConversionTransformation::Intent renderingIntent, KoColorConversionTransformation::ConversionFlags conversionFlags) const
{
    PkWriteLocker l(&d->registrylock);
    return d->colorConversionSystem->createColorConverter(srcColorSpace, dstColorSpace, renderingIntent, conversionFlags);
}

void KoColorSpaceRegistry::createColorConverters(const KoColorSpace *colorSpace, const PkList<PkPair<KoID, KoID> > &possibilities, KoColorConversionTransformation *&fromCS, KoColorConversionTransformation *&toCS) const
{
    PkWriteLocker l(&d->registrylock);
    d->colorConversionSystem->createColorConverters(colorSpace, possibilities, fromCS, toCS);
}

PkString KoColorSpaceRegistry::Private::defaultProfileForCsIdImpl(const PkString &csID)
{
    PkString defaultProfileName;

    KoColorSpaceFactory *csf = colorSpaceFactoryRegistry.value(csID);
    if (csf) {
        defaultProfileName = csf->defaultProfile();
    } else {
        dbgPigmentCSRegistry << "Unknown color space type : " << csID;
    }

    return defaultProfileName;
}

const KoColorProfile *KoColorSpaceRegistry::Private::profileForCsIdWithFallbackImpl(const PkString &csID, const PkString &profileName)
{
    const KoColorProfile *profile = 0;

    // last attempt at getting a profile, sometimes the default profile, like adobe cmyk isn't available.
    profile = profileStorage.profileByName(profileName);
    if (!profile) {
        dbgPigmentCSRegistry << "Profile not found :" << profileName;

        // first try: default
        profile = profileStorage.profileByName(defaultProfileForCsIdImpl(csID));

        if (!profile) {
            // second try: first profile in the list
            PkList<const KoColorProfile *> profiles = profileStorage.profilesFor(colorSpaceFactoryRegistry.value(csID));
            if (profiles.isEmpty() || !profiles.first()) {
                dbgPigmentCSRegistry << "Couldn't fetch a fallback profile:" << profileName;
                qWarning() << "profileForCsIdWithFallbackImpl couldn't fetch a fallback profile for " << profileName;
                return 0;
            }

            profile = profiles.first();
        }
    }

    return profile;
}

const KoColorSpace *KoColorSpaceRegistry::Private::lazyCreateColorSpaceImpl(const PkString &csID, const KoColorProfile *profile)
{
    const KoColorSpace *cs = 0;

    /*
     * We need to check again here, a thread requesting the same colorspace could've added it
     * already, in between the read unlock and write lock.
     * TODO: We also potentially changed profileName content, which means we maybe are going to
     * create a colorspace that's actually in the space registry cache, but currently this might
     * not be an issue because the colorspace should be cached also by the factory, so it won't
     * create a new instance. That being said, having two caches with the same stuff doesn't make
     * much sense.
     */
    cs = getCachedColorSpaceImpl(csID, profile->name());
    if (!cs) {
        KoColorSpaceFactory *csf = colorSpaceFactoryRegistry.value(csID);
        if (!csf) {
            qWarning() << "Unable to create color space factory for" << csID;
            return 0;
        }
        cs = csf->grabColorSpace(profile);
        if (!cs) {
            dbgPigmentCSRegistry << "Unable to create color space";
            qWarning() << "lazyCreateColorSpaceImpl was unable to create a color space for " << csID;
            return 0;
        }

        dbgPigmentCSRegistry << "colorspace count: " << csMap.count()
                             << ", adding name: " << idsToCacheName(cs->id(), cs->profile()->name())
                             << "\n\tcsID" << csID
                             << "\n\tcs->id()" << cs->id()
                             << "\n\tcs->profile()->name()" << cs->profile()->name()
                             << "\n\tprofile->name()" << profile->name();
        Q_ASSERT(cs->id() == csID);
        Q_ASSERT(cs->profile()->name() == profile->name());
        csMap[idsToCacheName(cs->id(), cs->profile()->name())] = cs;
        cs->d->deletability = OwnedByRegistryDoNotDelete;
    }

    return cs;
}

template<class LockPolicy>
const KoColorSpace * KoColorSpaceRegistry::Private::colorSpace1(const PkString &csID, const PkString &pName)
{
    PkString profileName = pName;

    const KoColorSpace *cs = 0;

    {
        typename LockPolicy::ReadLocker l(&registrylock);

        if (profileName.isEmpty()) {
            profileName = defaultProfileForCsIdImpl(csID);
        }

        if (!profileName.isEmpty()) {
            // quick attempt to fetch a cached color space
            cs = getCachedColorSpaceImpl(csID, profileName);
        }
    }

    if (!cs) {
        // slow attempt to create a color space
        typename LockPolicy::WriteLocker l(&registrylock);

        const KoColorProfile *profile =
            profileForCsIdWithFallbackImpl(csID, profileName);

        if (!profile) return 0;

        cs = lazyCreateColorSpaceImpl(csID, profile);
    }
    else {
        KIS_SAFE_ASSERT_RECOVER_NOOP(cs->id() == csID);
        KIS_SAFE_ASSERT_RECOVER_NOOP(cs->profile()->name() == profileName);
    }

    return cs;
}


const KoColorSpace * KoColorSpaceRegistry::Private::colorSpace1(const PkString &csID, const KoColorProfile *profile)
{
    if (csID.isEmpty()) {
        return 0;
    } else if (!profile) {
        return colorSpace1(csID);
    }

    const KoColorSpace *cs = 0;

    {
        PkReadLocker l(&registrylock);
        cs = getCachedColorSpaceImpl(csID, profile->name());
    }

    // the profile should have already been added to the registry by createColorProfile() method
    KIS_SAFE_ASSERT_RECOVER(profileStorage.containsProfile(profile)) {
        // warning! locking happens inside addProfile!
        q->addProfile(profile);
    }

    if (!cs) {
        // The profile was not stored and thus not the combination either
        PkWriteLocker l(&registrylock);
        KoColorSpaceFactory *csf = colorSpaceFactoryRegistry.value(csID);

        if (!csf) {
            dbgPigmentCSRegistry << "Unknown color space type :" << csf;
            return 0;
        }

        if (!csf->profileIsCompatible(profile)) {
            dbgPigmentCSRegistry << "Profile is not compatible:" << csf << profile->name();
            return 0;
        }

        cs = lazyCreateColorSpaceImpl(csID, profile);
    }

    return cs;
}

const KoColorSpace * KoColorSpaceRegistry::alpha8()
{
    if (!d->alphaCs) {
        d->alphaCs = d->colorSpace1(KoAlphaColorSpace::colorSpaceId());
    }
    Q_ASSERT(d->alphaCs);
    return d->alphaCs;
}

const KoColorSpace * KoColorSpaceRegistry::alpha16()
{
    if (!d->alphaU16Cs) {
        d->alphaU16Cs = d->colorSpace1(KoAlphaU16ColorSpace::colorSpaceId());
    }
    Q_ASSERT(d->alphaU16Cs);
    return d->alphaU16Cs;
}

#ifdef HAVE_OPENEXR
const KoColorSpace * KoColorSpaceRegistry::alpha16f()
{
    if (!d->alphaF16Cs) {
        d->alphaF16Cs = d->colorSpace1(KoAlphaF16ColorSpace::colorSpaceId());
    }
    Q_ASSERT(d->alphaF16Cs);
    return d->alphaF16Cs;
}
#endif

const KoColorSpace * KoColorSpaceRegistry::alpha32f()
{
    if (!d->alphaF32Cs) {
        d->alphaF32Cs = d->colorSpace1(KoAlphaF32ColorSpace::colorSpaceId());
    }
    Q_ASSERT(d->alphaF32Cs);
    return d->alphaF32Cs;
}

const KoColorSpace *KoColorSpaceRegistry::graya8(const PkString &profile)
{

    if (profile.isEmpty()) {
        KoColorSpaceFactory* factory = d->colorSpaceFactoryRegistry.get(GrayAColorModelID.id());
        return KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), factory->defaultProfile());
    }
    else {
        return KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), profile);
    }

}

const KoColorSpace *KoColorSpaceRegistry::graya8(const KoColorProfile *profile)
{
    if (!profile) {
        return graya8();
    }
    else {
        return KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer8BitsColorDepthID.id(), profile);
    }

}

const KoColorSpace *KoColorSpaceRegistry::graya16(const PkString &profile)
{
    if (profile.isEmpty()) {
        KoColorSpaceFactory* factory = d->colorSpaceFactoryRegistry.get(GrayAColorModelID.id());
        return KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), factory->defaultProfile());
    }
    else {
        return KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), profile);
    }

}

const KoColorSpace *KoColorSpaceRegistry::graya16(const KoColorProfile *profile)
{
    if (!profile) {
        return graya16();
    }
    else {
        return KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), Integer16BitsColorDepthID.id(), profile);
    }
}


const KoColorSpace * KoColorSpaceRegistry::rgb8(const PkString &profileName)
{
    if (profileName.isEmpty()) {
        if (!d->rgbU8sRGB) {
            d->rgbU8sRGB = d->colorSpace1(KoRgbU8ColorSpace::colorSpaceId());
        }
        Q_ASSERT(d->rgbU8sRGB);
        return d->rgbU8sRGB;
    }
    return d->colorSpace1(KoRgbU8ColorSpace::colorSpaceId(), profileName);
}

const KoColorSpace * KoColorSpaceRegistry::rgb8(const KoColorProfile * profile)
{
    if (profile == 0) {
        if (!d->rgbU8sRGB) {
            d->rgbU8sRGB = d->colorSpace1(KoRgbU8ColorSpace::colorSpaceId());
        }
        Q_ASSERT(d->rgbU8sRGB);
        return d->rgbU8sRGB;
    }
    return d->colorSpace1(KoRgbU8ColorSpace::colorSpaceId(), profile);
}

const KoColorSpace * KoColorSpaceRegistry::rgb16(const PkString &profileName)
{
    return d->colorSpace1(KoRgbU16ColorSpace::colorSpaceId(), profileName);
}

const KoColorSpace * KoColorSpaceRegistry::rgb16(const KoColorProfile * profile)
{
    return d->colorSpace1(KoRgbU16ColorSpace::colorSpaceId(), profile);
}

const KoColorSpace * KoColorSpaceRegistry::lab16(const PkString &profileName)
{
    if (profileName.isEmpty()) {
        if (!d->lab16sLAB) {
            d->lab16sLAB = d->colorSpace1(KoLabColorSpace::colorSpaceId());
        }
        return d->lab16sLAB;
    }
    return d->colorSpace1(KoLabColorSpace::colorSpaceId(), profileName);
}

const KoColorSpace * KoColorSpaceRegistry::lab16(const KoColorProfile * profile)
{
    if (profile == 0) {
        if (!d->lab16sLAB) {
            d->lab16sLAB = d->colorSpace1(KoLabColorSpace::colorSpaceId());
        }
        Q_ASSERT(d->lab16sLAB);
        return d->lab16sLAB;
    }
    return d->colorSpace1(KoLabColorSpace::colorSpaceId(), profile);
}

const KoColorProfile *KoColorSpaceRegistry::p2020G10Profile() const
{
    return profileByName("Rec2020-elle-V4-g10.icc");
}

const KoColorProfile *KoColorSpaceRegistry::p2020PQProfile() const
{
    return profileByName("High Dynamic Range UHDTV Wide Color Gamut Display (Rec. 2020) - SMPTE ST 2084 PQ EOTF");
}

const KoColorProfile *KoColorSpaceRegistry::p709G10Profile() const
{
    return profileByName("sRGB-elle-V2-g10.icc");
}

const KoColorProfile *KoColorSpaceRegistry::p709SRGBProfile() const
{
    return profileByName("sRGB-elle-V2-srgbtrc.icc");
}

const KoColorProfile *KoColorSpaceRegistry::profileFor(const PkVector<double> &colorants, ColorPrimaries colorPrimaries, TransferCharacteristics transferFunction) const
{
    if (colorPrimaries == PRIMARIES_ITU_R_BT_709_5) {
        if (transferFunction == TRC_IEC_61966_2_1) {
            return p709SRGBProfile();
        } else if (transferFunction == TRC_LINEAR) {
            return p709G10Profile();
        }
    }

    if (colorPrimaries == PRIMARIES_ITU_R_BT_2020_2_AND_2100_0) {
        if (transferFunction == TRC_ITU_R_BT_2100_0_PQ) {
            return p2020PQProfile();
        } else if (transferFunction == TRC_LINEAR) {
            return p2020G10Profile();
        }
    }

    PkList<const KoColorProfile*> list = d->profileStorage.profilesFor(colorants, colorPrimaries, transferFunction);
    if (!list.empty()) {
        return list.first();
    }

    KoColorSpaceEngine *engine = KoColorSpaceEngineRegistry::instance()->get("icc");
    if (engine) {
        return engine->getProfile(colorants, colorPrimaries, transferFunction);
    }

    return nullptr;
}

PkList<KoID> KoColorSpaceRegistry::colorModelsList(ColorSpaceListVisibility option) const
{
    PkReadLocker l(&d->registrylock);

    PkList<KoID> ids;
    PkList<KoColorSpaceFactory*> factories = d->colorSpaceFactoryRegistry.values();
    for (KoColorSpaceFactory* factory : factories) {
        if (!ids.contains(factory->colorModelId())
                && (option == AllColorSpaces || factory->userVisible())) {
            ids << factory->colorModelId();
        }
    }
    return ids;
}
PkList<KoID> KoColorSpaceRegistry::colorDepthList(const KoID& colorModelId, ColorSpaceListVisibility option) const
{
    return colorDepthList(colorModelId.id(), option);
}


PkList<KoID> KoColorSpaceRegistry::colorDepthList(const PkString & colorModelId, ColorSpaceListVisibility option) const
{
    PkReadLocker l(&d->registrylock);

    PkList<KoID> ids;
    PkList<KoColorSpaceFactory*> factories = d->colorSpaceFactoryRegistry.values();
    for (KoColorSpaceFactory* factory : factories) {
        if (!ids.contains(KoID(factory->colorDepthId()))
                && factory->colorModelId().id() == colorModelId
                && (option == AllColorSpaces || factory->userVisible())) {
            ids << factory->colorDepthId();
        }
    }
    PkList<KoID> r;

    if (ids.contains(Integer8BitsColorDepthID)) r << Integer8BitsColorDepthID;
    if (ids.contains(Integer16BitsColorDepthID)) r << Integer16BitsColorDepthID;
    if (ids.contains(Float16BitsColorDepthID)) r << Float16BitsColorDepthID;
    if (ids.contains(Float32BitsColorDepthID)) r << Float32BitsColorDepthID;
    if (ids.contains(Float64BitsColorDepthID)) r << Float64BitsColorDepthID;

    return r;
}

PkString KoColorSpaceRegistry::Private::colorSpaceIdImpl(const PkString & colorModelId, const PkString & colorDepthId) const
{
    for (auto it = colorSpaceFactoryRegistry.constBegin(); it != colorSpaceFactoryRegistry.constEnd(); ++it) {
        if (it.value()->colorModelId().id() == colorModelId && it.value()->colorDepthId().id() == colorDepthId) {
            return it.value()->id();
        }
    }
    return "";
}

PkString KoColorSpaceRegistry::colorSpaceId(const PkString & colorModelId, const PkString & colorDepthId) const
{
    PkReadLocker l(&d->registrylock);
    return d->colorSpaceIdImpl(colorModelId, colorDepthId);
}

PkString KoColorSpaceRegistry::colorSpaceId(const KoID& colorModelId, const KoID& colorDepthId) const
{
    return colorSpaceId(colorModelId.id(), colorDepthId.id());
}

KoID KoColorSpaceRegistry::colorSpaceColorModelId(const PkString & _colorSpaceId) const
{
    PkReadLocker l(&d->registrylock);

    KoColorSpaceFactory* factory = d->colorSpaceFactoryRegistry.get(_colorSpaceId);
    if (factory) {
        return factory->colorModelId();
    } else {
        return KoID();
    }
}

KoID KoColorSpaceRegistry::colorSpaceColorDepthId(const PkString & _colorSpaceId) const
{
    PkReadLocker l(&d->registrylock);

    KoColorSpaceFactory* factory = d->colorSpaceFactoryRegistry.get(_colorSpaceId);
    if (factory) {
        return factory->colorDepthId();
    } else {
        return KoID();
    }
}

const KoColorConversionSystem* KoColorSpaceRegistry::colorConversionSystem() const
{
    return d->colorConversionSystem;
}

KoColorConversionCache* KoColorSpaceRegistry::colorConversionCache() const
{
    return d->colorConversionCache;
}

const KoColorSpace* KoColorSpaceRegistry::permanentColorspace(const KoColorSpace* _colorSpace)
{
    if (_colorSpace->d->deletability != NotOwnedByRegistry) {
        return _colorSpace;
    } else if (*_colorSpace == *d->alphaCs) {
        return d->alphaCs;
    } else {
        const KoColorSpace* cs = d->colorSpace1(_colorSpace->id(), _colorSpace->profile());
        Q_ASSERT(cs);
        Q_ASSERT(*cs == *_colorSpace);
        return cs;
    }
}

PkList<KoID> KoColorSpaceRegistry::listKeys() const
{
    PkReadLocker l(&d->registrylock);
    PkList<KoID> answer;
    for (const PkString& key : d->colorSpaceFactoryRegistry.keys()) {
        answer.append(KoID(key, d->colorSpaceFactoryRegistry.get(key)->name()));
    }

    return answer;
}

struct KoColorSpaceRegistry::Private::ProfileRegistrationInterface : public KoColorSpaceFactory::ProfileRegistrationInterface
{
    ProfileRegistrationInterface(KoColorSpaceRegistry::Private *_d) : d(_d) {}

    const KoColorProfile* profileByName(const PkString &profileName) const override {
        return d->profileStorage.profileByName(profileName);
    }

    void registerNewProfile(KoColorProfile *profile) override {
        d->profileStorage.addProfile(profile);
        d->colorConversionSystem->insertColorProfile(profile);
    }

    KoColorSpaceRegistry::Private *d {nullptr};
};

const KoColorProfile* KoColorSpaceRegistry::createColorProfile(const PkString& colorModelId, const PkString& colorDepthId, const PkByteArray& rawData)
{
    PkWriteLocker l(&d->registrylock);
    KoColorSpaceFactory* factory_ = d->colorSpaceFactoryRegistry.get(d->colorSpaceIdImpl(colorModelId, colorDepthId));

    Private::ProfileRegistrationInterface interface(d);
    return factory_->colorProfile(rawData, &interface);
}

PkList<const KoColorSpace*> KoColorSpaceRegistry::allColorSpaces(ColorSpaceListVisibility visibility, ColorSpaceListProfilesSelection pSelection)
{
    PkList<const KoColorSpace*> colorSpaces;

    // TODO: thread-unsafe code: the factories might change right after the lock in released
    // HINT: used in a unittest only!

    d->registrylock.lockForRead();
    PkList<KoColorSpaceFactory*> factories = d->colorSpaceFactoryRegistry.values();
    d->registrylock.unlock();

    for (KoColorSpaceFactory* factory : factories) {
        // Don't test with ycbcr for now, since we don't have a default profile for it.
        if (factory->colorModelId().id().startsWith("Y")) continue;
        if (visibility == AllColorSpaces || factory->userVisible()) {
            if (pSelection == OnlyDefaultProfile) {
                const KoColorSpace *cs = d->colorSpace1(factory->id());
                if (cs) {
                    colorSpaces.append(cs);
                }
                else {
                    warnPigment << "Could not create colorspace for id" << factory->id() << "since there is no working default profile";
                }
            } else {
                PkList<const KoColorProfile*> profiles = KoColorSpaceRegistry::instance()->profilesFor(factory->id());
                for (const KoColorProfile * profile : profiles) {
                    const KoColorSpace *cs = d->colorSpace1(factory->id(), profile);
                    if (cs) {
                        colorSpaces.append(cs);
                    }
                    else {
                        warnPigment << "Could not create colorspace for id" << factory->id() << "and profile" << profile->name();
                    }
                }
            }
        }
    }

    return colorSpaces;
}
