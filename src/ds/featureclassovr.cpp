/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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

#include "datastore.h"
#include "featureclassovr.h"

#include "map/maptransform.h"
#include "util/error.h"

namespace ngs {

constexpr const char *ZOOM_LEVELS_OPTION = "ZOOM_LEVELS";
constexpr unsigned short TILE_SIZE = 256; //240; //512;// 160; // Only use for overviews now in pixelSize
constexpr double WORLD_WIDTH = DEFAULT_BOUNDS_X2.width();

//------------------------------------------------------------------------------
// TilingData
//------------------------------------------------------------------------------

class TilingData : public ThreadData {
public:
    TilingData(FeatureClassOverview *featureClass, FeaturePtr feature, bool own) :
        ThreadData(own), m_feature(feature), m_featureClass(featureClass) {

    }
    FeaturePtr m_feature;
    FeatureClassOverview *m_featureClass;
};

//------------------------------------------------------------------------------
// FeatureClass
//------------------------------------------------------------------------------

FeatureClassOverview::FeatureClassOverview(OGRLayer *layer,
                                           ObjectContainer * const parent,
                                           const enum ngsCatalogObjectType type,
                                           const std::string &name) :
    FeatureClass(layer, parent, type, name),
    m_ovrTable(nullptr),
    m_creatingOvr(false)
{
    if(nullptr != m_layer) {
        fillZoomLevels();
    }

    hasTilesTable();
}

bool FeatureClassOverview::onRowsCopied(const TablePtr srcTable,
                                        const Progress &progress,
                                        const Options &options)
{
    bool createOvr = options.asBool("CREATE_OVERVIEWS", false) &&
            !options.asString("ZOOM_LEVELS", "").empty();
    if(createOvr) {
        return createOverviews(progress, options);
    }
    return FeatureClass::onRowsCopied(srcTable, progress, options);
}

bool FeatureClassOverview::hasOverviews() const
{
    if(nullptr != m_ovrTable) {
        return true;
    }

    DataStore * const dataset = dynamic_cast<DataStore*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    return dataset->getOverviewsTable(storeName()) != nullptr;
}

double FeatureClassOverview::pixelSize(int zoom, bool precize)
{
    int tilesInMapOneDim = 1 << zoom;

    // Tile size. On ower zoom less size
    int tileSize = TILE_SIZE;
    if(precize) {
        tileSize *= 2;
    }
    else {
        tileSize -= (20 - zoom) * 8; // NOTE: Only useful fo points
    }

    long sizeOneDimPixels = tilesInMapOneDim * tileSize;
    return WORLD_WIDTH / sizeOneDimPixels;
}

bool FeatureClassOverview::hasTilesTable()
{
    if(m_ovrTable) {
        return true;
    }

    DataStore *parentDS = dynamic_cast<DataStore*>(m_parent);
    if(nullptr == parentDS) {
        return false;
    }

    m_ovrTable = parentDS->getOverviewsTable(name());
//    if(nullptr == m_ovrTable) {
//        m_ovrTable = parentDS->createOverviewsTable(name());
//    }

    return m_ovrTable != nullptr;
}

FeaturePtr FeatureClassOverview::getTileFeature(const Tile &tile)
{
    if(!hasTilesTable()) {
        return FeaturePtr();
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));

    m_ovrTable->SetAttributeFilter(CPLSPrintf("%s = %d AND %s = %d AND %s = %d",
                                              OVR_X_KEY, tile.x,
                                              OVR_Y_KEY, tile.y,
                                              OVR_ZOOM_KEY, tile.z));
    FeaturePtr out(m_ovrTable->GetNextFeature());
    m_ovrTable->SetAttributeFilter(nullptr);

    return out;
}

VectorTile FeatureClassOverview::getTileInternal(const Tile &tile)
{
    VectorTile vtile;
    FeaturePtr ovrTile = getTileFeature(tile);
    if(ovrTile) {
        int size = 0;
        GByte *data = ovrTile->GetFieldAsBinary(ovrTile->GetFieldIndex(OVR_TILE_KEY),
                                                &size);
        Buffer buff(data, size, false);
        vtile.load(buff);
    }
    return vtile;
}

