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

#include "api_priv.h"
#include "error.h"

#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "settings.h"

// stl
#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/rand.h>

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

constexpr unsigned int BLOCK_SIZE = 16;
constexpr unsigned int KEY_SIZE = 32;

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


static std::string toHex(unsigned char *value, int size)
{
    std::ostringstream out;

    for(int i = 0; i < size; ++i) {
        out << std::hex << std::setfill('0') << std::setw(2) <<
               static_cast<unsigned>(value[i]);
    }

    return out.str();
}

#ifdef _WIN32
#include <limits.h>
#include <intrin.h>
#elif __ANDROID__
    #include <cpu-features.h>
    #include <sys/socket.h>
    #include <linux/sockios.h>
    #include <linux/if.h>
    #include <linux/in.h>
#elif __APPLE__
    #include <ifaddrs.h>
    #include <arpa/inet.h>
    #include <net/if_dl.h>
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE
        #include <sys/types.h>
        #include <sys/sysctl.h>
        static uint32_t getSysInfoByName(const char* type_specifier) {
            size_t size = 0;
            uint32_t result = 0;
            sysctlbyname(type_specifier, nullptr, &size, nullptr, 0);
            if (size == sizeof(uint32_t)) {
                sysctlbyname(type_specifier, &result, &size, nullptr, 0);
            }
            return result;
        }
    #endif // TARGET_OS_IPHONE
#else
#endif



static std::string cpuHash()
{
    uint32_t regs[4];
    memset(regs, 0, sizeof(regs));
#ifdef _WIN32
    __cpuid((int *)regs, (int)i);
#elif __ANDROID__
    uint64_t s1 = android_getCpuFeatures();
    int s2 android_getCpuFamily();
    memcpy(regs, static_cast<const void*>(&s1), sizeof(uint64_t));
    memcpy(regs + sizeof(uint64_t), static_cast<const void*>(&s2), sizeof(int));
#elif TARGET_OS_IPHONE
    regs[0] = getSysInfoByName("hw.cpufamily");
    regs[1] = getSysInfoByName("hw.cputype");
    regs[2] = getSysInfoByName("hw.cpusubtype");
#else
    asm volatile
          ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
           : "a" (0), "c" (0));
#endif

//    std::string vendor;
//    vendor += std::string(reinterpret_cast<const char *>(&regs[1]), 4);
//    vendor += std::string(reinterpret_cast<const char *>(&regs[3]), 4);
//    vendor += std::string(reinterpret_cast<const char *>(&regs[2]), 4);
//    vendor += std::string(reinterpret_cast<const char *>(&regs[0]), 4);

    return CPLSPrintf("%u%u%u%u" CPL_FRMT_GIB "%d", regs[1], regs[3], regs[2],
            regs[0], CPLGetPhysicalRAM(), CPLGetNumCPUs());
}

static std::string macHash()
{
    std::set<std::string> macs;
#ifdef _WIN32
    IP_ADAPTER_INFO AdapterInfo[32];
    DWORD bufLen = sizeof(AdapterInfo);
    if(GetAdaptersInfo(AdapterInfo, &bufLen) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO adapterInfo = AdapterInfo;
        while(adapterInfo) {
            std::string address =
                    CPLSPrintf("%02x:%02x:%02x:%02x:%02x:%02x",
                               adapterInfo->Address[0], adapterInfo->Address[1],
                               adapterInfo->Address[2], adapterInfo->Address[3],
                               adapterInfo->Address[4], adapterInfo->Address[5]);
            CPLDebug("ngstore", "%s", address.c_str());
            macs.insert(address);
            adapterInfo = adapterInfo->Next;
        }
    }
#elif __APPLE__
    // https://stackoverflow.com/a/35242525/2901140
    struct ifaddrs *ifaddr = nullptr;
    struct ifaddrs *ifa = nullptr;
    if(getifaddrs(&ifaddr) == 0) {
        ifa = ifaddr;
        while(ifa != nullptr) {
            if(ifa->ifa_addr->sa_family == AF_LINK) {
//                struct sockaddr_in *s =
//                        reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
//                std::string ip_address = inet_ntoa(s->sin_addr);
                struct sockaddr_dl *d =
                        reinterpret_cast<struct sockaddr_dl*>(ifa->ifa_addr);
                unsigned char *ptr = reinterpret_cast<unsigned char *>(LLADDR(d));
                std::string address = CPLSPrintf("%02x:%02x:%02x:%02x:%02x:%02x",
                                                 *ptr, *(ptr+1), *(ptr+2),
                                                 *(ptr+3), *(ptr+4), *(ptr+5));
                CPLDebug("ngstore", "%s: %02x:%02x:%02x:%02x:%02x:%02x",
                         ifa->ifa_name, *ptr, *(ptr+1), *(ptr+2),  *(ptr+3),
                         *(ptr+4), *(ptr+5));
                macs.insert(address);
            }
            ifa = ifa->ifa_next;
        }
        freeifaddrs(ifaddr);
    }

#else // Android && Unix
    // https://stackoverflow.com/a/16859693/2901140
    int sockfd;
    if((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) >= 0) {
        struct ifconf conf;
        char ifconfbuf[128 * sizeof(struct ifreq)];
        memset(ifconfbuf, 0, sizeof(ifconfbuf));
        conf.ifc_buf = ifconfbuf;
        conf.ifc_len = sizeof( ifconfbuf );
        if(ioctl(sockfd, SIOCGIFCONF, &conf) == 0) {
            struct ifreq *ifr;
            for(ifr = conf.ifc_req; ifr < conf.ifc_req + conf.ifc_len; ++ifr) {
                  if(ifr->ifr_addr.sa_data == (ifr + 1)->ifr_addr.sa_data) {
                     continue;
                  }
                  if(ioctl(sockfd, SIOCGIFFLAGS, ifr) != 0) {
                     continue;
                  }
                  if(ioctl(sockfd, SIOCGIFHWADDR, ifr) == 0) {
                      unsigned char *ptr = &(ifr->ifr_addr.sa_data);
                      std::string address = CPLSPrintf("%02x:%02x:%02x:%02x:%02x:%02x",
                                                       *ptr, *(ptr+1), *(ptr+2),
                                                       *(ptr+3), *(ptr+4), *(ptr+5));
                      CPLDebug("ngstore", "%s", address.c_str());
                      macs.insert(address);
                  }
            }
        }
        close(sockfd);
    }
#endif
    if(macs.empty()) {
        return "UNSUPPORTED";
    }
    else {
        std::ostringstream stream;
        std::copy(macs.begin(), macs.end(),
                  std::ostream_iterator<std::string>(stream, ","));
        return stream.str();
    }
}

