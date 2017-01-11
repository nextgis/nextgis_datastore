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


public class DirectoryEntry
{
    protected String mBaseName;
    protected String mExtension;
    protected int    mType;


    public DirectoryEntry()
    {
        mType = DirectoryEntryType.DET_UNKNOWN;
    }


    public DirectoryEntry(
            String baseName,
            String extension,
            int type)
    {
        mBaseName = baseName;
        mExtension = extension;
        mType = type;
    }


    public String getBaseName()
    {
        return mBaseName;
    }


    public String getExtension()
    {
        return mExtension;
    }


    public String getFullName()
    {
        if (mBaseName.length() == 0 && mExtension.length() == 0) {
            return "";
        }
        if (mExtension.length() == 0) {
            return mBaseName;
        }
        return mBaseName + "." + mExtension;
    }


    public int getType()
    {
        return mType;
    }


    public boolean isDirectory()
    {
        return (mType & DirectoryEntryType.DET_DIRECTORY) > 0;
    }


    public boolean isFile()
    {
        return (mType & DirectoryEntryType.DET_FILE) > 0;
    }
}
