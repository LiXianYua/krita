/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KOTOOLSELECTION_H
#define KOTOOLSELECTION_H

#include "kritaflake_export.h"

#include <PkObject.h>

/**
 * Each tool can have a selection which is private to that tool and the specified shape
 * that it comes with.
 * This object is provided for applications to operate on that selection.  Copy paste
 * come to mind, but also marking the selected text bold.
 *
 * D-B (2026-09-12)：本类只保留 Pk 身份。此前它按 `QT_CORE_LIB` 分叉——Qt 桶是
 * `public QObject`（宿主对象树 / QPointer / 事件循环那一半），native 桶只留一条前置
 * 声明。裁决 D-B 拆掉了「Qt-QObject 与 Pk 双投递」设计，Qt 那一半身份被移除，
 * 于是这里只剩一条定义：`public PkObject`，布局与 mangled 拼法全树统一。
 */
class KRITAFLAKE_EXPORT KoToolSelection : public PkObject
{
public:
    /**
     * Constructor.
     * @param parent a parent for memory management purposes.
     */
    explicit KoToolSelection(PkObject *parent = nullptr);
    ~KoToolSelection() override;

    /// return true if the tool currently has something selected that can be copied or deleted.
    virtual bool hasSelection() {
        return false;
    }
};

#endif
