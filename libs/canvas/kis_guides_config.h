/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>
   SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KOGUIDESDATA_H
#define KOGUIDESDATA_H

#include "kritacanvas_export.h"
#include <memory>
#include <PkColor.h>
#include <PkPen.h>
#include <PkVector.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkGlobal.h>
#include <boost/operators.hpp>
#include <KoUnit.h>
#include <pk/geometry/PkTransform.h>

class KRITACANVAS_EXPORT KisGuidesConfig : boost::equality_comparable<KisGuidesConfig>
{
public:
    enum LineTypeInternal {
        LINE_SOLID = 0,
        LINE_DASHED,
        LINE_DOTTED
    };

public:
    KisGuidesConfig();
    ~KisGuidesConfig();

    KisGuidesConfig(const KisGuidesConfig &rhs);
    KisGuidesConfig& operator=(const KisGuidesConfig &rhs);
    bool operator==(const KisGuidesConfig &rhs) const;
    bool hasSamePositionAs(const KisGuidesConfig &rhs) const;

    /**
     * @brief Set the positions of the horizontal guide lines
     *
     * @param lines a list of positions of the horizontal guide lines
     */
    void setHorizontalGuideLines(const PkVector<qreal> &lines);

    /**
     * @brief Set the positions of the vertical guide lines
     *
     * @param lines a list of positions of the vertical guide lines
     */
    void setVerticalGuideLines(const PkVector<qreal> &lines);

    /**
     * @brief Add a guide line to the canvas.
     *
     * @param orientation the orientation of the guide line
     * @param position the position in document coordinates of the guide line
     */
    void addGuideLine(Pk::Orientation orientation, qreal position);
    void removeAllGuides();

    bool showGuides() const;
    void setShowGuides(bool value);
    bool lockGuides() const;
    void setLockGuides(bool value);
    bool snapToGuides() const;
    void setSnapToGuides(bool value);

    bool rulersMultiple2() const;
    void setRulersMultiple2(bool value);

    KoUnit::Type unitType() const;
    void setUnitType(KoUnit::Type type);

    LineTypeInternal guidesLineType() const;
    void setGuidesLineType(LineTypeInternal value);

    PkColor guidesColor() const;
    void setGuidesColor(const PkColor &value);

    PkPen guidesPen() const;

    /// Returns the list of horizontal guide lines.
    const PkVector<qreal>& horizontalGuideLines() const;

    /// Returns the list of vertical guide lines.
    const PkVector<qreal>& verticalGuideLines() const;

    bool hasGuides() const;

    void loadStaticData();
    void saveStaticData() const;

    PkXmlElement saveToXml(PkXmlDocument& doc, const PkString &tag) const;
    bool loadFromXml(const PkXmlElement &parent);

    bool isDefault() const;

    /// Transform the guides using the given \p transform. Please note that \p transform
    /// should be in 'document' coordinate system.
    /// Used with image-wide transformations.
    void transform(const PkTransform &transform);

private:
    class Private;
    const std::unique_ptr<Private> d;
};


#endif
