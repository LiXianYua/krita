/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ToolReferenceImages.h"

#include <QDesktopServices>
#include <QFile>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QAction>
#include <QApplication>

#include <KoSelection.h>
#include <KoShapeRegistry.h>
#include <KoShapeManager.h>
#include <KoShapeController.h>
#include <QFileDialog>
#include "KisMimeDatabase.h"

#include <KisReferenceImageToolServices.h>
#include <KisDocument.h>
#include <KisReferenceImage.h>
#include <KisReferenceImagesLayer.h>
#include <kis_image.h>
#include "QClipboard"
#include <KisCursorOverrideLock.h>

#include "KisReferenceImageCollection.h"

ToolReferenceImages::ToolReferenceImages(KoCanvasBase * canvas)
    : DefaultTool(canvas, false)
    , m_services(dynamic_cast<KisReferenceImageToolServices *>(canvas))
{
    setObjectName("ToolReferenceImages");
}

ToolReferenceImages::~ToolReferenceImages()
{
    PkObject::disconnect(m_imageNodeAddedConnection);
}

void ToolReferenceImages::activate(const QSet<KoShape*> &shapes)
{
    DefaultTool::activate(shapes);

    KIS_ASSERT_RECOVER_RETURN(m_services);
    KisImageSP currentImage = document()->image();
    KIS_ASSERT_RECOVER_RETURN(currentImage);
    PkObject::disconnect(m_imageNodeAddedConnection);
    m_imageNodeAddedConnection = PkObject::connect(
        currentImage.data(), &KisImage::sigNodeAddedAsync, currentImage.data(),
        [this](KisNodeSP node, KisNodeAdditionFlags flags) { slotNodeAdded(node, flags); });
    connect(document(), &KisDocument::sigReferenceImagesLayerChanged, this, qOverload<KisNodeSP>(&ToolReferenceImages::slotNodeAdded));

    auto referenceImageLayer = document()->referenceImagesLayer();
    if (referenceImageLayer) {
        setReferenceImageLayer(referenceImageLayer);
    }
}

void ToolReferenceImages::deactivate()
{
    PkObject::disconnect(m_imageNodeAddedConnection);
    DefaultTool::deactivate();
}

void ToolReferenceImages::slotNodeAdded(KisNodeSP node)
{
    slotNodeAdded(node, KisNodeAdditionFlag::None);
}

void ToolReferenceImages::slotNodeAdded(KisNodeSP node, KisNodeAdditionFlags flags)
{
    Q_UNUSED(flags)

    auto *referenceImagesLayer = dynamic_cast<KisReferenceImagesLayer*>(node.data());

    if (referenceImagesLayer) {
        setReferenceImageLayer(referenceImagesLayer);
    }
}

void ToolReferenceImages::setReferenceImageLayer(KisSharedPtr<KisReferenceImagesLayer> layer)
{
    m_layer = layer;
    connect(layer.data(), SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()));
    connect(layer->shapeManager(), SIGNAL(selectionChanged()), this, SLOT(repaintDecorations()));
    connect(layer->shapeManager(), SIGNAL(selectionContentChanged()), this, SLOT(repaintDecorations()));
}

bool ToolReferenceImages::hasSelection()
{
    const KoShapeManager *manager = shapeManager();
    return manager && manager->selection()->count() != 0;
}

void ToolReferenceImages::addReferenceImage()
{
    KIS_ASSERT_RECOVER_RETURN(m_services);

    QFileDialog dialog(m_services->referenceImageDialogParent());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setWindowTitle(i18n("Select a Reference Image"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDirectory(locations.first());
    }

    QString filename;
    if (dialog.exec()) filename = dialog.selectedFiles().value(0);
    if (filename.isEmpty()) return;
    if (!QFileInfo(filename).exists()) return;

    auto *reference = m_services->referenceImageFromFile(filename);
    if (reference) {
        if (document()->referenceImagesLayer()) {
            reference->setZIndex(document()->referenceImagesLayer()->shapes().size());
        }
        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), {reference}));
    }
}

