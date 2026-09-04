/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2022 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KO_SVG_TEXT_SHAPE_LAYOUT_FUNC_H
#define KO_SVG_TEXT_SHAPE_LAYOUT_FUNC_H

#include "KoSvgTextShape_p.h"

namespace KoSvgTextShapeLayoutFunc
{

void calculateLineHeight(CharacterResult cr, double &ascent, double &descent, bool isHorizontal, bool compare = false);

void addWordToLine(PkVector<CharacterResult> &result,
                   PkPointF &currentPos,
                   PkVector<int> &wordIndices,
                   LineBox &currentLine,
                   bool isHorizontal);

void finalizeLine(PkVector<CharacterResult> &result,
                  PkPointF &currentPos,
                  LineBox &currentLine,
                  PkPointF &lineOffset,
                  const KoSvgText::TextAnchor anchor,
                  const KoSvgText::WritingMode writingMode,
                  const bool ltr,
                  const bool inlineSize,
                  const bool textInShape,
                  const KoSvgText::ResolutionHandler &resHandler);

PkVector<LineBox> breakLines(const KoSvgTextProperties &properties,
                            const PkMap<int, int> &logicalToVisual,
                            PkVector<CharacterResult> &result,
                            PkPointF startPos, const KoSvgText::ResolutionHandler &resHandler);

PkList<PkPainterPath>
getShapes(PkList<KoShape *> shapesInside, PkList<KoShape *> shapesSubtract, const KoSvgTextProperties &properties);

PkVector<LineBox> flowTextInShapes(const KoSvgTextProperties &properties,
                                  const PkMap<int, int> &logicalToVisual,
                                  PkVector<CharacterResult> &result,
                                  PkList<PkPainterPath> shapes, PkPointF &startPos, const KoSvgText::ResolutionHandler &resHandler);

} // namespace KoSvgTextShapeLayoutFunc

#endif // KO_SVG_TEXT_SHAPE_LAYOUT_FUNC_H
