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

constexpr const char* KEY_URL = "url";
constexpr const char* KEY_EPSG = "epsg";
constexpr const char* KEY_Z_MIN = "z_min";
constexpr const char* KEY_Z_MAX = "z_max";
constexpr const char* KEY_Y_ORIGIN_TOP = "y_origin_top";
constexpr const char* KEY_EXTENT = "extent";
constexpr const char* KEY_LIMIT_EXTENT = "limit_extent";
constexpr const char* KEY_BAND_COUNT = "band_count";
constexpr const char* KEY_CACHE_EXPIRES = "cache_expires";
constexpr const char* KEY_CACHE_MAX_SIZE = "cache_max_size";

constexpr const int defaultCacheExpires = 7 * 24 * 60 * 60;
constexpr const int defaultCacheMaxSize = 128 * 1024 * 1024;

typedef struct _imageData {
    unsigned char* buffer;
    int width;
    int height;
} ImageData;

/**
 * @brief The Raster dataset class represent image or raster
 */
class Raster : public Object, public DatasetBase, public SpatialDataset
{
public:
    explicit Raster(std::vector<CPLString> siblingFiles,
           ObjectContainer * const parent = nullptr,
           const enum ngsCatalogObjectType type = CAT_RASTER_ANY,
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
    const Envelope& extent() const { return m_extent; }
    bool geoTransform(double* transform) const;
    int width() const;
    int height() const;
    int dataSize() const;
    unsigned short bandCount() const;
    GDALDataType dataType(int band = 1) const;
    int getBestOverview(int &xOff, int &yOff, int &xSize, int &ySize,
                        int bufXSize, int bufYSize) const;
    bool pixelData(void* data, int xOff, int yOff, int xSize, int ySize,
                   int bufXSize, int bufYSize, GDALDataType dataType,
                   int bandCount, int* bandList, bool read = true,
                   bool skipLastBand = false);
    bool cacheArea(const Progress &progress, const Options &options);

    // Object interface
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual char** metadata(const char* domain) const override;
    virtual bool setMetadataItem(const char* name, const char* value,
                                 const char* domain) override;

    // DatasetBase interface
    virtual bool open(unsigned int openFlags, const Options &options = Options()) override;
    virtual const char* options(ngsOptionType optionType) const override;

protected:
    void setExtent();

    // static
protected:
    static bool cacheAreaJobThreadFunc(ThreadData *threadData);


//private:
//    void freeLocks(bool all = false);

protected:
    Envelope m_extent;
    unsigned int m_openFlags;
    Options m_openOptions;

private:
    std::vector<CPLString> m_siblingFiles;
//    typedef struct _lockData {
//        Envelope env;
//        CPLMutex* mutexRef;
//        unsigned char zoom;
//    } LockData;

//    std::vector<LockData> m_dataLocks;
    CPLMutex* m_dataLock;
};

typedef std::shared_ptr<Raster> RasterPtr;

}

#endif // NGSRASTERDATASET_H
