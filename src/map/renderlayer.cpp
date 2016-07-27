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
#include "renderlayer.h"

using namespace ngs;

RenderLayer::RenderLayer() : Layer()
{

}

RenderLayer::RenderLayer(const CPLString &name, DatasetPtr dataset) :
    Layer(name, dataset)
{

}


void RenderLayer::prepareRender(OGREnvelope extent, double zoom, float level)
{
    // start bucket of buffers preparation
    m_cancelPrepare = false;
    m_renderPercent = 0;
}

void RenderLayer::cancelPrepareRender()
{
    // TODO: cancel only local data extraction threads.
    // Extract from remote sources may block cancel thread for a long time
    if(m_hPrepareThread) {
        m_cancelPrepare = true;
        // wait thread exit
        CPLJoinThread(m_hPrepareThread);
    }
}

double RenderLayer::render(const GlView *glView)
{
    return 1;//m_renderPercent;
}
