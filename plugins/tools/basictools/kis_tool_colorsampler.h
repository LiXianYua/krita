/*
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <elter@kde.org>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2018 Emmet & Eoin O'Neill <emmetoneill.pdx@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_COLOR_SAMPLER_H_
#define KIS_TOOL_COLOR_SAMPLER_H_

#include "KoToolFactoryBase.h"
#include "kis_tool.h"
#include <kis_icon.h>
#include <KoColorSet.h>
#include <QPainter>
#include <QKeySequence>
#include <KisAsyncColorSamplerHelper.h>

namespace KisToolUtils {
struct ColorSamplerConfig;
}

class KisToolColorSampler : public KisTool
{
    Q_OBJECT

public:
    KisToolColorSampler(KoCanvasBase *canvas);
    ~KisToolColorSampler() override;

public:
    struct Configuration {
        Configuration();

        bool toForegroundColor;
        bool updateColor;
        bool addPalette;
        bool normaliseValues;
        bool sampleMerged;
        int radius;
        int blend;

        void save() const;
        void load();
    };

public:
    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void requestUpdateOutline(const QPointF &outlineDocPoint, const KoPointerEvent *event);

    void activatePrimaryAction() override;
    void deactivatePrimaryAction() override;

    void paint(QPainter &gc, const KoViewConverter &converter) override;

protected:
    void activate(const QSet<KoShape*> &) override;
    void deactivate() override;

private Q_SLOTS:
    void slotColorPickerRequestedCursor(const QCursor &cursor);
    void slotColorPickerRequestedCursorReset();
    void slotColorPickerRequestedOutlineUpdate();
    void slotColorPickerSelectedColor(const KoColor &color);
    void slotColorPickerSelectionFinished(const KoColor &color);

private:
    // Configuration
    QScopedPointer<KisToolUtils::ColorSamplerConfig> m_config;

    bool m_isActivated {false};
    QPointF m_outlineDocPoint;

    QRectF m_oldColorPreviewUpdateRect;

    KoColor m_sampledColor;

    KisAsyncColorSamplerHelper m_helper;
};

class KisToolColorSamplerFactory : public KoToolFactoryBase
{
public:
    KisToolColorSamplerFactory()
            : KoToolFactoryBase("KritaSelected/KisToolColorSampler") {
        setToolTip(i18n("Color Sampler Tool"));
        setSection(ToolBoxSection::Fill);
        setPriority(2);
        setIconName(koIconNameCStr("krita_tool_color_sampler"));
        setShortcut(QKeySequence(Qt::Key_P));
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolColorSamplerFactory() override {}

    KoToolBase *createTool(KoCanvasBase *canvas) override {
        return new KisToolColorSampler(canvas);
    }
};

#endif // KIS_TOOL_COLOR_SAMPLER_H_
