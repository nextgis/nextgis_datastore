/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019-2020 NextGIS, <info@nextgis.com>
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
#ifndef NGSNGWCONNECTION_H
#define NGSNGWCONNECTION_H

#include "ds/coordinatetransformation.h"
#include "objectcontainer.h"
#include "remoteconnections.h"

#include "cpl_json.h"


namespace ngs {

constexpr const char *KEY_LOGIN = "login";
constexpr const char *KEY_PASSWORD = "password";
constexpr const char *KEY_IS_GUEST = "is_guest";

/**
 * @brief NGW namespace
 */
namespace ngw {
    std::string getPermisionsUrl(const std::string &url,
                                 const std::string &resourceId);
    std::string getResourceUrl(const std::string &url,
                               const std::string &resourceId);
    std::string getChildrenUrl(const std::string &url,
                               const std::string &resourceId);
    std::string getRouteUrl(const std::string &url);
    std::string getSchemaUrl(const std::string &url);
    std::string getCurrentUserUrl(const std::string &url);
    std::string getUploadUrl(const std::string &url);
    bool checkVersion(const std::string &version, int major, int minor, int patch);
    std::string createResource(const std::string &url, const std::string &payload,
                               char **httpOptions);
    bool deleteResource(const std::string &url, const std::string &resourceId,
        char **httpOptions);
    bool renameResource(const std::string &url, const std::string &resourceId,
        const std::string &newName, char **httpOptions);
    bool updateResource(const std::string &url, const std::string &resourceId,
        const std::string &payload, char **httpOptions);
    std::string objectTypeToNGWClsType(enum ngsCatalogObjectType type);
    std::string resmetaSuffix(CPLJSONObject::Type eType);

    std::string getFeatureUrl(const std::string &url,
                              const std::string &resourceId,
                              const std::string &featureId);

    // Tracks
    std::string getTrackerUrl();
    bool sendTrackPoints(const std::string &payload);

    // Features
    bool updateFeature(const std::string &url, const std::string &resourceId,
                       const std::string &featureId, const std::string &payload,
                       char **httpOptions);

    // Attachments
    std::string getAttachmentUrl(const std::string &url,
                                 const std::string &resourceId,
                                 const std::string &featureId,
                                 const std::string &attachmentId);
    std::string getAttachmentCreateUrl(const std::string &url,
                                 const std::string &resourceId,
                                 const std::string &featureId);
    std::string getAttachmentDownloadUrl(const std::string &url,
                                         const std::string &resourceId,
                                         const std::string &featureId,
                                         const std::string &attachmentId);
    bool deleteAttachment(const std::string &url,
                          const std::string &resourceId,
                          const std::string &featureId,
                          const std::string &attachmentId, char **httpOptions);
    bool deleteAttachments(const std::string &url,
                           const std::string &resourceId,
                           const std::string &featureId, char **httpOptions);

    GIntBig addAttachment(const std::string &url, const std::string &resourceId,
                          const std::string &featureId,
                          const std::string &payload, char **httpOptions);
} // namespace ngw

/**
 * @brief The NGWConnectionBase class
 */
class NGWConnectionBase : public ConnectionBase
{
public:
    std::string connectionUrl() const;
    bool isClsSupported(const std::string &cls) const;
    std::string userPwd() const;
    SpatialReferencePtr spatialReference() const;
protected:
    mutable std::string m_url, m_user;
    mutable std::string m_password; // TODO: When move authstore to GDAL remove password storing.
    std::vector<std::string> m_availableCls;
};

/**
 * @brief The NGWResouceBase class
 */
class NGWResourceBase
{
public:
    explicit NGWResourceBase(const CPLJSONObject &resource = CPLJSONObject(),
                             NGWConnectionBase *connection = nullptr);
    virtual ~NGWResourceBase() = default;
    bool remove();
    bool changeName(const std::string &newName);
    NGWConnectionBase *connection() const;
    std::string resourceId() const;
    std::string url() const;
    bool isSyncable() const;

    //static
public:
    static bool isNGWResource(const enum ngsCatalogObjectType type);


protected:
    Properties metadata(const std::string &domain) const;
    std::string metadataItem(const std::string &key,
                             const std::string &defaultValue,
                             const std::string &domain) const;
protected:
    std::string m_resourceId;
    NGWConnectionBase *m_connection;
    std::map<std::string, std::string> m_resmeta;
    std::string m_keyName, m_description;
    std::string m_creationDate;
    bool m_isSyncable;
};

/**
 * @brief The NGWResouce class
 */
class NGWResource : public Object, public NGWResourceBase
{
public:
    explicit NGWResource(ObjectContainer * const parent,
                         const enum ngsCatalogObjectType type,
                         const std::string &name,
                         const CPLJSONObject &resource = CPLJSONObject(),
                         NGWConnectionBase *connection = nullptr);
    virtual ~NGWResource() override;
    virtual CPLJSONObject asJson() const;

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual bool rename(const std::string &newName) override;
    virtual bool canRename() const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool sync() override;

protected:
    bool m_hasPendingChanges;
};

/**
 * @brief The NGWResourceGroup class
 */
class NGWResourceGroup : public ObjectContainer, public NGWResourceBase
{
public:
    explicit NGWResourceGroup(ObjectContainer * const parent,
                              const std::string &name,
                              const CPLJSONObject &resource = CPLJSONObject(),
                              NGWConnectionBase *connection = nullptr);
    virtual ObjectPtr getResource(const std::string &resourceId) const;
    virtual void addResource(const CPLJSONObject &resource);

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual bool rename(const std::string &newName) override;
    virtual bool canRename() const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;

    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
                    const std::string &name, const Options &options) override;
    virtual bool canPaste(const enum ngsCatalogObjectType type) const override;
    virtual int paste(ObjectPtr child, bool move = false,
                      const Options &options = Options(),
                      const Progress &progress = Progress()) override;

private:
    std::string normalizeDatasetName(const std::string &name) const;
    bool isNameValid(const std::string &name) const;
};

