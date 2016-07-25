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
#include "geometryutil.h"
#include "constants.h"

OGRGeometry *ngs::simplifyGeometry(const OGRGeometry *geometry, double distance)
{
    OGREnvelope env;
    geometry->getEnvelope (&env);
    double envH = getEnvelopeHeight (env);
    double envW = getEnvelopeWidth (env);

    if(envH < distance && envW < distance) {
        return new OGRPoint(env.MinX + envW * .5, env.MinY + envH * .5);
    }

    double tripleDist = distance * 3;
    if(envH < tripleDist && envW < tripleDist) {
        OGRLineString *out = new OGRLineString;
        out->addPoint(env.MinX, env.MinY);
        out->addPoint(env.MaxX, env.MaxY);
        return out;
    }

    // TODO: do we need this?
    // geometry->SimplifyPreserveTopology
    return geometry->Simplify (distance);
}


OGRRawPoint ngs::getEnvelopeCenter(const OGREnvelope &env)
{
    OGRRawPoint pt;
    pt.x = env.MinX + getEnvelopeWidth(env) * .5;
    pt.y = env.MinY + getEnvelopeHeight(env) * .5;
    return pt;
}

OGREnvelope ngs::rotateEnvelope(const OGREnvelope &env, double angle)
{
    double cosA = cos(angle);
    double sinA = sin(angle);
    OGREnvelope output;
    output.MinX = BIG_VALUE;
    output.MinY = BIG_VALUE;
    output.MaxX = -BIG_VALUE;
    output.MaxY = -BIG_VALUE;

    OGRRawPoint points[4];
    points[0] = OGRRawPoint(env.MinX, env.MinY);
    points[1] = OGRRawPoint(env.MaxX, env.MinY);
    points[2] = OGRRawPoint(env.MaxX, env.MaxY);
    points[3] = OGRRawPoint(env.MinX, env.MaxY);

    double x, y;
    for(OGRRawPoint &pt : points){
        x = pt.x * cosA - pt.y * sinA;
        y = pt.x * sinA + pt.y * cosA;

        if(x < output.MinX)
            output.MinX = x;
        if(x > output.MaxX)
            output.MaxX = x;

        if(y < output.MinY)
            output.MinY = y;
        if(y > output.MaxY)
            output.MaxY = y;
    }
    return output;
}

OGREnvelope ngs::setEnvelopeRatio(const OGREnvelope &env, double ratio)
{
    OGREnvelope output = env;

    double halfWidth = getEnvelopeWidth(env) * .5;
    double halfHeight = getEnvelopeHeight(env) * .5;
    OGRRawPoint center(env.MinX + halfWidth,  env.MinY + halfHeight);
    double envRatio = halfWidth / halfHeight;
    if(isEqual(envRatio, ratio))
        return output;
    if(ratio > envRatio) //increase width
    {
        double width = halfHeight * ratio;
        output.MaxX = center.x + width;
        output.MinX = center.x - width;
    }
    else					//increase height
    {
        double height = halfWidth / ratio;
        output.MaxY = center.y + height;
        output.MinY = center.y - height;
    }
    return output;
}

OGREnvelope ngs::setEnvelope(double minX, double maxX, double minY,
                                      double maxY)
{
    OGREnvelope env;
    env.MinX = minX;
    env.MaxX = maxX;
    env.MinY = minY;
    env.MaxY = maxY;
    return env;
}

double ngs::getEnvelopeWidth(const OGREnvelope &env)
{
    return env.MaxX - env.MinX;
}

double ngs::getEnvelopeHeight(const OGREnvelope &env)
{
    return env.MaxY - env.MinY;
}
