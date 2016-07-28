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
#include "geometryutil.h"
#include "featuredataset.h"

#include <iostream>

#define GL_MAX_VERTEX_COUNT 65532 //5
#define LOCK_TIME 0.3

using namespace ngs;

void ngs::FillGLBufferThread(void * layer)
{
    RenderLayer* renderLayer = static_cast<RenderLayer*>(layer);
    renderLayer->fillRenderBuffers ();
    renderLayer->m_hPrepareThread = nullptr;
}

//------------------------------------------------------------------------------
// RenderLayer
//------------------------------------------------------------------------------

RenderLayer::RenderLayer() : Layer(), m_hPrepareThread(nullptr)
{
    m_type = Layer::Type::Invalid;
}

RenderLayer::RenderLayer(const CPLString &name, DatasetPtr dataset) :
    Layer(name, dataset), m_hPrepareThread(nullptr)
{
    m_type = Layer::Type::Invalid;
}

RenderLayer::~RenderLayer()
{
    cancelPrepareRender();
}


void RenderLayer::prepareRender(OGREnvelope extent, double zoom, float level)
{
    // start bucket of buffers preparation
    m_cancelPrepare = false;
    m_renderPercent = 0;
    m_renderExtent = extent;
    m_renderZoom = zoom;
    m_renderLevel = level;
    m_hPrepareThread = CPLCreateJoinableThread(FillGLBufferThread, this);
}

void RenderLayer::cancelPrepareRender()
{
    // TODO: cancel only local data extraction threads?
    // Extract from remote sources may block cancel thread for a long time
    if(m_hPrepareThread) {
        m_cancelPrepare = true;
        // wait thread exit
        CPLJoinThread(m_hPrepareThread);
    }
}

//------------------------------------------------------------------------------
// FeatureRenderLayer

FeatureRenderLayer::FeatureRenderLayer() : RenderLayer(), m_hSpinLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Vector;
}

FeatureRenderLayer::FeatureRenderLayer(const CPLString &name, DatasetPtr dataset) :
    RenderLayer(name, dataset), m_hSpinLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Vector;
}

FeatureRenderLayer::~FeatureRenderLayer()
{
    if( m_hSpinLock )
        CPLDestroyLock(m_hSpinLock);
}

void FeatureRenderLayer::fillRenderBuffers()
{
    CPLAcquireLock(m_hSpinLock);
    m_pointBucket.clear ();
    m_polygonBucket.clear ();
    CPLReleaseLock(m_hSpinLock);
    m_bucketFilled = false;

    FeatureDataset* featureDataset = dynamic_cast<FeatureDataset*>(m_dataset.get());
    // check geometry column
    CPLString geomFieldName;
    bool originalGeom = true;
    if(m_renderZoom > 15) {
        geomFieldName = featureDataset->getGeometryColumn ();
    }
    else {
        float diff = 18;
        float currentDiff;
        char zoom = 18;
        for(auto sampleDist : sampleDists) {
            currentDiff = sampleDist.second - m_renderZoom;
            if(currentDiff > 0 && currentDiff < diff) {
                diff = currentDiff;
                zoom = sampleDist.second;
            }
        }
        if(zoom < 18) {
            geomFieldName.Printf ("ngs_geom_%d", zoom);
            originalGeom = false;
        }
        else {
            geomFieldName = featureDataset->getGeometryColumn ();
        }
    }

    CPLString statement;
    statement.Printf ("SELECT %s FROM %s", geomFieldName.c_str (),
                         m_dataset->name ().c_str ());
    GeometryPtr spatialFilter (envelopeToGeometry(m_renderExtent,
                                        featureDataset->getSpatialReference ()));

    ResultSetPtr features = featureDataset->executeSQL (statement, spatialFilter,
                                                        "");
    FeaturePtr feature;
    OGRGeometry* geom;
    int nBytes;
    GByte *pabyWKB;
    float counter = 0;
    GIntBig totalCount = features->GetFeatureCount ();
    while ((feature = features->GetNextFeature ()) != nullptr) {

        if(m_cancelPrepare) {
            break;
        }

        geom = nullptr;
        if(originalGeom) {
           geom =  feature->GetGeometryRef ();
        }
        else {
            pabyWKB = feature->GetFieldAsBinary (0, &nBytes);
            if(pabyWKB) {
                if( OGRGeometryFactory::createFromWkb (pabyWKB,
                        featureDataset->getSpatialReference (), &geom, nBytes)
                            != OGRERR_NONE) {
                    geom = nullptr;
                }
                // TODO: do we need it? CPLFree( pabyWKB );
            }
        }

        // fill render buffers
        fillRenderBuffers(geom);

        // TODO: if 5-10-20% read notify what we have some data to render

        m_renderPercent = counter / totalCount;
        counter++;
    }
    if(m_currentPointBuffer)
        m_pointBucket.push_back (move(m_currentPointBuffer));
    if(m_currentPolygonBuffer)
        m_polygonBucket.push_back (move(m_currentPolygonBuffer));
    m_bucketFilled = true;
}

