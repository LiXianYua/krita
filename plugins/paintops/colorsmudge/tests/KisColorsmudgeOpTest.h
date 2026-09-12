/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCOLORSMUDGEOPTEST_H
#define KISCOLORSMUDGEOPTEST_H

// 与 kistest.h / testutil.h 同一条 include 链；见 .cpp 顶部说明。
#include <simpletest.h>

class KisColorsmudgeOpTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void test();
    void test_data();
};

#endif // KISCOLORSMUDGEOPTEST_H
