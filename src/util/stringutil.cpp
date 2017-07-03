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
#include "stringutil.h"

// stl
#include <algorithm>
#include <map>

#include "openssl/md5.h"

namespace ngs {

const static std::map<const char*, const char*> ruMap = { {"а", "a"}, {"б", "b"},
    {"в", "v"}, {"г", "g"}, {"д", "d"}, {"е", "e"}, {"ё", "ye"}, {"ж", "zh"},
    {"з", "z"}, {"и", "i"}, {"й", "y"}, {"к", "k"}, {"л", "l"}, {"м", "m"},
    {"н", "n"}, {"о", "o"}, {"п", "p"}, {"р", "r"}, {"с", "s"}, {"т", "t"},
    {"у", "u"}, {"ф", "f"}, {"х", "ch"}, {"ц", "z"}, {"ч", "ch"}, {"ш", "sh"},
    {"щ", "ch"}, {"ь", "'"}, {"ы", "y"}, {"ъ", "'"}, {"э", "e"}, {"ю", "yu"},
    {"я", "ya"}, {"А", "A"}, {"Б", "B"}, {"В", "V"}, {"Г", "G"}, {"Д", "D"},
    {"Е", "E"}, {"Ё", "Ye"}, {"Ж", "Zh"}, {"З", "Z"}, {"И", "I"}, {"Й", "Y"},
    {"К", "K"}, {"Л", "L"}, {"М", "M"}, {"Н", "N"}, {"О", "O"}, {"П", "P"},
    {"Р", "R"}, {"С", "S"}, {"Т", "T"}, {"У", "U"}, {"Ф", "F"}, {"Х", "Ch"},
    {"Ц", "Z"}, {"Ч", "Ch"}, {"Ш", "Sh"}, {"Щ", "Ch"}, {"Ь", "'"}, {"Ы", "Y"},
    {"Ъ", "'"}, {"Э", "E"}, {"Ю", "Yu"}, {"Я", "Ya"}
 };

bool invalidChar (char c)
{
    return !isprint( static_cast<unsigned char>( c ) );
}

CPLString stripUnicode(const CPLString &str, const char replaceChar)
{
    CPLString out = str;
    std::replace_if(out.begin (), out.end (), invalidChar, replaceChar);

    return out;
}

CPLString normalize(const CPLString &str, const CPLString &lang)
{
    CPLString out = str;
    if(lang.empty())
        return stripUnicode(str);

    if(EQUALN(lang, "ru", 2)) {
        auto first = str.begin ();
        char buf[2] = {'\0', '\0'};
        while (first!=str.end ()) {
            buf[0] = *first;
            auto tr = ruMap.find(buf);
            if(tr != ruMap.end ())
                out += tr->second;
            else
                out += *first;
            ++first;
        }
    }
    else {
        return stripUnicode (str);
    }
    return out;
}

CPLString md5(const char *value)
{
    unsigned char digest[16];
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, value, strlen(value));
    MD5_Final(digest, &context);

    CPLString out;
    for(int i = 0; i < 16; ++i)
        out += CPLSPrintf("%02x", static_cast<unsigned int>(digest[i]));

    return out;
}

}
