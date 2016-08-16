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
class ProgressCallback
{
public:
    virtual ~ProgressCallback() {  }
    virtual int run(double complete, const char* message)
    {
        return 1;
    }
};
%}

%{
int
JavaProgressProxy( double complete, const char *message, void *progressArguments )
{
    ProgressCallback* psProgressCallback = (ProgressCallback*) progressArguments;
    return psProgressCallback->run(complete, message);
}
%}


%inline %{
class NotificationCallback
{
public:
    virtual ~NotificationCallback() {  }
    virtual void run(enum ngsSourceCodes src, const char* table, long row, enum ngsChangeCodes operation)
    {
    }
};
%}

%{
void
JavaNotificationProxy(enum ngsSourceCodes src, const char* table, long row, enum ngsChangeCodes operation, void *progressArguments)
{
    NotificationCallback* psNotificationCallback = (NotificationCallback*) progressArguments;
    return psNotificationCallback->run(src, table, row, operation);
}
%}
