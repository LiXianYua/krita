/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2022 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KO_ID_H_
#define _KO_ID_H_

#include <PkDebug.h>
#include <type_traits>
#include <PkString.h>
#include <PkSharedPointer.h>

#include <utility>

#include "kritaglobal_export.h"

/**
 * A KoID is a combination of a user-visible string and a string that uniquely
 * identifies a given resource across languages.
 */
class KRITAGLOBAL_EXPORT KoID
{
private:
    struct KoIDPrivate {
        KoIDPrivate(PkString _id, const PkString &_name);

        PkString id;
        PkString name;
    };

public:
    KoID();

    /**
     * Construct a KoID with the given id, and name. The id is the untranslated
     * official name of the resource; name is the human-visible string used in
     * the UI.
     */
    explicit KoID(const PkString &id, const PkString &name = PkString());

    KoID(const KoID &rhs);

    KoID &operator=(const KoID &rhs);

    PkString id() const;

    PkString name() const;

    friend inline bool operator==(const KoID &, const KoID &);
    friend inline bool operator!=(const KoID &, const KoID &);
    friend inline bool operator<(const KoID &, const KoID &);
    friend inline bool operator>(const KoID &, const KoID &);

    static bool compareNames(const KoID &id1, const KoID &id2)
    {
        return id1.name() < id2.name();
    }

private:
    PkSharedPointer<KoIDPrivate> m_d;
};

inline bool operator==(const KoID &v1, const KoID &v2)
{
    return v1.m_d == v2.m_d || v1.m_d->id == v2.m_d->id;
}

inline bool operator!=(const KoID &v1, const KoID &v2)
{
    return !(v1 == v2);
}

inline bool operator<(const KoID &v1, const KoID &v2)
{
    return v1.m_d->id < v2.m_d->id;
}

inline bool operator>(const KoID &v1, const KoID &v2)
{
    return v2.m_d->id < v1.m_d->id;
}

inline PkDebug operator<<(PkDebug dbg, const KoID &id)
{
    dbg.nospace() << id.name() << " (" << id.id() << " )";

    return dbg.space();
}

#endif