void ToolReferenceImages::addReferenceImageFromLayer()
{
    KIS_ASSERT_RECOVER_RETURN(m_services);
    m_services->createReferenceImageFromLayer();
}

void ToolReferenceImages::addReferenceImageFromVisible()
{
    KIS_ASSERT_RECOVER_RETURN(m_services);
    m_services->createReferenceImageFromVisible();
}

void ToolReferenceImages::pasteReferenceImage()
{
    KIS_ASSERT_RECOVER_RETURN(m_services);

    KisReferenceImage* reference = m_services->referenceImageFromClipboard();
    if (reference) {
        if (document()->referenceImagesLayer()) {
            reference->setZIndex(document()->referenceImagesLayer()->shapes().size());
        }
        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), {reference}));
    } else {
        if (canvas()->canvasWidget()) {
            QMessageBox::critical(canvas()->canvasWidget(), i18nc("@title:window", "Krita"), i18n("Could not load reference image from clipboard"));
        }
    }
}

void ToolReferenceImages::removeSelectedReferenceImages()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;
    if (!koSelection()) return;
    if (koSelection()->selectedEditableShapes().isEmpty()) return;

    canvas()->addCommand(layer->removeReferenceImages(document(), koSelection()->selectedEditableShapes()));
}

void ToolReferenceImages::removeAllReferenceImages()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    canvas()->addCommand(layer->removeReferenceImages(document(), layer->shapes()));
}

void ToolReferenceImages::loadReferenceImages()
{
    KIS_ASSERT_RECOVER_RETURN(m_services);

    QFileDialog dialog(m_services->referenceImageDialogParent());
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setMimeTypeFilters(QStringList() << "application/x-krita-reference-images");
    dialog.setWindowTitle(i18n("Load Reference Images"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDirectory(locations.first());
    }

    QString filename;
    if (dialog.exec()) filename = dialog.selectedFiles().value(0);
    if (filename.isEmpty()) return;
    if (!QFileInfo(filename).exists()) return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not open '%1'.", filename));
        return;
    }

    KisReferenceImageCollection collection;

    int currentZIndex = 0;
    if (document()->referenceImagesLayer()) {
        currentZIndex = document()->referenceImagesLayer()->shapes().size();
    }

    if (collection.load(&file)) {
        QList<KoShape*> shapes;
        Q_FOREACH(auto *reference, collection.referenceImages()) {
            reference->setZIndex(currentZIndex);
            shapes.append(reference);
            currentZIndex += 1;
        }

        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), shapes));
    } else {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not load reference images from '%1'.", filename));
    }
    file.close();
}

void ToolReferenceImages::saveReferenceImages()
{
    KisCursorOverrideLock cursorLock(Qt::BusyCursor);

    auto layer = m_layer.toStrongRef();
    if (!layer || layer->shapeCount() == 0) return;

    KIS_ASSERT_RECOVER_RETURN(m_services);

    QFileDialog dialog(m_services->referenceImageDialogParent());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    QString mimetype = "application/x-krita-reference-images";
    dialog.setMimeTypeFilters(QStringList() << mimetype);
    dialog.setWindowTitle(i18n("Save Reference Images"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDirectory(locations.first());
    }

    QString filename;
    if (dialog.exec()) filename = dialog.selectedFiles().value(0);
    if (filename.isEmpty()) return;

    QString fileMime = KisMimeDatabase::mimeTypeForFile(filename, false);
    if (fileMime != "application/x-krita-reference-images") {
        filename.append(filename.endsWith(".") ? "krf" : ".krf");
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not open '%1' for saving.", filename));
        return;
    }

    KisReferenceImageCollection collection(layer->referenceImages());
    bool ok = collection.save(&file);
    file.close();

    if (!ok) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Failed to save reference images."));
    }
}

void ToolReferenceImages::slotSelectionChanged()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    updateActions();
}

bool ToolReferenceImages::isValidForCurrentLayer() const
{
    return true;
}

