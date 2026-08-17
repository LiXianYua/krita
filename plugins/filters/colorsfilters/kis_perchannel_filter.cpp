/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_perchannel_filter.h"

#include <Qt>
#include <QLayout>
#include <QPixmap>
#include <QPainter>
#include <QDomDocument>
#include <QHBoxLayout>

#include "KoChannelInfo.h"
#include "KoBasicHistogramProducers.h"
#include "KoColorModelStandardIds.h"
#include "KoColorSpace.h"
#include "KoColorTransformation.h"
#include "KoCompositeColorTransformation.h"
#include "KoCompositeOp.h"
#include "KoID.h"

#include "kis_signals_blocker.h"

#include "kis_bookmarked_configuration_manager.h"
#include <filter/kis_filter_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>

#include "kis_histogram.h"
#include "kis_painter.h"
#include <KisGlobalResourcesInterface.h>

#include "kis_multichannel_utils.h"

KisPerChannelFilterConfiguration::KisPerChannelFilterConfiguration(int channelCount, KisResourcesInterfaceSP resourcesInterface)
        : KisMultiChannelFilterConfiguration(channelCount, "perchannel", 1, resourcesInterface)
{
    init();
}

KisPerChannelFilterConfiguration::KisPerChannelFilterConfiguration(const KisPerChannelFilterConfiguration &rhs)
    : KisMultiChannelFilterConfiguration(rhs)
{
}

KisPerChannelFilterConfiguration::~KisPerChannelFilterConfiguration()
{
}

KisFilterConfigurationSP KisPerChannelFilterConfiguration::clone() const
{
    return new KisPerChannelFilterConfiguration(*this);
}

KisCubicCurve KisPerChannelFilterConfiguration::getDefaultCurve()
{
    return KisCubicCurve();
}

// KisPerChannelFilter

KisPerChannelFilter::KisPerChannelFilter() : KisMultiChannelFilter(id(), i18n("&Color Adjustment curves..."))
{
    setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
}

KisFilterConfigurationSP  KisPerChannelFilter::factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    return new KisPerChannelFilterConfiguration(0, resourcesInterface);
}

KoColorTransformation* KisPerChannelFilter::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
    const KisPerChannelFilterConfiguration* configBC =
        dynamic_cast<const KisPerChannelFilterConfiguration*>(config.data()); // Somehow, this shouldn't happen
    Q_ASSERT(configBC);

    QList<bool> isIdentityList;
    for (const KisCubicCurve &curve : configBC->curves()) {
        isIdentityList.append(curve.isIdentity());
    }

    return KisMultiChannelUtils::createPerChannelTransformationFromTransfers(cs, configBC->transfers(), isIdentityList);
}
