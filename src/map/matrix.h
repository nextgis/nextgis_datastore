/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

#ifndef MATRIX_H
#define MATRIX_H

#include <array>
#include "ogr_geometry.h"

using namespace std;

namespace ngs {

class Matrix4
{
public:
    Matrix4();
    Matrix4(const Matrix4& other);
    Matrix4& operator=(const Matrix4& other);
    bool invert();
    void ortho(double left, double right, double bottom, double top, double near,
               double far);
    void perspective(double fovy, double aspect, double near, double far);
    Matrix4 copy() const;
    void clear();
    void translate(double x, double y, double z);
    void rotateX(double rad);
    void rotateY(double rad);
    void rotateZ(double rad);
    void scale(double x, double y, double z);
    void rotate(double x, double y, double z);
    void multiply(const Matrix4& other);
    OGRRawPoint project(const OGRRawPoint &pt) const;
    array<float, 16> dataF() const;

private:
    array<double, 16> m_values;
};

}
#endif // MATRIX_H
