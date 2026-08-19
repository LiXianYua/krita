/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2002, 2006 David Faure <faure@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoDirectoryStore.h"
#include "KoStore_p.h"

#include "PkFileStream.h"
#include <StoreDebug.h>

#include <cassert>
#include <filesystem>
#include <system_error>

// HMMM... I used the file-system backend here; maybe this should be made network transparent?

KoDirectoryStore::KoDirectoryStore(const PkString& path, Mode mode, bool writeMimetype)
    : KoStore(mode, writeMimetype)
    , m_basePath(path)
{
    init();
}

KoDirectoryStore::~KoDirectoryStore()
{
}

void KoDirectoryStore::init()
{
    KoStorePrivate *d = d_func();

    if (m_basePath.isEmpty() || m_basePath.at(m_basePath.size() - 1) != u'/')
        m_basePath += PkString("/");
    m_currentPath = m_basePath;

    std::error_code ec;
    if (std::filesystem::is_directory(m_basePath.PkToUtf8(), ec)) {
        d->good = true;
        return;
    }
    // Dir doesn't exist. If reading -> error. If writing -> create.
    if (d->mode == Write && std::filesystem::create_directories(m_basePath.PkToUtf8(), ec)) {
        debugStore << "KoDirectoryStore::init Directory created:" << m_basePath;
        d->good = true;
    }
}

bool KoDirectoryStore::openReadOrWrite(const PkString& name, PkStream::OpenModeFlag iomode)
{
    KoStorePrivate *d = d_func();

    int pos = -1;
    for (int i = name.size() - 1; i >= 0; --i) {
        if (name.at(i) == u'/') {
            pos = i;
            break;
        }
    }
    if (pos != -1) { // there are subdirs in the name -> maybe need to create them, when writing
        pushDirectory(); // remember where we were
        enterAbsoluteDirectory(PkString());
        bool ret = enterDirectory(name.left(pos));
        popDirectory();
        if (!ret)
            return false;
    }
    d->stream = new PkFileStream(m_basePath + name);
    if (!d->stream->open(iomode)) {
        delete d->stream;
        d->stream = 0;
        return false;
    }
    if (iomode == PkStream::ReadOnly)
        d->size = d->stream->size();
    return true;
}

bool KoDirectoryStore::enterRelativeDirectory(const PkString& dirName)
{
    m_currentPath += dirName;
    if (m_currentPath.isEmpty() || m_currentPath.at(m_currentPath.size() - 1) != u'/')
        m_currentPath += PkString("/");

    std::error_code ec;
    if (std::filesystem::is_directory(m_currentPath.PkToUtf8(), ec))
        return true;
    // Dir doesn't exist. If reading -> error. If writing -> create.
    if (mode() == Write) {
        // The directory to create is the full target path minus the trailing
        // slash (equivalent of the old "origDir.mkdir(dirName)").
        PkString dirTarget = m_currentPath;
        if (dirTarget.size() > 0 && dirTarget.at(dirTarget.size() - 1) == u'/')
            dirTarget = dirTarget.left(dirTarget.size() - 1);
        if (std::filesystem::create_directory(dirTarget.PkToUtf8(), ec)) {
            debugStore << "Created" << dirName;
            return true;
        }
    }
    return false;
}

bool KoDirectoryStore::enterAbsoluteDirectory(const PkString& path)
{
    m_currentPath = m_basePath + path;
    std::error_code ec;
    const bool exists = std::filesystem::is_directory(m_currentPath.PkToUtf8(), ec);
    assert(exists);   // We've been there before, therefore it must exist.
    return exists;
}

bool KoDirectoryStore::fileExists(const PkString& absPath) const
{
    debugStore << "KoDirectoryStore::fileExists" << m_basePath + absPath;
    std::error_code ec;
    return std::filesystem::exists((m_basePath + absPath).PkToUtf8(), ec);
}
