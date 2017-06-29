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
#ifndef NGSRASTERDATASET_H
#define NGSRASTERDATASET_H

#include "coordinatetransformation.h"
#include "dataset.h"
#include "ngstore/codes.h"

namespace ngs {

constexpr const char *KEY_URL = "url";
constexpr const char *KEY_EPSG = "epsg";
constexpr const char *KEY_Z_MIN = "z_min";
constexpr const char *KEY_Z_MAX = "z_max";
constexpr const char *KEY_Y_ORIGIN_TOP = "y_origin_top";
constexpr const char *KEY_EXTENT = "extent";

/**
 * @brief The Raster dataset class represent image or raster
 */
class Raster : public Object, public DatasetBase, public SpatialDataset
{
public:
    Raster(std::vector<CPLString> siblingFiles,
           ObjectContainer * const parent = nullptr,
           const enum ngsCatalogObjectType type = ngsCatalogObjectType::CAT_RASTER_ANY,
           const CPLString & name = "",
           const CPLString & path = "");
    virtual ~Raster();
public:
    enum WorldFileType {
        FIRSTLASTW,
        EXTPLUSWX,
        WLD,
        EXTPLUSW
    };

    bool writeWorldFile(enum WorldFileType type);
    const Envelope& getExtent() const { return m_extent; }
    bool getGeoTransform(double *transform) const;
    int getWidth() const;
    int getHeight() const;
    int getDataSize() const;
    GDALDataType getDataType(int band = 1) const;
    int getBestOverview(int &xOff, int &yOff, int &xSize, int &ySize,
                        int bufXSize, int bufYSize) const;
    bool pixelData(void *data, int xOff, int yOff, int xSize, int ySize,
                   int bufXSize, int bufYSize, GDALDataType dataType,
                   int bandCount, int *bandList, bool read = true,
                   bool skipLastBand = false, unsigned char zoom = 0);

    // DatasetBase interface
    virtual bool open(unsigned int openFlags, const Options &options = Options()) override;
    virtual const char *options(ngsOptionType optionType) const override;

protected:
    void setExtent();

private:
    void freeLocks(bool all = false);

protected:
    Envelope m_extent;

private:
    std::vector<CPLString> m_siblingFiles;
    typedef struct _lockData {
        Envelope env;
        CPLMutex* mutexRef;
        unsigned char zoom;
    } LockData;

    std::vector<LockData> m_dataLocks;
    CPLMutex* m_dataLock;
};

typedef std::shared_ptr<Raster> RasterPtr;

}

#endif // NGSRASTERDATASET_H
