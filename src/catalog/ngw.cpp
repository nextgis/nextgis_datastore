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
#include "api_priv.h"
#include "ngw.h"

#include "cpl_http.h"
#include "cpl_json.h"

namespace ngs {

//------------------------------------------------------------------------------
// NGWResourceGroup
//------------------------------------------------------------------------------

NGWResourceGroup::NGWResourceGroup(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &path) :
    ObjectContainer(parent, CAT_CONTAINER_NGWGROUP, name, path)
{

}

//------------------------------------------------------------------------------
// NGWConnection
//------------------------------------------------------------------------------


//char **OGRNGWDataset::GetHeaders() const
//{
//    char **papszOptions = nullptr;
//    papszOptions = CSLAddString(papszOptions, "HEADERS=Accept: */*");
//    if( !osUserPwd.empty() )
//    {
//        papszOptions = CSLAddString(papszOptions, "HTTPAUTH=BASIC");
//        std::string osUserPwdOption("USERPWD=");
//        osUserPwdOption += osUserPwd;
//        papszOptions = CSLAddString(papszOptions, osUserPwdOption.c_str());
//    }
//    return papszOptions;
//}

NGWConnection::NGWConnection(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &path) :
    NGWResourceGroup(parent, name, path)
{
    m_type = CAT_CONTAINER_NGWCONNECTION;
}

bool NGWConnection::loadChildren()
{
    if(m_childrenLoaded) {
        return true;
    }

//    if(m_url.empty()) {

//    }

    return true;
}

//bool FillChildren()
//{

//}

}
