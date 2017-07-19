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

#include "overlay.h"

namespace ngs
{

Overlay::Overlay(ngsMapOverlyType type)
        : m_type(type)
        , m_visible(false)
{
}

CurrentLocationOverlay::CurrentLocationOverlay()
        : Overlay(MOT_LOCATION)
{
}

CurrentTrackOverlay::CurrentTrackOverlay()
        : Overlay(MOT_TRACK)
{
}

EditLayerOverlay::EditLayerOverlay()
        : Overlay(MOT_EDIT)
{
}

}  // namespace ngs