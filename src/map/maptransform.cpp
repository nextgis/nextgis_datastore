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
#include "glm/gtc/matrix_transform.hpp"

namespace ngs {

constexpr double DEFAULT_RATIO = 1.0;
constexpr unsigned short MAX_TILES_COUNT = 32768; // 1.5 mb // 4096 * (4 + 4 + 1 + 8 * 4) = 164 kb

MapTransform::MapTransform(int width, int height) :
    m_displayWidht(width),
    m_displayHeight(height),
    m_rotate{0.0},
    m_ratio(DEFAULT_RATIO),
    m_YAxisInverted(false),
    m_XAxisLooped(true),
    m_extraZoom(0),
    m_scaleMax(std::numeric_limits<double>::max()), //14.0), //
    m_scaleMin(std::numeric_limits<double>::min()),
    m_extentLimit(DEFAULT_BOUNDS_Y2X4),
    m_reduceFactor(1.0)
{
    setExtent(DEFAULT_BOUNDS);
    setDisplaySize(width, height, false);
}


bool MapTransform::setRotate(enum ngsDirection dir, double rotate)
{
    m_rotate[dir] = rotate;
    return updateExtent();
}

Envelope MapTransform::worldToDisplay(const Envelope &env) const
{
    OGRRawPoint pt[4];
    pt[0] = worldToDisplay(OGRRawPoint(env.minX(), env.minY()));
    pt[1] = worldToDisplay(OGRRawPoint(env.minX(), env.maxY()));
    pt[2] = worldToDisplay(OGRRawPoint(env.maxX(), env.maxY()));
    pt[3] = worldToDisplay(OGRRawPoint(env.maxX(), env.minY()));

    OGRRawPoint minPt(BIG_VALUE, BIG_VALUE);
    OGRRawPoint maxPt(-BIG_VALUE, -BIG_VALUE);

    for(const OGRRawPoint &cpt : pt) {
        if(cpt.x < minPt.x) {
            minPt.x = cpt.x;
        }
        if(cpt.x > maxPt.x) {
            maxPt.x = cpt.x;
        }

        if(cpt.y < minPt.y) {
            minPt.y = cpt.y;
        }
        if(cpt.y > maxPt.y) {
            maxPt.y = cpt.y;
        }
    }

    return Envelope(minPt.x, minPt.y, maxPt.x, maxPt.y);
}

Envelope MapTransform::displayToWorld(const Envelope &env) const
{
    OGRRawPoint pt[4];
    pt[0] = displayToWorld(OGRRawPoint(env.minX(), env.minY()));
    pt[1] = displayToWorld(OGRRawPoint(env.minX(), env.maxY()));
    pt[2] = displayToWorld(OGRRawPoint(env.maxX(), env.maxY()));
    pt[3] = displayToWorld(OGRRawPoint(env.maxX(), env.minY()));

    OGRRawPoint minPt(BIG_VALUE, BIG_VALUE);
    OGRRawPoint maxPt(-BIG_VALUE, -BIG_VALUE);

    for(const OGRRawPoint &cpt : pt) {
        if(cpt.x < minPt.x) {
            minPt.x = cpt.x;
        }
        if(cpt.x > maxPt.x) {
            maxPt.x = cpt.x;
        }

        if(cpt.y < minPt.y) {
            minPt.y = cpt.y;
        }
        if(cpt.y > maxPt.y) {
            maxPt.y = cpt.y;
        }
    }

    return Envelope(minPt.x, minPt.y, maxPt.x, maxPt.y);
}

OGRRawPoint MapTransform::getMapDistance(double w, double h) const
{
    OGRRawPoint beg = displayToWorld(OGRRawPoint(0, 0));
    OGRRawPoint end = displayToWorld(OGRRawPoint(w, h));
    return OGRRawPoint(end.x - beg.x, end.y - beg.y);
}

OGRRawPoint MapTransform::getDisplayLength(double w, double h) const
{
    OGRRawPoint beg = worldToDisplay(OGRRawPoint(0, 0));
    OGRRawPoint end = worldToDisplay(OGRRawPoint(w, h));
    return OGRRawPoint(end.x - beg.x, end.y - beg.y);
}

void MapTransform::setDisplaySize(int width, int height, bool isYAxisInverted)
{
    m_displayWidht = static_cast<int>(double(width) / m_reduceFactor);
    m_displayHeight = static_cast<int>(double(height) / m_reduceFactor);

    m_YAxisInverted = isYAxisInverted;

//    double scaleX = fabs(double(m_displayWidht)) * .5;
//    double scaleY = fabs(double(m_displayHeight)) * .5;
//    m_scaleView = std::min(scaleX, scaleY);

    m_ratio = double(width) / height;

    updateExtent();
}

bool MapTransform::setScale(double scale)
{
    m_scale = fixScale(scale);
    return updateExtent();
}

bool MapTransform::setCenter(double x, double y)
{
    m_center = fixCenter(x, y);
    return updateExtent();
}

bool MapTransform::setScaleAndCenter(double scale, double x, double y)
{
    m_scale = fixScale(scale);
    m_center = fixCenter(x, y);
    return updateExtent();
}

bool MapTransform::setExtent(const Envelope &env)
{
    double w = env.width();
    double h = env.height();
    double scaleX = std::fabs(double(m_displayWidht) / w);
    double scaleY = std::fabs(double(m_displayHeight) / h);
    m_scale = fixScale(std::min(scaleX, scaleY));
    auto center = env.center();
    m_center = fixCenter(center.x, center.y);
    return updateExtent();
/*
    m_extent = env;
    m_extent.setRatio(m_ratio);

    fixExtent();

    double w = m_extent.width();
    double h = m_extent.height();
    double scaleX = std::fabs(double(m_displayWidht) / w);
    double scaleY = std::fabs(double(m_displayHeight) / h);
    m_scale = fixScale(std::min(scaleX, scaleY));

//    scaleX = 1.0 / w;
//    scaleY = 1.0 / h;
//    m_scaleScene = std::min(scaleX, scaleY);

//    scaleX = w / DEFAULT_BOUNDS_X2.getMaxX();
//    scaleY = h / DEFAULT_BOUNDS_X2.getMaxY();
    m_scaleWorld = 1 / m_scale;//std::min(scaleX, scaleY);

    initMatrices();

    if(isEqual(m_rotate[DIR_Z], 0.0)) {
        m_rotateExtent = m_extent;
    }
    else {
        //m_rotateExtent = rotateEnvelope (m_extent, m_rotate[Z]);
        setRotateExtent();
    }

    return true; // return false if extent modified to fit limits
    */
}

bool MapTransform::updateExtent()
{
    double doubleScale = m_scale * 2.0;
    double halfWidth = double(m_displayWidht) / doubleScale;
    double halfHeight = double(m_displayHeight) / doubleScale;
    m_extent.setMinX(m_center.x - halfWidth);
    m_extent.setMaxX(m_center.x + halfWidth);
    m_extent.setMinY(m_center.y - halfHeight);
    m_extent.setMaxY(m_center.y + halfHeight);

//    double scaleX = 1.0 / (halfWidth + halfWidth);
//    double scaleY = 1.0 / (halfHeight + halfHeight);
//    m_scaleScene = std::min(scaleX, scaleY);

//    double scaleX = halfWidth / DEFAULT_BOUNDS.getMaxX();
//    double scaleY = halfHeight / DEFAULT_BOUNDS.getMaxY();
    m_scaleWorld = 1.0 / m_scale;//std::min(scaleX, scaleY);

    fixExtent();

    initMatrices();

    if(isEqual(m_rotate[DIR_Z], 0.0)){
        m_rotateExtent = m_extent;
    }
    else {
        //m_rotateExtent = rotateEnvelope (m_extent, m_rotate[Z]);
        setRotateExtent();
    }

    return true;  // return false if extent modified to fit limits
}

void MapTransform::initMatrices()
{
    // world -> scene matrix
    m_sceneMatrix = glm::ortho(static_cast<float>(m_extent.minX()),
                               static_cast<float>(m_extent.maxX()),
                               static_cast<float>(m_extent.minY()),
                               static_cast<float>(m_extent.maxY()),
                               static_cast<float>(DEFAULT_BOUNDS.minX()),
                               static_cast<float>(DEFAULT_BOUNDS.maxX()));

    // TODO:
//    if(!isEqual(m_rotate[DIR_X], 0.0)){
//        m_sceneMatrix.rotateX(m_rotate[DIR_X]);
//    }

    /* TODO: no idea about Y axis rotation
    if(!isEqual(m_rotate[Y], 0.0)){
        m_sceneMatrix.rotateY (m_rotate[Y]);
    }
    */

    // world -> scene inv matrix
    m_invSceneMatrix = glm::inverse(m_sceneMatrix);

    // scene -> view inv matrix
//    float maxDeep = std::max(m_displayWidht, m_displayHeight);

    m_invViewMatrix = glm::ortho(0.0f, static_cast<float>(m_displayWidht),
                                 0.0f, static_cast<float>(m_displayHeight),
                                 -1.f, 1.f); //0.0f, maxDeep);

    // TODO:
//    if(!isEqual(m_rotate[DIR_X], 0.0)){
//        m_invViewMatrix.rotateX(-m_rotate[DIR_X]);
//    }

    // scene -> view matrix
    m_viewMatrix = glm::inverse(m_invViewMatrix);

    m_worldToDisplayMatrix = m_viewMatrix * m_sceneMatrix;

    m_invWorldToDisplayMatrix = m_invSceneMatrix * m_invViewMatrix;

    // TODO: Z axis rotation
//    if(!isEqual(m_rotate[DIR_Z], 0.0)){
//        OGRRawPoint center = m_extent.center();
//        m_sceneMatrix.translate(center.x, center.y, 0);
//        m_sceneMatrix.rotateZ(m_rotate[DIR_Z]);
//        m_sceneMatrix.translate(-center.x, -center.y, 0);

//        m_worldToDisplayMatrix.rotateZ(-m_rotate[DIR_Z]);
//        m_invWorldToDisplayMatrix.rotateZ(m_rotate[DIR_Z]);
//    }
}

void MapTransform::setRotateExtent()
{

    glm::vec4 pt, inPt[4];
    inPt[0] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    inPt[1] = glm::vec4(0.0f, m_displayHeight, 0.0f, 1.0f);
    inPt[2] = glm::vec4(m_displayWidht, m_displayHeight, 0.0f, 1.0f);
    inPt[3] = glm::vec4(m_displayWidht, 0.0f, 0.0f, 1.0f);

    pt = m_invWorldToDisplayMatrix * inPt[0];
    m_rotateExtent.setMinX(double(pt[0]));
    m_rotateExtent.setMaxX(double(pt[0]));
    m_rotateExtent.setMinY(double(pt[1]));
    m_rotateExtent.setMaxY(double(pt[1]));


    for(unsigned char i = 1; i < 4; ++i) {
        pt = m_invWorldToDisplayMatrix * inPt[i];
        if(double(pt[0]) > m_rotateExtent.maxX())
            m_rotateExtent.setMaxX(double(pt[0]));
        if(double(pt[0]) < m_rotateExtent.minX())
            m_rotateExtent.setMinX(double(pt[0]));
        if(double(pt[1]) > m_rotateExtent.maxY())
            m_rotateExtent.setMaxY(double(pt[1]));
        if(double(pt[1]) < m_rotateExtent.minY())
            m_rotateExtent.setMinY(double(pt[1]));
    }
}

double MapTransform::fixScale(double scale)
{
    return glm::clamp(scale, m_scaleMin, m_scaleMax);
}

void MapTransform::fixExtent()
{
    if(m_XAxisLooped) {
        while(m_extent.minX() > DEFAULT_BOUNDS.minX() + 5000000) {
            m_extent.setMinX(m_extent.minX() - DEFAULT_BOUNDS_X2.maxX());
            m_extent.setMaxX(m_extent.maxX() - DEFAULT_BOUNDS_X2.maxX());
        }
        while(m_extent.maxX() < DEFAULT_BOUNDS.minX() + 5000000) {
            m_extent.setMinX(m_extent.minX() + DEFAULT_BOUNDS_X2.maxX());
            m_extent.setMaxX(m_extent.maxX() + DEFAULT_BOUNDS_X2.maxX());
        }
    }
    m_extent.fix();
    m_center = m_extent.center();
}

OGRRawPoint MapTransform::fixCenter(double x, double y)
{

    double halfHeight = m_extent.height() / 2;
    if(y - halfHeight < m_extentLimit.minY() ||
            y + halfHeight > m_extentLimit.maxY()) {
        y = m_center.y;
    }

    if(!m_XAxisLooped) {
        double halfWidth = m_extent.width() / 2;
        if(x - halfWidth < m_extentLimit.minX() ||
                x + halfWidth > m_extentLimit.maxX()) {
            x = m_center.x;
        }
    }

    return OGRRawPoint(x, y);
}

unsigned char MapTransform::getZoom() const {
    // 156412.0 is m/pixel on 0 zoom. See http://wiki.openstreetmap.org/wiki/Zoom_levels
    double metersPerTile = 156543.04; //156412.0; // DEFAULT_BOUNDS.width() * exp2(0) / 256;
    double retVal = lg(metersPerTile / m_scaleWorld) + m_extraZoom;
    return retVal < 0.0 ? 0.0 : static_cast<unsigned char>(retVal + 0.5);
}

void MapTransform::setExtentLimits(const Envelope &extentLimit)
{
    m_extentLimit = extentLimit;

    double s1 = m_displayWidht / m_extentLimit.width();
    double s2 = m_displayHeight / m_extentLimit.height();

    double minScale = std::max(s1, s2);
    if(minScale > m_scaleMin)
        m_scaleMin = minScale;

    if(m_scale < minScale)
        setScale(minScale);
}

// static
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
    double tilesSizeOneDim = DEFAULT_BOUNDS.maxX() / halfTilesInMapOneDim;
    int begX = static_cast<int>(std::floor(extent.minX() / tilesSizeOneDim +
                                       halfTilesInMapOneDim));
    int begY = static_cast<int>(std::floor(extent.minY() / tilesSizeOneDim +
                                       halfTilesInMapOneDim));
    int endX = static_cast<int>(std::ceil(extent.maxX() / tilesSizeOneDim +
                                      halfTilesInMapOneDim));
    int endY = static_cast<int>(std::ceil(extent.maxY() / tilesSizeOneDim +
                                      halfTilesInMapOneDim));
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
    double fullBoundsMinX = DEFAULT_BOUNDS.minX();
    double fullBoundsMinY = DEFAULT_BOUNDS.minY();
    for (int x = begX; x < endX; ++x) {
        realX = x;
        crossExt = 0;
        if (realX < 0) {
            crossExt = -1;
            realX += tilesInMapOneDim;
        } else if (realX >= tilesInMapOneDim) {
            crossExt = 1;
            realX -= tilesInMapOneDim;
        }
        double minX = fullBoundsMinX + realX * tilesSizeOneDim;
        env.setMinX(minX);
        env.setMaxX(minX + tilesSizeOneDim);

        for (int y = begY; y < endY; ++y) {

            if (reverseY) {
                realY = tilesInMapOneDim - y - 1;
            } else {
                realY = y;
            }

            if (realY < 0 || realY >= tilesInMapOneDim) {
                continue;
            }

            double minY = fullBoundsMinY + realY * tilesSizeOneDim;
            env.setMinY(minY);
            env.setMaxY(minY + tilesSizeOneDim);
            Tile tile = {realX, realY, zoom, crossExt};
            result.push_back( { tile, env } );

            if(result.size() > MAX_TILES_COUNT) { // Limit for tiles array size
                return result;
            }
        }
    }

    return result;
}

OGRRawPoint MapTransform::worldToDisplay(const OGRRawPoint &pt) const
{
    glm::vec4 newPt(static_cast<float>(pt.x), static_cast<float>(pt.y), 0.0f, 1.0f);

    glm::vec4 out = m_worldToDisplayMatrix * newPt;

    if(m_YAxisInverted) {
        out[1] = m_displayHeight - out[1];
    }

    out[0] *= m_reduceFactor;
    out[1] *= m_reduceFactor;

    return OGRRawPoint(static_cast<double>(out[0]), static_cast<double>(out[1]));
}

OGRRawPoint MapTransform::displayToWorld(const OGRRawPoint &pt) const
{
    glm::vec4 newPt(static_cast<float>(pt.x), static_cast<float>(pt.y), 0.0f, 1.0f);

    newPt[0] /= m_reduceFactor;
    newPt[1] /= m_reduceFactor;

    if(m_YAxisInverted) {
        newPt[1] = m_displayHeight - newPt[1];
    }

    glm::vec4 out = m_invWorldToDisplayMatrix * newPt;

    return OGRRawPoint(static_cast<double>(out[0]), static_cast<double>(out[1]));
}

} // namespace ngs
