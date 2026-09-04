/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2002 Lars Siebold <khandha5@gmx.net>
   SPDX-FileCopyrightText: 2002 Werner Trobin <trobin@kde.org>
   SPDX-FileCopyrightText: 2002 Lennart Kudling <kudling@kde.org>
   SPDX-FileCopyrightText: 2002-2003, 2005 Rob Buis <buis@kde.org>
   SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
   SPDX-FileCopyrightText: 2005 Raphael Langerhorst <raphael.langerhorst@kdemail.net>
   SPDX-FileCopyrightText: 2005 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2005, 2008 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2006 Inge Wallin <inge@lysator.liu.se>
   SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SVGWRITER_H
#define SVGWRITER_H

#include "kritaflake_export.h"
#include <PkList.h>
#include <PkSize.h>

class SvgSavingContext;
class KoShapeLayer;
class KoShapeGroup;
class KoShape;
class KoPathShape;
class PkStream;
class PkStream;
class PkString;

/// Implements exporting shapes to SVG
class KRITAFLAKE_EXPORT SvgWriter
{
public:
    /// Creates svg writer to export specified layers
    SvgWriter(const PkList<KoShapeLayer*> &layers);

    /// Creates svg writer to export specified shapes
    SvgWriter(const PkList<KoShape*> &toplevelShapes);

    /// Destroys the svg writer
    virtual ~SvgWriter();

    /// Writes svg to specified output device
    bool save(PkStream &outputDevice, const PkSizeF &pageSize);
#ifndef PkStream
    bool save(PkStream &outputDevice, const PkSizeF &pageSize);
#endif

    /// Writes svg to the specified file
    bool save(const PkString &filename, const PkSizeF &pageSize, bool writeInlineImages);

    bool saveDetached(PkStream &outputDevice);

    bool saveDetached(SvgSavingContext &savingContext);

    void setDocumentTitle(PkString title);
    void setDocumentDescription(PkString description);

private:
    void saveShapes(const PkList<KoShape*> shapes, SvgSavingContext &savingContext);

    void saveLayer(KoShapeLayer *layer, SvgSavingContext &context);
    void saveGroup(KoShapeGroup *group, SvgSavingContext &context);
    void saveShape(KoShape *shape, SvgSavingContext &context);
    void savePath(KoPathShape *path, SvgSavingContext &context);
    void saveGeneric(KoShape *shape, SvgSavingContext &context);

    PkList<KoShape*> m_toplevelShapes;
    bool m_writeInlineImages;
    PkString m_documentTitle;
    PkString m_documentDescription;
};

#endif // SVGWRITER_H
