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
#include "layer.h"
#include "style.h"
#include "view.h"

#include "util/error.h"

namespace ngs {

//------------------------------------------------------------------------------
// IGlRenderLayer
//------------------------------------------------------------------------------

GlRenderLayer::GlRenderLayer() : m_dataMutex(CPLCreateMutex())
{
    CPLReleaseMutex(m_dataMutex);
}

void GlRenderLayer::free(GlTilePtr tile)
{
    CPLMutexHolder holder(m_dataMutex);
    auto it = m_tiles.find(tile->getTile());
    if(it != m_tiles.end()) {
        if(it->second) {
            it->second->destroy();
        }
        m_tiles.erase(it);
    }
}

//------------------------------------------------------------------------------
// GlFeatureLayer
//------------------------------------------------------------------------------

GlFeatureLayer::GlFeatureLayer(const CPLString &name) : FeatureLayer(name),
    GlRenderLayer()
{
}

void GlFeatureLayer::fill(GlTilePtr tile)
{
}

bool GlFeatureLayer::draw(GlTilePtr tile)
{
    Envelope ext = tile->getExtent();
    ext.resize(0.9);

    std::array<OGRPoint, 6> points;
    points[0] = OGRPoint(ext.getMinX(), ext.getMinY());
    points[1] = OGRPoint(ext.getMinX(), ext.getMaxY());
    points[2] = OGRPoint(ext.getMaxX(), ext.getMaxY());
    points[3] = OGRPoint(ext.getMaxX(), ext.getMinY());
    points[4] = OGRPoint(ext.getMinX(), ext.getMinY());
    points[5] = OGRPoint(ext.getMaxX(), ext.getMaxY());
    for(size_t i = 0; i < points.size() - 1; ++i) {
        Normal normal = ngsGetNormals(points[i], points[i + 1]);

        GlBuffer buffer1;
        // 0
        buffer1.addVertex(points[i].getX());
        buffer1.addVertex(points[i].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(-normal.x);
        buffer1.addVertex(-normal.y);
        buffer1.addIndex(0);

        // 1
        buffer1.addVertex(points[i + 1].getX());
        buffer1.addVertex(points[i + 1].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(-normal.x);
        buffer1.addVertex(-normal.y);
        buffer1.addIndex(1);

        // 2
        buffer1.addVertex(points[i].getX());
        buffer1.addVertex(points[i].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(normal.x);
        buffer1.addVertex(normal.y);
        buffer1.addIndex(2);

        // 3
        buffer1.addVertex(points[i + 1].getX());
        buffer1.addVertex(points[i + 1].getY());
        buffer1.addVertex(0.0f);
        buffer1.addVertex(normal.x);
        buffer1.addVertex(normal.y);

        buffer1.addIndex(1);
        buffer1.addIndex(2);
        buffer1.addIndex(3);

        SimpleLineStyle style;
        style.setLineWidth(14.0f);
        style.setColor({0, 0, 255, 255});
        buffer1.bind();
        style.prepare(tile->getSceneMatrix(), tile->getInvViewMatrix());
        style.draw(buffer1);

        buffer1.destroy();
        style.destroy();
    }

    return true;
}

//------------------------------------------------------------------------------
// GlRasterLayer
//------------------------------------------------------------------------------

GlRasterLayer::GlRasterLayer(const CPLString &name) : RasterLayer(name),
    GlRenderLayer(),
    m_red(1),
    m_green(2),
    m_blue(3),
    m_alpha(0),
    m_transparancy(0),
    m_dataType(GDT_Byte)
{
    // Create default style
    m_imageStyle = new SimpleImageStyle;
    m_style = StylePtr(m_imageStyle);
}

void GlRasterLayer::fill(GlTilePtr tile)
{
    Envelope rasterExtent = m_raster->getExtent();
    const Envelope & tileExtent = tile->getExtent();

    // FIXME: Reproject tile extent to raster extent

    Envelope outExt = rasterExtent.intersect(tileExtent);

    if(!rasterExtent.isInit()) {
        CPLMutexHolder holder(m_dataMutex);
        m_tiles[tile->getTile()] = GlObjectPtr();
        m_images[tile->getTile()] = GlObjectPtr();
        return;
    }

    // Create inverse geotransform to get pixel data
    double geoTransform[6] = { 0 };
    double invGeoTransform[6] = { 0 };
    bool noTransform = false;
    if(m_raster->getGeoTransform(geoTransform)) {
        noTransform = GDALInvGeoTransform(geoTransform, invGeoTransform) == 0;
    }
    else {
        noTransform = true;
    }

    // Calc output buffer width and height
    int outWidth = static_cast<int>(rasterExtent.getWidth() *
                                    tile->getSizeInPixels() /
                                    tileExtent.getWidth());
    int outHeight = static_cast<int>(rasterExtent.getHeight() *
                                     tile->getSizeInPixels() /
                                     tileExtent.getHeight());

    if(noTransform) {
        // Swap min/max Y
        rasterExtent.setMaxY(m_raster->getHeight() - rasterExtent.getMinY());
        rasterExtent.setMinY(m_raster->getHeight() - rasterExtent.getMaxY());
    }
    else {
        double minX, minY, maxX, maxY;
        GDALApplyGeoTransform(invGeoTransform, rasterExtent.getMinX(),
                              rasterExtent.getMinY(), &minX, &maxY);

        GDALApplyGeoTransform(invGeoTransform, rasterExtent.getMaxX(),
                              rasterExtent.getMaxY(), &maxX, &minY);
        rasterExtent.setMinX(minX);
        rasterExtent.setMaxX(maxX);
        rasterExtent.setMinY(minY);
        rasterExtent.setMaxY(maxY);
    }

    rasterExtent.fix();

    // Get width & height in pixels of raster area
    int width = static_cast<int>(std::ceil(rasterExtent.getWidth()));
    int height = static_cast<int>(std::ceil(rasterExtent.getHeight()));
    int minX = static_cast<int>(std::floor(rasterExtent.getMinX()));
    int minY = static_cast<int>(std::floor(rasterExtent.getMinY()));

    // Correct data
    if(minX < 0) {
        minX = 0;
    }
    if(minY < 0) {
        minY = 0;
    }
    if(width - minX > m_raster->getWidth()) {
        width = m_raster->getWidth() - minX;
    }
    if(height - minY > m_raster->getHeight()) {
        height = m_raster->getHeight() - minY;
    }

    // TODO: Check if 0 band is working else memset 255 for buffer and read data skipping 4 byte
    int bandCount = 4;
    int bands[4];
    bands[0] = m_red;
    bands[1] = m_green;
    bands[2] = m_blue;
    bands[3] = m_alpha;

    if(outWidth > width && outHeight > height ) { // Read origina raster
        outWidth = width;
        outHeight = height;
    }
    else { // Get closest overview and get overview data
        int minXOv = minX;
        int minYOv = minY;
        int outWidthOv = width;
        int outHeightOv = height;
        int overview = m_raster->getBestOverview(minXOv, minYOv, outWidthOv, outHeightOv,
                                                 outWidth, outHeight);
        if(overview >= 0) {
            outWidth = outWidthOv;
            outHeight = outHeightOv;
        }
    }

    int dataSize = GDALGetDataTypeSize(m_dataType) / 8;
    size_t bufferSize = static_cast<size_t>(outWidth * outHeight *
                                            dataSize * 4); // NOTE: We use RGBA to store textures
    GLubyte* pixData = static_cast<GLubyte*>(CPLMalloc(bufferSize));
    if(m_alpha == 0) {
        std::memset(pixData, 255 - m_transparancy, bufferSize);
        if(!m_raster->pixelData(pixData, minX, minY, width, height, outWidth,
                                outHeight, m_dataType, bandCount, bands, true, true)) {
            CPLFree(pixData);
            CPLMutexHolder holder(m_dataMutex);
            m_tiles[tile->getTile()] = GlObjectPtr();
            m_images[tile->getTile()] = GlObjectPtr();
            return;
        }
    }
    else {
        if(!m_raster->pixelData(pixData, minX, minY, width, height, outWidth,
                                outHeight, m_dataType, bandCount, bands)) {
            CPLFree(pixData);
            CPLMutexHolder holder(m_dataMutex);
            m_tiles[tile->getTile()] = GlObjectPtr();
            m_images[tile->getTile()] = GlObjectPtr();
            return;
        }
    }

    GlImage *image = new GlImage;
    image->setImage(pixData, outWidth, outHeight); // NOTE: May be not working NOD

    // FIXME: Reproject intersect raster extent to tile extent
    GlBuffer* tileExtentBuff = new GlBuffer;
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMinX()));
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMinY()));
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addVertex(1.0f);
    tileExtentBuff->addIndex(0);
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMinX()));
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMaxY()));
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addIndex(1);
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMaxX()));
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMaxY()));
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addVertex(1.0f);
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addIndex(2);
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMaxX()));
    tileExtentBuff->addVertex(static_cast<float>(outExt.getMinY()));
    tileExtentBuff->addVertex(0.0f);
    tileExtentBuff->addVertex(1.0f);
    tileExtentBuff->addVertex(1.0f);
    tileExtentBuff->addIndex(0);
    tileExtentBuff->addIndex(2);
    tileExtentBuff->addIndex(3);

    CPLMutexHolder holder(m_dataMutex);
    m_tiles[tile->getTile()] = GlObjectPtr(tileExtentBuff);
    m_images[tile->getTile()] = GlObjectPtr(image);
}

