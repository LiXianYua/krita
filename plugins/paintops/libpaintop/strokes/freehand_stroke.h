/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __FREEHAND_STROKE_H
#define __FREEHAND_STROKE_H


#include <PkPen.h>
#include <PkFlags.h>
#include <kritapaintop_export.h>
#include "kis_types.h"
#include "kis_node.h"
#include "kis_painter_based_stroke_strategy.h"
#include <kis_distance_information.h>
#include <brushengine/kis_paint_information.h>
#include "kis_lod_transform.h"
#include "KoColor.h"



class PAINTOP_EXPORT FreehandStrokeStrategy : public KisPainterBasedStrokeStrategy
{
public:
    enum Flag {
        None = 0x0,
        SupportsContinuedInterstrokeData = 0x1,
        SupportsTimedMergeId = 0x2
    };
    PK_DECLARE_FLAGS(Flags, Flag)

public:
    class Data : public KisStrokeJobData {
    public:
        enum DabType {
            POINT,
            LINE,
            CURVE,
            POLYLINE,
            POLYGON,
            RECT,
            ELLIPSE,
            PAINTER_PATH,
            QPAINTER_PATH,
            QPAINTER_PATH_FILL
        };

        Data(int _strokeInfoId,
             const KisPaintInformation &_pi)
            : KisStrokeJobData(KisStrokeJobData::UNIQUELY_CONCURRENT),
              strokeInfoId(_strokeInfoId),
              type(POINT), pi1(_pi)
        {}

        Data(int _strokeInfoId,
             const KisPaintInformation &_pi1,
             const KisPaintInformation &_pi2)
            : KisStrokeJobData(KisStrokeJobData::UNIQUELY_CONCURRENT),
              strokeInfoId(_strokeInfoId),
              type(LINE), pi1(_pi1), pi2(_pi2)
        {}

        Data(int _strokeInfoId,
             const KisPaintInformation &_pi1,
             const PkPointF &_control1,
             const PkPointF &_control2,
             const KisPaintInformation &_pi2)
            : KisStrokeJobData(KisStrokeJobData::UNIQUELY_CONCURRENT),
              strokeInfoId(_strokeInfoId),
              type(CURVE), pi1(_pi1), pi2(_pi2),
              control1(_control1), control2(_control2)
        {}

        Data(int _strokeInfoId,
             DabType _type,
             const vQPointF &_points)
            : KisStrokeJobData(KisStrokeJobData::UNIQUELY_CONCURRENT),
              strokeInfoId(_strokeInfoId),
            type(_type), points(_points)
        {}

        Data(int _strokeInfoId,
             DabType _type,
             const PkRectF &_rect)
            : KisStrokeJobData(KisStrokeJobData::UNIQUELY_CONCURRENT),
              strokeInfoId(_strokeInfoId),
            type(_type), rect(_rect)
        {}

        Data(int _strokeInfoId,
             DabType _type,
             const PkPainterPath &_path)
            : KisStrokeJobData(KisStrokeJobData::UNIQUELY_CONCURRENT),
              strokeInfoId(_strokeInfoId),
            type(_type), path(_path)
        {}

        Data(int _strokeInfoId,
             DabType _type,
             const PkPainterPath &_path,
             const PkPen &_pen, const KoColor &_customColor)
            : KisStrokeJobData(KisStrokeJobData::UNIQUELY_CONCURRENT),
              strokeInfoId(_strokeInfoId),
            type(_type), path(_path),
            pen(_pen), customColor(_customColor)
        {}

        KisStrokeJobData* createLodClone(int levelOfDetail) override;

    private:
        Data(const Data &rhs, int levelOfDetail)
            : KisStrokeJobData(rhs),
              strokeInfoId(rhs.strokeInfoId),
              type(rhs.type)
        {
            KisLodTransform t(levelOfDetail);

            switch(type) {
            case Data::POINT:
                pi1 = t.map(rhs.pi1);
                break;
            case Data::LINE:
                pi1 = t.map(rhs.pi1);
                pi2 = t.map(rhs.pi2);
                break;
            case Data::CURVE:
                pi1 = t.map(rhs.pi1);
                pi2 = t.map(rhs.pi2);
                control1 = t.map(rhs.control1);
                control2 = t.map(rhs.control2);
                break;
            case Data::POLYLINE:
                points = t.map(rhs.points);
                break;
            case Data::POLYGON:
                points = t.map(rhs.points);
                break;
            case Data::RECT:
                rect = t.map(rhs.rect);
                break;
            case Data::ELLIPSE:
                rect = t.map(rhs.rect);
                break;
            case Data::PAINTER_PATH:
                path = t.map(rhs.path);
                break;
            case Data::QPAINTER_PATH:
                path = t.map(rhs.path);
                pen = rhs.pen;
                break;
            case Data::QPAINTER_PATH_FILL:
                path = t.map(rhs.path);
                pen = rhs.pen;
                customColor = rhs.customColor;
                break;
            };
        }
    public:
        int strokeInfoId;

        DabType type;
        KisPaintInformation pi1;
        KisPaintInformation pi2;
        PkPointF control1;
        PkPointF control2;

        vQPointF points;
        PkRectF rect;
        PkPainterPath path;
        PkPen pen;
        KoColor customColor;
    };

public:
    FreehandStrokeStrategy(KisResourcesSnapshotSP resources,
                           KisFreehandStrokeInfo *strokeInfo,
                           const KUndo2MagicString &name,
                           Flags flags = None);

    FreehandStrokeStrategy(KisResourcesSnapshotSP resources,
                           PkVector<KisFreehandStrokeInfo*> strokeInfos,
                           const KUndo2MagicString &name,
                           Flags flags = None);

    ~FreehandStrokeStrategy() override;

    void initStrokeCallback() override;
    void finishStrokeCallback() override;

    void doStrokeCallback(KisStrokeJobData *data) override;

    KisStrokeStrategy* createLodClone(int levelOfDetail) override;

    void notifyUserStartedStroke() override;
    void notifyUserEndedStroke() override;

protected:
    FreehandStrokeStrategy(const FreehandStrokeStrategy &rhs, int levelOfDetail);

private:
    void init(FreehandStrokeStrategy::Flags flags);

    void tryDoUpdate(bool forceEnd = false);
    void issueSetDirtySignals();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

PK_DECLARE_OPERATORS_FOR_FLAGS(FreehandStrokeStrategy::Flags)

#endif /* __FREEHAND_STROKE_H */
