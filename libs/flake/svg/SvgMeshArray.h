/*
 *  SPDX-FileCopyrightText: 2020 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SVGMESHARRAY_H
#define SVGMESHARRAY_H

#include <PkXmlCompat.h>

#include <pk/color/PkColor.h>

#include "SvgMeshPatch.h"

struct SvgMeshPosition {
    int row;
    int col;
    SvgMeshPatch::Type segmentType;

    SvgMeshPosition()
        : row(-1)
        , col(-1)
        , segmentType(SvgMeshPatch::Size)
    {
    }

    SvgMeshPosition(int row, int col, SvgMeshPatch::Type type)
        : row(row)
        , col(col)
        , segmentType(type)
    {
    }

    bool isValid() const {
        return row >= 0 && col >= 0;
    }
};

class KRITAFLAKE_EXPORT SvgMeshArray
{
public:
    SvgMeshArray();

    SvgMeshArray(const SvgMeshArray& other);

    ~SvgMeshArray();

    /// creates a default mesh in OBB coordinates (because it's easier and more logical in this case)
    void createDefaultMesh(const int nrows, const int ncols, const PkColor color, const PkSizeF size);

    void newRow();

    bool addPatch(PkList<PkPair<PkString, PkColor>> stops, const PkPointF initialPoint);

    /// Get the point of a node in mesharray
    SvgMeshStop getStop(const SvgMeshPatch::Type edge, const int row, const int col) const;

    SvgMeshStop getStop(const SvgMeshPosition &pos) const;

    /// Get the Path Points for a segment of the meshpatch
    std::array<PkPointF, 4> getPath(const SvgMeshPatch::Type edge, const int row, const int col) const;

    // overload
    SvgMeshPath getPath(const SvgMeshPosition &pos) const;

    SvgMeshPatch* getPatch(const int row, const int col) const;

    int numRows() const;
    int numColumns() const;

    void setTransform(const PkTransform& matrix);

    PkRectF boundingRect() const;

    /// Return the paths connected to the corner. Can be thought of as edges connected to a vertex
    PkVector<SvgMeshPosition> getConnectedPaths(const SvgMeshPosition &position) const;

    void modifyHandle(const SvgMeshPosition &position, const std::array<PkPointF, 4> &newPath);
    void modifyCorner(const SvgMeshPosition &position, const PkPointF &newPos);

    void modifyColor(const SvgMeshPosition &position, const PkColor &color);

private:
    /// return the shared path between two patches.
    /// NOTE: Not to be confused with getConnectedPaths
    PkVector<SvgMeshPosition> getSharedPaths(const SvgMeshPosition &position) const;

    //  get color of a stop
    PkColor getColor(SvgMeshPatch::Type edge, int row, int col) const;

private:
    /// where each vector is a meshrow
    PkVector<PkVector<SvgMeshPatch*>> m_array;
};

#endif // SVGMESHARRAY_H
