/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
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

#ifndef NGSGLOVERLAY_H
#define NGSGLOVERLAY_H

// stl
#include <memory>

#include "layer.h"
#include "map/overlay.h"

namespace ngs
{

class GlRenderOverlay
{
public:
    GlRenderOverlay(const Matrix4& sceneMatrix, const Matrix4& invViewMatrix);
    virtual ~GlRenderOverlay();

    virtual bool fill(bool isLastTry) = 0;
    virtual bool draw() = 0;

protected:
    GlObjectPtr m_glBuffer;
    const Matrix4& m_sceneMatrix;
    const Matrix4& m_invViewMatrix;
    StylePtr m_style;
    CPLMutex* m_dataMutex;
};

class GlEditLayerOverlay : public EditLayerOverlay, public GlRenderOverlay
{
public:
    explicit GlEditLayerOverlay(
            const Matrix4& sceneMatrix, const Matrix4& invViewMatrix);
    virtual ~GlEditLayerOverlay() = default;

    // EditLayerOverlay interface
public:
    void setGeometry(GeometryPtr geometry);

    // GlRenderOverlay interface
public:
    virtual bool fill(bool isLastTry) override;
    virtual bool draw() override;

protected:
    VectorGlObject* fillPoint();
};

}  // namespace ngs

#endif  // NGSGLOVERLAY_H
