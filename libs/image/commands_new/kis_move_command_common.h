/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@kde.org>
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_MOVE_COMMAND_COMMON_H
#define KIS_MOVE_COMMAND_COMMON_H

#include <PkPoint.h>
#include "kundo2command.h"
#include "kritaimage_export.h"
#include "kis_types.h"

/**
 * KisMoveCommandCommon is a general template for a command that moves
 * entities capable of setX() and setY() actions. Generally in Krita
 * you should now move the device itself, only the node containing
 * that device. But the case of the selections is a bit special, so we
 * move them separately.
 */
template <class ObjectSP>
class KRITAIMAGE_EXPORT KisMoveCommandCommon : public KUndo2Command
{
public:
    KisMoveCommandCommon(ObjectSP object, const PkPoint& oldPos, const PkPoint& newPos, KUndo2Command *parent = 0)
        : KUndo2Command(kundo2_text("Move"), parent),
          m_oldPos(oldPos),
          m_newPos(newPos),
          m_object(object)
    {
    }

    void redo() override {
        moveTo(m_newPos);
    }

    void undo() override {
        moveTo(m_oldPos);
    }

private:
    void moveTo(const PkPoint& pos) {
        /**
         * FIXME: Hack alert:
         * Our iterators don't have guarantees on thread-safety
         * when the offset varies. When it is fixed, remove the locking.
         * see: KisIterator::stressTest(), KisToolMove::mousePressEvent()
         */
        m_object->setX(pos.x());
        m_object->setY(pos.y());
    }

private:
    PkPoint m_oldPos;
    PkPoint m_newPos;

protected:
    ObjectSP m_object;
};

#endif
