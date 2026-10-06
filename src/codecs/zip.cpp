#include "forgeconvert/codecs/zip.hpp"
#include <limits>
#include <set>
namespace forgeconvert::codecs {
std::uint32_t crc32(std::string_view bytes) {
    std::uint32_t crc=0xffffffffU;
    for (unsigned char c: bytes) {
        crc ^= c;
        for (int i=0;i<8;++i) crc=(crc>>1)^((crc&1U)?0xedb88320U:0U);
    }
    return crc^0xffffffffU;
}
std::uint32_t adler32(std::string_view bytes) {
    std::uint32_t a=1,b=0;
    for (unsigned char c: bytes) { a=(a+c)%65521; b=(b+a)%65521; }
    return (b<<16)|a;
}
Result<std::string> zip_store(const std::vector<ZipEntry>& entries, std::size_t max_bytes) {
    try {
        if(entries.size()>=65535) fail(ErrorCode::unsupported,"ZIP64 entry count required");
        std::string out, central;
        auto put=[](std::string& s,std::uint32_t v,int n) { for(int i=0;i<n;++i) s.push_back(static_cast<char>((v>>(8*i))&255)); };
        std::set<std::string> names;
        for(const auto& entry:entries) {
            if(entry.name.empty() || entry.name.size()>65535 || entry.name.front()=='/' || entry.name.find("..")!=std::string::npos || entry.name.find('\\')!=std::string::npos || !names.insert(entry.name).second)
                fail(ErrorCode::arguments,"Invalid or duplicate ZIP entry name");
            if(entry.data.size()>=0xffffffffULL || out.size()>=0xffffffffULL) fail(ErrorCode::unsupported,"ZIP64 required");
            const auto crc=crc32(entry.data), size=static_cast<std::uint32_t>(entry.data.size()), offset=static_cast<std::uint32_t>(out.size());
            put(out,0x04034b50,4); put(out,20,2); put(out,0x800,2); put(out,0,2);
            put(out,0,2); put(out,33,2); put(out,crc,4); put(out,size,4); put(out,size,4);
            put(out,static_cast<std::uint32_t>(entry.name.size()),2); put(out,0,2); out+=entry.name; out+=entry.data;
            put(central,0x02014b50,4); put(central,20,2); put(central,20,2); put(central,0x800,2); put(central,0,2);
            put(central,0,2); put(central,33,2); put(central,crc,4); put(central,size,4); put(central,size,4);
            put(central,static_cast<std::uint32_t>(entry.name.size()),2);
            put(central,0,2); put(central,0,2); put(central,0,2); put(central,0,2); put(central,0,4); put(central,offset,4); central+=entry.name;
            if(out.size()>max_bytes || central.size()>max_bytes-out.size()) fail(ErrorCode::limit,"output_bytes exceeded");
        }
        if(out.size()+central.size()+22>max_bytes) fail(ErrorCode::limit,"output_bytes exceeded");
        if(out.size()+central.size()+22>=0xffffffffULL) fail(ErrorCode::unsupported,"ZIP64 required");
        auto offset=static_cast<std::uint32_t>(out.size()), size=static_cast<std::uint32_t>(central.size()); out+=central;
        put(out,0x06054b50,4); put(out,0,2); put(out,0,2); put(out,static_cast<std::uint32_t>(entries.size()),2); put(out,static_cast<std::uint32_t>(entries.size()),2);
        put(out,size,4); put(out,offset,4); put(out,0,2);
        return out;
    } catch(const Failure& e) { return e.error; }
}
}
