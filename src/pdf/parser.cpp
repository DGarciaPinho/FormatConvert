#include "pdf/internal.hpp"
#include <charconv>
#include <cmath>
#include <limits>
namespace forgeconvert::pdf {
namespace {
bool white(unsigned char c) { return c==0 || c==9 || c==10 || c==12 || c==13 || c==32; }
bool delimiter(unsigned char c) { return white(c) || std::string_view("()<>[]{}/%").find(static_cast<char>(c))!=std::string_view::npos; }
int hex(char c) { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; if(c>='A'&&c<='F') return c-'A'+10; return -1; }
bool number_token(std::string_view s) {
    if(s.empty()) return false;
    std::size_t i=(s[0]=='+'||s[0]=='-')?1:0; bool digit=false,dot=false;
    for(;i<s.size();++i) { if(s[i]=='.'&&!dot) dot=true; else if(s[i]>='0'&&s[i]<='9') digit=true; else return false; }
    return digit;
}
}
void Parser::whitespace() {
    while(pos<data.size()) {
        budget.tick(pos);
        if(white(static_cast<unsigned char>(data[pos]))) ++pos;
        else if(data[pos]=='%') { while(pos<data.size() && data[pos]!='\r' && data[pos]!='\n') { ++pos; budget.tick(pos); } }
        else break;
    }
}
bool Parser::consume(std::string_view t) {
    whitespace();
    if(data.substr(pos,t.size())!=t) return false;
    if(!t.empty() && !delimiter(static_cast<unsigned char>(t.back())) && pos+t.size()<data.size() && !delimiter(static_cast<unsigned char>(data[pos+t.size()]))) return false;
    pos+=t.size(); return true;
}
void Parser::expect(std::string_view t) { if(!consume(t)) fail(ErrorCode::invalid_input,"Expected "+std::string(t),pos); }
std::string Parser::token() {
    whitespace(); auto begin=pos;
    while(pos<data.size() && !delimiter(static_cast<unsigned char>(data[pos]))) { ++pos; budget.tick(pos); }
    if(begin==pos) fail(ErrorCode::invalid_input,"Expected token",pos);
    if(pos-begin>budget.limits.string_bytes) fail(ErrorCode::limit,"string_bytes exceeded",begin);
    return std::string(data.substr(begin,pos-begin));
}
Value Parser::value(std::size_t depth,bool references) {
    if(depth>=budget.limits.depth) fail(ErrorCode::limit,"depth exceeded",pos);
    budget.tick(pos); whitespace(); if(pos>=data.size()) fail(ErrorCode::invalid_input,"Unexpected end of PDF",pos);
    Value v; char c=data[pos];
    auto append=[&](char x) { if(v.text.size()>=budget.limits.string_bytes) fail(ErrorCode::limit,"string_bytes exceeded",pos); v.text+=x; };
    if(c=='/') {
        ++pos; v.kind=Kind::name;
        while(pos<data.size() && !delimiter(static_cast<unsigned char>(data[pos]))) {
            budget.tick(pos); c=data[pos++];
            if(c=='#') { if(data.size()-pos<2 || hex(data[pos])<0 || hex(data[pos+1])<0) fail(ErrorCode::invalid_input,"Invalid name escape",pos); c=static_cast<char>(hex(data[pos])*16+hex(data[pos+1])); pos+=2; }
            append(c);
        }
    } else if(c=='(') {
        ++pos; v.kind=Kind::string; std::size_t nesting=1;
        while(nesting) {
            budget.tick(pos); if(pos>=data.size()) fail(ErrorCode::invalid_input,"Unterminated literal string",pos);
            c=data[pos++];
            if(c=='\\') {
                if(pos>=data.size()) fail(ErrorCode::invalid_input,"Truncated string escape",pos);
                c=data[pos++];
                switch(c) { case 'n': c='\n'; break; case 'r': c='\r'; break; case 't': c='\t'; break; case 'b': c='\b'; break; case 'f': c='\f'; break;
                    case '\r': if(pos<data.size() && data[pos]=='\n') ++pos; continue;
                    case '\n': continue;
                    default: if(c>='0'&&c<='7') { unsigned n=static_cast<unsigned>(c-'0'); for(int k=1;k<3 && pos<data.size()&&data[pos]>='0'&&data[pos]<='7';++k) n=n*8+static_cast<unsigned>(data[pos++]-'0'); c=static_cast<char>(n&255); }
                }
                append(c);
            } else if(c=='(') { if(++nesting>budget.limits.depth) fail(ErrorCode::limit,"string nesting depth exceeded",pos); append(c); }
            else if(c==')') { if(--nesting) append(c); }
            else { if(c=='\r') { if(pos<data.size()&&data[pos]=='\n') ++pos; c='\n'; } append(c); }
        }
    } else if(c=='<' && data.substr(pos,2)!="<<") {
        ++pos; v.kind=Kind::string; int high=-1;
        for(;;) {
            budget.tick(pos); if(pos>=data.size()) fail(ErrorCode::invalid_input,"Unterminated hex string",pos);
            c=data[pos++]; if(c=='>') break;
            if(white(static_cast<unsigned char>(c))) continue;
            int h=hex(c); if(h<0) fail(ErrorCode::invalid_input,"Invalid hex string",pos-1);
            if(high<0) high=h; else { append(static_cast<char>(high*16+h)); high=-1; }
        }
        if(high>=0) append(static_cast<char>(high*16));
    } else if(c=='[') {
        ++pos; v.kind=Kind::array;
        while(!consume("]")) v.array.push_back(value(depth+1,references));
    } else if(data.substr(pos,2)=="<<") {
        pos+=2; v.kind=Kind::dictionary;
        while(!consume(">>")) {
            auto key=value(depth+1,references); if(key.kind!=Kind::name) fail(ErrorCode::invalid_input,"Dictionary key must be a name",pos);
            auto item=value(depth+1,references);
            if(!v.dict.emplace(key.text,std::move(item)).second) fail(ErrorCode::invalid_input,"Duplicate dictionary key",pos);
        }
    } else {
        auto t=token();
        if(t=="null") v.kind=Kind::null;
        else if(t=="true"||t=="false") { v.kind=Kind::boolean; v.number=t=="true"?1:0; }
        else if(number_token(t)) {
            v.kind=Kind::number;
            auto s=std::string_view(t); if(s.front()=='+') s.remove_prefix(1);
            auto result=std::from_chars(s.data(),s.data()+s.size(),v.number);
            if(result.ec!=std::errc{} || result.ptr!=s.data()+s.size() || !std::isfinite(v.number) || std::abs(v.number)>1e12)
                fail(ErrorCode::invalid_input,"Number outside supported finite range",pos-t.size());
            if(references && t.find('.')==std::string::npos && v.number>=0) {
                auto saved=pos; whitespace();
                if(pos<data.size() && data[pos]>='0' && data[pos]<='9') {
                    auto second=token();
                    if(number_token(second)&&second.find('.')==std::string::npos&&consume("R")) {
                        Value g; g.kind=Kind::number;
                        auto r=std::from_chars(second.data(),second.data()+second.size(),g.number);
                        if(r.ec!=std::errc{}) fail(ErrorCode::invalid_input,"Invalid reference generation",saved);
                        v.id=integer(v,"object number"); v.generation=integer(g,"generation"); v.kind=Kind::reference;
                        if(v.generation>65535) fail(ErrorCode::invalid_input,"Generation outside range",saved);
                    } else pos=saved;
                } else pos=saved;
            }
        } else { v.kind=Kind::keyword; v.text=std::move(t); }
    }
    return v;
}
double numeric(const Value& v) { if(v.kind!=Kind::number) fail(ErrorCode::invalid_input,"Expected number"); return v.number; }
std::size_t integer(const Value& v,std::string_view label) {
    double n=numeric(v);
    if(n<0 || std::floor(n)!=n || n>=static_cast<double>(std::numeric_limits<std::size_t>::max())) fail(ErrorCode::invalid_input,"Invalid integer: "+std::string(label));
    return static_cast<std::size_t>(n);
}
const Value& field(const Value& v,std::string_view key) {
    if(v.kind!=Kind::dictionary) fail(ErrorCode::invalid_input,"Expected dictionary");
    auto it=v.dict.find(std::string(key)); if(it==v.dict.end()) fail(ErrorCode::invalid_input,"Missing key: "+std::string(key)); return it->second;
}
}
