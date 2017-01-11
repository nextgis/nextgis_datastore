/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 32 of the License, or
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

%module Api

%inline %{
// DirectoryEntry constructor and fields
jclass classEntry = NULL;
jmethodID constructorEntry = NULL;
jfieldID fieldBaseName = NULL;
jfieldID fieldExtension = NULL;
jfieldID fieldType = NULL;

// DirectoryContainer constructor and fields
jclass classContainer = NULL;
jmethodID constructorContainer = NULL;
jfieldID fieldEntries = NULL;
jfieldID fieldParentPath = NULL;
jfieldID fieldDirectoryName = NULL;


class DirectoryContainerLoadCallback
{
public:
    virtual ~DirectoryContainerLoadCallback() {  }
    virtual void run(ngsDirectoryContainer* container)
    {
    }
};
%}

%{
void
JavaDirectoryContainerLoadProxy(ngsDirectoryContainer* container, void* callbackArguments)
{
    DirectoryContainerLoadCallback* callback = (DirectoryContainerLoadCallback*) callbackArguments;
    callback->run(container);
}
%}