void GlRasterLayer::free(GlTilePtr tile)
{
    GlRenderLayer::free(tile);

    CPLMutexHolder holder(m_dataMutex);
    auto it = m_images.find(tile->getTile());
    if(it != m_images.end()) {
        it->second->destroy();
        m_images.erase(it);
    }
}


bool GlRasterLayer::draw(GlTilePtr tile)
{
    CPLMutexHolder holder(m_dataMutex);
    auto buffIt = m_tiles.find(tile->getTile());
    auto imgIt = m_images.find(tile->getTile());
    if(buffIt == m_tiles.end() || imgIt == m_images.end()) {
        return false; // Data not yet loaded
    }
    else if(!buffIt->second || !imgIt->second) {
        return true; // Out of tile extent
    }

    // Bind everything before call prepare and set matrices
    GlImage* img = static_cast<GlImage*>(imgIt->second.get());
    m_imageStyle->setImage(img);
    if(buffIt->second->bound()) {
        buffIt->second->rebind();
    }
    else {
        buffIt->second->bind();
    }

    m_imageStyle->prepare(tile->getSceneMatrix(), tile->getInvViewMatrix());
    const GlBuffer& buff = (*static_cast<GlBuffer*>(buffIt->second.get()));
    m_imageStyle->draw(buff);

    return true;

//    Envelope ext = tile->getExtent();
//    ext.resize(0.9);

//    std::array<OGRPoint, 6> points;
//    points[0] = OGRPoint(ext.getMinX(), ext.getMinY());
//    points[1] = OGRPoint(ext.getMinX(), ext.getMaxY());
//    points[2] = OGRPoint(ext.getMaxX(), ext.getMaxY());
//    points[3] = OGRPoint(ext.getMaxX(), ext.getMinY());
//    points[4] = OGRPoint(ext.getMinX(), ext.getMinY());
//    points[5] = OGRPoint(ext.getMaxX(), ext.getMaxY());
//    for(size_t i = 0; i < points.size() - 1; ++i) {
//        Normal normal = ngsGetNormals(points[i], points[i + 1]);

//        GlBuffer buffer1;
//        // 0
//        buffer1.addVertex(points[i].getX());
//        buffer1.addVertex(points[i].getY());
//        buffer1.addVertex(0.0f);
//        buffer1.addVertex(-normal.x);
//        buffer1.addVertex(-normal.y);
//        buffer1.addIndex(0);

//        // 1
//        buffer1.addVertex(points[i + 1].getX());
//        buffer1.addVertex(points[i + 1].getY());
//        buffer1.addVertex(0.0f);
//        buffer1.addVertex(-normal.x);
//        buffer1.addVertex(-normal.y);
//        buffer1.addIndex(1);

//        // 2
//        buffer1.addVertex(points[i].getX());
//        buffer1.addVertex(points[i].getY());
//        buffer1.addVertex(0.0f);
//        buffer1.addVertex(normal.x);
//        buffer1.addVertex(normal.y);
//        buffer1.addIndex(2);

//        // 3
//        buffer1.addVertex(points[i + 1].getX());
//        buffer1.addVertex(points[i + 1].getY());
//        buffer1.addVertex(0.0f);
//        buffer1.addVertex(normal.x);
//        buffer1.addVertex(normal.y);

//        buffer1.addIndex(1);
//        buffer1.addIndex(2);
//        buffer1.addIndex(3);

//        SimpleLineStyle style;
//        style.setLineWidth(14.0f);
//        style.setColor({0, 0, 255, 255});
//        buffer1.bind();
//        style.prepare(tile->getSceneMatrix(), tile->getInvViewMatrix());
//        style.draw(buffer1);

//        buffer1.destroy();
//        style.destroy();
//    }

//    return true;
}


} // namespace ngs