std::string md5(const std::string &val)
{
    unsigned char digest[MD5_DIGEST_LENGTH];

    const unsigned char *preparedVal =
            reinterpret_cast<const unsigned char*>(val.c_str());
    MD5(preparedVal, val.size(), digest);

    return toHex(digest, MD5_DIGEST_LENGTH);
}

std::string crypt_salt()
{
    return random(BLOCK_SIZE);
}

std::string crypt_key()
{
    return random(KEY_SIZE);
}

std::string random(int size)
{
    unsigned char *key = new unsigned char[size];
    int rc = RAND_bytes(key, size);
    if (rc != 1) {
        errorMessage(_("Failed to generate random string."));
        return "";
    }

    return toHex(key, size);
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

using EVP_CIPHER_CTX_free_ptr = std::unique_ptr<EVP_CIPHER_CTX,
    decltype(&::EVP_CIPHER_CTX_free)>;

// Adapted from https://wiki.openssl.org/images/5/5d/Evp-encrypt-cxx.tar.gz
std::string encrypt(const std::string& ptext)
{
    if(ptext.size() > 256) {
        errorMessage(_("Too long text to encrypt"));
        return "";
    }

    // Load the necessary cipher
    EVP_add_cipher(EVP_aes_256_cbc());

    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);

    Settings &settings = Settings::instance();
    std::string iv = settings.getString("crypt/iv", "");

    int count = 0;
    GByte *pabyiv = CPLHexToBinary(iv.c_str(), &count);
    GByte *pabykey = CPLHexToBinary(CPLGetConfigOption("CRYPT_KEY", ""), &count);

    int rc = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, pabykey,
                                pabyiv);

    CPLFree(pabyiv);
    CPLFree(pabykey);

    if (rc != 1) {
        errorMessage(_("EVP_EncryptInit_ex failed"));
        return "";
    }

    // Recovered text expands upto BLOCK_SIZE
    int out_len1 = 0;
    unsigned char ctext[512];

    rc = EVP_EncryptUpdate(ctx.get(), ctext, &out_len1,
                           reinterpret_cast<const unsigned char*>(&ptext[0]),
                           static_cast<int>(ptext.size()));
    if (rc != 1) {
        errorMessage(_("EVP_EncryptUpdate failed"));
        return "";
    }

    int out_len2 = 0;
    rc = EVP_EncryptFinal_ex(ctx.get(), ctext + out_len1, &out_len2);
    if (rc != 1) {
        errorMessage(_("EVP_EncryptFinal_ex failed"));
        return "";
    }

    return toHex(ctext, out_len1 + out_len2);
}

std::string decrypt(const std::string& ctext)
{
    std::string rtext;
    // Load the necessary cipher
    EVP_add_cipher(EVP_aes_256_cbc());

    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    Settings &settings = Settings::instance();
    std::string iv = settings.getString("crypt/iv", "");

    int count = 0;
    GByte *pabyiv = CPLHexToBinary(iv.c_str(), &count);
    GByte *pabykey = CPLHexToBinary(CPLGetConfigOption("CRYPT_KEY", ""), &count);
    int rc = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, pabykey,
                                pabyiv);

    CPLFree(pabyiv);
    CPLFree(pabykey);

    if (rc != 1) {
        errorMessage(_("EVP_DecryptInit_ex failed"));
        return "";
    }

    GByte *ctextIn = CPLHexToBinary(ctext.c_str(), &count);

    // Recovered text contracts upto BLOCK_SIZE
    rtext.resize(static_cast<size_t>(count));
    int out_len1 = 0;
    rc = EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(&rtext[0]),
            &out_len1, ctextIn, count);
    if (rc != 1) {
        errorMessage(_("EVP_DecryptUpdate failed"));
        return "";
    }

    int out_len2 = 0;
    rc = EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(&rtext[0]) + out_len1,
            &out_len2);

    if (rc != 1) {
        errorMessage(_("EVP_DecryptFinal_ex failed"));
        return "";
    }

    // Set recovered text size now that we know it
    rtext.resize(static_cast<size_t>(out_len1 + out_len2));

    return rtext;
}

std::string deviceId()
{
    std::string deviceHash = md5(cpuHash() + "-" + macHash());
    deviceHash.insert(8, "-");
    deviceHash.insert(5, "-");
    return deviceHash.substr(0, 16);
}

}
