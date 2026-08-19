/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2022 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoID.h"

KoID::TranslatedString::TranslatedString(
    const boost::optional<KLocalizedString> &source)
    : PkString(!source->isEmpty() ? source->toString() : PkString())
{
}

KoID::TranslatedString::TranslatedString(const PkString &value)
    : PkString(value)
{
}

KoID::KoIDPrivate::KoIDPrivate(PkString _id, const KLocalizedString &_name)
    : id(std::move(_id))
    , name(_name)
{
}

KoID::KoIDPrivate::KoIDPrivate(PkString _id, const PkString &_name)
    : id(std::move(_id))
    , name(StorageType::init_value_tag(), _name)
{
}

KoID::KoID()
    : m_d(new KoIDPrivate(PkString(), PkString()))
{
}

KoID::KoID(const PkString &id, const PkString &name)
    : m_d(new KoIDPrivate(id, name))
{
}

KoID::KoID(const PkString &id, const KLocalizedString &name)
    : m_d(new KoIDPrivate(id, name))
{
}

KoID::KoID(const KoID &rhs)
    : m_d(rhs.m_d)
{
}

KoID &KoID::operator=(const KoID &rhs)
{
    if (this != &rhs) {
        m_d = rhs.m_d;
    }
    return *this;
}

PkString KoID::id() const
{
    return m_d->id;
}

PkString KoID::name() const
{
    return *m_d->name;
}
