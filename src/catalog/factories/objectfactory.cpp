/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#include "objectfactory.h"

namespace ngs {

ObjectFactory::ObjectFactory() : m_enabled(true)
{

}

bool ObjectFactory::getEnabled() const
{
    return m_enabled;
}

void ObjectFactory::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

ObjectFactory::FORMAT_RESULT ObjectFactory::isFormatSupported(
        const CPLString &name, std::vector<CPLString> extensions,
        FORMAT_EXT testExts)
{
    ObjectFactory::FORMAT_RESULT out;
    out.isSupported = false;

    unsigned char counter = 0;

    for(const auto& extension : extensions) {
        if(EQUAL(extension, testExts.mainExt)) { // Check main format extension
            counter++;
            out.name = CPLFormFilename(nullptr, name, extension);
        }
        else {
            int i = 0;
            bool breakCompare = false;
            while(testExts.mainExts[i] != nullptr) { // Check additional format extensions [required]
                if(EQUAL(extension, testExts.mainExts[i])) {
                    counter++;
                    breakCompare = true;
                    out.siblingFiles.push_back(CPLFormFilename(nullptr, name,
                                                               extension));
                }
                i++;
            }
            if(!breakCompare) {
                i = 0;
                while(testExts.extraExts[i] != nullptr) { // Check additional format extensions [optional]
                    if(EQUAL(extension, testExts.extraExts[i])) {
                        out.siblingFiles.push_back(CPLFormFilename(nullptr, name,
                                                                   extension));
                    }
                    i++;
                }
            }
        }
    }

    if(counter > CSLCount(testExts.mainExts) ) {
        out.isSupported = true;
    }

    return out;
}

void ObjectFactory::eraseNames(const CPLString &name,
                               const std::vector<CPLString> &siblingFiles,
                               std::vector<const char *> * const names)
{
    auto lastItem = names->end();
    for(const auto& siblingFile : siblingFiles) {
        lastItem = std::remove(names->begin(), lastItem, siblingFile);
    }
    lastItem = std::remove(names->begin(), lastItem, name);

    if(lastItem != names->end()) {
        names->erase(lastItem, names->end());
    }
}


}
