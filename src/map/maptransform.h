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
#ifndef MAPTRANSFORM_H
#define MAPTRANSFORM_H

#include "ogr_core.h"

#include "matrix.h"
#include "api.h"

namespace ngs {

class MapTransform
{
public:
    MapTransform(int width, int height);
    virtual ~MapTransform();

    int getDisplayWidht() const;
    int getDisplayHeight() const;
    double getRotate(enum ngsDirection dir) const;
    bool setRotate(enum ngsDirection dir, double rotate);

    OGREnvelope getExtent() const;
    OGRRawPoint getCenter() const;
    OGRRawPoint worldToDisplay(const OGRRawPoint &pt);
    OGRRawPoint displayToWorld(const OGRRawPoint &pt);
    void setDisplaySize(int width, int height, bool isYAxisInverted);
    bool setScale(double scale);
    bool setCenter(double x, double y);
    bool setScaleAndCenter(double scale, double x, double y);
    bool setExtent(const OGREnvelope& env);
    double getZoom() const;
    double getScale() const;
    Matrix4 getSceneMatrix() const;
    Matrix4 getInvViewMatrix() const;

    bool getXAxisLooped() const;

protected:
    bool updateExtent();
    void initMatrices();
    void setRotateExtent();
protected:
    int m_displayWidht, m_displayHeight;
    OGRRawPoint m_center;
    double m_rotate[3];
    double m_scale, m_scaleScene, m_scaleView, m_scaleWorld;
    OGREnvelope m_extent, m_rotateExtent;
    double m_ratio;
    bool m_YAxisInverted, m_XAxisLooped;
    /* NOTE: sceneMatrix transform from world coordinates to GL coordinates -1 x 1
     * viewMatrix transform from GL coordinates to display coordinates 640 x 480
     */
    Matrix4 m_sceneMatrix, m_viewMatrix, m_worldToDisplayMatrix;
    Matrix4 m_invSceneMatrix, m_invViewMatrix, m_invWorldToDisplayMatrix;
};

}

#endif // MAPTRANSFORM_H
