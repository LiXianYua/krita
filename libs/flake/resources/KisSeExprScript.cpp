/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <FlakeDebug.h>
#include <KoStore.h>
#include <KoStoreDevice.h>
#include <PkFileStream.h>
#include <PkMemoryStream.h>
#include <PkScopedPointer.h>
#include <kis_assert.h>

#include "shapes/ImageShapePngData.h"

#include <filesystem>

#include "KisSeExprScript.h"

struct KisSeExprScript::Private {
    PkString script;
    PkByteArray data;
};

KisSeExprScript::KisSeExprScript(const PkString &filename)
    : KoResource(filename)
    , d(new Private)
{
    PkString n = name();
    n.replace("_", " ");
    setName(n);
    if (n.endsWith(defaultFileExtension())) {
        const std::string stem = std::filesystem::u8path(n.PkToUtf8()).stem().u8string();
        setName(PkString::PkFromUtf8(stem.data(), static_cast<int>(stem.size())));
    }
}

KisSeExprScript::KisSeExprScript(const PkImage &image, const PkString &script, const PkString &name, const PkString &folderName)
    : KoResource(PkString())
    , d(new Private)
{
    setScript(script);
    setImage(image);
    setName(name);

    const std::filesystem::path folder = std::filesystem::u8path(folderName.PkToUtf8());
    std::filesystem::path filePath =
        folder / std::filesystem::u8path((name + defaultFileExtension()).PkToUtf8());

    int i = 1;
    while (std::filesystem::exists(filePath)) {
        filePath = folder / std::filesystem::u8path(
            (name + PkString::number(i) + defaultFileExtension()).PkToUtf8());
        i++;
    }

    const std::string nativePath = filePath.u8string();
    setFilename(PkString::PkFromUtf8(nativePath.data(), static_cast<int>(nativePath.size())));
}

KisSeExprScript::KisSeExprScript(KisSeExprScript *rhs)
    : KisSeExprScript(*rhs)
{
}

KisSeExprScript::KisSeExprScript(const KisSeExprScript &rhs)
    : KoResource(rhs)
    , d(new Private)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(isDirty() == rhs.isDirty());
    // only valid if we could clone the settings
    setScript(rhs.script());
    setValid(rhs.valid());
}

KisSeExprScript::~KisSeExprScript()
{
    delete d;
}

bool KisSeExprScript::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(resourcesInterface);

    if (!dev->isOpen())
        dev->open(PkStream::ReadOnly);

    d->data = dev->readAll();

    // TODO: test
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(d->data.size() != 0, false);

    if (filename().isEmpty()) {
        warnFlake << "Cannot load SeExpr script" << name() << ", there is no filename set";
        return false;
    }

    PkMemoryStream buf;
    buf.open(PkStream::WriteOnly);
    buf.write(d->data.constData(), static_cast<PkStream::pk_int64>(d->data.size()));
    buf.close();
    buf.open(PkStream::ReadOnly);

    PkScopedPointer<KoStore> store(KoStore::createStore(
        &buf, KoStore::Read, PkByteArray("application/x-krita-seexpr-script"), KoStore::Zip));
    if (!store || store->bad())
        return false;

    bool storeOpened = store->open("script.se");
    if (!storeOpened) {
        return false;
    }

    const PkByteArray scriptData = store->read(store->size());
    d->script = PkString::fromUtf8(scriptData.constData(), static_cast<int>(scriptData.size()));
    store->close();

    if (store->open("preview.png")) {
        const PkByteArray pngData = store->read(store->size());
        setImage(ImageShapePngData::decodePng(pngData));

        (void)store->close();
    }

    buf.close();

    setValid(true);
    setDirty(false);

    return true;
}

bool KisSeExprScript::saveToDevice(PkStream *dev) const
{
    KoStore *store(KoStore::createStore(
        dev, KoStore::Write, PkByteArray("application/x-krita-seexpr-script"), KoStore::Zip));
    if (!store || store->bad())
        return false;

    if (!store->open("script.se")) {
        return false;
    }

    KoStoreDevice storeDev(store);
    storeDev.open(PkStream::WriteOnly);

    const PkByteArray scriptUtf8 = d->script.toUtf8();
    storeDev.write(scriptUtf8.constData(), scriptUtf8.size());

    if (!store->close()) {
        return false;
    }

    if (!store->open("preview.png")) {
        return false;
    }

    const PkByteArray pngBytes =
        ImageShapePngData::decodeBase64(ImageShapePngData::encodeBase64(image()));

    KoStoreDevice previewDev(store);
    previewDev.open(PkStream::WriteOnly);
    previewDev.write(pngBytes.constData(), static_cast<PkStream::pk_int64>(pngBytes.size()));
    if (!store->close()) {
        return false;
    }

    return store->finalize();
}

std::pair<PkString, PkString> KisSeExprScript::resourceType() const
{
    return std::pair<PkString, PkString>(ResourceType::SeExprScripts, PkString());
}

PkString KisSeExprScript::defaultFileExtension() const
{
    return PkString(".kse");
}

PkString KisSeExprScript::script() const
{
    return d->script;
}

void KisSeExprScript::setScript(const PkString &script)
{
    d->script = script;
}

KoResourceSP KisSeExprScript::clone() const
{
    return KoResourceSP(new KisSeExprScript(*this));
}

PkString KisSeExprScript::name() const
{
    PkString result = KoResource::name();
    result.replace("_", " ");
    return result;
}
