/*
 * SPDX-FileCopyrightText: 2017 Jouni Pentikäinen <joupent@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qalgorithms.h>
#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>

#include "KisReferenceImagesLayer.h"

#include <PkList.h>

#include <KoKeepShapesSelectedCommand.h>
#include <KoSelection.h>
#include <KoShapeCreateCommand.h>
#include <KoShapeDeleteCommand.h>
#include <KoShapeManager.h>
#include <kundo2magicstring.h>

#include "KisDocument.h"

static PkList<KoShape *> toPkList(const QList<KoShape *> &shapes)
{
    PkList<KoShape *> result;
    for (KoShape *shape : shapes) {
        result.append(shape);
    }
    return result;
}

struct AddReferenceImagesCommand : KoShapeCreateCommand
{
    AddReferenceImagesCommand(KisDocument *document,
                              KisSharedPtr<KisReferenceImagesLayer> layer,
                              const PkList<KoShape *> referenceImages,
                              KUndo2Command *parent = nullptr)
        : KoShapeCreateCommand(layer->shapeController(),
                               referenceImages,
                               layer.data(),
                               parent,
                               kundo2_text("Add reference image"))
        , m_document(document)
        , m_layer(layer)
    {
    }

    void redo() override
    {
        auto layer = m_document->referenceImagesLayer();
        KIS_SAFE_ASSERT_RECOVER_NOOP(!layer || layer == m_layer);

        if (!layer) {
            m_document->setReferenceImagesLayer(m_layer, true);
        }

        KoShapeCreateCommand::redo();
    }

    void undo() override
    {
        KoShapeCreateCommand::undo();

        if (m_layer->shapeCount() == 0) {
            m_document->setReferenceImagesLayer(nullptr, true);
        }
    }

private:
    KisDocument *m_document;
    KisSharedPtr<KisReferenceImagesLayer> m_layer;
};

struct RemoveReferenceImagesCommand : KoShapeDeleteCommand
{
    RemoveReferenceImagesCommand(KisDocument *document,
                                 KisSharedPtr<KisReferenceImagesLayer> layer,
                                 PkList<KoShape *> referenceImages,
                                 KUndo2Command *parent = nullptr)
        : KoShapeDeleteCommand(layer->shapeController(), referenceImages, parent)
        , m_document(document)
        , m_layer(layer)
    {
    }

    void redo() override
    {
        KoShapeDeleteCommand::redo();

        if (m_layer->shapeCount() == 0) {
            m_document->setReferenceImagesLayer(nullptr, true);
        }
    }

    void undo() override
    {
        auto layer = m_document->referenceImagesLayer();
        KIS_SAFE_ASSERT_RECOVER_NOOP(!layer || layer == m_layer);

        if (!layer) {
            m_document->setReferenceImagesLayer(m_layer, true);
        }

        KoShapeDeleteCommand::undo();
    }

private:
    KisDocument *m_document;
    KisSharedPtr<KisReferenceImagesLayer> m_layer;
};

KUndo2Command *KisReferenceImagesLayer::addReferenceImages(KisDocument *document,
                                                           QList<KoShape *> referenceImages)
{
    PkList<KoShape *> pkReferenceImages;
    for (KoShape *shape : referenceImages) {
        pkReferenceImages.append(shape);
    }
    KisSharedPtr<KisReferenceImagesLayer> layer = document->referenceImagesLayer();
    if (!layer) {
        layer = new KisReferenceImagesLayer(document->shapeController(), document->image());
    }

    KUndo2Command *parentCommand = new KUndo2Command();

    new KoKeepShapesSelectedCommand(toPkList(layer->shapeManager()->selection()->selectedShapes()),
                                    PkList<KoShape *>(),
                                    layer->selectedShapesProxy(),
                                    KisCommandUtils::FlipFlopCommand::State::INITIALIZING,
                                    parentCommand);
    AddReferenceImagesCommand *command =
        new AddReferenceImagesCommand(document, layer, pkReferenceImages, parentCommand);
    parentCommand->setText(command->text());
    new KoKeepShapesSelectedCommand(PkList<KoShape *>(),
                                    pkReferenceImages,
                                    layer->selectedShapesProxy(),
                                    KisCommandUtils::FlipFlopCommand::State::FINALIZING,
                                    parentCommand);

    return parentCommand;
}

KUndo2Command *KisReferenceImagesLayer::removeReferenceImages(KisDocument *document,
                                                              QList<KoShape *> referenceImages)
{
    PkList<KoShape *> pkReferenceImages;
    for (KoShape *shape : referenceImages) {
        pkReferenceImages.append(shape);
    }
    return new RemoveReferenceImagesCommand(document, this, pkReferenceImages);
}
