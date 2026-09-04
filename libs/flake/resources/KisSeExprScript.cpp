/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2020 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include <FlakeDebug.h>
#include <KoStore.h>
#include <KoStoreDevice.h>
#include <PkMemoryStream.h>
#include <QDir>
#include <QFileInfo>
#include <QTextDecoder>
#include <kis_assert.h>

#include "KisSeExprScript.h"

struct KisSeExprScript::Private {
    PkString script;
    PkByteArray data;
};

KisSeExprScript::KisSeExprScript(const PkString &filename)
    : KoResource(toPkString(filename))
    , d(new Private)
{
    PkString n = toQString(name()).replace("_", " ");
    setName(toPkString(n));
    if (n.endsWith(toQString(defaultFileExtension()))) {
        const QFileInfo f(n);
        setName(toPkString(f.completeBaseName()));
    }
}

KisSeExprScript::KisSeExprScript(const PkImage &image, const PkString &script, const PkString &name, const PkString &folderName)
    : KoResource(PkString())
    , d(new Private)
{
    setScript(script);
    setImage(toPkImage(image));
    setName(toPkString(name));

    QFileInfo fileInfo(folderName + QDir::separator() + name + toQString(defaultFileExtension()));

    int i = 1;
    while (fileInfo.exists()) {
        fileInfo.setFile(folderName + QDir::separator() + name + PkString::number(i) + toQString(defaultFileExtension()));
        i++;
    }

    setFilename(toPkString(fileInfo.filePath()));
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

    d->data = pkReadAllAsQByteArray(dev);

    // TODO: test
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(d->data.size() != 0, false);

    if (filename().isEmpty()) {
        warnFlake << "Cannot load SeExpr script" << name() << ", there is no filename set";
        return false;
    }

    if (d->data.isNull()) {
        PkFileStream file(toQString(filename()));
        if (file.size() == 0) {
            warnFlake << "Cannot load SeExpr script" << name() << "there is no data available";
            return false;
        }

        if (!file.open(PkStream::ReadOnly)) {
            warnFlake << "Cannot load SeExpr script" << name() << ":" << file.errorString();
            return false;
        }
        d->data = file.readAll();
        file.close();
    }

    PkMemoryStream buf(&d->data);
    buf.open(PkMemoryStream::ReadOnly);
    PkDeviceStream bufStream;
    bufStream.attach(&buf);

    PkScopedPointer<KoStore> store(KoStore::createStore(&bufStream, KoStore::Read, toPkByteArray("application/x-krita-seexpr-script"), KoStore::Zip));
    if (!store || store->bad())
        return false;

    bool storeOpened = store->open("script.se");
    if (!storeOpened) {
        return false;
    }

    d->script = PkString(toQByteArray(store->read(store->size())));
    store->close();

    if (store->open("preview.png")) {
        KoStoreDevice previewDev(store.data());
        previewDev.open(PkStream::ReadOnly);
        PkStreamIoDevice previewIo;
        previewIo.attach(&previewDev);

        PkImage preview = PkImage();
        preview.load(&previewIo, "PNG");
        setImage(toPkImage(preview));

        (void)store->close();
    }

    buf.close();

    setValid(true);
    setDirty(false);

    return true;
}

bool KisSeExprScript::saveToDevice(PkStream *dev) const
{
    KoStore *store(KoStore::createStore(dev, KoStore::Write, toPkByteArray("application/x-krita-seexpr-script"), KoStore::Zip));
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

    KoStoreDevice previewDev(store);
    previewDev.open(PkStream::WriteOnly);
    PkStreamIoDevice previewIo;
    previewIo.attach(&previewDev);

    toQImage(image()).save(&previewIo, "PNG");
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
    return toPkString(toQString(KoResource::name()).replace("_", " "));
}