bool FeatureClassOverview::setTileFeature(FeaturePtr tile)
{
    if(!hasTilesTable()) {
        return false;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_ovrTable->SetFeature(tile) == OGRERR_NONE;
}

bool FeatureClassOverview::createTileFeature(FeaturePtr tile)
{
    if(!hasTilesTable()) {
        return false;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_ovrTable->CreateFeature(tile) == OGRERR_NONE;
}

bool FeatureClassOverview::tilingDataJobThreadFunc(ThreadData *threadData)
{
    TilingData *data = static_cast<TilingData*>(threadData);
    // Get tiles for geometry
    OGRGeometry *geom = data->m_feature->GetGeometryRef();
    if(nullptr == geom) {
        return true;
    }

    GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(geom));
    GIntBig fid = data->m_feature->GetFID();

    OGREnvelope env;
    geom->getEnvelope(&env);
    bool precisePixelSize = !(OGR_GT_Flatten(geom->getGeometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geom->getGeometryType()) == wkbMultiPoint);

    auto zoomLevels = data->m_featureClass->zoomLevels();
    for(auto it = zoomLevels.rbegin(); it != zoomLevels.rend(); ++it) {
        unsigned char zoomLevel = *it;
        CPLDebug("ngstore", "tilingDataJobThreadFunc for zoom %d", zoomLevel);
        Envelope extent = extraExtentForZoom(zoomLevel, env);

        std::vector<TileItem> items = MapTransform::getTilesForExtent(
                    extent, zoomLevel, false, true);

        double step = FeatureClassOverview::pixelSize(zoomLevel, precisePixelSize);
        geosGeom->simplify(step);
        for(auto tileItem : items) {
            Envelope ext = tileItem.env;
            ext.resize(TILE_RESIZE);

            auto vItems = data->m_featureClass->tileGeometry(fid, geosGeom, ext);
            data->m_featureClass->addOverviewItem(tileItem.tile, vItems);
        }
    }

    return true;
}

bool FeatureClassOverview::createOverviews(const Progress &progress, const Options &options)
{
    CPLDebug("ngstore", "start create overviews");
    m_genTiles.clear();
    bool force = options.asBool("FORCE", false);
    if(!force && hasOverviews()) {
        return true;
    }

    DataStore *parentDS = dynamic_cast<DataStore*>(m_parent);
    if(nullptr == parentDS) {
        progress.onProgress(COD_CREATE_FAILED, 0.0,
                            _("Unsupported feature class"));
        return errorMessage(_("Unsupported feature class"));
    }

    if(nullptr == m_ovrTable) {
        m_ovrTable = parentDS->getOverviewsTable(name());
    }

    if(nullptr == m_ovrTable) {
        m_ovrTable = parentDS->createOverviewsTable(name());
    }
    else {
        parentDS->clearOverviewsTable(name());
    }

    // Drop index
    parentDS->dropOverviewsTableIndex(name());

    // Fill overview layer with data
    std::string zoomLevelListStr = options.asString(ZOOM_LEVELS_OPTION, "");
    fillZoomLevels(zoomLevelListStr);
    if(m_zoomLevels.empty()) {
        return true;
    }

    setProperty("zoom_levels", zoomLevelListStr, NG_ADDITIONS_KEY);

    // Tile and simplify geometry
    progress.onProgress(COD_IN_PROCESS, 0.0,
                        _("Start tiling and simplifying geometry"));

    // Multithreaded thread pool
    CPLDebug("ngstore", "fill pool create overviews");
    ThreadPool threadPool;
    threadPool.init(getNumberThreads(), tilingDataJobThreadFunc);
    emptyFields(true);
    reset();

    FeaturePtr feature;
    while((feature = nextFeature())) {
        threadPool.addThreadData(new TilingData(this, feature, true));
    }

    Progress newProgress(progress);
    newProgress.setTotalSteps(2);
    newProgress.setStep(0);
    threadPool.waitComplete(newProgress);
    threadPool.clearThreadData();

    emptyFields(false);
    reset();

    // Save tiles
    m_creatingOvr = true;
    parentDS->lockExecuteSql(true);
    parentDS->startBatchOperation();

    CPLDebug("ngstore", "finish create overviews");
    double counter = 0.0;
    newProgress.setStep(1);
    for(auto item : m_genTiles) {
        if(!item.second.isValid() || item.second.empty()) {
            continue;
        }
        BufferPtr data = item.second.save();

        FeaturePtr newFeature = OGRFeature::CreateFeature(
                    m_ovrTable->GetLayerDefn() );

        newFeature->SetField(OVR_ZOOM_KEY, item.first.z);
        newFeature->SetField(OVR_X_KEY, item.first.x);
        newFeature->SetField(OVR_Y_KEY, item.first.y);
        newFeature->SetField(newFeature->GetFieldIndex(OVR_TILE_KEY), data->size(),
                          data->data());

//        CPLDebug("ngstore", "Tile size to store %d", data->size());

        if(m_ovrTable->CreateFeature(newFeature) != OGRERR_NONE) {
            outMessage(COD_INSERT_FAILED, _("Failed to create feature"));
        }

        newProgress.onProgress(COD_IN_PROCESS, counter/m_genTiles.size(),
                               _("Save tiles ..."));
        counter++;
    }

    parentDS->stopBatchOperation();
    m_genTiles.clear();

    // Create index
    parentDS->createOverviewsTableIndex(name());
    parentDS->lockExecuteSql(false);
    m_creatingOvr = false;

    progress.onProgress(COD_FINISHED, 1.0,
                        _("Finish tiling and simplifying geometry"));

    CPLDebug("ngstore", "finish create overviews");
    return true;
}

VectorTile FeatureClassOverview::getTile(const Tile &tile, const Envelope &tileExtent)
{
    VectorTile vtile;
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || m_creatingOvr) {
        return vtile;
    }

    if(!extent().intersects(tileExtent)) {
        return vtile;
    }

    if(hasOverviews()) {
        if(tile.z <= *m_zoomLevels.rbegin()) {
//
//            // Get closest zoom level
//            unsigned char diff = 127;
//            unsigned char z = 0;
//            for(auto zoomLevel : m_zoomLevels) {
//                unsigned char newDiff = static_cast<unsigned char>(
//                            ABS(zoomLevel - tile.z));
//                if(newDiff < diff) {
//                    z = zoomLevel;
//                    diff = newDiff;
//                    if(diff == 0)
//                        break;
//                }
//            }

            vtile = getTileInternal(tile);
//            if(vtile.isValid()) {
                return vtile;
//            }
        }
    }

    // Tiling on the fly
    CPLDebug("ngstore", "Tiling on the fly in %s", m_name.c_str());

    // Calc grid step for zoom
    bool precisePixelSize = !(OGR_GT_Flatten(geometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geometryType()) == wkbMultiPoint);

    double step = pixelSize(tile.z, precisePixelSize);

    std::vector<FeaturePtr> features;
    OGREnvelope extEnv;
    if(!m_fastSpatialFilter) {
        extEnv = tileExtent.toOgrEnvelope();
    }

    // Lock threads here
    dataset->lockExecuteSql(true);
    m_featureMutex.acquire(10.5);
    emptyFields(true);
    GeometryPtr extGeom = tileExtent.toGeometry(spatialReference());
    setSpatialFilter(extGeom);
    //reset();
    FeaturePtr feature;
    while((feature = nextFeature())) {
        if(m_fastSpatialFilter) {
            features.push_back(feature);
        }
        else {
            OGRGeometry* geom = feature->GetGeometryRef();
            if(geom) {
                OGREnvelope env;
                geom->getEnvelope(&env);
                if(env.IsInit() && env.Intersects(extEnv)) {
                    features.push_back(feature);
                }
            }
        }
    }
    emptyFields(false);
    setSpatialFilter();
    dataset->lockExecuteSql(false);
    m_featureMutex.release();

    while(!features.empty()) {
        feature = features.back();

        OGRGeometry* geom = feature->GetGeometryRef();
        if(nullptr != geom) {

            GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(geom));
            GIntBig fid = feature->GetFID();
            geosGeom->simplify(step);

            VectorTileItemArray items = tileGeometry(fid, geosGeom, tileExtent);
            if(!items.empty()) {
                vtile.add(items, false);
            }
        }
        features.pop_back();
    }

