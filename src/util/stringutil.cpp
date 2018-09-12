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

#include "cpl_string.h"

// stl
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

#include <openssl/md5.h>

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

std::string stripUnicode(const std::string &str, const char replaceChar)
{
    std::string out = str;
    std::replace_if(out.begin (), out.end (), invalidChar, replaceChar);

    return out;
}

std::string normalize(const std::string &str, const std::string &lang)
{
    std::string out = str;
    if(lang.empty())
        return stripUnicode(str);

    if(EQUALN(lang.c_str(), "ru", 2)) {
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

std::vector<std::string> fillStringList(char **strings)
{
    std::vector<std::string> out;
    if(!strings) {
        return out;
    }
    int counter = 0;
    while(strings[counter] != nullptr) {
        out.push_back(strings[counter++]);
    }
    return out;
}

bool compare(const std::string &first, const std::string &second,
             bool caseSensetive)
{
    if(caseSensetive) {
        return first == second;
    }
    else {
        return EQUAL(first.c_str(), second.c_str());
    }
}

bool comparePart(const std::string &first, const std::string &second,
                 unsigned int count, bool caseSensetive)
{
    if(first.size() < count || second.size() < count) {
        return false;
    }
    if(caseSensetive) {
        return first.substr(0, count) == second.substr(0, count);
    }
    else {
        return EQUALN(first.c_str(), second.c_str(), count);
    }
}

bool startsWith(const std::string &str, const std::string &part, bool caseSensetive)
{
    return comparePart(str, part, static_cast<unsigned>(part.size()), caseSensetive);
}

std::string md5(const std::string &val)
{
    unsigned char digest[MD5_DIGEST_LENGTH];

    const unsigned char *preparedVal =
            reinterpret_cast<const unsigned char*>(val.c_str());
    MD5(preparedVal, val.size(), digest);

    std::ostringstream out;

    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        out << std::hex << std::setfill('0') << std::setw(2) <<
               static_cast<unsigned>(digest[i]);
    }

    return out.str();
}

int compareStrings(const std::string &first, const std::string &second,
                   bool caseSensetive)
{
    if(caseSensetive) {
        return first.compare(second);
    }
    else {
        return STRCASECMP(first.c_str(), second.c_str());
    }
}

std::string fromCString(const char *str)
{
    if(str == nullptr) {
        return "";
    }
    return str;
}

}
