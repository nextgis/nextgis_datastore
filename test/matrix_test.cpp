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
#include "constants.h"

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
    OGRRawPoint pt, ppt;
    pt.x = 240.0;
    pt.y = 320.0;

    ngs::Matrix4 matWld2Scn = createMatix();
    ppt = matWld2Scn.project (pt);
    // center not moved
    EXPECT_DOUBLE_EQ(ppt.x, 0.0);
    EXPECT_DOUBLE_EQ(ppt.y, 0.0);

    double rad = 90.0 * DEG2RAD;
    matWld2Scn.rotateZ (rad);

    ppt = matWld2Scn.project (pt);
    // center not moved
    EXPECT_NE(ppt.x, 0.0);
    EXPECT_NE(ppt.y, 0.0);

    ngs::Matrix4 matScn2Wld = createMatix();
    matScn2Wld.rotateZ (-rad);
    matScn2Wld.invert ();

    matScn2Wld.multiply (matWld2Scn);

    ppt = matScn2Wld.project (pt);
    // center not moved
    EXPECT_DOUBLE_EQ(ppt.x, -240.0);
    EXPECT_DOUBLE_EQ(ppt.y, -320.0);
    // 1 unit shift
    pt.x = 241.0;
    pt.y = 320.0;
    ppt = matScn2Wld.project (pt);
    EXPECT_DOUBLE_EQ(ppt.x, -241.0);
    EXPECT_DOUBLE_EQ(ppt.y, -320.0);
}

TEST(MatrixTests, TestComplexProject) {

    int display_sqw = 100;
    double map_sqw = DEFAULT_MAX_X;
    bool m_isYAxisInverted = false;
    int m_displayWidht = display_sqw;
    int m_displayHeight = display_sqw;

    OGREnvelope m_extent;
    m_extent.MinX = -map_sqw;
    m_extent.MaxX = map_sqw;
    m_extent.MinY = -map_sqw;
    m_extent.MaxY = map_sqw;

    ngs::Matrix4 m_sceneMatrix, m_viewMatrix, m_worldToDisplayMatrix;
    ngs::Matrix4 m_invSceneMatrix, m_invViewMatrix, m_invWorldToDisplayMatrix;

    OGRRawPoint pt(display_sqw, display_sqw);
    OGRRawPoint ismPt;
    OGRRawPoint ivmPt;
    OGRRawPoint iwdPt;


    // world -> scene matrix
    m_sceneMatrix.clear ();

    if (m_isYAxisInverted) {
        m_sceneMatrix.ortho (m_extent.MinX, m_extent.MaxX,
                             m_extent.MaxY, m_extent.MinY, -1, 1);
    } else {
        m_sceneMatrix.ortho (m_extent.MinX, m_extent.MaxX,
                             m_extent.MinY, m_extent.MaxY, -1, 1);
    }

    // world -> scene inv matrix
    m_invSceneMatrix = m_sceneMatrix;
    m_invSceneMatrix.invert ();

    // scene -> view inv matrix
    m_invViewMatrix.clear ();
    m_invViewMatrix.ortho (0, m_displayWidht, 0, m_displayHeight, -1, 1);

    // scene -> view matrix
    m_viewMatrix = m_invViewMatrix;
    m_viewMatrix.invert ();

    m_worldToDisplayMatrix = m_viewMatrix;
    m_worldToDisplayMatrix.multiply (m_sceneMatrix);

    m_invWorldToDisplayMatrix = m_invSceneMatrix;
    m_invWorldToDisplayMatrix.multiply (m_invViewMatrix);


    ivmPt = m_invViewMatrix.project(pt);
    ismPt = m_invSceneMatrix.project(ivmPt);
    iwdPt = m_invWorldToDisplayMatrix.project(pt);

    EXPECT_DOUBLE_EQ(ivmPt.x, 1.0);
    EXPECT_DOUBLE_EQ(ivmPt.y, 1.0);
    EXPECT_DOUBLE_EQ(ismPt.x, map_sqw);
    EXPECT_DOUBLE_EQ(ismPt.y, map_sqw);
    EXPECT_DOUBLE_EQ(iwdPt.x, map_sqw);
    EXPECT_DOUBLE_EQ(iwdPt.y, map_sqw);
}
