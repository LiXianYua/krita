/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2002 Werner Trobin <trobin@kde.org>
   SPDX-FileCopyrightText: 2008 David Faure <faure@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __koStore_p_h_
#define __koStore_p_h_

#include "KoStore.h"
#include "PkString.h"
#include "PkStringList.h"
#include "PkStack.h"

class PkStream;

class KoStorePrivate
{
public:
    explicit KoStorePrivate(KoStore *qq, KoStore::Mode _mode, bool _writeMimetype)
        : q(qq),
          mode(_mode),
          size(0),
          stream(nullptr),
          isOpen(false),
          good(false),
          finalized(false),
          writeMimetype(_writeMimetype),
          substituteThis("__TEXT"),
          substituteWith("Text")
    {
    }
    virtual ~KoStorePrivate()
    {
        delete stream;
    }

    PkString toExternalNaming(const PkString &internalNaming) const;
    bool enterDirectoryInternal(const PkString &directory);
    bool extractFile(const PkString &sourceName, PkStream &buffer);

    /// The store this private class belongs to
    KoStore *q;
    /// Name of the file given to the constructor, when a filename was given
    PkString localFileName;
    /// The mode of the store
    KoStore::Mode mode;
    /// Store the filenames (with full path inside the archive) when writing, to avoid duplicates
    PkStringList filesList;
    /// The "current directory" (path), using internal naming. Empty means root.
    PkStringList currentPath;
    /// Current filename (between an open() and a close())
    PkString fileName;
    /// Current size of the file named m_sName
    PkStream::pk_int64 size;
    /// The stream for the current read or write operation, opened by openRead/openWrite
    PkStream *stream;
    /// True while a file is open (between open() and close())
    bool isOpen;
    /// True if everything is fine, false if an error occurred. Must be set by the constructor.
    bool good;
    /// Set by finalize(); call it only once.
    bool finalized;
    /// Stack of saved directory positions (with pushDirectory/popDirectory)
    PkStack<PkString> directoryStack;
    /// Whether the store should be a "valid" store which contains a mimetype
    bool writeMimetype;
    /// Name of the files to be substituted
    PkString substituteThis;
    /// Name of the files to be used as replacement
    PkString substituteWith;
};

#endif