//    Debug test
//    TablePtr features = dataset->executeSQL(CPLSPrintf("SELECT * FROM %s",
//                                                       getName().c_str()),
//                                            extGeom);
//    if(features) {
//        FeaturePtr feature;
//        while(true) {
//            feature = features->nextFeature();
//            if(!feature)
//                break;
//            VectorTileItem item = tileGeometry(feature, extGeom.get(), step);
//            if(item.isValid()) {
//                vtile.add(item, false);
//            }
//        }
//    }

    return vtile;
}

VectorTileItemArray FeatureClassOverview::tileGeometry(GIntBig fid,
                                                       GEOSGeometryPtr geom,
                                                       const Envelope &env) const
{
    VectorTileItemArray out;
    if(!geom->isValid()) {
        return out;
    }

    GEOSGeometryPtr clipGeom = geom->clip(env);
    clipGeom->fillTile(fid, out);

    return out;
}

bool FeatureClassOverview::destroy()
{
    DataStore * const dataset = dynamic_cast<DataStore*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    if(dataset->type() == CAT_CONTAINER_SIMPLE) {
        return dataset->destroy();
    }

    std::string name = m_name;
    if(!Table::destroy()) {
        return false;
    }

    dataset->destroyOverviewsTable(name); // Overviews table maybe not exists

    return true;
}

