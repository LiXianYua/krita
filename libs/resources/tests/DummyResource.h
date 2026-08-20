/*
 * SPDX-FileCopyrightText: 2018 boud <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef DUMMYRESOURCE_H
#define DUMMYRESOURCE_H

#include "KoResource.h"
#include <QDebug>
#include <QRandomGenerator64>
#include <KoMD5Generator.h>
#include <KisResourceTypes.h>
#include <PkAuxTypes.h>
#include <PkStream.h>

class DummyResource : public KoResource {
public:
    DummyResource(const PkString &f, const PkString &resourceType)
        : KoResource(f)
        , m_resourceType(resourceType)
    {
        QRandomGenerator64 qrg;
        QByteArray ba(1024, '0');
        for (int i = 0; i < 1024 / 8; i+=8) {
            quint64 v = qrg.generate64();
            ba[i] = v;
        }

        m_something = QString::fromUtf8(ba);

        setMD5Sum(KoMD5Generator::generateHash(PkByteArray(ba.constData(), ba.size())));

        PkImage img(512, 512, PkImage::Format_RGB32);
        img.fill(0xff0000ffU);
        setImage(img);

        // add some random metadata to the resource
        addMetaData(PkString("test_metadata"), PkVariant(toPkString(expectedMetaData())));

        setValid(true);
    }

    explicit DummyResource(const char *f)
        : DummyResource(PkString(f), ResourceType::PaintOpPresets)
    {
    }

    DummyResource(const char *f, const PkString &resourceType)
        : DummyResource(PkString(f), resourceType)
    {
    }

    DummyResource(const QString &f, const QString &resourceType)
        : DummyResource(toPkString(f), toPkString(resourceType))
    {
    }

    DummyResource(const QString &f, const PkString &resourceType)
        : DummyResource(toPkString(f), resourceType)
    {
    }

    DummyResource(const DummyResource &rhs)
        : KoResource(rhs),
          m_something(rhs.m_something),
          m_resourceType(rhs.m_resourceType)
    {
    }

    KoResourceSP clone() const override
    {
        return KoResourceSP(new DummyResource(*this));
    }

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override
    {
        Q_UNUSED(resourcesInterface);

        if (!dev->isOpen()) {
            dev->open(PkStream::ReadOnly);
        }
        QByteArray bytes;
        char buffer[4096];
        for (;;) {
            const PkStream::pk_int64 count = dev->read(buffer, sizeof(buffer));
            if (count < 0) {
                return false;
            }
            if (count == 0) {
                break;
            }
            bytes.append(buffer, static_cast<int>(count));
        }
        setSomething(QString::fromUtf8(bytes));
        setValid(true);
        return true;
    }

    bool saveToDevice(PkStream *dev) const override
    {
        if (!dev->isOpen()) {
            dev->open(PkStream::WriteOnly);
        }
        const QByteArray bytes = m_something.toUtf8();
        return dev->write(bytes.constData(), bytes.size()) == bytes.size();
    }

    void setSomething(const QString &something)
    {
        m_something = something;
        addMetaData(PkString("test_metadata"), PkVariant(toPkString(expectedMetaData())));
    }

    QString something() const
    {
        return m_something;
    }

    QString expectedMetaData() const 
    {
        return m_something.left(8);
    }

    std::pair<PkString, PkString> resourceType() const override {
        return std::make_pair(m_resourceType, PkString());
    }

private:

    static PkString toPkString(const QString &value)
    {
        const QByteArray utf8 = value.toUtf8();
        return PkString::PkFromUtf8(utf8.constData(), utf8.size());
    }

    QString m_something;
    PkString m_resourceType;
};

#endif // DUMMYRESOURCE_H
