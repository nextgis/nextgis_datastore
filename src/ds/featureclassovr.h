/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#ifndef NGSFEATUREDATASETOVR_H
#define NGSFEATUREDATASETOVR_H

#include "featureclass.h"

namespace ngs {

constexpr double TILE_RESIZE = 1.1;

/**
 * @brief The FeatureClassOverview class
 */
class FeatureClassOverview : public FeatureClass
{
public:
    explicit FeatureClassOverview(OGRLayer *layer,
                          ObjectContainer * const parent = nullptr,
                          const enum ngsCatalogObjectType type = CAT_FC_ANY,
                          const std::string &name = "");
    virtual ~FeatureClassOverview() override = default;
    virtual bool onRowsCopied(const TablePtr srcTable,
                              const Progress &progress = Progress(),
                              const Options &options = Options()) override;
    bool hasOverviews() const;
    bool createOverviews(const Progress &progress = Progress(),
                         const Options &options = Options());
    VectorTile getTile(const Tile &tile, const Envelope &tileExtent = Envelope());
    std::set<unsigned char> zoomLevels() const { return m_zoomLevels; }
    void addOverviewItem(const Tile &tile, const VectorTileItemArray &items);

    // static
    static double pixelSize(int zoom, bool precize = false);
    static Envelope extraExtentForZoom(unsigned char zoom, const Envelope &env);

    // Object interface
public:
    virtual bool destroy() override;

    // Table interface
protected:
    virtual void onFeatureInserted(FeaturePtr feature) override;
    virtual void onFeatureUpdated(FeaturePtr oldFeature, FeaturePtr newFeature) override;
    virtual void onFeatureDeleted(FeaturePtr delFeature) override;
    virtual void onFeaturesDeleted() override;

protected:
    VectorTileItemArray tileGeometry(GIntBig fid, GEOSGeometryPtr geom,
                                     const Envelope &env) const;
    void fillZoomLevels(const std::string &zoomLevels = "");

/*
    void tileLine(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                  float step, VectorTileItemArray& vitemArray)  const;
    void tilePolygon(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                     float step, VectorTileItemArray& vitemArray)  const;
    void tileMultiLine(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                       float step, VectorTileItemArray& vitemArray)  const;
    void tileMultiPolygon(GIntBig fid, OGRGeometry* geom, OGRGeometry* extent,
                          float step, VectorTileItemArray& vitemArray)  const;
                          */

    bool hasTilesTable();
    FeaturePtr getTileFeature(const Tile &tile);
    VectorTile getTileInternal(const Tile &tile);
    bool setTileFeature(FeaturePtr tile);
    bool createTileFeature(FeaturePtr tile);

    // static
protected:
    static bool tilingDataJobThreadFunc(ThreadData *threadData);

protected:
    OGRLayer *m_ovrTable;
    std::set<unsigned char> m_zoomLevels;
    Mutex m_genTileMutex;
    bool m_creatingOvr;

private:
    std::map<Tile, VectorTile> m_genTiles;
};

using FeatureClassOverviewPtr = std::shared_ptr<FeatureClassOverview>;

} // namespace ngs

#endif // NGSFEATUREDATASETOVR_H