void FeatureClassOverview::fillZoomLevels(const std::string &zoomLevels)
{
    std::string _zoomLevels;
    if(zoomLevels.empty()) {
        _zoomLevels = property("zoom_levels", "", NG_ADDITIONS_KEY);
    }
    else {
        _zoomLevels = zoomLevels;
    }
    char** zoomLevelArray = CSLTokenizeString2(_zoomLevels.c_str(), ",", 0);
    if(nullptr != zoomLevelArray) {
        int i = 0;
        const char* zoomLevel;
        m_zoomLevels.clear();
        while((zoomLevel = zoomLevelArray[i++]) != nullptr) {
            m_zoomLevels.insert(static_cast<unsigned char>(std::stoi(zoomLevel)));
        }
        CSLDestroy(zoomLevelArray);
    }
}

Envelope FeatureClassOverview::extraExtentForZoom(unsigned char zoom, const Envelope &env)
{
    Envelope extent = env;
    int tilesInMapOneDim = 1 << zoom;
    double halfTilesInMapOneDim = tilesInMapOneDim * 0.5;
    double tilesSizeOneDim = DEFAULT_BOUNDS.maxX() / halfTilesInMapOneDim;
    double extraSize = tilesSizeOneDim * TILE_RESIZE - tilesSizeOneDim;
    extent.setMinX(extent.minX() - extraSize);
    extent.setMinY(extent.minY() - extraSize);
    extent.setMaxX(extent.maxX() + extraSize);
    extent.setMaxY(extent.maxY() + extraSize);
    return extent;
}

void FeatureClassOverview::onFeatureInserted(FeaturePtr feature)
{
    FeatureClass::onFeatureInserted(feature);
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || dataset->isBatchOperation()) {
        return;
    }

    OGRGeometry *geom = feature->GetGeometryRef();
    if(nullptr == geom) {
        return;
    }
    bool precisePixelSize = !(OGR_GT_Flatten(geom->getGeometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geom->getGeometryType()) == wkbMultiPoint);

    GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(geom));
    GIntBig fid = feature->GetFID();

    OGREnvelope env;
    geom->getEnvelope(&env);
    Envelope extentBase = env;
    extentBase.fix();

    auto zoomLevelsList = zoomLevels();
    for(auto it = zoomLevelsList.rbegin(); it != zoomLevelsList.rend(); ++it) {
        unsigned char zoomLevel = *it;
        Envelope extent = extraExtentForZoom(zoomLevel, extentBase);
        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);

        double step = FeatureClassOverview::pixelSize(zoomLevel, precisePixelSize);
        geosGeom->simplify(step);

        for(auto tileItem : items) {

            Envelope ext = tileItem.env;
            ext.resize(TILE_RESIZE);

            auto vItem = tileGeometry(fid, geosGeom, ext);

            FeaturePtr tile = getTileFeature(tileItem.tile);
            VectorTile  vtile;
            bool create = true;
            if(tile) {
                int size = 0;
                GByte *data = tile->GetFieldAsBinary(
                            tile->GetFieldIndex(OVR_TILE_KEY), &size);
                Buffer buff(data, size, false);
                vtile.load(buff);
                create = false;
            }
            else {
                tile = OGRFeature::CreateFeature(m_ovrTable->GetLayerDefn());

                tile->SetField(OVR_ZOOM_KEY, tileItem.tile.z);
                tile->SetField(OVR_X_KEY, tileItem.tile.x);
                tile->SetField(OVR_Y_KEY, tileItem.tile.y);
            }
            vtile.add(vItem, true);

            // Add tile back
            if(vtile.isValid()) {
                BufferPtr data = vtile.save();
                tile->SetField(tile->GetFieldIndex(OVR_TILE_KEY), data->size(),
                               data->data());

                if(create) {
                    createTileFeature(tile);
                }
                else {
                    setTileFeature(tile);
                }
            }
        }
    }
}

