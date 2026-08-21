/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2014 Mohit Goyal <mohit.bits2011@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef KUNDO2STACK_H
#define KUNDO2STACK_H

#include <compat/QObject>
#include "pk/string/PkString.h"
#include "pk/container/PkList.h"
#include "pk/time/PkDateTime.h"
#include "pk/container/PkVector.h"


#include "kritacommand_export.h"

class PkObject;
class KUndo2CommandPrivate;

#ifndef QT_NO_UNDOCOMMAND

#include "kundo2magicstring.h"
#include "kundo2commandextradata.h"
#include "KisCumulativeUndoData.h"


/**
 * WARNING: In general, don't derive undo commands from PkObject. And
 *          if you really need it, don't use PkObject lifetime tracking
 *          for the commands:  KUndo2Command has its own, *nonvirtual*
 *          hierarchy, and don't make it a parent or a child of any
 *          PkObject. Otherwise two different parents will try to track
 *          the lifetime of your command and, most probably, you'll
 *          get a crash.
 *
 *          As a general rule: an undo command should be derived
 *          from PkObject only for the sake of signal/slots capabilities.
 *          Nothing else.
 */
class KRITACOMMAND_EXPORT KUndo2Command
{
    KUndo2CommandPrivate *d {0};

public:
    explicit KUndo2Command(KUndo2Command *parent = 0);
    explicit KUndo2Command(const KUndo2MagicString &text, KUndo2Command *parent = 0);
    virtual ~KUndo2Command();

    virtual void undo();
    virtual void redo();

    PkString actionText() const;
    KUndo2MagicString text() const;
    void setText(const KUndo2MagicString &text);

    virtual int id() const;
    virtual int timedId() const;
    virtual void setTimedID(int timedID);
    virtual bool mergeWith(const KUndo2Command *other);
    virtual bool timedMergeWith(KUndo2Command *other);

    virtual bool canAnnihilateWith(const KUndo2Command *other) const;

    int childCount() const;
    const KUndo2Command *child(int index) const;

    bool hasParent() const;
    void setTime();
    virtual void setTime(const PkTime &time);
    virtual PkTime time() const;
    void setEndTime();
    virtual void setEndTime(const PkTime &time);
    virtual PkTime endTime() const;

    virtual PkVector<KUndo2Command*> mergeCommandsVector() const;
    virtual bool isMerged() const;
    virtual void undoMergedCommands();
    virtual void redoMergedCommands();

    /**
     * \return user-defined object associated with the command
     *
     * \see setExtraData()
     */
    KUndo2CommandExtraData* extraData() const;

    /**
     * The user can assign an arbitrary object associated with the
     * command. The \p data object is owned by the command. If you assign
     * the object twice, the first one will be destroyed.
     */
    void setExtraData(KUndo2CommandExtraData *data);

private:
    KUndo2Command(const KUndo2Command&) = delete;
    KUndo2Command& operator=(const KUndo2Command&) = delete;
    friend class KUndo2QStack;


    bool m_hasParent {false};
    int m_timedID {-1};

    PkTime m_timeOfCreation;
    PkTime m_endOfCommand;
    PkVector<KUndo2Command*> m_mergeCommandsVector;
};

#endif // QT_NO_UNDOCOMMAND

#ifndef QT_NO_UNDOSTACK

class KRITACOMMAND_EXPORT KUndo2QStack : public PkObject
{
    Q_OBJECT
//    Q_DECLARE_PRIVATE(KUndo2QStack)

public:
    explicit KUndo2QStack(PkObject *parent = 0);
    ~KUndo2QStack() override;
    void clear();

    void push(KUndo2Command *cmd);

    bool canUndo() const;
    bool canRedo() const;
    PkString undoText() const;
    PkString redoText() const;

    int count() const;
    int index() const;
    PkString actionText(int idx) const;
    PkString text(int idx) const;

    bool isActive() const;
    bool isClean() const;
    int cleanIndex() const;

    void beginMacro(const KUndo2MagicString &text);
    void endMacro();

    void setUndoLimit(int limit);
    int undoLimit() const;

    const KUndo2Command *command(int index) const;

    void setUseCumulativeUndoRedo(bool value);
    bool useCumulativeUndoRedo() const;

    void setCumulativeUndoData(const KisCumulativeUndoData &data);
    KisCumulativeUndoData cumulativeUndoData();

public Q_SLOTS:
    void setClean();
    virtual void setIndex(int idx);
    virtual void undo();
    virtual void redo();
    void setActive(bool active = true);

    void purgeRedoState();

Q_SIGNALS:
    void indexChanged(int idx);
    void cleanChanged(bool clean);
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void undoTextChanged(const PkString &undoActionText);
    void redoTextChanged(const PkString &redoActionText);

protected:
    virtual void notifySetIndexChangedOneCommand();

private:
    // from the undo stack private data
    PkList<KUndo2Command*> m_command_list;
    PkList<KUndo2Command*> m_macro_stack;
    int m_index;
    int m_clean_index;
    int m_undo_limit;
    bool m_useCumulativeUndoRedo;
    KisCumulativeUndoData m_cumulativeUndoData;

    // also from the undo stack private data
    void setIndex(int idx, bool clean);
    bool checkUndoLimit();

    KUndo2QStack(const KUndo2QStack&) = delete;
    KUndo2QStack& operator=(const KUndo2QStack&) = delete;
};

class KRITACOMMAND_EXPORT KUndo2Stack : public KUndo2QStack
{
public:
    explicit KUndo2Stack(PkObject *parent = 0);
};

#endif // QT_NO_UNDOSTACK

#endif // KUNDO2STACK_H
