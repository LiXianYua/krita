/*
 * This file is part of the KDE project
 * SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2016 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef KISSWATCH_H
#define KISSWATCH_H

#include "kritapigment_export.h"
#include "KoColor.h"
#include "KoColorModelStandardIds.h"
#include <PkString.h>

class PkByteArray;
class PkDataStream;

class KRITAPIGMENT_EXPORT KisSwatch
{
public:
    KisSwatch() = default;
    KisSwatch(const KoColor &color, const PkString &name = PkString());

public:
    PkString name() const { return m_name; }
    void setName(const PkString &name);

    PkString id() const { return m_id; }
    void setId(const PkString &id);

    KoColor color() const { return m_color; }
    void setColor(const KoColor &color);

    bool spotColor() const { return m_spotColor; }
    void setSpotColor(bool spotColor);

    bool isValid() const { return m_valid; }

    void writeToStream(PkDataStream& stream, const PkString& groupName, int originalRow , int originalColumn);
    static KisSwatch fromByteArray(PkByteArray& data, PkString &groupName, int &originalRow, int &originalColumn);
    static KisSwatch fromByteArray(PkByteArray &data);

public:
    bool operator==(const KisSwatch& rhs) const {
        return m_color == rhs.m_color && m_name == rhs.m_name;
    }

private:
    KoColor m_color;
    PkString m_name;
    PkString m_id;
    bool m_spotColor {false};
    bool m_valid {false};
};

#endif // KISSWATCH_H