/**
 * @brief The NGWTrackersGroup class
 */
class NGWTrackersGroup : public NGWResourceGroup
{
public:
    explicit NGWTrackersGroup(ObjectContainer * const parent,
                              const std::string &name,
                              const CPLJSONObject &resource = CPLJSONObject(),
                              NGWConnectionBase *connection = nullptr);

    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
                    const std::string &name, const Options &options) override;
};

/**
 * @brief The NGWConnection class
 */
class NGWConnection : public NGWResourceGroup, public NGWConnectionBase
{
public:
    explicit NGWConnection(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &path);
    virtual ~NGWConnection() override;

    // NGWResourceGroup
    virtual bool loadChildren() override;

    // Object interface
public:
    virtual bool destroy() override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool setProperty(const std::string &key,
                             const std::string &value,
                             const std::string &domain) override;

    // ConnectionBase
    virtual bool open() override;
    virtual void close() override;

public:
    void fillProperties() const;

private:
    void fillCapabilities();

private:
    mutable std::string m_searchApiUrl, m_versionApiUrl;
};

/**
 * @brief The NGWServiceLayer class
 */
class NGWServiceLayer
{
public:
    explicit NGWServiceLayer(const std::string &key, const std::string &name,
                             NGWResourceBase *resource);
    explicit NGWServiceLayer(const std::string &key, const std::string &name,
                             int resourceId);
    virtual ~NGWServiceLayer() = default;

    // Simple class, no need getter/setter
    std::string m_key;
    std::string m_name;
    int m_resourceId;
};

using NGWServiceLayerPtr = std::shared_ptr<NGWServiceLayer>;

/**
 * @brief The NGWWFSServiceLayer class
 */
class NGWWFSServiceLayer : public NGWServiceLayer
{
public:
    explicit NGWWFSServiceLayer(const std::string &key, const std::string &name,
                                NGWResourceBase *resource, int maxfeatures = 1000);

    explicit NGWWFSServiceLayer(const std::string &key, const std::string &name,
                                int resourceId, int maxfeatures = 1000);
    int m_maxfeatures;
};

/**
 * @brief The NGWWMSServiceLayer class
 */
class NGWWMSServiceLayer : public NGWServiceLayer
{
public:
    explicit NGWWMSServiceLayer(const std::string &key, const std::string &name,
                                NGWResourceBase *resource,
                                const std::string &minScaleDenom = "",
                                const std::string &maxScaleDenom = "");
    explicit NGWWMSServiceLayer(const std::string &key, const std::string &name,
                                int resourceId,
                                const std::string &minScaleDenom = "",
                                const std::string &maxScaleDenom = "");
    std::string m_minScaleDenom;
    std::string m_maxScaleDenom;
};

/**
 * @brief The NGWService class. Base class for WMS and WFS services
 */
class NGWService : public NGWResource
{
public:
    explicit NGWService(ObjectContainer * const parent,
                        const enum ngsCatalogObjectType type,
                        const std::string &name,
                        const CPLJSONObject &resource = CPLJSONObject(),
                        NGWConnectionBase *connection = nullptr);

    std::vector<NGWServiceLayerPtr> layers() const;
    bool addLayer(const std::string &key, const std::string &name,
                  NGWResourceBase *resource);
    bool changeLayer(const std::string &oldKey, const std::string &key,
                     const std::string &name, NGWResourceBase *resource);
    bool deleteLayer(const std::string &key);

    // NGWResource interface
public:
    virtual CPLJSONObject asJson() const override;

private:
    std::vector<NGWServiceLayerPtr> m_layers;
};

/**
 * @brief The NGWStyle class
 */
class NGWStyle : public NGWResource
{
public:
    explicit NGWStyle(ObjectContainer * const parent,
                      const enum ngsCatalogObjectType type,
                      const std::string &name,
                      const CPLJSONObject &resource = CPLJSONObject(),
                      NGWConnectionBase *connection = nullptr);
public:
    static NGWStyle *createStyle(NGWResourceBase *parent,
                                 const enum ngsCatalogObjectType type,
                                 const std::string &name,
                                 const Options &options);

    // Object interface
public:
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool setProperty(const std::string &key,
                             const std::string &value,
                             const std::string &domain) override;

    // NGWResource interface
public:
    virtual CPLJSONObject asJson() const override;

private:
    std::string m_style, m_stylePath;

};

}

#endif // NGSNGWCONNECTION_H
