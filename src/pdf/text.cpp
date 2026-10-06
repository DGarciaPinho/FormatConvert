#include "pdf/internal.hpp"
#include "forgeconvert/xml/writer.hpp"
#include <algorithm>
#include <cmath>
namespace forgeconvert::pdf {
namespace {
Matrix compose(const Matrix& outer,const Matrix& inner) {
    return {outer.a*inner.a+outer.c*inner.b,outer.b*inner.a+outer.d*inner.b,
            outer.a*inner.c+outer.c*inner.d,outer.b*inner.c+outer.d*inner.d,
            outer.a*inner.e+outer.c*inner.f+outer.e,outer.b*inner.e+outer.d*inner.f+outer.f};
}
void translate(Matrix& m,double x,double y) { m.e+=m.a*x+m.c*y; m.f+=m.b*x+m.d*y; }
void finite(const Matrix& m) {
    for(double v:{m.a,m.b,m.c,m.d,m.e,m.f}) if(!std::isfinite(v)||std::abs(v)>1e9) fail(ErrorCode::invalid_input,"Matrix outside supported finite range");
}
unsigned decode(unsigned char c,bool winansi) {
    if(c<32 || c==127) fail(ErrorCode::unsupported,"Control glyph code unsupported");
    if(c<128) {
        if(!winansi && c==39) return 0x2019;
        if(!winansi && c==96) return 0x2018;
        return c;
    }
    if(!winansi) fail(ErrorCode::unsupported,"Non-ASCII StandardEncoding unsupported; specify WinAnsiEncoding");
    constexpr unsigned special[32]={0x20ac,0,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0,0x17d,0,0,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0,0x17e,0x178};
    if(c<160) { auto cp=special[c-128]; if(!cp) fail(ErrorCode::unsupported,"Undefined WinAnsi code"); return cp; }
    if(c==160) return 32; // WinAnsi space glyph
    if(c==173) return '-'; // WinAnsi hyphen glyph
    return c;
}
double helvetica_width(unsigned cp) {
    constexpr int widths[]={
        278,278,355,556,556,889,667,191,333,333,389,584,278,333,278,278,
        556,556,556,556,556,556,556,556,556,556,278,278,584,584,584,556,
        1015,667,667,722,722,667,611,778,722,278,500,667,556,833,722,778,
        667,778,722,667,611,722,667,944,667,667,611,278,278,278,469,556,
        333,556,556,500,556,556,278,556,556,222,222,500,222,833,556,556,
        556,556,333,500,278,556,500,722,500,500,500,334,260,334,584};
    if(cp>=32 && cp<=126) return widths[cp-32];
    // Accented Helvetica glyphs share their base Latin glyph's horizontal advance.
    unsigned base=0;
    if(cp>=0xc0&&cp<=0xc5) base='A'; else if(cp==0xc7) base='C'; else if(cp>=0xc8&&cp<=0xcb) base='E';
    else if(cp>=0xcc&&cp<=0xcf) base='I'; else if(cp==0xd1) base='N'; else if((cp>=0xd2&&cp<=0xd6)||cp==0xd8) base='O';
    else if(cp>=0xd9&&cp<=0xdc) base='U'; else if(cp==0xdd||cp==0x178) base='Y';
    else if(cp>=0xe0&&cp<=0xe5) base='a'; else if(cp==0xe7) base='c'; else if(cp>=0xe8&&cp<=0xeb) base='e';
    else if(cp>=0xec&&cp<=0xef) base='i'; else if(cp==0xf1) base='n'; else if((cp>=0xf2&&cp<=0xf6)||cp==0xf8) base='o';
    else if(cp>=0xf9&&cp<=0xfc) base='u'; else if(cp==0xfd||cp==0xff) base='y';
    if(base) return widths[base-32];
    switch(cp) {
        case 0xa1: return 333; case 0xa2: case 0xa3: case 0xa4: case 0xa5: return 556;
        case 0xa6: return 260; case 0xa7: return 556; case 0xa8: return 333; case 0xa9: return 737;
        case 0xaa: return 370; case 0xab: return 556; case 0xac: return 584; case 0xae: return 737;
        case 0xaf: return 333; case 0xb0: return 400; case 0xb1: return 584; case 0xb2: case 0xb3: return 333;
        case 0xb4: return 333; case 0xb5: return 556; case 0xb6: return 537; case 0xb7: return 278; case 0xb8: return 333;
        case 0xb9: return 333; case 0xba: return 365; case 0xbb: return 556; case 0xbc: case 0xbd: case 0xbe: return 834;
        case 0xbf: return 611; case 0xc6: return 1000; case 0xd0: return 722; case 0xd7: return 584; case 0xde: return 667;
        case 0xdf: return 611; case 0xe6: return 889; case 0xf0: return 556; case 0xf7: return 584; case 0xfe: return 556;
        case 0x20ac: return 556; case 0x201a: return 222; case 0x192: return 556; case 0x201e: return 333;
        case 0x2026: return 1000; case 0x2020: case 0x2021: return 556; case 0x2c6: case 0x2dc: return 333;
        case 0x2030: return 1000; case 0x160: return 667; case 0x161: return 500; case 0x2039: case 0x203a: return 333;
        case 0x152: return 1000; case 0x153: return 944; case 0x17d: return 611; case 0x17e: return 500;
        case 0x2018: case 0x2019: return 222; case 0x201c: case 0x201d: return 333; case 0x2022: return 350;
        case 0x2013: return 556; case 0x2014: case 0x2122: return 1000;
        default: fail(ErrorCode::unsupported,"Helvetica glyph metric unsupported");
    }
}
struct Font { bool winansi=false; std::map<unsigned,double> widths; };
Font font(Reader& reader,const Value& resources,const std::string& name) {
    auto fonts=reader.resolve(field(resources,"Font")); auto f=reader.resolve(field(fonts,name));
    if(field(f,"Subtype").text!="Type1" || field(f,"BaseFont").text!="Helvetica") fail(ErrorCode::unsupported,"Only Type1 Helvetica font is supported: "+name);
    if(f.dict.contains("ToUnicode")) fail(ErrorCode::unsupported,"ToUnicode CMap unsupported");
    Font result;
    if(f.dict.contains("Encoding")) {
        auto enc=reader.resolve(f.dict.at("Encoding"));
        if(enc.kind!=Kind::name || (enc.text!="WinAnsiEncoding"&&enc.text!="StandardEncoding")) fail(ErrorCode::unsupported,"Font encoding/Differences unsupported");
        result.winansi=enc.text=="WinAnsiEncoding";
    }
    if(f.dict.contains("Widths")) {
        auto first=integer(reader.resolve(field(f,"FirstChar")),"FirstChar"), last=integer(reader.resolve(field(f,"LastChar")),"LastChar");
        auto widths=reader.resolve(f.dict.at("Widths"));
        if(first>last||last>255||widths.kind!=Kind::array||widths.array.size()!=last-first+1) fail(ErrorCode::invalid_input,"Invalid font Widths");
        for(std::size_t i=0;i<widths.array.size();++i) {
            auto w=numeric(reader.resolve(widths.array[i])); if(w<0||w>10000) fail(ErrorCode::invalid_input,"Invalid font width");
            result.widths[static_cast<unsigned>(first+i)]=w;
        }
    }
    return result;
}
struct State {
    Matrix ctm;
    std::string font;
    double size=0, character=0, word=0, scale=1, leading=0, rise=0;
    std::array<double,3> color{0,0,0};
};
}
void interpret(Reader& reader,Page& page,const Value& resources,const std::vector<Content>& streams,std::size_t page_number) {
    std::string data;
    std::vector<std::pair<std::size_t,const Content*>> segments;
    for(const auto& s:streams) { segments.push_back({data.size(),&s}); data+=s.bytes; data+='\n'; }
    Parser parser(data,reader.budget);
    State state; std::vector<State> stack;
    Matrix tm, line;
    bool in_text=false;
    std::map<std::string,Font> fonts;
    std::vector<Value> args;
    std::size_t operator_offset=0, sequence=0;
    auto origin=[&]() {
        auto it=std::upper_bound(segments.begin(),segments.end(),operator_offset,[](std::size_t off,const auto& s) { return off<s.first; });
        if(it==segments.begin()) return Origin{page_number,0,operator_offset,sequence++};
        --it; return Origin{page_number,it->second->object,it->second->offset+operator_offset-it->first,sequence++};
    };
    auto count=[&](std::size_t n) { if(args.size()!=n) fail(ErrorCode::invalid_input,"Wrong operator operand count",operator_offset); };
    auto number=[&](std::size_t i) { return numeric(args.at(i)); };
    auto text_context=[&]() { if(!in_text) fail(ErrorCode::invalid_input,"Text operator outside BT/ET",operator_offset); };
    auto show=[&](const Value& string) {
        text_context(); if(string.kind!=Kind::string) fail(ErrorCode::invalid_input,"Text-show operand must be a string",operator_offset);
        if(state.size<=0||state.font.empty()) fail(ErrorCode::invalid_input,"Text shown without valid Tf",operator_offset);
        const auto& f=fonts.at(state.font);
        for(unsigned char code:string.text) {
            reader.budget.tick(operator_offset);
            auto cp=decode(code,f.winansi); auto transform=compose(state.ctm,tm); finite(transform);
            if(std::abs(transform.b)>1e-7||std::abs(transform.c)>1e-7||transform.a<=0||transform.d<=0)
                fail(ErrorCode::unsupported,"Only positive axis-aligned horizontal text is supported",operator_offset);
            auto width=f.widths.contains(code)?f.widths.at(code):helvetica_width(cp);
            auto advance=(width/1000*state.size+state.character+(code==32?state.word:0))*state.scale;
            double size=state.size*transform.d;
            if(size<=0||size>1000||!std::isfinite(advance)) fail(ErrorCode::invalid_input,"Invalid transformed text size/advance",operator_offset);
            page.fragments.push_back({xml::utf8(cp),transform.e,transform.f+state.rise*transform.d,advance*transform.a,size,"Helvetica",state.color,origin()});
            translate(tm,advance,0); finite(tm);
        }
    };
    while(true) {
        parser.whitespace(); if(parser.pos>=data.size()) break;
        auto token_offset=parser.pos; auto value=parser.value(0,false);
        if(value.kind!=Kind::keyword) {
            args.push_back(std::move(value)); if(args.size()>reader.options.limits.depth) fail(ErrorCode::limit,"operator operand limit exceeded",token_offset); continue;
        }
        operator_offset=token_offset; const auto& op=value.text;
        try {
            if(op=="BT") { count(0); if(in_text) fail(ErrorCode::invalid_input,"Nested BT"); in_text=true; tm={}; line={}; }
            else if(op=="ET") { count(0); text_context(); in_text=false; }
            else if(op=="Tf") {
                count(2); if(args[0].kind!=Kind::name) fail(ErrorCode::invalid_input,"Tf requires font name");
                state.font=args[0].text; state.size=number(1); if(state.size<=0||state.size>1000) fail(ErrorCode::unsupported,"Font size outside supported range");
                if(!fonts.contains(state.font)) fonts.emplace(state.font,font(reader,resources,state.font));
            } else if(op=="Tm") {
                count(6); text_context(); tm={number(0),number(1),number(2),number(3),number(4),number(5)}; finite(tm); line=tm;
            } else if(op=="Td"||op=="TD") {
                count(2); text_context(); if(op=="TD") state.leading=-number(1); translate(line,number(0),number(1)); finite(line); tm=line;
            } else if(op=="T*") { count(0); text_context(); translate(line,0,-state.leading); finite(line); tm=line; }
            else if(op=="Tc"||op=="Tw"||op=="Tz"||op=="TL"||op=="Ts") {
                count(1); double n=number(0);
                if(std::abs(n)>10000) fail(ErrorCode::unsupported,"Text state value outside supported range");
                if(op=="Tc") state.character=n; else if(op=="Tw") state.word=n;
                else if(op=="Tz") { if(n<=0||n>1000) fail(ErrorCode::unsupported,"Horizontal scaling outside supported range"); state.scale=n/100; }
                else if(op=="TL") state.leading=n; else state.rise=n;
            } else if(op=="Tj") { count(1); show(args[0]); }
            else if(op=="TJ") {
                count(1); text_context(); if(args[0].kind!=Kind::array) fail(ErrorCode::invalid_input,"TJ requires array");
                if(state.size<=0) fail(ErrorCode::invalid_input,"TJ without Tf");
                for(const auto& item:args[0].array) {
                    reader.budget.tick(operator_offset);
                    if(item.kind==Kind::string) show(item);
                    else { translate(tm,-numeric(item)/1000*state.size*state.scale,0); finite(tm); }
                }
            } else if(op=="'"||op=="\"") {
                count(op=="'"?1:3); text_context();
                if(op=="\"") { state.word=number(0); state.character=number(1); }
                translate(line,0,-state.leading); finite(line); tm=line; show(args.back());
            } else if(op=="q") {
                count(0); if(in_text) fail(ErrorCode::invalid_input,"q inside text object");
                if(stack.size()>=reader.options.limits.depth) fail(ErrorCode::limit,"graphics stack depth exceeded");
                stack.push_back(state);
            } else if(op=="Q") {
                count(0); if(in_text||stack.empty()) fail(ErrorCode::invalid_input,"Unbalanced Q"); state=stack.back(); stack.pop_back();
            } else if(op=="cm") {
                count(6); if(in_text) fail(ErrorCode::unsupported,"cm inside text object unsupported"); state.ctm=compose(state.ctm,{number(0),number(1),number(2),number(3),number(4),number(5)}); finite(state.ctm);
            } else if(op=="g"||op=="rg") {
                count(op=="g"?1:3); for(std::size_t i=0;i<3;++i) { double c=number(op=="g"?0:i); if(c<0||c>1) fail(ErrorCode::invalid_input,"Color outside range"); state.color[i]=c; }
            } else if(op=="Tr") {
                count(1); if(number(0)!=0) reader.loss("Non-fill text rendering mode unsupported");
            } else if(op=="BI"||op=="ID"||op=="EI") {
                fail(ErrorCode::unsupported,"Inline images unsupported; cannot safely skip binary data");
            } else {
                reader.loss("Unsupported content operator omitted: "+op);
            }
            args.clear();
        } catch(Failure& e) { if(!e.error.byte_offset) e.error.byte_offset=origin().byte_offset; throw; }
    }
    if(!args.empty()||in_text||!stack.empty()) fail(ErrorCode::invalid_input,"Unbalanced/truncated content stream");
}
}
