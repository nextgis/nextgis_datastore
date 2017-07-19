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

#include "map/overlay.h"

namespace ngs
{

class GlRenderOverlay
{
public:
    GlRenderOverlay();
    virtual ~GlRenderOverlay() = default;

    virtual bool draw() = 0;
};

typedef std::shared_ptr<GlRenderOverlay> GlRenderOverlayPtr;

class GlCurrentLocationOverlay : public CurrentLocationOverlay,
                                 public GlRenderOverlay
{
public:
    explicit GlCurrentLocationOverlay();
    virtual ~GlCurrentLocationOverlay() = default;

    virtual bool draw() override;
};

class GlCurrentTrackOverlay : public CurrentTrackOverlay, public GlRenderOverlay
{
public:
    explicit GlCurrentTrackOverlay();
    virtual ~GlCurrentTrackOverlay() = default;

    virtual bool draw() override;
};

class GlEditLayerOverlay : public EditLayerOverlay, public GlRenderOverlay
{
public:
    explicit GlEditLayerOverlay();
    virtual ~GlEditLayerOverlay() = default;

    virtual bool draw() override;
};

}  // namespace ngs

#endif  // NGSGLOVERLAY_H
