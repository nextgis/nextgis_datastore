/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
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
 
#ifndef COMMON_H
#define COMMON_H
 
#ifdef NGM_STATIC
#   define EXTERN 
#else
#   if defined (_WIN32) || defined (WINDOWS)
#    ifdef NGM_EXPORTS
#      ifdef __GNUC__
#        define EXTERN __attribute__((dllexport))
#      else        
#        define EXTERN __declspec(dllexport)
#      endif 
#    else
#      ifdef __GNUC__
#        define EXTERN __attribute__((dllimport))
#      else        
#        define EXTERN __declspec(dllimport)
#      endif 
#    endif
#   else
#     if __GNUC__ >= 4
#       define EXTERN __attribute__((visibility("default")))
#     else
#       define EXTERN                
#     endif 
#   endif
#endif

#endif // COMMON_H
