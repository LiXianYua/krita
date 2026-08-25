/*
 *  SPDX-FileCopyrightText: 2020 Saurabh Kumar <saurabhk660@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STORYBOARD_ITEM
#define STORYBOARD_ITEM

#include <PkVariant.h>
#include <PkList.h>
#include <PkSharedPointer.h>
#include <PkImage.h>

#include <memory>

#include "kritaimpex_export.h"
#include "kis_types.h"

//each storyboardItem contains pointer to child data
class StoryboardItem;
class PkXmlDocument;
class PkXmlElement;

/**
 * @struct Comment
 * @brief This class is a simple combination of two variables.
 * It stores the name and visibility of comments. It is used in
 * @c CommentModel and @c StoryboardModel.
 */
class StoryboardComment
{
public:
    StoryboardComment() = default;

    PkString name;
    bool visibility = true;
};

/**
 * @class CommentBox
 * @brief This class is a simple combination of two PkVariants.
 * It can be converted to and from PkVariant type and
 * is used in StoryboardModel.
 */
class CommentBox
{
public:
    CommentBox()
    : content("")
    , scrollValue(0)
    {}
    CommentBox(const CommentBox& other)
    : content(other.content)
    , scrollValue(other.scrollValue)
    {}
    ~CommentBox()
    {}

    /**
     * @brief the text content of the Comment
     */
    PkVariant content;
    /**
     * @brief the value of the scroll bar of the comment scrollbar
     */
    PkVariant scrollValue;
};


/**
 * @class ThumbnailData
 * @brief This class is a simple combination of two PkVariants.
 * It can be converted to and from PkVariant type and
 * is used in StoryboardModel.
 */
class ThumbnailData
{
public:
    ThumbnailData()
    : frameNum("")
    , pixmap(PkVariant::fromValue<PkImage>(PkImage()))
    {}
    ThumbnailData(const ThumbnailData& other)
    : frameNum(other.frameNum)
    , pixmap(other.pixmap)
    {}
    ~ThumbnailData()
    {}

    /**
     * @brief the frame number corresponding to this item
     * in the timeline docker
     */
    PkVariant frameNum;

    /**
     * @brief a scaled down thumbnail version of the frame
     */
    PkVariant pixmap;
};

/**
 * @class StoryboardChild
 * @brief This class makes up the StoryboardItem
 * class. It consists of pointer to its parent item
 * and the data stored as PkVariant.
 */
class StoryboardChild
{
public:
    StoryboardChild(PkVariant data)
        : m_data(data)
    {}

    StoryboardChild(const StoryboardChild &rhs)
        : m_data(rhs.m_data)
    {}

    // parent 链用 std::shared_ptr/std::weak_ptr：父指针来自
    // std::enable_shared_from_this（StoryboardItem 的基类），产出 std::shared_ptr；
    // PkSharedPointer 无公开的 std::shared_ptr 桥（fromShared 私有），无法在本锁内
    // 转成 PkSharedPointer。弱引用存父，跟踪的是真实所有权控制块，生命周期语义与
    // 原 Qt 弱指针一致。
    std::shared_ptr<StoryboardItem> parent()
    {
        return m_parentItem.lock();
    }

    void setParent(std::shared_ptr<StoryboardItem> parent)
    {
        m_parentItem = parent;
    }

    PkVariant data()
    {
        return m_data;
    }
    void setData(PkVariant value)
    {
        m_data = value;
    }

private:
    PkVariant m_data;
    std::weak_ptr<StoryboardItem> m_parentItem;
};

/**
 * @class StoryboardItem
 * @brief This class stores a list of StoryboardChild objects
 * and provides functionality to manipulate the list. Specific
 * item type must be stored at specific indices
 * @param childType enum for the indices and corresponding data type to be stored.
 */
class KRITAIMPEX_EXPORT StoryboardItem : public std::enable_shared_from_this<StoryboardItem>
{
public:
    explicit StoryboardItem();
    StoryboardItem(const StoryboardItem& other);
    ~StoryboardItem();

    void appendChild(PkVariant data);
    void cloneChildrenFrom(const StoryboardItem &other);
    void insertChild(int row, PkVariant data = PkVariant());
    void removeChild(int row);
    void moveChild(int from, int to);
    int childCount() const;
    PkSharedPointer<StoryboardChild> child(int row) const;

    PkXmlElement toXML(PkXmlDocument doc);
    void loadXML(const PkXmlElement &itemNode);

    static StoryboardItemList cloneStoryboardItemList(const StoryboardItemList &list);


    /**
     * @enum childType
     * @brief This enum defines the data type to be stored at particular indices
     * @param FrameNumber Store the frame number at index 0. Data type stored here should be @c ThumbnailData.
     * @param ItemName Store the item name at index 1. Data type stored here should be @c string.
     * @param DurationSecond Store the duration in second at index 2. Data type should be @c int.
     * @param DurationFrame Store the duration in frame at index 3. Data type should be @c int.
     * @param Comments Store the comments at indices @a greater_than_or_equal_to to index 4. Data type should be @c CommentBox.
     */
    enum childType{

        /**
         * @brief Store the frame number at index 0. Data type stored here should be @c ThumbnailData
         */
        FrameNumber,
        /**
         * @brief Store the item name at index 1. Data type stored here should be @c string.
         */
        ItemName,
        /**
         * @brief Store the duration in second at index 2. Data type stored here should be @c int.
         */
        DurationSecond,
        /**
         * @brief Store the duration in frame at index 3. Data type stored here should be @c int.
         */
        DurationFrame,
        /**
         * @brief Store the comments at indices @a greater_than_or_equal_to to index 4. Data type stored here should be @c CommentBox
         */
        Comments
    };

private:
    PkList<PkSharedPointer<StoryboardChild>> m_childData;
};

#endif
