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

#include "cpl_conv.h"

#include "ds/featureclass.h"
#include "util/buffer.h"

TEST(GlTests, TestTileBuffer) {
    ngs::Buffer buffer1;

    for(GUInt16 i = 0; i < 11; ++i)
        buffer1.put(i);

    for(float i = 0.0; i < 5; ++i)
        buffer1.put(i);


    GByte* dataBuff = buffer1.data();
    int sizeBuff = buffer1.size();
    EXPECT_GE(sizeBuff, 1);

    GByte* dataBuff2 = static_cast<GByte*>(CPLMalloc(sizeBuff));
    memcpy(dataBuff2, dataBuff, sizeBuff);

    ngs::Buffer buffer2(dataBuff2, sizeBuff);
    GUInt16 val = 0;
    for(int i = 0; i < 11; ++i)
        val = buffer2.getUShort();

    EXPECT_EQ(val, 10);

    float fval = 0.0;
    for(int i = 0; i < 5; ++i)
        fval = buffer2.getFloat();

    EXPECT_FLOAT_EQ(4.0, fval);
}

TEST(GlTests, TestTileBufferSaveLoad) {
    ngs::VectorTile vtile0;

    ngs::VectorTileItem vitem0;
    vitem0.addPoint({12345.6f, 65432.1f});
    vitem0.addIndex(0);
    vitem0.addId(777);
    vitem0.setValid(true);
    vtile0.add(vitem0, true);

    ngs::VectorTileItem vitem1;
    vitem1.addPoint({23456.7f, 76543.2f});
    vitem1.addIndex(0);
    vitem1.addId(555);
    vitem1.setValid(true);
    vtile0.add(vitem1, true);

    ngs::VectorTileItem vitem2;
    vitem2.addPoint({12345.6f, 65432.1f});
    vitem2.addIndex(0);
    vitem2.addId(888);
    vitem2.setValid(true);
    vtile0.add(vitem2, true);

    ngs::BufferPtr buffer = vtile0.save();

    ngs::VectorTile vtile1;
    buffer->seek(0);
    vtile1.load(*buffer.get());

    EXPECT_EQ(vtile1.items().size(), 2);

    ngs::VectorTileItem vitem3 = vtile1.items()[0];
    EXPECT_EQ(vitem3.pointCount(), 1);
    ngs::SimplePoint pt0 = vitem3.point(0);
    EXPECT_FLOAT_EQ(pt0.x, 12345.6f);
    EXPECT_FLOAT_EQ(pt0.y, 65432.1f);

    std::set<GIntBig> idset1;
    idset1.insert(777);
    idset1.insert(888);
    EXPECT_EQ(vitem3.isIdsPresent(idset1), true);

    ngs::VectorTileItem vitem4 = vtile1.items()[1];
    EXPECT_EQ(vitem4.pointCount(), 1);

    ngs::SimplePoint pt1 = vitem4.point(0);
    EXPECT_FLOAT_EQ(pt1.x, 23456.7f);
    EXPECT_FLOAT_EQ(pt1.y, 76543.2f);

    std::set<GIntBig> idset2;
    idset2.insert(555);
    EXPECT_EQ(vitem4.isIdsPresent(idset2), true);
}

/*
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
