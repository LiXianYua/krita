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
        // 对齐原 file API 的 open(WriteOnly) 隐式截断语义：PkFileStream::open
        // 只认显式 Truncate 位才 O_TRUNC（见 PkFileStream.cpp），不加的话同名
        // 文件先长后短覆盖写会残留旧内容尾部（数据损坏）。
        return openReadOrWrite(name, PkStream::WriteOnly | PkStream::Truncate);
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

    // OpenMode（而非 OpenModeFlag）：openWrite 要传 WriteOnly|Truncate，枚举按位或
    // 得到 int，参数收 OpenModeFlag 会编不过；且 PkStream::open() 本身就收 OpenMode。
    bool openReadOrWrite(const PkString &name, PkStream::OpenMode ioMode);
private:
    // Path to base directory (== the ctor argument)
    PkString m_basePath;

    // Path to current directory
    PkString m_currentPath;
};

#endif
