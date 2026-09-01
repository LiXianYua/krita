/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_COLORIZE_STROKE_STRATEGY_H
#define __KIS_COLORIZE_STROKE_STRATEGY_H

#include <PkScopedPointer.h>
#include <PkObject.h>
#include <PkSignalCompat.h>

#include "kis_types.h"
#include "KisRunnableBasedStrokeStrategy.h"

class KoColor;

namespace KisLazyFillTools {
struct FilteringOptions;
}


class KisColorizeStrokeStrategy : public PkObject, public KisRunnableBasedStrokeStrategy
{
public:
    KisColorizeStrokeStrategy(KisPaintDeviceSP src,
                              KisPaintDeviceSP dst,
                              KisPaintDeviceSP filteredSource,
                              bool filteredSourceValid,
                              const PkRect &boundingRect,
                              KisNodeSP progressNode,
                              bool prefilterOnly = false);
    KisColorizeStrokeStrategy(const KisColorizeStrokeStrategy &rhs, int levelOfDetail);
    ~KisColorizeStrokeStrategy() override;

    void setFilteringOptions(const KisLazyFillTools::FilteringOptions &value);
    KisLazyFillTools::FilteringOptions filteringOptions() const;

    void addKeyStroke(KisPaintDeviceSP dev, const KoColor &color);

    void initStrokeCallback() override;
    void cancelStrokeCallback() override;
    void tryCancelCurrentStrokeJobAsync() override;
    // TODO: suspend/resume

    KisStrokeStrategy *createLodClone(int levelOfDetail) override;

signals:
    void sigFinished(bool prefilterOnly);
    void sigCancelled();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_COLORIZE_STROKE_STRATEGY_H */
