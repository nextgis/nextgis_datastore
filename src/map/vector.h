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
    using OGRPoint::OGRPoint;
    using OGRPoint::operator=;

    bool operator==(const OGRPoint& rhs) const
    {
        return Equals(const_cast<OGRPoint*>(&rhs));
    }

    explicit operator bool() const
    {
        return !IsEmpty();
    }

    template <typename C>
    typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator*(C c) const
    {
        return {getX() * c, getY() * c};
    }

    void operator*=(double c)
    {
        setX(getX() * c);
        setY(getY() * c);
    }

    template <typename C>
    typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator/(C c) const
    {
        return {getX() / c, getY() / c};
    }

    void operator/=(double c)
    {
        setX(getX() / c);
        setY(getY() / c);
    }

    template <typename C>
    typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator-(C c) const
    {
        return {getX() - c, getY() - c};
    }

    Vector2 operator-(const OGRPoint& c) const
    {
        return {getX() - c.getX(), getY() - c.getY()};
    }

    template <typename C>
    typename std::enable_if <std::is_arithmetic <C>::value, Vector2>::type
    operator+(C c) const
    {
        return {getX() + c, getY() + c};
    }

    Vector2 operator+(const OGRPoint& c) const
    {
        return {getX() + c.getX(), getY() + c.getY()};
    }

    // returns the magnitude (length) of the vector
    double magnitude() const
    {
        return std::sqrt(getX() * getX() + getY() * getY());
    }

    Vector2 unit() const
    {
        auto magn = magnitude();
        if (magn == 0) {
            return *this;
        }
        return (*this) / magn;
    }

    // returns k X v (cross product). this is a vector perpendicular to v
    Vector2 cross() const
    {
        return {-getY(), getX()};
    }

    // returns the unit normal to a line between this point and the point b
    Vector2 normal(const OGRPoint& b) const
    {
        return ((*this) - b).unit().cross();
    }
};

using Vector2SharedPtr = std::shared_ptr<Vector2>;

template <typename... Args>
Vector2SharedPtr makeSharedVector2(Args&&... args)
{
    return std::make_shared<Vector2>(std::forward<Args>(args)...);
}

#endif //VECTOR_H
