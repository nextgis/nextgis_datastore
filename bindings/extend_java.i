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
 
%include "arrays_java.i";

%module Api

// Do we need typemap to android or java.awt Color?
%extend _ngsRGBA {
   char *toString() {
       static char tmp[1024];
       sprintf(tmp,"RGBS (%g, %g, %g, %g)", self->R, self->G, self->B, self->A);
       return tmp;
   }
   _ngsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
       ngsRGBA *rgba = (ngsRGBA *) malloc(sizeof(ngsRGBA));
       rgba->R = r;
       rgba->G = g;
       rgba->B = b;
       rgba->A = a;
       return rgba;
   }
};
