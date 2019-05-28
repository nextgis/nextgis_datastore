/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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

constexpr const char *KEY_EPSG = "epsg";
constexpr const char *KEY_Z_MIN = "z_min";
constexpr const char *KEY_Z_MAX = "z_max";
constexpr const char *KEY_Y_ORIGIN_TOP = "y_origin_top";
constexpr const char *KEY_EXTENT = "extent";
constexpr const char *KEY_LIMIT_EXTENT = "limit_extent";
constexpr const char *KEY_BAND_COUNT = "band_count";
constexpr const char *KEY_CACHE_EXPIRES = "cache_expires";
constexpr const char *KEY_CACHE_MAX_SIZE = "cache_max_size";

constexpr const int defaultCacheExpires = 7 * 24 * 60 * 60; // 7 days
constexpr const int defaultCacheMaxSize = 32 * 1024 * 1024; // 32 Mb

typedef struct _imageData {
    unsigned char *buffer;
    int width;
    int height;
} ImageData;

/**
 * @brief The Raster dataset class represents image or raster
 */
class Raster : public Object, public DatasetBase, public SpatialDataset
{
public:
    explicit Raster(std::vector<std::string> siblingFiles,
                    ObjectContainer * const parent = nullptr,
                    const enum ngsCatalogObjectType type = CAT_RASTER_ANY,
                    const std::string &name = "",
                    const std::string &path = "");
    virtual ~Raster() override;
public:
    enum WorldFileType {
        FIRSTLASTW,
        EXTPLUSWX,
        WLD,
        EXTPLUSW
    };

    bool writeWorldFile(enum WorldFileType type);
    const Envelope &extent() const { return m_extent; }
    bool geoTransform(double *transform) const;
    int width() const;
    int height() const;
    int dataSize() const;
    unsigned short bandCount() const;
    GDALDataType dataType(int band = 1) const;
    int getBestOverview(int &xOff, int &yOff, int &xSize, int &ySize,
                        int bufXSize, int bufYSize) const;
    bool pixelData(void *data, int xOff, int yOff, int xSize, int ySize,
                   int bufXSize, int bufYSize, GDALDataType dataType,
                   int bandCount, int *bandList, bool read = true,
                   bool skipLastBand = false);
    bool cacheArea(const Progress &progress, const Options &options);

    // Object interface
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual bool setProperty(const std::string &name, const std::string &value,
                             const std::string &domain) override;

    // DatasetBase interface
    virtual bool open(unsigned int openFlags = GDAL_OF_SHARED|GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR,
                      const Options &options = Options()) override;
    virtual std::string options(ngsOptionType optionType) const override;

protected:
    void setExtent();

    // static
protected:
    static bool cacheAreaJobThreadFunc(ThreadData *threadData);

protected:
    Envelope m_extent, m_pixelExtent;
    unsigned int m_openFlags;
    Options m_openOptions;

private:
    std::vector<std::string> m_siblingFiles;
    Mutex m_dataLock;
};

using RasterPtr = std::shared_ptr<Raster>;

}

#endif // NGSRASTERDATASET_H