/*
const int MAX_FID_COUNT = 500000;

void ngs::FillGLBufferThread(void * layer)
{
    RenderLayer* renderLayer = static_cast<RenderLayer*>(layer);

    renderLayer->fillRenderBuffers ();
}

//------------------------------------------------------------------------------
// RenderLayer
//------------------------------------------------------------------------------

RenderLayer::RenderLayer() : Layer(), m_hPrepareThread(nullptr), m_mapView(nullptr),
    m_hThreadLock(CPLCreateLock(LOCK_SPIN)), m_featureCount(0)
{
    m_type = Layer::Type::Invalid;
}

RenderLayer::RenderLayer(const CPLString &name, DatasetPtr dataset) :
    Layer(name, dataset), m_hPrepareThread(nullptr), m_mapView(nullptr),
    m_hThreadLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Invalid;
}

RenderLayer::~RenderLayer()
{
    cancelFillThread();    
    CPLDestroyLock(m_hThreadLock);
}


void RenderLayer::prepareFillThread(OGREnvelope extent, double zoom, float level)
{
    cancelFillThread();

    // start bucket of buffers preparation
    m_cancelPrepare = false;
    //m_renderPercent = 0;
    m_renderExtent = extent;
    m_renderZoom = static_cast<unsigned char>(zoom);
    m_renderLevel = level;

    // create or refill virtual tiles for current extent and zooms
    CPLLockHolder tilesHolder(m_hThreadLock);
    m_hPrepareThread = CPLCreateJoinableThread(FillGLBufferThread, this);
}

void RenderLayer::cancelFillThread()
{
    CPLLockHolder tilesHolder(m_hThreadLock);
    if(m_hPrepareThread) {
        m_cancelPrepare = true;
        // wait thread exit
        CPLJoinThread(m_hPrepareThread);
        m_hPrepareThread = nullptr;
    }
}

void RenderLayer::finishFillThread()
{
    CPLLockHolder tilesHolder(m_hThreadLock);
    CPLJoinThread(m_hPrepareThread);
    m_hPrepareThread = nullptr;
}

void RenderLayer::draw(ngsDrawState state, OGREnvelope extent, double zoom,
                         float level)
{
    switch (state) {
    case DS_REDRAW:
        // clear cache
        clearTiles();
    case DS_NORMAL:
        // start extract data thread
        prepareFillThread(extent, zoom, level);
    case DS_PRESERVED:
        // draw cached data
        drawTiles ();
        break;
    }
}

float RenderLayer::getComplete() const
{
    return m_complete;
}

bool RenderLayer::isComplete() const
{
    return m_isComplete;
}

int RenderLayer::getFeatureCount() const
{
    return m_featureCount;
}

//------------------------------------------------------------------------------
// FeatureRenderLayer
//------------------------------------------------------------------------------

struct tileIs {
    tileIs( const GlBufferBucket &item ) {
        toFind.x = item.X();
        toFind.y = item.Y();
        toFind.z = item.zoom ();
        toFind.crossExtent = item.crossExtent();
    }
    bool operator() (const TileItem &item) {
        return item.x == toFind.x &&
               item.y == toFind.y &&
               item.z == toFind.z &&
               item.crossExtent == toFind.crossExtent;
    }
    TileItem toFind;
};

FeatureRenderLayer::FeatureRenderLayer() : RenderLayer(),
    m_hTilesLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Vector;
}

FeatureRenderLayer::FeatureRenderLayer(const CPLString &name, DatasetPtr dataset) :
    RenderLayer(name, dataset), m_hTilesLock(CPLCreateLock(LOCK_SPIN))
{
    m_type = Layer::Type::Vector;
    initStyle();
}

FeatureRenderLayer::~FeatureRenderLayer()
{
    CPLDestroyLock(m_hTilesLock);
}

void FeatureRenderLayer::initStyle()
{
    FeatureDataset* featureDataset = ngsDynamicCast(FeatureDataset, m_dataset);
    OGRwkbGeometryType geomType = featureDataset->getGeometryType();

    switch (OGR_GT_Flatten(geomType)) {
        case wkbMultiPoint:
        case wkbPoint: {
            SimplePointStyle* style = new SimplePointStyle();
            m_style.reset(style);
            m_style->setColor({0, 0, 255, 255});
            style->setType(PT_CIRCLE);
            style->setSize(9.0f);
        } break;

        case wkbMultiLineString:
        case wkbLineString: {
            SimpleLineStyle* style = new SimpleLineStyle();
            m_style.reset(style);
            m_style->setColor({0, 255, 255, 255});
            style->setLineWidth(2);
        } break;

        case wkbMultiPolygon:
        case wkbPolygon: {
//            SimpleFillStyle* style = new SimpleFillStyle();
//            m_style.reset(style);
//            m_style->setColor({255, 0, 0, 255});
            SimpleFillBorderedStyle* style = new SimpleFillBorderedStyle();
            m_style.reset(style);
            m_style->setColor({255, 0, 0, 255});
            style->setBorderWidth(2);
            style->setBorderColor({0, 0, 0, 255});
        } break;

        default:
            break;
    }
}

void FeatureRenderLayer::fillRenderBuffers()
{
    cout << "GlBuffer::getHardBuffersCount() before fillRenderBuffers: "
         << GlBuffer::getGlobalHardBuffersCount() << "\n";

    m_complete = 0;
    m_isComplete = false;
    m_featureCount = -1;
    float counter = 0;
    OGREnvelope renderExtent = m_renderExtent;
    FeaturePtr feature;
    OGRGeometry* geom;
    FeatureDataset* featureDataset = ngsDynamicCast(FeatureDataset, m_dataset);
    bool fidExists;

    vector<TileItem> tiles = getTilesForExtent(m_renderExtent, m_renderZoom,
                                            false, m_mapView->getXAxisLooped ());
    // remove already exist tiles
    auto iter = m_tiles.begin ();
    while (iter != m_tiles.end()) {
        auto pos = find_if(tiles.begin(), tiles.end(), tileIs(**iter) );
        if(pos != tiles.end ()) {
            tiles.erase (pos);
        }
        ++iter;
    }

    int fidCount = getFidCount();
    for(TileItem &tileItem : tiles) {
        if(m_cancelPrepare) {
            return;
        }

        if(! (isEqual(renderExtent.MaxX, m_renderExtent.MaxX) &&
              isEqual(renderExtent.MaxY, m_renderExtent.MaxY) &&
              isEqual(renderExtent.MinX, m_renderExtent.MinX) &&
              isEqual(renderExtent.MinY, m_renderExtent.MinY))) {
            // if extent changed - refresh tiles
            return fillRenderBuffers();
        }

        if (fidCount > MAX_FID_COUNT) {
            break;
        }

        int numPoints = 4;
        if (!GlBuffer::canGlobalStoreVertices(4 * numPoints, true)
                || !GlBuffer::canGlobalStoreIndices(6 * numPoints)) {
            cout << "can not store, m_renderZoom " << ((int) m_renderZoom)
                 << "\n";
            cout << "GlBuffer::getGlobalVertexBufferSize() "
                 << GlBuffer::getGlobalVertexBufferSize() << "\n";
            cout << "GlBuffer::getGlobalIndexBufferSize() "
                 << GlBuffer::getGlobalIndexBufferSize() << "\n";
            cout << "GlBuffer::getHardBuffersCount() after fillRenderBuffers: "
                 << GlBuffer::getGlobalHardBuffersCount() << "\n";
            return;
        }

        GlBufferBucketSharedPtr tile =
                makeSharedGlBufferBucket(GlBufferBucket(tileItem.x, tileItem.y,
                        tileItem.z, tileItem.env, tileItem.crossExtent));
        GeometryPtr spatialFilter = envelopeToGeometry(
                tile->extent(), featureDataset->getSpatialReference());
        ResultSetPtr resSet =
                featureDataset->getGeometries(tile->zoom(), spatialFilter);

        while ((feature = resSet->GetNextFeature ()) != nullptr) {
            if(m_cancelPrepare) {
                return;
            }

            fidExists = false;
            for (const GlBufferBucketSharedPtr& tile1 : m_tiles) {
                if (tile1->zoom() == m_renderZoom
                        && tile1->crossExtent() == tile->crossExtent()
                        && tile1->hasFid(feature->GetFID())) {
                    fidExists = true;
                    break;
                }
            }

            if(fidExists)
                continue;

            geom = feature->GetGeometryRef ();
            if(nullptr == geom)
                continue;

            tile->fill(feature->GetFID (), geom, m_renderLevel);

            ++fidCount;
            if (fidCount > MAX_FID_COUNT) {
                break;
            }
        }

        tile->setFilled(true);
        {
            CPLLockHolder tilesHolder(m_hTilesLock);
            m_tiles.emplace_back(std::move(tile));
        }
        ++counter;
        m_complete = counter / tiles.size ();

        if(m_mapView) {
            m_mapView->notify();
        }
    }


    // free memory remove not visible tiles
    OGREnvelope testExt = resizeEnvelope(m_renderExtent, 2);
    iter = m_tiles.begin();
    while (iter != m_tiles.end()) {
        const GlBufferBucketSharedPtr currentTile = *iter;

        if (currentTile->zoom() != m_renderZoom
                || (currentTile->crossExtent() == 0
                           && !currentTile->intersects(testExt))) {
                CPLLockHolder tilesHolder(m_hTilesLock);
            iter = m_tiles.erase(iter);
        } else {
            if (-1 == m_featureCount) {
                m_featureCount = 0;
            }
            m_featureCount += currentTile->getFidCount();
            ++iter;
        }
    }

    m_complete = 1;
    m_isComplete = true;
    if(m_mapView) {
        m_mapView->notify();
    }

    if(! (isEqual(renderExtent.MaxX, m_renderExtent.MaxX) &&
          isEqual(renderExtent.MaxY, m_renderExtent.MaxY) &&
          isEqual(renderExtent.MinX, m_renderExtent.MinX) &&
          isEqual(renderExtent.MinY, m_renderExtent.MinY))) {
        // if extent changed - refresh tiles
        return fillRenderBuffers();
    }

    cout << "m_renderZoom " << ((int) m_renderZoom) << "\n";
    cout << "GlBuffer::getGlobalVertexBufferSize() "
         << GlBuffer::getGlobalVertexBufferSize() << "\n";
    cout << "GlBuffer::getGlobalIndexBufferSize() "
         << GlBuffer::getGlobalIndexBufferSize() << "\n";
    cout << "GlBuffer::getHardBuffersCount() after fillRenderBuffers: "
         << GlBuffer::getGlobalHardBuffersCount() << "\n";
}

void ngs::FeatureRenderLayer::clearTiles()
{
    CPLLockHolder tilesHolder(m_hTilesLock);
    m_tiles.clear ();
}

void ngs::FeatureRenderLayer::drawTiles()
{
    cout << "GlBuffer::getHardBuffersCount() before draw: "
         << GlBuffer::getGlobalHardBuffersCount() << "\n";

    CPLLockHolder tilesHolder(m_hTilesLock);

    // load program if already not, set matrix and fill color in prepare
    m_style->prepareProgram();
    m_style->prepareData(m_mapView->getSceneMatrix(), m_mapView->getInvViewMatrix());

    int finalVertexBufferSize = 0;
    int finalIndexBufferSize = 0;
    for (const GlBufferBucketSharedPtr& tile : m_tiles) {
        tile->draw(*m_style.get());
        finalVertexBufferSize += tile->getVertexBufferSize();
        finalIndexBufferSize += tile->getIndexBufferSize();
    }

    cout << "drawTiles(), finalVertexBufferSize == " << finalVertexBufferSize
         << endl;
    cout << "drawTiles(), finalIndexBufferSize == " << finalIndexBufferSize
         << endl;
    cout << "GlBuffer::getHardBuffersCount() after  draw: "
         << GlBuffer::getGlobalHardBuffersCount() << "\n";
}

void FeatureRenderLayer::refreshTiles()
{
    vector<TileItem> tiles = getTilesForExtent(m_renderExtent, m_renderZoom,
                                            false, m_mapView->getXAxisLooped ());

    // remove exist items in m_tiles from tiles
    // remove non exist items in tiles from m_tiles
    CPLLockHolder tilesHolder(m_hTilesLock);
    auto iter = m_tiles.begin();
    while (iter != m_tiles.end()) {
        if (tiles.empty())
            break;
        auto pos = find_if(tiles.begin(), tiles.end(), tileIs(**iter));
        if (pos == tiles.end()) {
            // erase returns the new iterator
            iter = m_tiles.erase(iter);
        } else {
            tiles.erase(pos);
            ++iter;
        }
    }

    for (const TileItem& tile : tiles) {
        m_tiles.emplace_back(makeSharedGlBufferBucket(GlBufferBucket(
                tile.x, tile.y, tile.z, tile.env, tile.crossExtent)));
    }
}


int FeatureRenderLayer::load(const JSONObject &store,
                                  DatasetContainerPtr dataStore,
                                  const CPLString &mapPath)
{
    int nRet = Layer::load (store, dataStore, mapPath);
    if(nRet != ngsErrorCodes::EC_SUCCESS)
        return nRet;
    initStyle();
    return nRet;
}

int FeatureRenderLayer::getFidCount() const
{
    int fidCount = 0;
    for (const GlBufferBucketSharedPtr& tile : m_tiles) {
        fidCount += tile->getFidCount();
    }
    return fidCount;
}
*/