void FeatureRenderLayer::fillRenderBuffers(OGRGeometry* geom)
{
    if( nullptr == geom )
        return;

    switch (OGR_GT_Flatten (geom->getGeometryType ())) {
    case wkbPoint:
    {
        if(nullptr == m_currentPointBuffer)
            m_currentPointBuffer = ngsVertexBufferPtr(new ngsVertexBuffer);
        else if(m_currentPointBuffer->m_vertices.size () - 3 >
                GL_MAX_VERTEX_COUNT) {
            m_pointBucket.push_back (move(m_currentPointBuffer));
            m_currentPointBuffer = ngsVertexBufferPtr(new ngsVertexBuffer);
        }
        OGRPoint *pt = static_cast<OGRPoint*>(geom);
        m_currentPointBuffer->m_vertices.push_back (static_cast<float>(pt->getX ()));
        m_currentPointBuffer->m_vertices.push_back (static_cast<float>(pt->getY ()));
        // TODO: add getZ + level
        m_currentPointBuffer->m_vertices.push_back (m_renderLevel);
        m_currentPointBuffer->m_indices.push_back (
                    m_currentPointBuffer->m_vertices.size () / 3 - 1);
    }
        break;
    case wkbLineString:
        break;
    case wkbPolygon:
    {
        OGRPolygon* polygon = static_cast<OGRPolygon*>(geom);
        // TODO: not only external ring must be extracted
        OGRLinearRing* ring = polygon->getExteriorRing ();
        int numPoints = ring->getNumPoints () - 1;
        if(numPoints < 3)
            break;

        if(nullptr == m_currentPolygonBuffer) {
            m_currentPolygonBuffer = ngsVertexBufferPtr(new ngsVertexBuffer);
            m_currentPolygonBuffer->m_vertices.reserve (8196);
            m_currentPolygonBuffer->m_indices.reserve (8196);
        }
        else if(m_currentPolygonBuffer->m_vertices.size () +
                numPoints * 3 > GL_MAX_VERTEX_COUNT) {
            CPLLockHolderOptionalLockD(m_hSpinLock);
            m_polygonBucket.push_back (move(m_currentPolygonBuffer));
            m_currentPolygonBuffer = ngsVertexBufferPtr(new ngsVertexBuffer);
            m_currentPolygonBuffer->m_vertices.reserve (8196);
            m_currentPolygonBuffer->m_indices.reserve (8196);
        }

        int startPolyIndex = m_currentPolygonBuffer->m_vertices.size () / 3;
        for(int i = 0; i < numPoints; ++i) {
            OGRPoint pt;
            ring->getPoint (i, &pt);
            // add point coordinates in float
            m_currentPolygonBuffer->m_vertices.push_back (static_cast<float>(pt.getX ()));
            m_currentPolygonBuffer->m_vertices.push_back (static_cast<float>(pt.getY ()));
            // TODO: add getZ + level
            m_currentPolygonBuffer->m_vertices.push_back (m_renderLevel);

            // add triangle indices unsigned short
            if(i < numPoints - 2 ) {
                m_currentPolygonBuffer->m_indices.push_back (startPolyIndex);
                m_currentPolygonBuffer->m_indices.push_back (startPolyIndex + i + 1);
                m_currentPolygonBuffer->m_indices.push_back (startPolyIndex + i + 2);
            }
        }
    }
        break;
    case wkbMultiPoint:
    {
        OGRMultiPoint* mpt = static_cast<OGRMultiPoint*>(geom);
        for(int i = 0; i < mpt->getNumGeometries (); ++i) {
            fillRenderBuffers (mpt->getGeometryRef (i));
        }
    }
        break;
    case wkbMultiLineString:
    {
        OGRMultiLineString* mln = static_cast<OGRMultiLineString*>(geom);
        for(int i = 0; i < mln->getNumGeometries (); ++i) {
            fillRenderBuffers (mln->getGeometryRef (i));
        }
    }
        break;
    case wkbMultiPolygon:
    {
        OGRMultiPolygon* mplg = static_cast<OGRMultiPolygon*>(geom);
        for(int i = 0; i < mplg->getNumGeometries (); ++i) {
            fillRenderBuffers (mplg->getGeometryRef (i));
        }
    }
        break;
    case wkbGeometryCollection:
    {
        OGRGeometryCollection* coll = static_cast<OGRGeometryCollection*>(geom);
        for(int i = 0; i < coll->getNumGeometries (); ++i) {
            fillRenderBuffers (coll->getGeometryRef (i));
        }
    }
        break;
    /* TODO: case wkbCircularString:
        return "cir";
    case wkbCompoundCurve:
        return "ccrv";
    case wkbCurvePolygon:
        return "crvplg";
    case wkbMultiCurve:
        return "mcrv";
    case wkbMultiSurface:
        return "msurf";
    case wkbCurve:
        return "crv";
    case wkbSurface:
        return "surf";*/
    }
}

double FeatureRenderLayer::render(const GlView *glView)
{
    // draw elements
    CPLLockHolderOptionalLockD(m_hSpinLock);
    while (!m_polygonBucket.empty()) {
        ngsVertexBufferPtr buff(move(m_polygonBucket.back ()));
        m_polygonBucket.pop_back ();
        if(nullptr != buff) {
            cout << "vertices: " << buff->m_vertices.size () <<
                    " indices: " << buff->m_indices.size() << endl;
            glView->drawPolygons (buff->m_vertices, buff->m_indices);
        }
    }
    if(m_bucketFilled) {
        m_renderPercent = 1;
        m_bucketFilled = false;
    }

    // report percent
    return m_renderPercent;
}
