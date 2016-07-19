/******************************************************************************
*  Project: NextGIS ...
*  Purpose: Application for ...
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2012-2016 NextGIS, info@nextgis.ru
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "dataset.h"
#include "datastore.h"
#include "api.h"

using namespace ngs;

Dataset::Dataset(DataStore *datastore, const string &name,
                 const string &alias) : m_name(name),
                 m_deleted(false), m_datastore(datastore)
{
    if(alias.empty ())
        m_alias = m_name;
    else
        m_alias = alias;

    m_type = UNDEFINED;
}

Dataset::Type Dataset::type() const
{
    return m_type;
}

string Dataset::name() const
{
    return m_name;
}

string Dataset::alias() const
{
    return m_alias;
}

int Dataset::destroy()
{
    int nRet = m_datastore->destroyDataset(m_type, m_name);
    if(nRet == ngsErrorCodes::SUCCESS)
        m_deleted = true;
    return nRet;
}

bool Dataset::isDeleted() const
{
    return m_deleted;
}








