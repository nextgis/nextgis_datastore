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

#include "util.h"

#include "catalog/file.h"
#include "catalog/folder.h"
#include "util/error.h"

namespace ngs {

CREATE_FEATURE_DEFN_RESULT createFeatureDefinition(const std::string& name,
                                        const Options &options)
{
    CREATE_FEATURE_DEFN_RESULT out;
    OGRFeatureDefn *fieldDefinition = OGRFeatureDefn::CreateFeatureDefn(name.c_str());
    int fieldCount = options.asInt("FIELD_COUNT", 0);

    for(int i = 0; i < fieldCount; ++i) {
        std::string fieldName = options.asString(CPLSPrintf("FIELD_%d_NAME", i), "");
        if(fieldName.empty()) {
            OGRFeatureDefn::DestroyFeatureDefn(fieldDefinition);
            errorMessage(_("Name for field %d is not defined"), i);
            return out;
        }

        std::string fieldAlias = options.asString(CPLSPrintf("FIELD_%d_ALIAS", i), "");
        if(fieldAlias.empty()) {
            fieldAlias = fieldName;
        }
        fieldData data = { fieldName, fieldAlias };
        out.fields.push_back(data);

        OGRFieldType fieldType = FeatureClass::fieldTypeFromName(
                    options.asString(CPLSPrintf("FIELD_%d_TYPE", i), ""));
        OGRFieldDefn field(fieldName.c_str(), fieldType);
        std::string defaultValue = options.asString(
                    CPLSPrintf("FIELD_%d_DEFAULT_VAL", i), "");
        if(!defaultValue.empty()) {
            field.SetDefault(defaultValue.c_str());
        }
        fieldDefinition->AddFieldDefn(&field);
    }
    out.defn = std::unique_ptr<OGRFeatureDefn>(fieldDefinition);
    return out;
}

void setMetadata(const ObjectPtr &object, std::vector<fieldData> fields,
                 const Options& options)
{
    Table *table = ngsDynamicCast(Table, object);
    if(nullptr == table) {
        return;
    }

    // Store aliases and field original names in properties
    for(size_t i = 0; i < fields.size(); ++i) {
        table->setProperty("FIELD_" + std::to_string(i) + "_NAME",
            fields[i].name, NG_ADDITIONS_KEY);
        table->setProperty("FIELD_" + std::to_string(i) + "_ALIAS",
            fields[i].alias, NG_ADDITIONS_KEY);
    }

    bool saveEditHistory = options.asBool(LOG_EDIT_HISTORY_KEY, false);
    table->setProperty(LOG_EDIT_HISTORY_KEY, saveEditHistory ? "ON" : "OFF",
        NG_ADDITIONS_KEY);

    // Store user defined options in properties
    for(const auto &it : options) {
        if(comparePart(it.first, USER_PREFIX_KEY, USER_PREFIX_KEY_LEN)) {
            table->setProperty(it.first.substr(USER_PREFIX_KEY_LEN),
                it.second, USER_KEY);
        }
    }
}

namespace ngw {

OGRLayer *createAttachmentsTable(GDALDataset *ds, const std::string &name,
                                 const std::string &filesPath)
{
    resetError();
    OGRLayer *attLayer = ds->CreateLayer(name.c_str(), nullptr, wkbNone, nullptr);
    if (nullptr == attLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create folder for files
    if(!filesPath.empty()) {
        std::string attachmentsPath =
                File::resetExtension(filesPath, Dataset::attachmentsFolderExtension());
        if(!Folder::isExists(attachmentsPath)) {
            Folder::mkDir(attachmentsPath, true);
        }
    }

    // Create table  fields
    OGRFieldDefn fidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn nameField(ATTACH_FILE_NAME_FIELD, OFTString);
    OGRFieldDefn descField(ATTACH_DESCRIPTION_FIELD, OFTString);
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));

    if(attLayer->CreateField(&fidField) != OGRERR_NONE ||
       attLayer->CreateField(&nameField) != OGRERR_NONE ||
       attLayer->CreateField(&descField) != OGRERR_NONE ||
       attLayer->CreateField(&ridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return attLayer;
}

OGRLayer *createEditHistoryTable(GDALDataset *ds, const std::string &name)
{
    resetError();
    OGRLayer *logLayer = ds->CreateLayer(name.c_str(), nullptr, wkbNone, nullptr);
    if (nullptr == logLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create table  fields
    OGRFieldDefn fidField(FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn afidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn opField(OPERATION_FIELD, OFTInteger64);
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));
    OGRFieldDefn aridField(ATTACHMENT_REMOTE_ID_KEY, OFTInteger64);
    aridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));

    if(logLayer->CreateField(&fidField) != OGRERR_NONE ||
       logLayer->CreateField(&afidField) != OGRERR_NONE ||
       logLayer->CreateField(&opField) != OGRERR_NONE ||
       logLayer->CreateField(&ridField) != OGRERR_NONE ||
       logLayer->CreateField(&aridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return logLayer;
}

} // namespace ngw

} // namespace ngs
