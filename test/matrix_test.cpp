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

#define DEG2RAD M_PI / 180.0

ngs::Matrix4 createMatix(){
    ngs::Matrix4 mat4;
    double w = 480.0;
    double h = 640.0;
    if(w > h){
        double add = (w - h) * .5;
        mat4.ortho (0, w, add, add + h, -1, 1);
    }
    else if(w < h){
        double add = (h - w) * .5;
        mat4.ortho (0, w, -add, h + add, -1, 1);
    }
    else {
        mat4.ortho (0, w, 0, h, -1, 1);
    }
    return mat4;
}

ngs::Matrix4 createWGSMatix(){
    ngs::Matrix4 mat4;
    double w = 360.0;
    double h = 180.0;
    if(w > h){
        double add = (w - h) * .5;
        mat4.ortho (-180, 180, -90 - add, 90 + add, -1, 1);
    }
    else if(w < h){
        double add = (h - w) * .5;
        mat4.ortho (-180 - add, -90, 180 + add, 90, -1, 1);
    }
    else {
        mat4.ortho (-180, 180, -90, 90, -1, 1);
    }
    return mat4;
}


TEST(MatrixTests, TestProject) {

    ngs::Matrix4 mat4;
    mat4.translate (1000, 1000, 0);
    mat4.scale (0.5, 0.5, 1);
    OGRRawPoint pt(0, 0);
    OGRRawPoint ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 1000);
    EXPECT_EQ(ppt.y, 1000);

    pt.x = 10;
    pt.y = 10;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 1005);
    EXPECT_EQ(ppt.y, 1005);

}

TEST(MatrixTests, TestWorldToScene) {
    ngs::Matrix4 mat4 = createMatix();
    OGRRawPoint pt, ppt;
    pt.x = 240;
    pt.y = 320;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 0);
    EXPECT_EQ(ppt.y, 0);
    pt.x = 0;
    pt.y = 0;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, -1);
    EXPECT_EQ(ppt.y, -0.8);
    pt.x = 480;
    pt.y = 640;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 1);
    EXPECT_EQ(ppt.y, 0.8);

    ngs::Matrix4 matWgs = createWGSMatix ();
    pt.x = 0.0;
    pt.y = 0.0;
    ppt = matWgs.project (pt);
    EXPECT_EQ(ppt.x, 0);
    EXPECT_EQ(ppt.y, 0);
    pt.x = 180.0;
    pt.y = 180.0;
    ppt = matWgs.project (pt);
    EXPECT_EQ(ppt.x, 1);
    EXPECT_EQ(ppt.y, 1);
    pt.x = -180.0;
    pt.y = -180.0;
    ppt = matWgs.project (pt);
    EXPECT_EQ(ppt.x, -1);
    EXPECT_EQ(ppt.y, -1);

}

TEST(MatrixTests, TestSceneToWorld) {
    ngs::Matrix4 mat4 = createMatix();
    OGRRawPoint pt, ppt;
    mat4.invert ();
    pt.x = 0;
    pt.y = 0;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 240);
    EXPECT_EQ(ppt.y, 320);
    pt.x = -1;
    pt.y = -0.8;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 0);
    EXPECT_EQ(ppt.y, 0);
    pt.x = 1;
    pt.y = 0.8;
    ppt = mat4.project (pt);
    EXPECT_EQ(ppt.x, 480);
    EXPECT_EQ(ppt.y, 640);
}

TEST(MatrixTests, TestWorldToWorld) {
    ngs::Matrix4 matWld2Scn = createMatix();
    ngs::Matrix4 matScn2Wld = createMatix();
    matScn2Wld.invert ();
    OGRRawPoint pt, ppt;
    pt.x = 0;
    pt.y = 0;
    matScn2Wld.multiply (matWld2Scn);
    ppt = matScn2Wld.project (pt);
    EXPECT_EQ(ppt.x, 0);
    EXPECT_EQ(ppt.y, 0);
    pt.x = 240;
    pt.y = 320;
    ppt = matScn2Wld.project (pt);
    EXPECT_EQ(ppt.x, 240);
    EXPECT_EQ(ppt.y, 320);
}

TEST(MatrixTests, TestRotate) {
    ngs::Matrix4 matWld2Scn = createMatix();
    double rad = -90 * DEG2RAD;
    matWld2Scn.rotateZ (rad);
    ngs::Matrix4 matScn2Wld = createMatix();
    matScn2Wld.invert ();
    OGRRawPoint pt, ppt;
    pt.x = 240.0;
    pt.y = 320.0;
    matScn2Wld.multiply (matWld2Scn);
    ppt = matScn2Wld.project (pt);
    EXPECT_DOUBLE_EQ(ppt.x, 320.0);
    EXPECT_DOUBLE_EQ(ppt.y, -240.0);
}

TEST(MatrixTests, TestRotateByCenter) {
    ngs::Matrix4 matWld2Scn = createMatix();
    double rad = -90.0 * DEG2RAD;
    matWld2Scn.invert ();
    /* TODO: double aspectRatio;
    double cosA = cos(rad);
    double sinA = sin(rad);
    double w = 480 * cosA + 640 * -sinA;
    double h = 480 * sinA + 640 * cosA;
    aspectRatio = h / w;
    matWld2Scn.scale (aspectRatio, -aspectRatio, 1);*/
    matWld2Scn.rotateZ (rad);
    matWld2Scn.invert ();
    ngs::Matrix4 matScn2Wld = createMatix();
    matScn2Wld.invert ();
    OGRRawPoint pt, ppt;
    pt.x = 240.0;
    pt.y = 320.0;
    matScn2Wld.multiply (matWld2Scn);
    ppt = matScn2Wld.project (pt);
    // center not moved
    EXPECT_DOUBLE_EQ(ppt.x, 240.0);
    EXPECT_DOUBLE_EQ(ppt.y, 320.0);
    // 1 unit shift
    /* TODO: pt.x = 240.0;
    pt.y = 321.0;
    ppt = matScn2Wld.project (pt);
    EXPECT_DOUBLE_EQ(ppt.x, 241.0);
    EXPECT_DOUBLE_EQ(ppt.y, 320.0);
    */
}
