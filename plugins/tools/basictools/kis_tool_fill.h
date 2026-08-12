/*
 *  kis_tool_fill.h - part of Krayon^Krita
 *
 *  SPDX-FileCopyrightText: 2004 Bart Coppens <kde@bartcoppens.be>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_FILL_H_
#define KIS_TOOL_FILL_H_

#include <QPoint>
#include <QList>
#include <QVector>

#include "kis_tool_paint.h"
#include <flake/kis_node_shape.h>
#include <KoIcon.h>
#include <kis_icon.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kis_signal_compressor.h>
#include <kis_signal_auto_connection.h>
#include <kis_resources_snapshot.h>
#include <commands_new/KisMergeLabeledLayersCommand.h>
#include <KoCompositeOpRegistry.h>

class KisToolFill : public KisToolPaint
{
    Q_OBJECT

public:
    enum FillMode
    {
        FillMode_FillSelection,
        FillMode_FillContiguousRegion,
        FillMode_FillSimilarRegions
    };

    enum FillType
    {
        FillType_FillWithForegroundColor,
        FillType_FillWithBackgroundColor,
        FillType_FillWithPattern
    };

    enum ContiguousFillMode
    {
        ContiguousFillMode_FloodFill,
        ContiguousFillMode_BoundaryFill
    };

    enum Reference
    {
        Reference_CurrentLayer,
        Reference_AllLayers,
        Reference_ColorLabeledLayers
    };

    enum ContinuousFillMode
    {
        ContinuousFillMode_DoNotUse,
        ContinuousFillMode_FillAnyRegion,
        ContinuousFillMode_FillSimilarRegions
    };

    KisToolFill(KoCanvasBase * canvas);
    ~KisToolFill() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void continueAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void endAlternateAction(KoPointerEvent *event, AlternateAction action) override;

public Q_SLOTS:
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;

protected:
    bool wantsAutoScroll() const override { return false; }

protected Q_SLOTS:
    void resetCursorStyle() override;
    void slotUpdateFill();

private:
    static constexpr int minimumDragDistance{4};
    static constexpr int minimumDragDistanceSquared{minimumDragDistance * minimumDragDistance};

    FillMode m_fillMode {FillMode_FillContiguousRegion};

    FillType m_fillType {FillType_FillWithForegroundColor};
    qreal m_patternScale {100.0};
    qreal m_patternRotation {0.0};
    bool m_useCustomBlendingOptions {false};
    int m_customOpacity {100};
    QString m_customCompositeOp {COMPOSITE_OVER};

    ContiguousFillMode m_contiguousFillMode {ContiguousFillMode_FloodFill};
    KoColor m_contiguousFillBoundaryColor;
    int m_threshold {8};
    int m_opacitySpread {100};
    int m_closeGap {0};
    bool m_useSelectionAsBoundary {true};

    bool m_antiAlias {true};
    int m_sizemod {0};
    int m_stopGrowingAtDarkestPixel {false};
    int m_feather {0};

    Reference m_reference {Reference_CurrentLayer};
    QList<int> m_selectedColorLabels;
    bool m_useActiveLayer {false};

    ContinuousFillMode m_continuousFillMode {ContinuousFillMode_FillAnyRegion};
    
    KisSelectionSP m_fillMask;
    QSharedPointer<KoColor> m_referenceColor;
    KisPaintDeviceSP m_referencePaintDevice;
    KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP m_referenceNodeList;
    int m_previousTime;
    KisResourcesSnapshotSP m_resourcesSnapshot;
    QTransform m_transform;

    FillMode m_effectiveFillMode {FillMode_FillSelection};
    bool m_isFilling {false};
    bool m_isDragging {false};
    QPoint m_fillStartWidgetPosition;
    KisSignalCompressor m_compressorFillUpdate;
    QSharedPointer<QRect> m_dirtyRect;
    QVector<QPoint> m_seedPoints;
    KisStrokeId m_fillStrokeId;

    KConfigGroup m_configGroup;

    void beginFilling(const QPoint &seedPoint);
    void addFillingOperation(const QPoint &seedPoint);
    void addFillingOperation(const QVector<QPoint> &seedPoints);
    void addUpdateOperation();
    void endFilling();

    void loadConfiguration();
    KoColor loadContiguousFillBoundaryColorFromConfig();
};


#include "KisToolPaintFactoryBase.h"

class KisToolFillFactory : public KisToolPaintFactoryBase
{

public:
    KisToolFillFactory()
            : KisToolPaintFactoryBase("KritaFill/KisToolFill") {
        setToolTip(i18n("Fill Tool"));
        setSection(ToolBoxSection::Fill);
        setPriority(0);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
        setIconName(koIconNameCStr("krita_tool_color_fill"));
        setShortcut( QKeySequence( Qt::Key_F ) );
        setPriority(14);
    }

    ~KisToolFillFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolFill(canvas);
    }

};

#endif //__filltool_h__

