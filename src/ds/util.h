/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019 NextGIS, <info@nextgis.com>
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

#ifndef NGWBASEFUNCTIONS_H
#define NGWBASEFUNCTIONS_H

#include "storefeatureclass.h"

#include <gdal_priv.h>
#include <string>

#include "catalog/object.h"
#include "util/options.h"

namespace ngs {

constexpr const char *NG_PREFIX = "nga_";
constexpr int NG_PREFIX_LEN = length(NG_PREFIX);

struct fieldData {
    std::string name, alias;
};

typedef struct _createFeatureDefnResult {
    std::unique_ptr<OGRFeatureDefn> defn;
    std::vector<fieldData> fields;
} CREATE_FEATURE_DEFN_RESULT;

CREATE_FEATURE_DEFN_RESULT createFeatureDefinition(const std::string& name,
                                        const Options &options);

void setMetadata(const ObjectPtr &object, std::vector<fieldData> fields,
                 const Options& options);
/**
 * @brief NGW namespace
 */
namespace ngw {

constexpr const char *NGW_ID = "NGW_ID";
constexpr const char *NGW_CONNECTION = "NGW_CONNECTION";

constexpr const char *REMOTE_ID_KEY = "rid";
constexpr const char *ATTACHMENT_REMOTE_ID_KEY = "arid";
constexpr GIntBig INIT_RID_COUNTER = NOT_FOUND; //-1000000;
constexpr const char *SYNC_KEY = "SYNC";
constexpr const char *SYNC_ATT_KEY = "SYNC_ATTACHMENTS";
constexpr const char *SYNC_BIDIRECTIONAL = "BIDIRECTIONAL";
constexpr const char *SYNC_UPLOAD = "UPLOAD";
constexpr const char *SYNC_DOWNLOAD = "DOWNLOAD";
constexpr const char *SYNC_DISABLE = "DISABLE";
constexpr const char *ATTACHMENTS_DOWNLOAD_MAX_SIZE = "ATTACHMENTS_DOWNLOAD_MAX_SIZE";

OGRLayer *createAttachmentsTable(GDALDataset *ds, const std::string &name);
OGRLayer *createEditHistoryTable(GDALDataset *ds, const std::string &name);

std::string downloadAttachment(StoreObject *storeObject, GIntBig fid, GIntBig aid,
                               const Progress &progress);
} // namespace ngw

} // namespace ngs

#endif // NGWBASEFUNCTIONS_H
