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
#ifndef NGSSTRINGUTIL_H
#define NGSSTRINGUTIL_H

#include <string>
#include <vector>

namespace ngs {

int constexpr length(const char* str)
{
    return *str ? 1 + length(str + 1) : 0;
}

std::string stripUnicode(const std::string &str, const char replaceChar = 'x');
std::string normalize(const std::string &str, const std::string &lang = "");
std::vector<std::string> fillStringList(char** strings);
bool compare(const std::string &first, const std::string &second,
             bool caseSensetive = false);
bool comparePart(const std::string &first, const std::string &second,
             unsigned int count, bool caseSensetive = false);
int compareStrings(const std::string &first, const std::string &second,
                   bool caseSensetive = false);
bool startsWith(const std::string &str, const std::string &part,
                bool caseSensetive = false);
bool endsWith(const std::string &str, const std::string &part,
              bool caseSensetive = false);
std::string md5(const std::string &val);
std::string fromCString(const char *str);
std::string random(int size);
std::string crypt_salt();
std::string crypt_key();
std::string encrypt(const std::string& ptext);
std::string decrypt(const std::string& ctext);
std::string deviceId(bool regenerate = false);
bool toBool(const std::string &val);

}
#endif // NGSSTRINGUTIL_H
