/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOFONTGLYPHMODEL_H
#define KOFONTGLYPHMODEL_H

#include <PkByteArray.h>
#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkString.h>
#include "KoFontLibraryResourceUtils.h"
#include "data/KoUnicodeBlockData.h"
#include "kritaflake_export.h"
#include <PkHash.h>
#include <PkMap.h>
#include <PkVariant.h>
#include <PkVector.h>

struct KoOpenTypeFeatureInfo;
/**
 * @brief The KoFontGlyphModel class
 * Creates a tree model of all the glyphs in a given face.
 *
 * The primary parents are the basic codepoints, the children of those parents (if any),
 * are glyph variations.
 */
class KRITAFLAKE_EXPORT KoFontGlyphModel : public PkObject
{
public:
    class Index
    {
    public:
        Index()
            : m_row(-1)
            , m_column(-1)
            , m_parentRow(-1)
            , m_internalId(0)
        {
        }

        bool isValid() const { return m_row >= 0 && m_column >= 0; }
        int row() const { return m_row; }
        int column() const { return m_column; }
        uint internalId() const { return m_internalId; }
        Index parent() const;

    private:
        friend class KoFontGlyphModel;
        Index(int row, int column, int parentRow, uint internalId)
            : m_row(row)
            , m_column(column)
            , m_parentRow(parentRow)
            , m_internalId(internalId)
        {
        }

        int m_row;
        int m_column;
        int m_parentRow;
        uint m_internalId;
    };

    enum GlyphType {
        Base,
        UnicodeVariationSelector,
        OpenType
    };

    explicit KoFontGlyphModel(PkObject *parent = nullptr);
    ~KoFontGlyphModel() override;

    enum Roles {
        DisplayRole = 0,
        ToolTipRole = 3,
        OpenTypeFeatures = 0x0101,
        GlyphLabel,
        ChildCount
    };

    PkVariant data(const Index &index, int role = DisplayRole) const;

    Index index(int row, int column, const Index &parent = Index()) const;
    Index parent(const Index &child) const;
    int rowCount(const Index &parent = Index()) const;
    int columnCount(const Index &parent = Index()) const;
    bool hasChildren(const Index &parent = Index()) const;

    Index indexForString(PkString grapheme);

    /**
     * @brief setFace
     * set the face to retrieve glyph data for.
     * @param face -- the face.
     * @param language -- the language for which to retrieve data for, OpenType data can have different glyphs depending on the language.
     * @param samplesOnly -- Whether to only retrieve enough data for 6 samples, or to retrieve the full glyph layout. Turning this on speeds up loading.
     */
    void setFace(FT_FaceSP face, PkString language = PkString(), bool samplesOnly = false);

    PkHash<int, PkByteArray> roleNames() const;

    void modelAboutToBeReset();
    void modelReset();

    /**
     * @brief blocks
     * @return list of Unicode blocks available in the font.
     */
    PkVector<KoUnicodeBlockData> blocks() const;

    /**
     * @brief featureInfo
     * @return list of OpenTypeFeatures available in the font.
     */
    PkMap<PkString, KoOpenTypeFeatureInfo> featureInfo() const;

private:
    struct Private;
    PkScopedPointer<Private> d;
};

#endif // KOFONTGLYPHMODEL_H
