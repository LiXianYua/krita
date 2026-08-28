#include <PkVector.h>
#include <PkPoint.h>
/*
 *  SPDX-FileCopyrightText: 2008-2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _TRAJECTORY_H_
#define _TRAJECTORY_H_


#include <PkVector.h>
#include <PkPointF>

class Trajectory
{

public:
    Trajectory();
    ~Trajectory();
    const PkVector<PkPointF> &getLinearTrajectory(const PkPointF &start, const PkPointF &end, double space);
    PkVector<PkPointF> getDDATrajectory(PkPointF start, PkPointF end, double space);

    inline int size() const {
        return m_size;
    }

private:
    PkVector<PkPointF> m_path;
    int m_i;
    int m_size;

private:
    void addPoint(PkPointF pos);
    void reset();

};
#endif

