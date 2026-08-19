/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef koStoreDevice_h
#define koStoreDevice_h

#include "KoStore.h"
#include <kritastore_export.h>


/**
 * This class implements a PkStream around KoStore, so that
 * it can be used to create a document tree from it, to be written or read
 * using stream serialization or text streaming
 */
class KRITASTORE_EXPORT KoStoreDevice : public PkStream
{
public:
    /// Note: KoStore::open() should be called before calling this.
    explicit KoStoreDevice(KoStore * store) : m_store(store) {
        // calligra-1.x behavior compat: a KoStoreDevice is automatically open
        setOpenMode(m_store->mode() == KoStore::Read ? PkStream::ReadOnly : PkStream::WriteOnly);
    }
    ~KoStoreDevice() override;

    bool isSequential() const override {
        return true;
    }

    bool open(PkStream::OpenMode m) override {
        setOpenMode(m);
        if (m & PkStream::ReadOnly)
            return (m_store->mode() == KoStore::Read);
        if (m & PkStream::WriteOnly)
            return (m_store->mode() == KoStore::Write);
        return false;
    }
    void close() override {}

    PkStream::pk_int64 size() const override {
        if (m_store->mode() == KoStore::Read)
            return m_store->size();
        else
            return 0xffffffff;
    }

    // See PkStream
    PkStream::pk_int64 pos() const override {
        return m_store->pos();
    }
    bool seek(PkStream::pk_int64 pos) override {
        return m_store->seek(pos);
    }
    bool atEnd() const override {
        return m_store->atEnd();
    }

protected:
    KoStore *m_store;

    PkStream::pk_int64 readData(char *data, PkStream::pk_int64 maxlen) override {
        return m_store->read(data, maxlen);
    }

    PkStream::pk_int64 writeData(const char *data, PkStream::pk_int64 len) override {
        return m_store->write(data, len);
    }

};

#endif
