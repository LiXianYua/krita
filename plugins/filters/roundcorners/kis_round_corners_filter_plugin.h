/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _KIS_ROUND_CORNERS_FILTER_PLUGIN_H_
#define _KIS_ROUND_CORNERS_FILTER_PLUGIN_H_

#include <QObject>
#include <PkVariant.h>

class KisRoundCornersFilterPlugin : public QObject
{
    Q_OBJECT
public:
    KisRoundCornersFilterPlugin(QObject *parent, const PkVariantList &);
    ~KisRoundCornersFilterPlugin() override;
};

#endif
