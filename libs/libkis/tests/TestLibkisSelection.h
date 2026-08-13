/*
 * SPDX-FileCopyrightText: 2026 OpenAI
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TESTLIBKISSELECTION_H
#define TESTLIBKISSELECTION_H

#include <QObject>

class TestLibkisSelection : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCopyPasteUsesSelectionClipStore();
};

#endif // TESTLIBKISSELECTION_H
