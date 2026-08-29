/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef TOOL_REFERENCE_IMAGES_H
#define TOOL_REFERENCE_IMAGES_H

#include <QPointer>
#include <PkConnection.h>

#include <KoToolFactoryBase.h>

#include <KisNodeAdditionFlags.h>
#include <kis_node.h>

#include <defaulttool/DefaultTool.h>
#include <defaulttool/DefaultToolFactory.h>

class KisReferenceImagesLayer;
class KisDocument;
class KisReferenceImageToolServices;

class ToolReferenceImages : public DefaultTool
{
    Q_OBJECT

public:
    ToolReferenceImages(KoCanvasBase * canvas);
    ~ToolReferenceImages() override;

    virtual quint32 priority() {
        return 3;
    }

    void mouseDoubleClickEvent(KoPointerEvent */*event*/) override {}

    bool hasSelection() override;

    void deleteSelection() override;

    QMenu* popupActionsMenu() override;

protected:
    bool isValidForCurrentLayer() const override;
    KoShapeManager *shapeManager() const override;
    KoSelection *koSelection() const override;

    void updateDistinctiveActions(const QList<KoShape*> &editableShapes) override;

public Q_SLOTS:
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;

    void addReferenceImage();
    void pasteReferenceImage();
    void addReferenceImageFromLayer();
    void addReferenceImageFromVisible();
    void removeSelectedReferenceImages();
    void removeAllReferenceImages();
    void saveReferenceImages();
    void loadReferenceImages();

    void slotNodeAdded(KisNodeSP node);
    void slotNodeAdded(KisNodeSP node, KisNodeAdditionFlags flags);
    void slotSelectionChanged();

    void cut() override;
    void copy() const override;
    bool paste() override;

    bool selectAll() override;
    void deselect() override;


private:
    PkConnection m_imageNodeAddedConnection;
    KisWeakSharedPtr<KisReferenceImagesLayer> m_layer;
    KisReferenceImageToolServices *m_services;

    KisDocument *document() const;
    void setReferenceImageLayer(KisSharedPtr<KisReferenceImagesLayer> layer);
};


class ToolReferenceImagesFactory : public DefaultToolFactory
{
public:
    ToolReferenceImagesFactory()
    : DefaultToolFactory("ToolReferenceImages") {
        setToolTip(i18n("Reference Images Tool"));
        setSection(ToolBoxSection::View);
        setPriority(2);
        setActivationShapeId("flake/always");
    };


    ~ToolReferenceImagesFactory() override {}

    KoToolBase * createTool(KoCanvasBase * canvas) override {
        return new ToolReferenceImages(canvas);
    }

    QList<QAction *> createActionsImpl() override;

};


#endif