KoShapeManager *ToolReferenceImages::shapeManager() const
{
    auto layer = m_layer.toStrongRef();
    return layer ? layer->shapeManager() : nullptr;
}

KoSelection *ToolReferenceImages::koSelection() const
{
    auto manager = shapeManager();
    return manager ? manager->selection() : nullptr;
}

void ToolReferenceImages::updateDistinctiveActions(const QList<KoShape*> &)
{
    action("object_group")->setEnabled(false);
    action("object_unite")->setEnabled(false);
    action("object_intersect")->setEnabled(false);
    action("object_subtract")->setEnabled(false);
    action("object_split")->setEnabled(false);
    action("object_ungroup")->setEnabled(false);
}

void ToolReferenceImages::deleteSelection()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    QList<KoShape *> shapes = koSelection()->selectedShapes();

    if (!shapes.empty()) {
        canvas()->addCommand(layer->removeReferenceImages(document(), shapes));
    }
}

QMenu* ToolReferenceImages::popupActionsMenu()
{
    if (m_contextMenu) {
        m_contextMenu->clear();
        m_contextMenu->addSection(i18n("Reference Image Actions"));
        m_contextMenu->addSeparator();

        QMenu *transform = m_contextMenu->addMenu(i18n("Transform"));

        transform->addAction(action("object_transform_rotate_90_cw"));
        transform->addAction(action("object_transform_rotate_90_ccw"));
        transform->addAction(action("object_transform_rotate_180"));
        transform->addSeparator();
        transform->addAction(action("object_transform_mirror_horizontally"));
        transform->addAction(action("object_transform_mirror_vertically"));
        transform->addSeparator();
        transform->addAction(action("object_transform_reset"));

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("edit_cut"));
        m_contextMenu->addAction(action("edit_copy"));
        m_contextMenu->addAction(action("edit_paste"));

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("object_order_front"));
        m_contextMenu->addAction(action("object_order_raise"));
        m_contextMenu->addAction(action("object_order_lower"));
        m_contextMenu->addAction(action("object_order_back"));
    }

    return m_contextMenu.data();
}

void ToolReferenceImages::cut()
{
    copy();
    deleteSelection();
}

void ToolReferenceImages::copy() const
{
    QList<KoShape *> shapes = koSelection()->selectedShapes();
    if (!shapes.isEmpty()) {
        KoShape* shape = shapes.at(0);
        KisReferenceImage *reference = dynamic_cast<KisReferenceImage*>(shape);
        KIS_SAFE_ASSERT_RECOVER_RETURN(reference);
        QClipboard *cb = QApplication::clipboard();
        cb->setImage(reference->getImage());
    }
}

bool ToolReferenceImages::paste()
{
    pasteReferenceImage();
    return true;
}

bool ToolReferenceImages::selectAll()
{
    Q_FOREACH(KoShape *shape, shapeManager()->shapes()) {
        if (!shape->isSelectable()) continue;
        koSelection()->select(shape);
    }
    repaintDecorations();

    return true;
}

void ToolReferenceImages::deselect()
{
    koSelection()->deselectAll();
    repaintDecorations();
}

KisDocument *ToolReferenceImages::document() const
{
    KIS_ASSERT(m_services);
    return m_services->referenceImageDocument();
}

QList<QAction *> ToolReferenceImagesFactory::createActionsImpl()
{
    QList<QAction *> defaultActions = DefaultToolFactory::createActionsImpl();
    QList<QAction *> actions;

    QStringList actionNames;
    actionNames << "object_order_front"
                << "object_order_raise"
                << "object_order_lower"
                << "object_order_back"
                << "object_transform_rotate_90_cw"
                << "object_transform_rotate_90_ccw"
                << "object_transform_rotate_180"
                << "object_transform_mirror_horizontally"
                << "object_transform_mirror_vertically"
                << "object_transform_reset";

    Q_FOREACH(QAction *action, defaultActions) {
        if (actionNames.contains(action->objectName())) {
            actions << action;
        } else {
            delete action;
        }
    }
    return actions;
}
