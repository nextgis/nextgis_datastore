/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#ifndef VECTOR_H
#define VECTOR_H

#include "ogr_geometry.h"

#include <cmath>


class Vector2
        : public OGRPoint
{
public:

    inline Vector2()
            : OGRPoint(0, 0)
    { }

    inline Vector2(
            double x,
            double y)
            : OGRPoint(x, y)
    { }

    inline Vector2(const OGRPoint& c)
            : OGRPoint(c.getX(), c.getY())
    { }

    inline bool operator==(const OGRPoint& rhs) const
    {
        return getX() == rhs.getX() && getY() == rhs.getX();
    }

    template <typename C>
    inline typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator*(C c) const
    {
        return {getX() * c, getY() * c};
    }

    inline void operator*=(double c)
    {
        setX(getX() * c);
        setY(getY() * c);
    }

    template <typename C>
    inline typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator/(C c) const
    {
        return {getX() / c, getY() / c};
    }

    inline void operator/=(double c)
    {
        setX(getX() / c);
        setY(getY() / c);
    }

    template <typename C>
    inline typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator-(C c) const
    {
        return {getX() - c, getY() - c};
    }

    inline Vector2 operator-(const OGRPoint& c) const
    {
        return {getX() - c.getX(), getY() - c.getY()};
    }

    template <typename C>
    inline typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator+(C c) const
    {
        return {getX() + c, getY() + c};
    }

    inline Vector2 operator+(const OGRPoint& c) const
    {
        return {getX() + c.getX(), getY() + c.getY()};
    }

    // returns the magnitude (length) of the vector
    inline double magnitude()
    {
        return std::sqrt(getX() * getX() + getY() * getY());
    }

    inline Vector2 unit()
    {
        auto magn = magnitude();
        if (magn == 0) {
            return *this;
        }
        return (*this) / magn;
    }

    // returns k X v (cross product). this is a vector perpendicular to v
    inline Vector2 cross()
    {
        return {-getY(), getX()};
    }

    // returns the unit normal to a line between this point and the point b
    inline Vector2 normal(const OGRPoint& b)
    {
        return ((*this) - b).unit().cross();
    }
};

#endif //VECTOR_H
