/*
 * This file is part of the KDE project
 * SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2016 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <PkXmlCompat.h>

#include <resources/KisSwatch.h>

#include <PkDataStream.h>
#include <PkStream.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>

KisSwatch::KisSwatch(const KoColor &color, const PkString &name)
    : m_color(color)
    , m_name(name)
    , m_valid(true)
{ }

void KisSwatch::setName(const PkString &name)
{
    m_name = name;
    m_valid = true;
}

void KisSwatch::setId(const PkString &id)
{
    m_id = id;
    m_valid = true;
}

void KisSwatch::setColor(const KoColor &color)
{
    m_color = color;
    m_valid = true;
}

void KisSwatch::setSpotColor(bool spotColor)
{
    m_spotColor = spotColor;
    m_valid = true;
}

void KisSwatch::writeToStream(PkDataStream &stream, const PkString& groupName, int originalRow, int originalColumn)
{
    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("Color");
    root.setAttribute("bitdepth", color().colorSpace()->colorDepthId().id());
    doc.appendChild(root);
    color().toXML(doc, root);

    stream << name() << id() << spotColor()
           << originalRow << originalColumn
           << groupName << doc.toString();
}

KisSwatch KisSwatch::fromByteArray(PkByteArray &data, PkString &oldGroupName, int &originalRow, int &originalColumn)
{
    PkDataStream stream(&data, PkStream::ReadOnly);
    KisSwatch s;
    PkString name, id;
    bool spotColor;
    PkString colorXml;

    // PkDataStream 没有 atEnd()；status 一旦离开 Ok 只增不改（setStatus 仅在
    // 当前为 Ok 时写入）。所以循环条件用 status == Ok，每批读完再验一次：
    // 整批成功 status 仍为 Ok；读到流尾时首个字段读取置 ReadPastEnd，后续字段
    // 短路，整批无效，break 丢弃。空输入 → 首读即 ReadPastEnd → 零次处理，
    // 行为与原 Qt 数据流的 atEnd() 版本一致。
    while (stream.status() == PkDataStream::Ok) {
        stream >> name >> id >> spotColor
                >> originalRow >> originalColumn
                >> oldGroupName
                >> colorXml;
        if (stream.status() != PkDataStream::Ok) {
            break;
        }

        s.setName(name);
        s.setId(id);
        s.setSpotColor(spotColor);

        PkXmlDocument doc;
        doc.setContent(colorXml);
        PkXmlElement e = doc.documentElement();
        PkXmlElement c = e.firstChildElement();
        if (!c.isNull()) {
            PkString colorDepthId = c.attribute("bitdepth", Integer8BitsColorDepthID.id());
            s.setColor(KoColor::fromXML(c, colorDepthId));
        }
    }

    return s;
}

KisSwatch KisSwatch::fromByteArray(PkByteArray &data)
{
    PkString s;
    int x, y;
    return fromByteArray(data, s, y, x);
}
