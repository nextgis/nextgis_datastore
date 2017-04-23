/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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
#include "geometry.h"

#include "api_priv.h"


namespace ngs {

constexpr double BIG_VALUE = 10000000.0;
constexpr double DBLNAN = 0.0;

Envelope::Envelope() :
    m_minX(DBLNAN),
    m_minY(DBLNAN),
    m_maxX(DBLNAN),
    m_maxY(DBLNAN)
{

}

bool Envelope::isInit() const
{
    return !isEqual(m_minX, DBLNAN) || !isEqual(m_minY, DBLNAN) ||
           !isEqual(m_maxX, DBLNAN) || !isEqual(m_maxY, DBLNAN);
}

OGRRawPoint Envelope::getCenter() const
{
    OGRRawPoint pt;
    pt.x = m_minX + getWidth() * .5;
    pt.y = m_minY + getHeight() * .5;
    return pt;
}

void Envelope::rotate(double angle)
{
    double cosA = cos(angle);
    double sinA = sin(angle);

    OGRRawPoint points[4];
    points[0] = OGRRawPoint(m_minX, m_minY);
    points[1] = OGRRawPoint(m_maxX, m_minY);
    points[2] = OGRRawPoint(m_maxX, m_maxY);
    points[3] = OGRRawPoint(m_minX, m_maxY);

    m_minX = BIG_VALUE;
    m_minY = BIG_VALUE;
    m_maxX = -BIG_VALUE;
    m_maxY = -BIG_VALUE;

    double x, y;
    for(OGRRawPoint &pt : points){
        x = pt.x * cosA - pt.y * sinA;
        y = pt.x * sinA + pt.y * cosA;

        if(x < m_minX)
            m_minX = x;
        if(x > m_maxX)
            m_maxX = x;

        if(y < m_minY)
            m_minY = y;
        if(y > m_maxY)
            m_maxY = y;
    }
}

void Envelope::setRatio(double ratio)
{
    double halfWidth = getWidth() * .5;
    double halfHeight = getHeight() * .5;
    OGRRawPoint center(m_minX + halfWidth,  m_minY + halfHeight);
    double envRatio = halfWidth / halfHeight;
    if(isEqual(envRatio, ratio))
        return;
    if(ratio > envRatio) //increase width
    {
        double width = halfHeight * ratio;
        m_maxX = center.x + width;
        m_minX = center.x - width;
    }
    else					//increase height
    {
        double height = halfWidth / ratio;
        m_maxY = center.y + height;
        m_minY = center.y - height;
    }
}

void Envelope::resize(double value)
{
    double w = getWidth() * .5;
    double h = getHeight() * .5;
    double x = m_minX + w;
    double y = m_minY + h;

    w *= value;
    h *= value;

    m_minX = x - w;
    m_maxX = x + w;
    m_minY = y - h;
    m_maxY = y + h;
}

GeometryPtr Envelope::toGeometry(OGRSpatialReference * const spatialRef)
{
    if(!isInit())
        return GeometryPtr();
    OGRLinearRing ring;
    ring.addPoint(m_minX, m_minY);
    ring.addPoint(m_minX, m_maxY);
    ring.addPoint(m_maxX, m_maxY);
    ring.addPoint(m_maxX, m_minY);
    ring.closeRings();

    OGRPolygon* rgn = new OGRPolygon();
    rgn->addRing(&ring);
    rgn->flattenTo2D();
    if(nullptr != spatialRef) {
        spatialRef->Reference();
        rgn->assignSpatialReference(spatialRef);
    }
    return GeometryPtr(static_cast<OGRGeometry*>(rgn));
}

OGREnvelope Envelope::getOgrEnvelope() const
{
    OGREnvelope env;
    env.MaxX = m_maxX;
    env.MaxY = m_maxY;
    env.MinX = m_minX;
    env.MinY = m_minY;
    return env;
}

Normals ngsGetNormals(const OGRPoint &beg, const OGRPoint &end)
{
    float deltaX = static_cast<float>(end.getX() - beg.getX());
    float deltaY = static_cast<float>(end.getY() - beg.getY());

    float normX = -deltaY;
    float normY = deltaX;

    float norm_length = std::sqrt(normX * normX + normY * normY);

    Normals out;
    if(norm_length == 0.0f)
        norm_length = 0.01f;

    //normalize the normal vector
    normX /= norm_length;
    normY /= norm_length;

    out.left = {normX, normY};
    out.right = {-normX, -normY};

    return out;
}

} // namespace ngs

//const static array<pair<double, char>, 4> gSampleDists = {
//{{ getPixelSize(6)  * SAMPLE_DISTANCE_PX, 6 },
// { getPixelSize(9)  * SAMPLE_DISTANCE_PX, 9 },
// { getPixelSize(12) * SAMPLE_DISTANCE_PX, 12 },
// { getPixelSize(15) * SAMPLE_DISTANCE_PX, 15 }
//}};

//OGRGeometry* simplifyGeometry(const OGRGeometry* geometry, double distance);
//OGRGeometry *ngs::simplifyGeometry(const OGRGeometry *geometry, double distance)
//{
//    OGREnvelope env;
//    geometry->getEnvelope (&env);
//    double envH = getEnvelopeHeight (env);
//    double envW = getEnvelopeWidth (env);
//    double tripleDist = distance / 3;

//    if(envH < tripleDist || envW < tripleDist) {
//        return new OGRPoint(env.MinX + envW * .5, env.MinY + envH * .5);
//    }

//    if(envH < distance || envW < distance) {
//        OGRLineString *out = new OGRLineString;
//        out->addPoint(env.MinX, env.MinY);
//        out->addPoint(env.MaxX, env.MaxY);
//        return out;
//    }

//    // TODO: do we need this?
//    // geometry->SimplifyPreserveTopology
//    return geometry->Simplify (distance * 3);
//}
