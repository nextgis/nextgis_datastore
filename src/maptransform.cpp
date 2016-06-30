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
#include "maptransform.h"
#include "constants.h"

#include <algorithm>
#include <iostream>

#define DEFAULT_MAX_X 180.0
#define DEFAULT_MAX_Y 90.0
#define DEFAULT_MIN_X -DEFAULT_MAX_X
#define DEFAULT_MIN_Y -DEFAULT_MAX_Y
#define BIG_VALUE 10000000.0

using namespace std;
using namespace ngs;

MapTransform::MapTransform(int width, int height) : m_rotate(0)
{
    setDisplaySize(width, height);
}

MapTransform::~MapTransform()
{

}

int MapTransform::getDisplayHeight() const
{
    return m_displayHeight;
}
int MapTransform::getDisplayWidht() const
{
    return m_displayWidht;
}

bool MapTransform::isSizeChanged() const
{
    return m_sizeChanged;
}

void MapTransform::setSizeChanged(bool sizeChanged)
{
    m_sizeChanged = sizeChanged;
}

double MapTransform::getRotate() const
{
    return m_rotate;
}

void MapTransform::setRotate(double rotate)
{
    m_rotate = rotate;
}

OGREnvelope MapTransform::getExtent() const
{
    return m_extent;
}

OGRRawPoint MapTransform::getCenter() const
{
    return m_center;
}

OGRRawPoint MapTransform::worldToDisplay(const OGRRawPoint &pt)
{
    return m_worldToDisplayMatrix.project (pt);
}

OGRRawPoint MapTransform::displayToWorld(const OGRRawPoint &pt)
{
    return m_invWorldToDisplayMatrix.project (pt);
}

void MapTransform::setDisplaySize(int width, int height)
{
    m_sizeChanged = true;
    m_displayWidht = width;
    m_displayHeight = height;

    double scaleX = fabs(double(m_displayWidht)) * .5;
    double scaleY = fabs(double(m_displayHeight)) * .5;

    m_scaleView = min(scaleX, scaleY);

    m_ratio = double(width) / height;
}

bool MapTransform::setScale(double scale)
{
    m_scale = scale;
    return updateExtent();
}

bool MapTransform::setCenter(double x, double y)
{
    m_center.x = x;
    m_center.y = y;
    return updateExtent();
}

bool MapTransform::setScaleAndCenter(double scale, double x, double y)
{
    m_scale = scale;
    m_center.x = x;
    m_center.y = y;
    return updateExtent();
}

bool MapTransform::setExtent(const OGREnvelope &env)
{
    m_center = getEnvelopeCenter (env);
    m_extent = setEnvelopeRatio(env, m_ratio);
    double w = getEnvelopeWidth (env);
    double h = getEnvelopeHeight (env);
    double scaleX = fabs(double(m_displayWidht) / w);
    double scaleY = fabs(double(m_displayHeight) / h);
    m_scale = min(scaleX, scaleY);

    scaleX = 1.0 / w;
    scaleY = 1.0 / h;
    m_scaleScene = min(scaleX, scaleY);

    if(!isEqual(m_rotate, 0.0)){
        m_extent = rotateEnvelope (m_extent, m_rotate);
    }

    initMatrices();
    return true; // return false if extent modified to fit limits
}

OGRRawPoint MapTransform::getEnvelopeCenter(const OGREnvelope &env)
{
    OGRRawPoint pt;
    pt.x = env.MinX + getEnvelopeWidth(env) * .5;
    pt.y = env.MinY + getEnvelopeHeight(env) * .5;
    return pt;
}

OGREnvelope MapTransform::rotateEnvelope(const OGREnvelope &env, double angle)
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

OGREnvelope MapTransform::setEnvelopeRatio(const OGREnvelope &env, double ratio)
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

double MapTransform::getEnvelopeWidth(const OGREnvelope &env)
{
    return env.MaxX - env.MinX;
}

double MapTransform::getEnvelopeHeight(const OGREnvelope &env)
{
    return env.MaxY - env.MinY;
}

bool MapTransform::updateExtent()
{
    double doubleScale = m_scale * 2;
    double halfWidth = double(m_displayWidht) / doubleScale;
    double halfHeight = double(m_displayWidht) / doubleScale;
    m_extent.MinX = m_center.x - halfWidth;
    m_extent.MaxX = m_center.x + halfWidth;
    m_extent.MinY = m_center.y - halfHeight;
    m_extent.MaxY = m_center.y + halfHeight;

    double scaleX = 1.0 / (halfWidth + halfWidth);
    double scaleY = 1.0 / (halfHeight + halfHeight);
    m_scaleScene = min(scaleX, scaleY);

    if(!isEqual(m_rotate, 0.0)){
        m_extent = rotateEnvelope (m_extent, m_rotate);
    }

    initMatrices();
    return true;  // return false if extent modified to fit limits
}

void MapTransform::initMatrices()
{
    m_sceneMatrix.clear ();

    double w = getEnvelopeWidth (m_extent);
    double h = getEnvelopeHeight (m_extent);
    if(w > h){
        double add = (w - h) * .5;
        m_sceneMatrix.ortho (0, w, add, add + h, -1, 1);
    }
    else if(w < h){
        double add = (h - w) * .5;
        m_sceneMatrix.ortho (0, w, -add, h + add, -1, 1);
    }
    else {
        m_sceneMatrix.ortho (0, w, 0, h, -1, 1);
    }

    if(!isEqual(m_rotate, 0.0)){
        // TODO: rotate m_sceneMatrix
    }

    m_invSceneMatrix = m_sceneMatrix;
    m_invSceneMatrix.invert ();

    m_invViewMatrix.clear ();
    w = m_displayWidht;
    h = m_displayHeight;
    if(w > h){
        double add = (w - h) * .5;
        m_invViewMatrix.ortho (0, w, add, add + h, -1, 1);
    }
    else if(w < h){
        double add = (h - w) * .5;
        m_invViewMatrix.ortho (0, w, -add, h + add, -1, 1);
    }
    else {
        m_invViewMatrix.ortho (0, w, 0, h, -1, 1);
    }

    m_viewMatrix = m_invViewMatrix;
    m_viewMatrix.invert ();

    m_worldToDisplayMatrix = m_viewMatrix;
    m_worldToDisplayMatrix.multiply (m_sceneMatrix);

    m_invWorldToDisplayMatrix = m_invViewMatrix;
    m_invWorldToDisplayMatrix.multiply (m_invSceneMatrix);
}

double MapTransform::getScale() const
{
    return m_scale;
}

double MapTransform::getZoom() const {
    return log(m_scale) / M_LN2;
}
