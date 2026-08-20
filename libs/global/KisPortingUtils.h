#include <QString>
#include <QTextStream>
#include <QWidget>
#include <QList>
#include <QScreen>
#include <QGuiApplication>
/*
 *  SPDX-FileCopyrightText: 2024 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_PORTING_UTILS_H
#define KIS_PORTING_UTILS_H

#include <string>
#include <sstream>
#include <vector>

#include <PkString.h>


#include <PkList.h>



#include "kritaglobal_export.h"

// This file provides Qt porting utilities. It is being gradually migrated away from Qt.
// TODO: Remove Qt dependencies as pk replacements become available.

inline void setUtf8OnStream(PkTextStream &stream)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#else
    Q_UNUSED(stream)
#endif
}

inline PkList<PkScreen *> screens()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    return PkGuiApplication::screens();
#else
    return PkGuiApplication::screens();
#endif
}

inline PkWidget *widgetScreen(const PkWidget *w)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    if (w) {
        return w->screen() ? w->screen() : PkGuiApplication::primaryScreen();
    }
    return PkGuiApplication::primaryScreen();
#else
    if (w) {
        return w->screen() ? w->screen() : PkGuiApplication::primaryScreen();
    }
    return PkGuiApplication::primaryScreen();
#endif
}

inline PkString pixelsToPoints(const PkScreen *screen, int pixels)
{
    if (screen) {
        return PkString::number(pixels * 72.0 / screen->physicalDotsPerInchY(), 'f', 1);
    }
    return PkString::number(0.0, 'f', 1);
}

#endif /* KIS_PORTING_UTILS_H */
