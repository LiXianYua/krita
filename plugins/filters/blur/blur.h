/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BLURPLUGIN_H
#define BLURPLUGIN_H

#include <QObject>
#include <PkVariant.h>

class BlurFilterPlugin : public QObject
{
    Q_OBJECT
public:
    BlurFilterPlugin(QObject *parent, const PkVariantList &);
    ~BlurFilterPlugin() override;
};

#endif
