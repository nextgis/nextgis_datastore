/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*  Author: NikitaFeodonit, nfeodonit@yandex.com
*******************************************************************************
*  Copyright (C) 2016-2017 NextGIS, <info@nextgis.com>
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#ifndef NGSLINESTRINGFILLER_H_
#define NGSLINESTRINGFILLER_H_

#include "glview.h"

// CGAL
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Point_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>

// Standard library
#include <cstdint>

namespace ngs
{

class LineStringFiller
{
public:
    explicit LineStringFiller(const OGRLineString* line,
            float level,
            int_least8_t crossExtent,
            const LineCapType layoutLineCap,
            const LineJoinType layoutLineJoin,
            const GlBufferSharedPtr currBuffer);
    virtual ~LineStringFiller();

    int insertVertex(size_t i, enum ngsBufferType indexType = BF_INDICES);

protected:
    void addCurrentLineVertex(const Vector2& currPt,
            float level,
            const Vector2& normal,
            double endLeft,
            double endRight,
            bool round,
            GlBufferSharedPtr currBuffer,
            enum ngsBufferType indexType);

    void addPieSliceLineVertex(const Vector2& currPt,
            float level,
            const Vector2& extrude,
            bool lineTurnsLeft,
            bool firstPt,
            GlBufferSharedPtr currBuffer,
            enum ngsBufferType indexType);

protected:
    const GlBufferSharedPtr m_currBuffer;

    // all new vectors/points is empty, IsEmpty() == true
    Vector2 m_firstPt;
    Vector2 m_lastPt;
    Vector2 m_currPt;
    Vector2 m_prevPt;
    Vector2 m_nextPt;
    Vector2 m_prevNormal;
    Vector2 m_nextNormal;

    const OGRLineString* m_line;
    const float m_level;
    const int_least8_t m_crossExtent;
    size_t m_numPoints;
    bool m_closed;

    const LineCapType m_layoutLineCap;
    const LineJoinType m_layoutLineJoin;
    float m_miterLimit;
    LineCapType m_beginCap;
    LineCapType m_endCap;
    //    const double m_sharpCornerOffset;

    bool m_startOfLine;
    int m_e1;
    int m_e2;
    int m_e3;
};

// Based on the
// http://doc.cgal.org/latest/Triangulation_2/index.html#title29
// "8.4 Example: Triangulating a Polygonal Domain".
// The following code inserts nested polygons into
// a constrained Delaunay triangulation and counts the number of facets
// that are inside the domain delimited by these polygons.
// Note that the following code does not work
// if the boundaries of the polygons intersect.
class PolygonTriangulator
{
protected:
    struct VertexInfo2
    {
        VertexInfo2()
                : m_index(-1)
        {
        }

        bool inDomain()
        {
            return m_index != -1;
        }

        bool isAdditionalVertex()
        {
            return m_index != -2;
        }

        void setAsAdditionalVertexInDomain()
        {
            m_index = -2;
        }

        int m_index;
    };

    struct FaceInfo2
    {
        FaceInfo2()
                : m_nestingLevel(-1)
        {
        }

        bool inDomain()
        {
            return m_nestingLevel % 2 == 1;
        }

        int m_nestingLevel;
    };
    typedef CGAL::Exact_predicates_inexact_constructions_kernel Kern;
    typedef CGAL::Triangulation_vertex_base_with_info_2<VertexInfo2, Kern> Vbi;
    typedef CGAL::Triangulation_vertex_base_2<Kern, Vbi> Vb;
    typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo2, Kern> Fbi;
    typedef CGAL::Constrained_triangulation_face_base_2<Kern, Fbi> Fb;
    typedef CGAL::Triangulation_data_structure_2<Vb, Fb> TDS;
    typedef CGAL::Exact_predicates_tag Itag;
    typedef CGAL::Polygon_2<Kern> Polygon_2;

public:
    typedef CGAL::Constrained_Delaunay_triangulation_2<Kern, TDS, Itag> CDT;

public:
    explicit PolygonTriangulator();
    virtual ~PolygonTriangulator();

    size_t insertConstraint(
            const OGRLineString* line, const size_t startIndex = 0);
    void markDomains();
    void insertVerticesToBuffer(float level,
            int_least8_t crossExtent,
            const GlBufferSharedPtr buffer,
            const std::vector<size_t>* borderIndices = nullptr);

    void triangulate(const OGRPolygon* polygon);

    CDT& getCdt()
    {
        return m_cdt;
    }
    size_t getNumTriangles()
    {
        return m_numTriangles;
    }
    size_t getNumVertices()
    {
        return m_numVertices;
    }

protected:
    void markDomains(CDT& ct,
            CDT::Face_handle start,
            int index,
            std::list<CDT::Edge>& border);
    void markDomains(CDT& cdt);

protected:
    CDT m_cdt;  // TODO: place in heap memory
    size_t m_numTriangles;
    size_t m_numVertices;
};

}  // namespace ngs

#endif  // NGSLINESTRINGFILLER_H_
