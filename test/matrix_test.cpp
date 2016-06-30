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

#include "test.h"
#include "matrix.h"

TEST(MatrixTests, TestProject) {

    ngs::Matrix4 mat4;
    mat4.translate (1000, 1000, 0);
    mat4.scale (0.5, -0.5, 1);
    OGRRawPoint pt(0, 0);
    OGRRawPoint ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 1000);
    EXPECT_EQ(ppt.y, 1000);

    pt.x = 10;
    pt.y = 10;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 1005);
    EXPECT_EQ(ppt.y, 1005);

    mat4.clear ();
    double scale = 2.0 / 480;
    mat4.translate (240, 320, 0);
    mat4.scale (scale, scale, 1);
    pt.x = 240;
    pt.y = 320;
    ppt = mat4.project (pt);
    pt.y = 640;
    ppt = mat4.project (pt);
    int x = 0;
}
