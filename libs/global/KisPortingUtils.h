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

#include <QString>
#include <QTextStream>
#include <QWidget>
#include <QList>
#include <QScreen>
#include <QGuiApplication>

#include "kritaglobal_export.h"

// This file provides Qt porting utilities. It is being gradually migrated away from Qt.
// TODO: Remove Qt dependencies as pk replacements become available.

inline void setUtf8OnStream(QTextStream &stream)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#else
    Q_UNUSED(stream)
#endif
}

inline QList<QScreen *> screens()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    return QGuiApplication::screens();
#else
    return QGuiApplication::screens();
#endif
}

inline QWidget *widgetScreen(const QWidget *w)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    if (w) {
        return w->screen() ? w->screen() : QGuiApplication::primaryScreen();
    }
    return QGuiApplication::primaryScreen();
#else
    if (w) {
        return w->screen() ? w->screen() : QGuiApplication::primaryScreen();
    }
    return QGuiApplication::primaryScreen();
#endif
}

inline QString pixelsToPoints(const QScreen *screen, int pixels)
{
    if (screen) {
        return QString::number(pixels * 72.0 / screen->physicalDotsPerInchY(), 'f', 1);
    }
    return QString::number(0.0, 'f', 1);
}

#endif /* KIS_PORTING_UTILS_H */
