/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2002 David Faure <faure@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef koDirectoryStore_h
#define koDirectoryStore_h

#include "KoStore.h"

class KoDirectoryStore : public KoStore
{
public:
    KoDirectoryStore(const PkString& path, Mode _mode, bool writeMimetype);
    ~KoDirectoryStore() override;
protected:
    void init();
    bool openWrite(const PkString &name) override {
        return openReadOrWrite(name, PkStream::WriteOnly);
    }
    bool openRead(const PkString &name) override {
        return openReadOrWrite(name, PkStream::ReadOnly);
    }
    bool closeRead() override {
        return true;
    }
    bool closeWrite() override {
        return true;
    }
    bool enterRelativeDirectory(const PkString &dirName) override;
    bool enterAbsoluteDirectory(const PkString &path) override;
    bool fileExists(const PkString &absPath) const override;

    bool openReadOrWrite(const PkString &name, PkStream::OpenModeFlag ioMode);
private:
    // Path to base directory (== the ctor argument)
    PkString m_basePath;

    // Path to current directory
    PkString m_currentPath;
};

#endif
