#include "forgeconvert/xml/writer.hpp"
namespace forgeconvert::xml {
std::string utf8(unsigned cp) {
    std::string s;
    if(cp<0x80) s+=static_cast<char>(cp);
    else if(cp<0x800) { s+=static_cast<char>(0xc0|(cp>>6)); s+=static_cast<char>(0x80|(cp&63)); }
    else if(cp<0x10000) { s+=static_cast<char>(0xe0|(cp>>12)); s+=static_cast<char>(0x80|((cp>>6)&63)); s+=static_cast<char>(0x80|(cp&63)); }
    else { s+=static_cast<char>(0xf0|(cp>>18)); s+=static_cast<char>(0x80|((cp>>12)&63)); s+=static_cast<char>(0x80|((cp>>6)&63)); s+=static_cast<char>(0x80|(cp&63)); }
    return s;
}
Result<std::string> escape(std::string_view text) {
    std::string out;
    for(std::size_t i=0;i<text.size();) {
        auto start=i; unsigned c=static_cast<unsigned char>(text[i++]), cp=c, n=0, minimum=0;
        if(c>=0x80) {
            if(c>=0xc2 && c<=0xdf) { n=1; cp=c&31; minimum=0x80; }
            else if(c>=0xe0 && c<=0xef) { n=2; cp=c&15; minimum=0x800; }
            else if(c>=0xf0 && c<=0xf4) { n=3; cp=c&7; minimum=0x10000; }
            else return Error{ErrorCode::invalid_input,"Invalid UTF-8",start};
            if(n>text.size()-i) return Error{ErrorCode::invalid_input,"Truncated UTF-8",start};
            for(unsigned k=0;k<n;++k) { c=static_cast<unsigned char>(text[i++]); if((c&0xc0)!=0x80) return Error{ErrorCode::invalid_input,"Invalid UTF-8 continuation",i-1}; cp=(cp<<6)|(c&63); }
            if(cp<minimum) return Error{ErrorCode::invalid_input,"Overlong UTF-8",start};
        }
        if(!(cp==9 || cp==10 || cp==13 || (cp>=32 && cp<=0xd7ff) || (cp>=0xe000 && cp<=0xfffd) || (cp>=0x10000 && cp<=0x10ffff)))
            return Error{ErrorCode::invalid_input,"Character prohibited by XML 1.0",start};
        switch(cp) { case '&': out+="&amp;"; break; case '<': out+="&lt;"; break; case '>': out+="&gt;"; break; case '"': out+="&quot;"; break; case '\'': out+="&apos;"; break; default: out.append(text.substr(start,i-start)); }
    }
    return out;
}
}
