/*
 *  SPDX-FileCopyrightText: 2020 Saurabh Kumar <saurabhk660@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "StoryboardItem.h"

#include <PkXmlElement.h>
#include <PkXmlDocument.h>

#include <string>

#include "kis_pointer_utils.h"

StoryboardItem::StoryboardItem()
    : m_childData()
{}

StoryboardItem::StoryboardItem(const StoryboardItem& other)
    : std::enable_shared_from_this<StoryboardItem>()
    , m_childData()
{
    cloneChildrenFrom(other);
}

StoryboardItem::~StoryboardItem()
{
    m_childData.clear();
}

void StoryboardItem::appendChild(PkVariant data)
{
    PkSharedPointer<StoryboardChild> child = toQShared( new StoryboardChild(data) );
    if (!weak_from_this().expired()) {
        child->setParent(shared_from_this());
    }
    m_childData.append(child);
}

void StoryboardItem::cloneChildrenFrom(const StoryboardItem& other)
{
    m_childData.clear();
    for (int i = 0; i < other.m_childData.count(); i++) {
        PkSharedPointer<StoryboardChild> child = toQShared( new StoryboardChild(*other.m_childData.at(i)));
        if (!weak_from_this().expired()) {
            child->setParent(shared_from_this());
        }
        m_childData.append(child);
    }
}

void StoryboardItem::insertChild(int row, PkVariant data)
{
    PkSharedPointer<StoryboardChild> child = toQShared( new StoryboardChild(data) );
    if (!weak_from_this().expired()) {
        child->setParent(shared_from_this());
    }
    m_childData.insert(row, child);
}

void StoryboardItem::removeChild(int row)
{
    m_childData.removeAt(row);
}

void StoryboardItem::moveChild(int from, int to)
{
    m_childData.move(from, to);
}

int StoryboardItem::childCount() const
{
    return m_childData.count();
}

PkSharedPointer<StoryboardChild> StoryboardItem::child(int row) const
{
    if (row < 0 || row >= m_childData.size()) {
        return nullptr;
    }
    return m_childData.at(row);
}

PkXmlElement StoryboardItem::toXML(PkXmlDocument doc)
{
    PkXmlElement itemElement = doc.createElement("storyboarditem");

    int frame = child(FrameNumber)->data().value<ThumbnailData>().frameNum.toInt();
    itemElement.setAttribute("frame", PkString(std::to_string(frame).c_str()));
    itemElement.setAttribute("item-name", child(ItemName)->data().toString());
    itemElement.setAttribute("duration-second", PkString(std::to_string(child(DurationSecond)->data().toInt()).c_str()));
    itemElement.setAttribute("duration-frame", PkString(std::to_string(child(DurationFrame)->data().toInt()).c_str()));

    for (int i = Comments; i < childCount(); i++) {
        CommentBox comment = child(i)->data().value<CommentBox>();
        PkXmlElement commentElement = doc.createElement("comment");

        commentElement.setAttribute("content", comment.content.toString());
        commentElement.setAttribute("scroll-value", PkString(std::to_string(comment.scrollValue.toInt()).c_str()));

        itemElement.appendChild(commentElement);
    }

    return itemElement;
}

void StoryboardItem::loadXML(const PkXmlElement &itemNode)
{
    ThumbnailData thumbnail;
    thumbnail.frameNum = itemNode.attribute("frame").toInt();
    appendChild(PkVariant::fromValue<ThumbnailData>(thumbnail));
    appendChild(itemNode.attribute("item-name"));
    appendChild(itemNode.attribute("duration-second").toInt());
    appendChild(itemNode.attribute("duration-frame").toInt());

    for (PkXmlElement commentNode = itemNode.firstChildElement(); !commentNode.isNull(); commentNode = commentNode.nextSiblingElement()) {
        if (commentNode.nodeName().toUpper() != "COMMENT") continue;

        CommentBox comment;
        if (commentNode.hasAttribute("content")) {
            comment.content = commentNode.attribute("content");
        }
        if (commentNode.hasAttribute("scroll-value")) {
            comment.scrollValue = commentNode.attribute("scroll-value");
        }
        appendChild(PkVariant::fromValue<CommentBox>(comment));
    }
}

StoryboardItemList StoryboardItem::cloneStoryboardItemList(const StoryboardItemList &list)
{
    StoryboardItemList clonedList;
    for (auto i = 0; i < list.count(); i++) {
        StoryboardItemSP item = toQShared( new StoryboardItem(*list.at(i)) );
        item->cloneChildrenFrom(*list.at(i));
        clonedList.append(item);
    }
    return clonedList;
}
