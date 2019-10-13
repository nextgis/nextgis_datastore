/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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
#ifndef NGSMAPVIEW_H
#define NGSMAPVIEW_H

#include "map.h"
#include "maptransform.h"

#include "ogr_geometry.h"

#include "overlay.h"

namespace ngs {

/**
 * @brief The MapView class Base class for map with render support
 */
class MapView : public Map, public MapTransform
{
public:
    MapView();
    explicit MapView(const std::string &name, const std::string &description,
            unsigned short epsg, const Envelope &bounds);
    virtual ~MapView() override = default;
    virtual bool draw(enum ngsDrawState state, const Progress &progress = Progress());
    virtual void invalidate(const Envelope &bounds) = 0;

    size_t overlayCount() const { return m_overlays.size(); }
    OverlayPtr getOverlay(enum ngsMapOverlayType type) const;
    void setOverlayVisible(int typeMask, bool visible);
    int overlayVisibleMask() const;
    virtual bool setOptions(const Options &options);
    virtual bool setSelectionStyleName(enum ngsStyleType styleType,
                                       const std::string &name) = 0;
    virtual bool setSelectionStyle(enum ngsStyleType styleType,
                                   const CPLJSONObject &style) = 0;
    virtual std::string selectionStyleName(enum ngsStyleType styleType) const = 0;
    virtual CPLJSONObject selectionStyle(enum ngsStyleType styleType) const = 0;
    virtual bool addIconSet(const std::string &name, const std::string &path,
                            bool ownByMap);
    virtual bool removeIconSet(const std::string &name);
    virtual ImageData iconSet(const std::string &name) const;
    virtual bool hasIconSet(const std::string &name) const;

    // Map interface
protected:
    virtual bool openInternal(const CPLJSONObject &root, MapFile * const mapFile) override;
    virtual bool saveInternal(CPLJSONObject &root, MapFile * const mapFile) override;

protected:
    virtual void clearBackground() = 0;
    virtual void createOverlays() = 0;
    virtual size_t overlayIndexForType(enum ngsMapOverlayType type) const;
    virtual ImageData iconSetData(const std::string &path) const;

protected:
    std::array<OverlayPtr, 4> m_overlays;

    typedef struct _iconSetItem {
        std::string name;
        std::string path;
        bool ownByMap;

        bool operator==(const struct _iconSetItem& other) const {
            return compare(name, other.name);
        }
        bool operator<(const struct _iconSetItem& other) const {
            return compareStrings(name, other.name) < 0;
        }
    } IconSetItem;
    std::vector<IconSetItem> m_iconSets;
};

using MapViewPtr = std::shared_ptr<MapView>;

class MapViewStab : public MapView
{
public:
    MapViewStab() : MapView() {}
    explicit MapViewStab(const std::string &name, const std::string &description,
            unsigned short epsg, const Envelope &bounds) : MapView(name, description, epsg, bounds) {}
	virtual void invalidate(const Envelope &bounds) override {}
	virtual bool setSelectionStyleName(enum ngsStyleType styleType,
                                       const std::string &name) override { return false; }
    virtual bool setSelectionStyle(enum ngsStyleType styleType,
                                   const CPLJSONObject &style) override { return false; }
    virtual std::string selectionStyleName(enum ngsStyleType styleType) const override { return ""; }
    virtual CPLJSONObject selectionStyle(enum ngsStyleType styleType) const override { return CPLJSONObject(); }
protected:
    virtual void clearBackground() override {}
    virtual void createOverlays() override {}
};

/**
 * @brief The IRenderLayer class Interface for renderable map layers
 */
// class IRenderLayer
// {
// public:
    // virtual ~IRenderLayer() = default;
    // virtual double draw(enum ngsDrawState state, MapView *map, float level,
                        // const Progress &progress = Progress()) = 0;
	// virtual bool setStyleName(const std::string &name) = 0;
    // virtual bool setStyle(const CPLJSONObject &style) = 0;
    // virtual CPLJSONObject style() const = 0;
    // virtual std::string styleName() const = 0;
// };

// class IOverlay
// {
// public:
    // virtual ~IOverlay() = default;
    // virtual double draw(enum ngsDrawState state, MapView *map, float level,
                        // const Progress &progress = Progress()) = 0;
// };

}
#endif // NGSMAPVIEW_H
