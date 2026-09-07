/*
 * SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SVGTEXTMERGEPROPERTIESRANGECOMMAND_H
#define SVGTEXTMERGEPROPERTIESRANGECOMMAND_H

#include <kundo2command.h>
#include "kritatoolsvgtext_export.h"

#include "KoSvgTextProperties.h"
#include <KoSvgTextShape.h>
#include <PkSet.h>

class KoSvgTextShape;
class KoSvgTextProperties;

/**
 * @brief The SvgTextMergePropertiesRangeCommand class
 * This sets properties on a specific range in a single text shape.
 *
 * For setting text on multiple shapes, see KoShapeMergeTextPropertiesCommand
 */
class KRITATOOLSVGTEXT_EXPORT SvgTextMergePropertiesRangeCommand : public KUndo2Command
{
public:
    SvgTextMergePropertiesRangeCommand(KoSvgTextShape *shape,
                                       const KoSvgTextProperties props,
                                       const int pos, const int anchor,
                                       const PkSet<KoSvgTextProperties::PropertyId> removeProperties = PkSet<KoSvgTextProperties::PropertyId>(),
                                       KUndo2Command *parent = 0);
    ~SvgTextMergePropertiesRangeCommand() override = default;

    void redo() override;

    void undo() override;

    int id() const override;
    bool mergeWith(const KUndo2Command *other) override;
private:
    KoSvgTextShape *m_shape;
    KoSvgTextProperties m_props;
    PkSet<KoSvgTextProperties::PropertyId> m_removeProperties;

    int m_pos;
    int m_anchor;
    int m_startIndex; // for testing merge.
    int m_endIndex; // for testing merge.
    KoSvgTextShapeMementoSP m_textData;
};

#endif // SVGTEXTMERGEPROPERTIESRANGECOMMAND_H