void FeatureClassOverview::onFeatureUpdated(FeaturePtr oldFeature,
                                            FeaturePtr newFeature)
{
    FeatureClass::onFeatureUpdated(oldFeature, newFeature);
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || dataset->isBatchOperation()) {
        return;
    }

    bool precisePixelSize = !(OGR_GT_Flatten(geometryType()) == wkbPoint ||
                              OGR_GT_Flatten(geometryType()) == wkbMultiPoint);


    OGRGeometry *originalGeom = oldFeature->GetGeometryRef();
    OGRGeometry *newGeom = newFeature->GetGeometryRef();
    Envelope extentBase;

    if(nullptr != originalGeom) {
        OGREnvelope env;
        originalGeom->getEnvelope(&env);
        extentBase = env;
    }

    if(nullptr != newGeom) {
        OGREnvelope env;
        newGeom->getEnvelope(&env);
        extentBase.merge(env);
    }

    extentBase.fix();

    GEOSGeometryPtr geosGeom(new GEOSGeometryWrap(newGeom));
    GIntBig fid = newFeature->GetFID();

    auto zoomLevelsList = zoomLevels();
    for(auto it = zoomLevelsList.rbegin(); it != zoomLevelsList.rend(); ++it) {
        unsigned char zoomLevel = *it;
        Envelope extent = extraExtentForZoom(zoomLevel, extentBase);

        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);

        double step = FeatureClassOverview::pixelSize(zoomLevel, precisePixelSize);
        geosGeom->simplify(step);

        for(auto tileItem : items) {
            FeaturePtr tile = getTileFeature(tileItem.tile);
            VectorTile  vtile;
            bool create = true;
            if(tile) {
                int size = 0;
                GByte *data = tile->GetFieldAsBinary(
                            tile->GetFieldIndex(OVR_TILE_KEY), &size);
                Buffer buff(data, size, false);
                vtile.load(buff);
                create = false;
            }
            else {
                tile = OGRFeature::CreateFeature(m_ovrTable->GetLayerDefn());

                tile->SetField(OVR_ZOOM_KEY, tileItem.tile.z);
                tile->SetField(OVR_X_KEY, tileItem.tile.x);
                tile->SetField(OVR_Y_KEY, tileItem.tile.y);
            }

            vtile.remove(oldFeature->GetFID());

            Envelope env = tileItem.env;
            env.resize(TILE_RESIZE);
            auto vItem = tileGeometry(fid, geosGeom, env);
            vtile.add(vItem, true);

            // Add tile back
            if(vtile.isValid()) {
                BufferPtr data = vtile.save();
                tile->SetField(tile->GetFieldIndex(OVR_TILE_KEY), data->size(),
                                                     data->data());

                if(create) {
                    createTileFeature(tile);
                }
                else {
                    setTileFeature(tile);
                }
            }
        }
    }
}

void FeatureClassOverview::onFeatureDeleted(FeaturePtr delFeature)
{
    FeatureClass::onFeatureDeleted(delFeature);
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset || dataset->isBatchOperation()) {
        return;
    }

    OGREnvelope env;OGRGeometry *geom = delFeature->GetGeometryRef();
    geom->getEnvelope(&env);

    for(auto zoomLevel : zoomLevels()) {
        Envelope extent = extraExtentForZoom(zoomLevel, env);
        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, false, true);
        for(auto tileItem : items) {
            FeaturePtr tile = getTileFeature(tileItem.tile);

            VectorTile  vtile;
            if(tile) {
                int size = 0;
                GByte *data = tile->GetFieldAsBinary(
                            tile->GetFieldIndex(OVR_TILE_KEY), &size);
                Buffer buff(data, size, false);
                if(vtile.load(buff)) {
                    vtile.remove(delFeature->GetFID());
                }
            }

            // Add tile back
            if(vtile.isValid()) {
                BufferPtr data = vtile.save();
                tile->SetField(tile->GetFieldIndex(OVR_TILE_KEY), data->size(),
                               data->data());

                setTileFeature(tile);
            }
            else if(tile) {
                ngsUnused(m_ovrTable->DeleteFeature(tile->GetFID()));
            }
        }
    }
}

void FeatureClassOverview::onFeaturesDeleted()
{
    DataStore *dataset = dynamic_cast<DataStore*>(m_parent);
    if(nullptr != dataset) {
        dataset->clearOverviewsTable(name());
    }
}

void FeatureClassOverview::addOverviewItem(const Tile &tile, const VectorTileItemArray &items)
{
    MutexHolder holder(m_genTileMutex, 150.0);
    m_genTiles[tile].add(items, true);
}

} // namespace ngs
