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

%include "typemaps.i"
%include "enumtypeunsafe.swg" // for use in cases

%module Api

%javaconst(1);

%rename (ErrorCodes) ngsErrorCodes;
%rename (ChangeCodes) ngsChangeCodes;
%rename (SourceCodes) ngsSourceCodes;
%rename (DrawState) ngsDrawState;

%rename (Colour) _ngsRGBA;
%rename (Coordinate) _ngsCoordinate;
%rename (Position) _ngsPosition;
%rename (LoadTaskInfo) _ngsLoadTaskInfo;

%rename (Direction) ngsDirection;
