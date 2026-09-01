/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_CONSTRAINED_RECT_H
#define __KIS_CONSTRAINED_RECT_H

#include <PkObject.h>
#include <PkRect.h>



class KisConstrainedRect : public PkObject
{
public:
    enum HandleType {
        None = 0,
        UpperLeft,
        UpperRight,
        LowerLeft,
        LowerRight,
        Upper,
        Lower,
        Left,
        Right,
        Inside,
        Creation
    };

public:
    KisConstrainedRect();
    ~KisConstrainedRect() override;

    void setRectInitial(const PkRect &rect);
    void setCropRect(const PkRect &cropRect);

    bool centered() const;
    void setCentered(bool value);

    bool canGrow() const;
    void setCanGrow(bool value);

    PkRect rect() const;

    qreal ratio() const;

    void moveHandle(HandleType handle, const PkPoint &offset, const PkRect &oldRect);
    PkPointF handleSnapPoint(HandleType handle, const PkPointF &cursorPos);

    void setRatio(qreal value);
    void setOffset(const PkPoint &offset);
    void setWidth(int value);
    void setHeight(int value);

    bool widthLocked() const;
    void setWidthLocked(bool value);

    bool heightLocked() const;
    void setHeightLocked(bool value);

    bool ratioLocked() const;
    void setRatioLocked(bool value);

    void normalize();

public:
    void sigValuesChanged();
    void sigLockValuesChanged();

private:

    int widthFromHeightUnsignedRatio(int height, qreal ratio, int oldWidth) const;
    int heightFromWidthUnsignedRatio(int width, qreal ratio, int oldHeight) const;

    void assignNewSize(const PkSize &newSize);
    void storeRatioSafe(const PkSize &newSize);
private:
    bool m_centered {false};
    bool m_canGrow {true};
    PkRect m_rect;
    qreal m_ratio {1.0};

    bool m_widthLocked {false};
    bool m_heightLocked {false};
    bool m_ratioLocked {false};

    PkRect m_cropRect;
};

#endif /* __KIS_CONSTRAINED_RECT_H */
