/*
 *  SPDX-FileCopyrightText: 2009 Lukas Tvrdy <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _METABALL_H_
#define _METABALL_H_

#include <cmath>


class Metaball
{
public:
    ~Metaball() {}
    Metaball(double x, double y, double radius):
        m_x(x),
        m_y(y),
        m_radius(radius) {}

    double equation(double x, double y) {
        //return m_radius / sqrt( pow((x - m_x),2) + pow((y - m_y),2) );
        return (m_radius * m_radius) / (pow((x - m_x), 2) + pow((y - m_y), 2));
    }

    double x() {
        return m_x;
    }

    double y() {
        return m_y;
    }

    double radius() {
        return m_radius;
    }
private:
    double m_x;
    double m_y;
    double m_radius;

};

#endif // _METABALL_H_
