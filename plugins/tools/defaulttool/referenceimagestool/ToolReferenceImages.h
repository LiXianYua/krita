/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef TOOL_REFERENCE_IMAGES_H
#define TOOL_REFERENCE_IMAGES_H

#include <PkPointer.h>
#include <PkConnection.h>
#include <pk/signal/compat/QPointer>

#include <KoToolFactoryBase.h>

#include <KisNodeAdditionFlags.h>
#include <kis_node.h>

#include <defaulttool/DefaultTool.h>
#include <defaulttool/DefaultToolFactory.h>

class KisReferenceImagesLayer;
class KisDocument;
class KisReferenceImagePlatformServices;

class ToolReferenceImages : public DefaultTool
{


public:
    ToolReferenceImages(KoCanvasBase * canvas);
    ~ToolReferenceImages() override;

    virtual quint32 priority() {
        return 3;
    }

    void mouseDoubleClickEvent(KoPointerEvent */*event*/) override {}

    bool hasSelection() override;

    void deleteSelection() override;

    DefaultToolMenu* popupActionsMenu() override;

protected:
    bool isValidForCurrentLayer() const override;
    KoShapeManager *shapeManager() const override;
    KoSelection *koSelection() const override;

    void updateDistinctiveActions(const PkList<KoShape*> &editableShapes) override;

public :
    void activate(const PkSet<KoShape*> &shapes) override;
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
    KisReferenceImagePlatformServices *m_services;

    KisDocument *document() const;
    void setReferenceImageLayer(KisSharedPtr<KisReferenceImagesLayer> layer);
};


class ToolReferenceImagesFactory : public DefaultToolFactory
{
public:
    ToolReferenceImagesFactory()
    : DefaultToolFactory("ToolReferenceImages") {
        setToolTip(PkString("Reference Images Tool"));
        setSection(ToolBoxSection::View);
        setPriority(2);
        setActivationShapeId("flake/always");
    };


    ~ToolReferenceImagesFactory() override {}

    KoToolBase * createTool(KoCanvasBase * canvas) override {
        return new ToolReferenceImages(canvas);
    }

    PkList<DefaultToolAction *> createActionsImpl();

};


#endif
