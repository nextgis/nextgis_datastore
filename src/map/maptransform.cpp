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

#include <algorithm>
#include <iostream>

#include "ngstore/util/constants.h"
#include "util/geometryutil.h"

using namespace std;
using namespace ngs;

// TODO: DEFAULT_MIN_X, DEFAULT_MAX_X, DEFAULT_MIN_Y, DEFAULT_MAX_Y - special struct not definition

MapTransform::MapTransform(int width, int height) : m_displayWidht(width),
    m_displayHeight(height), m_rotate{0},
    m_ratio(DEFAULT_RATIO), m_XAxisLooped(true), m_YAxisInverted(false)
{
    setExtent(setEnvelope(DEFAULT_MIN_X, DEFAULT_MAX_X, DEFAULT_MIN_Y,
                          DEFAULT_MAX_Y));
    setDisplaySize(width, height, false);
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

double MapTransform::getRotate(enum ngsDirection dir) const
{
    return m_rotate[dir];
}

bool MapTransform::setRotate(enum ngsDirection dir, double rotate)
{
    m_rotate[dir] = rotate;
    return updateExtent();
}

OGREnvelope MapTransform::getExtent() const
{
    return m_rotateExtent;
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

void MapTransform::setDisplaySize(int width, int height, bool isYAxisInverted)
{
    m_displayWidht = width;
    m_displayHeight = height;
    m_YAxisInverted = isYAxisInverted;

    double scaleX = fabs(double(m_displayWidht)) * .5;
    double scaleY = fabs(double(m_displayHeight)) * .5;

    m_scaleView = min(scaleX, scaleY);

    m_ratio = double(width) / height;

    updateExtent();
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

    if(m_XAxisLooped) {
        while (m_extent.MinX > DEFAULT_MAX_X) {
            m_extent.MinX -= DEFAULT_MAX_X2;
            m_extent.MaxX -= DEFAULT_MAX_X2;
        }
        while (m_extent.MaxX < DEFAULT_MIN_X) {
            m_extent.MinX += DEFAULT_MAX_X2;
            m_extent.MaxX += DEFAULT_MAX_X2;
        }
    }

    double w = getEnvelopeWidth (env);
    double h = getEnvelopeHeight (env);
    double scaleX = fabs(double(m_displayWidht) / w);
    double scaleY = fabs(double(m_displayHeight) / h);
    m_scale = min(scaleX, scaleY);

    scaleX = 1.0 / w;
    scaleY = 1.0 / h;
    m_scaleScene = min(scaleX, scaleY);

    scaleX = w / DEFAULT_MAX_X2;
    scaleY = h / DEFAULT_MAX_Y2;
    m_scaleWorld = 1 / min(scaleX, scaleY);

    initMatrices();

    if(isEqual(m_rotate[ngsDirection::Z], 0.0)) {
        m_rotateExtent = m_extent;
    }
    else {
        //m_rotateExtent = rotateEnvelope (m_extent, m_rotate[ngsDirection::Z]);
        setRotateExtent();
    }

    return true; // return false if extent modified to fit limits
}

bool MapTransform::updateExtent()
{
    double doubleScale = m_scale * 2;
    double halfWidth = double(m_displayWidht) / doubleScale;
    double halfHeight = double(m_displayHeight) / doubleScale;
    m_extent.MinX = m_center.x - halfWidth;
    m_extent.MaxX = m_center.x + halfWidth;
    m_extent.MinY = m_center.y - halfHeight;
    m_extent.MaxY = m_center.y + halfHeight;

    double scaleX = 1.0 / (halfWidth + halfWidth);
    double scaleY = 1.0 / (halfHeight + halfHeight);
    m_scaleScene = min(scaleX, scaleY);

    scaleX = halfWidth / DEFAULT_MAX_X;
    scaleY = halfHeight / DEFAULT_MAX_Y;
    m_scaleWorld = 1 / min(scaleX, scaleY);

    if(m_XAxisLooped) {
        while (m_extent.MinX > DEFAULT_MAX_X) {
            m_extent.MinX -= DEFAULT_MAX_X2;
            m_extent.MaxX -= DEFAULT_MAX_X2;
        }
        while (m_extent.MaxX < DEFAULT_MIN_X) {
            m_extent.MinX += DEFAULT_MAX_X2;
            m_extent.MaxX += DEFAULT_MAX_X2;
        }
    }

    initMatrices();

    if(isEqual(m_rotate[ngsDirection::Z], 0.0)){
        m_rotateExtent = m_extent;
    }
    else {
        //m_rotateExtent = rotateEnvelope (m_extent, m_rotate[ngsDirection::Z]);
        setRotateExtent();
    }

    return true;  // return false if extent modified to fit limits
}

void MapTransform::initMatrices()
{
    // world -> scene matrix
    m_sceneMatrix.clear ();

    if (m_YAxisInverted) {
        m_sceneMatrix.ortho (m_extent.MinX, m_extent.MaxX,
                             m_extent.MaxY, m_extent.MinY, DEFAULT_MIN_X, DEFAULT_MAX_X);
    } else {
        m_sceneMatrix.ortho (m_extent.MinX, m_extent.MaxX,
                             m_extent.MinY, m_extent.MaxY, DEFAULT_MIN_X, DEFAULT_MAX_X);
    }


    if(!isEqual(m_rotate[ngsDirection::X], 0.0)){
        m_sceneMatrix.rotateX (m_rotate[ngsDirection::X]);
    }

    /* TODO: no idea about Y axis rotation
    if(!isEqual(m_rotate[ngsDirection::Y], 0.0)){
        m_sceneMatrix.rotateY (m_rotate[ngsDirection::Y]);
    }
    */

    // world -> scene inv matrix
    m_invSceneMatrix = m_sceneMatrix;
    m_invSceneMatrix.invert ();

    // scene -> view inv matrix
    m_invViewMatrix.clear ();

    double maxDeep = max(m_displayWidht, m_displayHeight);

    m_invViewMatrix.ortho (0, m_displayWidht, 0, m_displayHeight, 0, maxDeep);

    if(!isEqual(m_rotate[ngsDirection::X], 0.0)){
        m_invViewMatrix.rotateX (-m_rotate[ngsDirection::X]);
    }

    // scene -> view matrix
    m_viewMatrix = m_invViewMatrix;
    m_viewMatrix.invert ();

    m_worldToDisplayMatrix = m_viewMatrix;
    m_worldToDisplayMatrix.multiply (m_sceneMatrix);

    m_invWorldToDisplayMatrix = m_invSceneMatrix;
    m_invWorldToDisplayMatrix.multiply (m_invViewMatrix);

    // Z axis rotation
    if(!isEqual(m_rotate[ngsDirection::Z], 0.0)){
        OGRRawPoint center = getEnvelopeCenter(m_extent);
        m_sceneMatrix.translate (center.x, center.y, 0);
        m_sceneMatrix.rotateZ (m_rotate[ngsDirection::Z]);
        m_sceneMatrix.translate (-center.x, -center.y, 0);

        m_worldToDisplayMatrix.rotateZ (-m_rotate[ngsDirection::Z]);
        m_invWorldToDisplayMatrix.rotateZ (m_rotate[ngsDirection::Z]);
    }
}

void MapTransform::setRotateExtent()
{

    OGRRawPoint pt, inPt[4];
    inPt[0] = OGRRawPoint(0, 0);
    inPt[1] = OGRRawPoint(0, m_displayHeight);
    inPt[2] = OGRRawPoint(m_displayWidht, m_displayHeight);
    inPt[3] = OGRRawPoint(m_displayWidht, 0);

    pt = m_invWorldToDisplayMatrix.project (inPt[0]);
    m_rotateExtent.MinX = pt.x;
    m_rotateExtent.MaxX = pt.x;
    m_rotateExtent.MinY = pt.y;
    m_rotateExtent.MaxY = pt.y;


    for(unsigned char i = 1; i < 4; ++i) {
        pt = m_invWorldToDisplayMatrix.project (inPt[i]);
        if(pt.x > m_rotateExtent.MaxX)
            m_rotateExtent.MaxX = pt.x;
        if(pt.x < m_rotateExtent.MinX)
            m_rotateExtent.MinX = pt.x;
        if(pt.y > m_rotateExtent.MaxY)
            m_rotateExtent.MaxY = pt.y;
        if(pt.y < m_rotateExtent.MinY)
            m_rotateExtent.MinY = pt.y;
    }
}

bool MapTransform::getXAxisLooped() const
{
    return m_XAxisLooped;
}

double MapTransform::getScale() const
{
    return m_scale;
}

Matrix4 MapTransform::getSceneMatrix() const
{
    return m_sceneMatrix;
}

Matrix4 MapTransform::getInvViewMatrix() const
{
    return m_invViewMatrix;
}

double MapTransform::getZoom() const {
    double retVal = log(m_scaleWorld) / M_LN2;
    return retVal < 0 ? 0 : retVal;
}
