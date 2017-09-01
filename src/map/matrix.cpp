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
#include "matrix.h"

#include <cmath>
#include <limits>

#include "api_priv.h"
#include "ngstore/api.h"
#include "ngstore/util/constants.h"

namespace ngs {

Matrix4::Matrix4()
{
    clear ();
}

Matrix4::Matrix4(const Matrix4 &other)
{
    m_values = other.m_values;
}

Matrix4 &Matrix4::operator=(const Matrix4 &other)
{
    m_values = other.m_values;
    return *this;
}

int Matrix4::invert()
{
    double a00 = m_values[0],
        a01 = m_values[1],
        a02 = m_values[2],
        a03 = m_values[3],
        a10 = m_values[4],
        a11 = m_values[5],
        a12 = m_values[6],
        a13 = m_values[7],
        a20 = m_values[8],
        a21 = m_values[9],
        a22 = m_values[10],
        a23 = m_values[11],
        a30 = m_values[12],
        a31 = m_values[13],
        a32 = m_values[14],
        a33 = m_values[15],

        b00 = a00 * a11 - a01 * a10,
        b01 = a00 * a12 - a02 * a10,
        b02 = a00 * a13 - a03 * a10,
        b03 = a01 * a12 - a02 * a11,
        b04 = a01 * a13 - a03 * a11,
        b05 = a02 * a13 - a03 * a12,
        b06 = a20 * a31 - a21 * a30,
        b07 = a20 * a32 - a22 * a30,
        b08 = a20 * a33 - a23 * a30,
        b09 = a21 * a32 - a22 * a31,
        b10 = a21 * a33 - a23 * a31,
        b11 = a22 * a33 - a23 * a32,

        // Calculate the determinant
        det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

    // https://stackoverflow.com/a/42927051
    static_assert(std::numeric_limits<double>::is_iec559, "Please use IEEE754");
    if(!std::isfinite(det = 1.0 / det))
        return COD_UNSUPPORTED;

    m_values[0] = (a11 * b11 - a12 * b10 + a13 * b09) * det;
    m_values[1] = (a02 * b10 - a01 * b11 - a03 * b09) * det;
    m_values[2] = (a31 * b05 - a32 * b04 + a33 * b03) * det;
    m_values[3] = (a22 * b04 - a21 * b05 - a23 * b03) * det;
    m_values[4] = (a12 * b08 - a10 * b11 - a13 * b07) * det;
    m_values[5] = (a00 * b11 - a02 * b08 + a03 * b07) * det;
    m_values[6] = (a32 * b02 - a30 * b05 - a33 * b01) * det;
    m_values[7] = (a20 * b05 - a22 * b02 + a23 * b01) * det;
    m_values[8] = (a10 * b10 - a11 * b08 + a13 * b06) * det;
    m_values[9] = (a01 * b08 - a00 * b10 - a03 * b06) * det;
    m_values[10] = (a30 * b04 - a31 * b02 + a33 * b00) * det;
    m_values[11] = (a21 * b02 - a20 * b04 - a23 * b00) * det;
    m_values[12] = (a11 * b07 - a10 * b09 - a12 * b06) * det;
    m_values[13] = (a00 * b09 - a01 * b07 + a02 * b06) * det;
    m_values[14] = (a31 * b01 - a30 * b03 - a32 * b00) * det;
    m_values[15] = (a20 * b03 - a21 * b01 + a22 * b00) * det;

    return COD_SUCCESS;
}

void Matrix4::ortho(double left, double right, double bottom, double top,
                    double near, double far)
{
    double lr = 1.0 / (left - right),
          bt = 1.0 / (bottom - top),
          nf = 1.0 / (near - far);
    m_values[0] = -2.0 * lr;
    m_values[1] = 0.0;
    m_values[2] = 0.0;
    m_values[3] = 0.0;
    m_values[4] = 0.0;
    m_values[5] = -2.0 * bt;
    m_values[6] = 0.0;
    m_values[7] = 0.0;
    m_values[8] = 0.0;
    m_values[9] = 0.0;
    m_values[10] = 2.0 * nf;
    m_values[11] = 0.0;
    m_values[12] = (left + right) * lr;
    m_values[13] = (top + bottom) * bt;
    m_values[14] = (far + near) * nf;
    m_values[15] = 1.0;
}

void Matrix4::perspective(double fovy, double aspect, double near, double far)
{
    double f = 1.0 / tan(fovy * 0.5),
        nf = 1.0 / (near - far);
    m_values[0] = f / aspect;
    m_values[1] = 0.0;
    m_values[2] = 0.0;
    m_values[3] = 0.0;
    m_values[4] = 0.0;
    m_values[5] = f;
    m_values[6] = 0.0;
    m_values[7] = 0.0;
    m_values[8] = 0.0;
    m_values[9] = 0.0;
    m_values[10] = (far + near) * nf;
    m_values[11] = -1.0;
    m_values[12] = 0.0;
    m_values[13] = 0.0;
    m_values[14] = (2.0 * far * near) * nf;
    m_values[15] = 0.0;
}

void Matrix4::clear()
{
    m_values[0] = 1.0;
    m_values[1] = 0.0;
    m_values[2] = 0.0;
    m_values[3] = 0.0;
    m_values[4] = 0.0;
    m_values[5] = 1.0;
    m_values[6] = 0.0;
    m_values[7] = 0.0;
    m_values[8] = 0.0;
    m_values[9] = 0.0;
    m_values[10] = 1.0;
    m_values[11] = 0.0;
    m_values[12] = 0.0;
    m_values[13] = 0.0;
    m_values[14] = 0.0;
    m_values[15] = 1.0;
}

void Matrix4::translate(double x, double y, double z)
{
    m_values[12] = m_values[0] * x + m_values[4] * y + m_values[8]  * z + m_values[12];
    m_values[13] = m_values[1] * x + m_values[5] * y + m_values[9]  * z + m_values[13];
    m_values[14] = m_values[2] * x + m_values[6] * y + m_values[10] * z + m_values[14];
    m_values[15] = m_values[3] * x + m_values[7] * y + m_values[11] * z + m_values[15];
}

void Matrix4::rotateX(double rad)
{
    double s = sin(rad), c = cos(rad),
        a10 = m_values[4],
        a11 = m_values[5],
        a12 = m_values[6],
        a13 = m_values[7],
        a20 = m_values[8],
        a21 = m_values[9],
        a22 = m_values[10],
        a23 = m_values[11];

    // Perform axis-specific matrix multiplication
    m_values[4] = a10 * c + a20 * s;
    m_values[5] = a11 * c + a21 * s;
    m_values[6] = a12 * c + a22 * s;
    m_values[7] = a13 * c + a23 * s;
    m_values[8] = a20 * c - a10 * s;
    m_values[9] = a21 * c - a11 * s;
    m_values[10] = a22 * c - a12 * s;
    m_values[11] = a23 * c - a13 * s;
}

void Matrix4::rotateY(double rad)
{
    double s = sin(rad), c = cos(rad),
        a00 = m_values[0],
        a01 = m_values[1],
        a02 = m_values[2],
        a03 = m_values[3],
        a20 = m_values[8],
        a21 = m_values[9],
        a22 = m_values[10],
        a23 = m_values[11];

    // Perform axis-specific matrix multiplication
    m_values[0] = a00 * c - a20 * s;
    m_values[1] = a01 * c - a21 * s;
    m_values[2] = a02 * c - a22 * s;
    m_values[3] = a03 * c - a23 * s;
    m_values[8] = a00 * s + a20 * c;
    m_values[9] = a01 * s + a21 * c;
    m_values[10] = a02 * s + a22 * c;
    m_values[11] = a03 * s + a23 * c;
}

void Matrix4::rotateZ(double rad)
{
    double s = sin(rad), c = cos(rad),
      a00 = m_values[0],
      a01 = m_values[1],
      a02 = m_values[2],
      a03 = m_values[3],
      a10 = m_values[4],
      a11 = m_values[5],
      a12 = m_values[6],
      a13 = m_values[7];

    // Perform axis-specific matrix multiplication
    m_values[0] = a00 * c + a10 * s;
    m_values[1] = a01 * c + a11 * s;
    m_values[2] = a02 * c + a12 * s;
    m_values[3] = a03 * c + a13 * s;
    m_values[4] = a10 * c - a00 * s;
    m_values[5] = a11 * c - a01 * s;
    m_values[6] = a12 * c - a02 * s;
    m_values[7] = a13 * c - a03 * s;
}

void Matrix4::scale(double x, double y, double z)
{
    m_values[0]  *= x;
    m_values[1]  *= x;
    m_values[2]  *= x;
    m_values[3]  *= x;
    m_values[4]  *= y;
    m_values[5]  *= y;
    m_values[6]  *= y;
    m_values[7]  *= y;
    m_values[8]  *= z;
    m_values[9]  *= z;
    m_values[10] *= z;
    m_values[11] *= z;
}

void Matrix4::rotate(double x, double y, double z)
{
    double A       = cos(x);
    double B       = sin(x);
    double C       = cos(y);
    double D       = sin(y);
    double E       = cos(z);
    double F       = sin(z);

    double AD      =   A * D;
    double BD      =   B * D;

    Matrix4 mat;
    mat.m_values[0]  =   C * E;
    mat.m_values[1]  =  -C * F;
    mat.m_values[2]  =  -D;
    mat.m_values[4]  = -BD * E + A * F;
    mat.m_values[5]  =  BD * F + A * E;
    mat.m_values[6]  =  -B * C;
    mat.m_values[8]  =  AD * E + B * F;
    mat.m_values[9]  = -AD * F + B * E;
    mat.m_values[10] =   A * C;

    mat.m_values[3]  =  mat.m_values[7] = mat.m_values[11] = mat.m_values[12] =
            mat.m_values[13] = mat.m_values[14] = 0;
    mat.m_values[15] =  1;

    multiply (mat);
}

void Matrix4::multiply(const Matrix4 &other)
{
    double a00 = m_values[0],
        a01 = m_values[1],
        a02 = m_values[2],
        a03 = m_values[3],
        a10 = m_values[4],
        a11 = m_values[5],
        a12 = m_values[6],
        a13 = m_values[7],
        a20 = m_values[8],
        a21 = m_values[9],
        a22 = m_values[10],
        a23 = m_values[11],
        a30 = m_values[12],
        a31 = m_values[13],
        a32 = m_values[14],
        a33 = m_values[15];

        // Cache only the current line of the second matrix
        double b0 = other.m_values[0],
                b1 = other.m_values[1],
                b2 = other.m_values[2],
                b3 = other.m_values[3];
        m_values[0] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
        m_values[1] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
        m_values[2] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
        m_values[3] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

        b0 = other.m_values[4];
        b1 = other.m_values[5];
        b2 = other.m_values[6];
        b3 = other.m_values[7];
        m_values[4] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
        m_values[5] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
        m_values[6] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
        m_values[7] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

        b0 = other.m_values[8];
        b1 = other.m_values[9];
        b2 = other.m_values[10];
        b3 = other.m_values[11];
        m_values[8] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
        m_values[9] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
        m_values[10] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
        m_values[11] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

        b0 = other.m_values[12];
        b1 = other.m_values[13];
        b2 = other.m_values[14];
        b3 = other.m_values[15];
        m_values[12] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
        m_values[13] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
        m_values[14] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
        m_values[15] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;
}

OGRRawPoint Matrix4::project(const OGRRawPoint &pt) const
{
    double v0 = m_values[0] * pt.x + m_values[4] * pt.y + /*m_values[8]  * 0 +*/ m_values[12] * 1;
    double v1 = m_values[1] * pt.x + m_values[5] * pt.y + /*m_values[9]  * 0 +*/ m_values[13] * 1;
    //double v2 = m_values[2] * pt.x + m_values[6] * pt.y + /*m_values[10] * 0 +*/ m_values[14] * 1;
    double v3 = m_values[3] * pt.x + m_values[7] * pt.y + /*m_values[11] * 0 +*/ m_values[15] * 1;

    OGRRawPoint outPt;
    outPt.x = v0 / v3;
    outPt.y = v1 / v3;
    return outPt;
}

std::array<float, 16> Matrix4::dataF() const
{
    std::array<float, 16> out;
    for(size_t i = 0; i < 16; ++i){
        out[i] = static_cast<float>(m_values[i]);
    }
    return out;
}

} // namespace ngs
