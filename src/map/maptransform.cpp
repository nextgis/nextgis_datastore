/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#include "maptransform.h"

#include <algorithm>
#include <iostream>

#include "api_priv.h"
#include "ds/geometry.h"

namespace ngs {

constexpr double DEFAULT_RATIO = 1.0;
constexpr unsigned short TILE_ZIE = 256;
constexpr unsigned short MAX_TILES_COUNT = 4096; // 4096 * (4 + 4 + 1 + 8 * 4) = 164 kb

MapTransform::MapTransform(int width, int height) :
    m_displayWidht(width),
    m_displayHeight(height),
    m_iniZoom(0.0),
    m_rotate{0.0},
    m_ratio(DEFAULT_RATIO),
    m_YAxisInverted(false),
    m_XAxisLooped(true)
{
    setExtent(DEFAULT_BOUNDS);
    setDisplaySize(width, height, false);
}


bool MapTransform::setRotate(enum ngsDirection dir, double rotate)
{
    m_rotate[dir] = rotate;
    return updateExtent();
}


void MapTransform::setDisplaySize(int width, int height, bool isYAxisInverted)
{
    m_displayWidht = width;
    m_displayHeight = height;
    m_iniZoom = std::min(double(m_displayWidht) / TILE_ZIE,
                         double(m_displayHeight) / TILE_ZIE);

    m_YAxisInverted = isYAxisInverted;

//    double scaleX = fabs(double(m_displayWidht)) * .5;
//    double scaleY = fabs(double(m_displayHeight)) * .5;
//    m_scaleView = std::min(scaleX, scaleY);

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

bool MapTransform::setExtent(const Envelope &env)
{
    m_extent = env;
    m_center = m_extent.getCenter();
    m_extent.setRatio(m_ratio);

    if(m_XAxisLooped) {
        while (m_extent.getMinX() > DEFAULT_BOUNDS.getMinX()) {
            m_extent.setMinX(m_extent.getMinX() - DEFAULT_BOUNDS_X2.getMaxX());
            m_extent.setMaxX(m_extent.getMaxX() - DEFAULT_BOUNDS_X2.getMaxX());
        }
        while (m_extent.getMaxX() < DEFAULT_BOUNDS.getMinX()) {
            m_extent.setMinX(m_extent.getMinX() + DEFAULT_BOUNDS_X2.getMaxX());
            m_extent.setMaxX(m_extent.getMaxX() + DEFAULT_BOUNDS_X2.getMaxX());
        }
    }

    double w = m_extent.getWidth();
    double h = m_extent.getHeight();
    double scaleX = std::fabs(double(m_displayWidht) / w);
    double scaleY = std::fabs(double(m_displayHeight) / h);
    m_scale = std::min(scaleX, scaleY);

//    scaleX = 1.0 / w;
//    scaleY = 1.0 / h;
//    m_scaleScene = std::min(scaleX, scaleY);

    scaleX = w / DEFAULT_BOUNDS_X2.getMaxX();
    scaleY = h / DEFAULT_BOUNDS_X2.getMaxY();
    m_scaleWorld = 1 / std::min(scaleX, scaleY);

    initMatrices();

    if(isEqual(m_rotate[ngsDirection::DIR_Z], 0.0)) {
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
    m_extent.setMinX(m_center.x - halfWidth);
    m_extent.setMaxX(m_center.x + halfWidth);
    m_extent.setMinY(m_center.y - halfHeight);
    m_extent.setMaxY(m_center.y + halfHeight);

//    double scaleX = 1.0 / (halfWidth + halfWidth);
//    double scaleY = 1.0 / (halfHeight + halfHeight);
//    m_scaleScene = std::min(scaleX, scaleY);

    double scaleX = halfWidth / DEFAULT_BOUNDS.getMaxX();
    double scaleY = halfHeight / DEFAULT_BOUNDS.getMaxY();
    m_scaleWorld = 1 / std::min(scaleX, scaleY);

    if(m_XAxisLooped) {
        while (m_extent.getMinX() > DEFAULT_BOUNDS_X2.getMaxX()) {
            m_extent.setMinX(m_extent.getMinX() - DEFAULT_BOUNDS_X2.getMaxX());
            m_extent.setMaxX(m_extent.getMaxX() - DEFAULT_BOUNDS_X2.getMaxX());
        }
        while (m_extent.getMaxX() < DEFAULT_BOUNDS_X2.getMinX()) {
            m_extent.setMinX(m_extent.getMinX() + DEFAULT_BOUNDS_X2.getMaxX());
            m_extent.setMaxX(m_extent.getMaxX() + DEFAULT_BOUNDS_X2.getMaxX());
        }
    }

    initMatrices();

    if(isEqual(m_rotate[ngsDirection::DIR_Z], 0.0)){
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
    m_sceneMatrix.clear();

    if (m_YAxisInverted) {
        m_sceneMatrix.ortho(m_extent.getMinX(), m_extent.getMaxX(),
                            m_extent.getMaxY(), m_extent.getMinY(),
                            DEFAULT_BOUNDS.getMinX(), DEFAULT_BOUNDS.getMaxX());
    } else {
        m_sceneMatrix.ortho(m_extent.getMinX(), m_extent.getMaxX(),
                            m_extent.getMinY(), m_extent.getMaxY(),
                            DEFAULT_BOUNDS.getMinX(), DEFAULT_BOUNDS.getMaxX());
    }


    if(!isEqual(m_rotate[ngsDirection::DIR_X], 0.0)){
        m_sceneMatrix.rotateX(m_rotate[ngsDirection::DIR_X]);
    }

    /* TODO: no idea about Y axis rotation
    if(!isEqual(m_rotate[ngsDirection::Y], 0.0)){
        m_sceneMatrix.rotateY (m_rotate[ngsDirection::Y]);
    }
    */

    // world -> scene inv matrix
    m_invSceneMatrix = m_sceneMatrix;
    m_invSceneMatrix.invert();

    // scene -> view inv matrix
    m_invViewMatrix.clear();

    double maxDeep = std::max(m_displayWidht, m_displayHeight);

    m_invViewMatrix.ortho(0, m_displayWidht, 0, m_displayHeight, 0, maxDeep);

    if(!isEqual(m_rotate[ngsDirection::DIR_X], 0.0)){
        m_invViewMatrix.rotateX(-m_rotate[ngsDirection::DIR_X]);
    }

    // scene -> view matrix
    m_viewMatrix = m_invViewMatrix;
    m_viewMatrix.invert();

    m_worldToDisplayMatrix = m_viewMatrix;
    m_worldToDisplayMatrix.multiply(m_sceneMatrix);

    m_invWorldToDisplayMatrix = m_invSceneMatrix;
    m_invWorldToDisplayMatrix.multiply(m_invViewMatrix);

    // Z axis rotation
    if(!isEqual(m_rotate[ngsDirection::DIR_Z], 0.0)){
        OGRRawPoint center = m_extent.getCenter();
        m_sceneMatrix.translate(center.x, center.y, 0);
        m_sceneMatrix.rotateZ(m_rotate[ngsDirection::DIR_Z]);
        m_sceneMatrix.translate(-center.x, -center.y, 0);

        m_worldToDisplayMatrix.rotateZ(-m_rotate[ngsDirection::DIR_Z]);
        m_invWorldToDisplayMatrix.rotateZ(m_rotate[ngsDirection::DIR_Z]);
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
    m_rotateExtent.setMinX(pt.x);
    m_rotateExtent.setMaxX(pt.x);
    m_rotateExtent.setMinY(pt.y);
    m_rotateExtent.setMaxY(pt.y);


    for(unsigned char i = 1; i < 4; ++i) {
        pt = m_invWorldToDisplayMatrix.project (inPt[i]);
        if(pt.x > m_rotateExtent.getMaxX())
            m_rotateExtent.setMaxX(pt.x);
        if(pt.x < m_rotateExtent.getMinX())
            m_rotateExtent.setMinX(pt.x);
        if(pt.y > m_rotateExtent.getMaxY())
            m_rotateExtent.setMaxY(pt.y);
        if(pt.y < m_rotateExtent.getMinY())
            m_rotateExtent.setMinY(pt.y);
    }
}

unsigned char MapTransform::getZoom() const {
    double retVal = m_iniZoom;
    if (m_scaleWorld > 1) {
        retVal += lg(m_scaleWorld);
    } else if (m_scaleWorld < 1) {
        retVal -= lg(1.0 / m_scaleWorld);
    }
    return retVal < 0 ? 0 : static_cast<unsigned char>(retVal + 0.5) - 2;
}

std::vector<TileItem> MapTransform::getTilesForExtent(
        const Envelope &extent, unsigned char zoom, bool reverseY, bool unlimitX)
{
    Envelope env;
    std::vector<TileItem> result;
    if(zoom == 0) { // If zoom 0 - return one tile
        env = DEFAULT_BOUNDS;
        TileItem item = { {0, 0, zoom, 0}, env };
        result.push_back (item);
        return result;
    }
    int tilesInMapOneDim = 1 << zoom;
    double halfTilesInMapOneDim = tilesInMapOneDim * 0.5;
    double tilesSizeOneDim = DEFAULT_BOUNDS.getMaxX() / halfTilesInMapOneDim;
    int begX = static_cast<int>( floor(extent.getMinX() / tilesSizeOneDim +
                                       halfTilesInMapOneDim) );
    int begY = static_cast<int>( floor(extent.getMinY() / tilesSizeOneDim +
                                       halfTilesInMapOneDim) );
    int endX = static_cast<int>( ceil(extent.getMaxX() / tilesSizeOneDim +
                                      halfTilesInMapOneDim) );
    int endY = static_cast<int>( ceil(extent.getMaxY() / tilesSizeOneDim +
                                      halfTilesInMapOneDim) );
    if(begY == endY) {
        endY++;
    }
    if(begX == endX) {
        endX++;
    }
    if(begY < 0) {
        begY = 0;
    }
    if(endY > tilesInMapOneDim) {
        endY = tilesInMapOneDim;
    }

    // This block unlimited X scroll of the map
    if(!unlimitX) {
        if(begX < 0) {
            begX = 0;
        }
        if(endX > tilesInMapOneDim) {
            endX = tilesInMapOneDim;
        }
    }
    else {
        if(begX < -tilesInMapOneDim) {
            begX = -tilesInMapOneDim;
        }
        if(endX >= tilesInMapOneDim + tilesInMapOneDim) {
            endX = tilesInMapOneDim + tilesInMapOneDim;
        }
    }

    // Normal fill from left bottom corner
    int realX, realY;
    char crossExt;
    size_t reserveSize = static_cast<size_t>((endX - begX) * (endY - begY));
    if(reserveSize > MAX_TILES_COUNT)
        reserveSize = MAX_TILES_COUNT;
    result.reserve(reserveSize);
    double fullBoundsMinX = DEFAULT_BOUNDS.getMinX();
    double fullBoundsMinY = DEFAULT_BOUNDS.getMinY();
    for (int x = begX; x < endX; ++x) {
        for (int y = begY; y < endY; ++y) {
            realX = x;
            crossExt = 0;
            if (realX < 0) {
                crossExt = -1;
                realX += tilesInMapOneDim;
            } else if (realX >= tilesInMapOneDim) {
                crossExt = 1;
                realX -= tilesInMapOneDim;
            }

            realY = y;
            if (reverseY) {
                realY = tilesInMapOneDim - y - 1;
            }

            if (realY < 0 || realY >= tilesInMapOneDim) {
                continue;
            }

            double minX = fullBoundsMinX + realX * tilesSizeOneDim;
            double minY = fullBoundsMinY + realY * tilesSizeOneDim;
            env.setMinX(minX);
            env.setMaxX(minX + tilesSizeOneDim);
            env.setMinY(minY);
            env.setMaxY(minY + tilesSizeOneDim);
            TileItem item = { {realX, realY, zoom, crossExt}, env };
            result.push_back(item);

            if(result.size() > MAX_TILES_COUNT) { // Limit for tiles array size
                return result;
            }
        }
    }

    return result;
}

/*
double ngs::getPixelSize(int zoom)
{
    int tilesInMapOneDim = 1 << zoom;
    long sizeOneDimPixels = tilesInMapOneDim * DEFAULT_TILE_SIZE;
    return DEFAULT_MAX_X * 2.0 / sizeOneDimPixels;
}
*/

} // namespace ngs
