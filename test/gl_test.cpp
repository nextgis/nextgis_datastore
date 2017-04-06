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
/*
#include "map/glview.h"

TEST(GlTests, TestCreate) {
#ifdef OFFSCREEN_GL
    ngs::GlOffScreenView view;
    EXPECT_EQ(view.init (), true);
    view.setSize (640, 480);
    EXPECT_EQ(view.isOk (), true);
#endif // OFFSCREEN_GL
}

TEST(GlTests, PolygonTriangulatorTest)
{
    OGRLinearRing externalRing;
    externalRing.addPoint(0, 0);
    externalRing.addPoint(2, 0);
    externalRing.addPoint(2, 2);
    externalRing.addPoint(0, 2);
    externalRing.closeRings();

    OGRLinearRing internalRing;
    internalRing.addPoint(0.5, 0.5);
    internalRing.addPoint(1.5, 0.5);
    internalRing.addPoint(1.5, 1.5);
    internalRing.addPoint(0.5, 1.5);
    internalRing.closeRings();

    OGRPolygon polygon;
    polygon.addRing(&externalRing);
    polygon.addRing(&internalRing);

    OGREnvelope env;
    ngs::GlBufferBucketSharedPtr tile =
            ngs::makeSharedGlBufferBucket(ngs::GlBufferBucket(1, 1, 1, env, 1));
    tile->fill(1, &polygon, 0);

    size_t vertexBufferSize = tile->getVertexBufferSize();

    // For SimpleFillStyle, 8 vertices * 3 coordinates
    //EXPECT_EQ(vertexBufferSize, 24);
    // For SimpleFillBorderedStyle, LineJoinType::Bevel
    EXPECT_EQ(vertexBufferSize, 220);

    size_t indexBufferSize = tile->getIndexBufferSize();
    EXPECT_EQ(indexBufferSize, 24);  // 8 triangles * 3 indices
}
*/
