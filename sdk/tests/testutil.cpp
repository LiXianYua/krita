/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] testutil.cpp 阻塞登记（S-06 Task 9）
//
// 本文件不进薄壳，保留 Qt 类型。include testutil.h（GAP，见该头登记），
// getHierarchy/checkHierarchy 用 QString/QStringList/qDebug。
// 关闭条件：testutil.h 关闭后随其端口化。


#include <testutil.h>
#include <KoResource.h>
#include <KoMD5Generator.h>

namespace TestUtil
{

QStringList getHierarchy(KisNodeSP root, const QString &prefix) {
    QStringList list;

    QString nextPrefix;
    if (root->parent()) {
        nextPrefix = prefix + "+";
        list << prefix + root->name();
    }

    KisNodeSP node = root->firstChild();
    while (node) {
        list += getHierarchy(node, nextPrefix);
        node = node->nextSibling();
    }

    return list;
}

bool checkHierarchy(KisNodeSP root, const QStringList &expected)
{
    QStringList result = getHierarchy(root);
    if (result != expected) {
        qDebug() << "Failed to compare hierarchy:";
        qDebug() << "   " << ppVar(result);
        qDebug() << "   " << ppVar(expected);
        return false;
    }

    return true;
}
}
