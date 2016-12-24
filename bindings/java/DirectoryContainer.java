/*
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 * *****************************************************************************
 * Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.nextgis.store.bindings;

import java.io.File;


public class DirectoryContainer
{
    protected String           mParentPath;
    protected String           mDirectoryName;
    protected DirectoryEntry[] mEntries;


    public DirectoryContainer()
    {
    }


    public String getParentPath()
    {
        return mParentPath;
    }


    public String getDirectoryName()
    {
        return mDirectoryName;
    }


    public String getPath()
    {
        if (mParentPath.length() == 0 && mDirectoryName.length() == 0) {
            return "";
        }

        if (mParentPath.length() == 0) {
            return mDirectoryName;
        }

        return mParentPath + File.separator + mDirectoryName;
    }


    public String getEntryPath(int index)
    {
        return getPath() + File.separator + mEntries[index].getFullName();
    }


    public DirectoryEntry[] getEntries()
    {
        return mEntries;
    }
}
