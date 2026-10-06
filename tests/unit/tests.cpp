#include "forgeconvert/codecs/zip.hpp"
#include "forgeconvert/xml/writer.hpp"
#include "forgeconvert/docx/writer.hpp"
#include "pdf/internal.hpp"
#include <iostream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <random>
using namespace forgeconvert;
namespace {
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
std::string pdf_file(const std::vector<std::string>& objects) {
    std::string s="%PDF-1.4\n"; std::vector<std::size_t> offsets;
    for(std::size_t i=0;i<objects.size();++i) { offsets.push_back(s.size()); s+=std::to_string(i+1)+" 0 obj\n"+objects[i]+"\nendobj\n"; }
    auto xref=s.size(); std::ostringstream x; x<<"xref\n0 "<<objects.size()+1<<"\n0000000000 65535 f \n";
    for(auto offset:offsets) x<<std::setw(10)<<std::setfill('0')<<offset<<" 00000 n \n";
    s+=x.str()+"trailer\n<< /Size "+std::to_string(objects.size()+1)+" /Root 1 0 R >>\nstartxref\n"+std::to_string(xref)+"\n%%EOF\n"; return s;
}
std::string fixture(std::string content,std::string extra="",bool indirect=false) {
    return pdf_file({"<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> >>",
        "<< /Type /Page /Parent 2 0 R /Contents 5 0 R >>",
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
        "<< /Length "+(indirect?std::string("6 0 R"):std::to_string(content.size()))+" "+extra+" >>\nstream\n"+content+"\nendstream",std::to_string(content.size())});
}
void expect_error(const std::string& bytes,ErrorCode code,ConversionOptions opts={}) {
    auto result=pdf::read(bytes,opts); check(!result,"Expected error"); check(result.error().code==code,result.error().message.c_str());
}
}
int main() {
    std::vector<std::pair<const char*,std::function<void()>>> tests={
      {"checksums",[] { check(codecs::crc32("123456789")==0xcbf43926,"CRC32"); check(codecs::crc32("")==0,"empty CRC"); check(codecs::adler32("Wikipedia")==0x11e60398,"Adler32"); check(codecs::adler32("")==1,"empty Adler"); }},
      {"XML",[] { auto r=xml::escape(" A & <é> \" '"); check(r&&r.value()==" A &amp; &lt;é&gt; &quot; &apos;","escaping"); check(!xml::escape(std::string(1,'\1')),"control accepted"); check(!xml::escape("\xc0\x80"),"overlong UTF8 accepted"); check(!xml::escape("\xed\xa0\x80"),"surrogate accepted"); check(!xml::escape("\xf4\x90\x80\x80"),"outside Unicode accepted"); check(!xml::escape("\xe2\x82"),"truncated UTF8 accepted"); }},
      {"ZIP/DOCX base",[] { auto zip=codecs::zip_store({{"x","123456789"}}); check(zip&&zip.value().substr(0,4)=="PK\3\4","ZIP signature"); check(!codecs::zip_store({{"../x","x"}}),"unsafe name"); check(!codecs::zip_store({{"x","x"},{"x","y"}}),"duplicate entry"); check(!codecs::zip_store({{"x","x"}},1),"ZIP output limit"); Document d; Page p; p.paragraphs.push_back({{{" Olá & <Word> ","Arial",12,{0,0,0}}}}); d.pages.push_back(p); auto word=docx::write(d); check(word&&word.value().find("xml:space=\"preserve\"")!=std::string::npos,"DOCX XML"); }},
      {"PDF tokenizer",[] { ResourceLimits limits; pdf::Budget b{limits}; pdf::Parser p("/F#31 (a(b)\\)\\101\\\r\nx) <4f6> [1 2 0 R true null] << /K -1.25 >>",b); check(p.value().text=="F1","name escape"); check(p.value().text=="a(b))Ax","string nesting/escapes"); check(p.value().text==std::string("O\x60",2),"odd hex"); auto a=p.value(); check(a.array.size()==4&&a.array[1].kind==pdf::Kind::reference,"reference/array"); check(pdf::numeric(pdf::field(p.value(),"K"))==-1.25,"dictionary number"); }},
      {"PDF structure and text",[] {
        auto r=pdf::read(fixture("q 1 0 0 1 10 20 cm BT /F1 12 Tf 1 0 0 1 50 700 Tm (Ol\\341) Tj [( mundo) -300 (!)] TJ 14 TL T* (Linha 2) Tj ET Q","",true),{});
        check(static_cast<bool>(r),r?"":r.error().message.c_str());
        auto& p=r.value().document.pages[0]; check(p.fragments.front().x==60&&p.fragments.front().y==720,"transform position"); check(p.paragraphs.size()==1,"paragraph grouping");
        std::string text; for(auto& run:p.paragraphs[0].runs) text+=run.text; check(text=="Olá mundo ! Linha 2","Unicode/TJ/layout");
      }},
      {"text state",[] {
        auto r=pdf::read(fixture("BT /F1 10 Tf 1 0 0 1 0 100 Tm 2 Tc 3 Tw 50 Tz 12 TL 1 Ts (A ) Tj 0 -12 TD (B) Tj 0 0 1 rg T* (C) Tj ET"),{}); check(static_cast<bool>(r),"text state PDF");
        const auto& f=r.value().document.pages[0].fragments; check(std::abs(f[1].x-4.335)<1e-8,"Tc/Tz advance"); check(f[2].y==89&&f[3].y==77,"TD/Ts/T*"); check(f[3].color[2]==1,"RGB");
      }},
      {"diagnostics",[] {
        expect_error(fixture("BT /F1 12 Tf (Hi) Tj ET","/Filter /FlateDecode"),ErrorCode::unsupported);
        expect_error(fixture(""),ErrorCode::unsupported);
        expect_error(fixture("BT /F1 12 Tf (Hi) Tj ET Do"),ErrorCode::unsupported);
        expect_error(fixture("BT /F1 12 Tf (Hi) Tj"),ErrorCode::invalid_input);
        expect_error(fixture("Q"),ErrorCode::invalid_input);
        expect_error(fixture("BT /F1 12 Tf 0 1 -1 0 0 0 Tm (Hi) Tj ET"),ErrorCode::unsupported);
        auto bytes=fixture("BT /F1 12 Tf (Hi) Tj ET"); auto at=bytes.find("/Root 1 0 R"); bytes.insert(at,"/Encrypt 99 0 R "); expect_error(bytes,ErrorCode::unsupported);
        ConversionOptions options; options.strict=false; auto r=pdf::read(fixture("BT /F1 12 Tf (Hi) Tj ET Do"),options); check(r&&r.value().report.warnings.size()==1,"loss report");
      }},
      {"limits/malformed",[] {
        ConversionOptions options; options.limits.steps=10; expect_error(fixture("BT /F1 12 Tf (Hi) Tj ET"),ErrorCode::limit,options);
        options={}; options.limits.stream_bytes=2; expect_error(fixture("BT /F1 12 Tf (Hi) Tj ET"),ErrorCode::limit,options);
        options={}; options.limits.pages=0; expect_error(fixture("BT /F1 12 Tf (Hi) Tj ET"),ErrorCode::limit,options);
        expect_error(pdf_file({"<< /Type /Catalog /Pages 2 0 R >>","<< /Type /Pages /Kids [2 0 R] /Count 1 >>"}),ErrorCode::invalid_input);
        auto bytes=fixture("BT /F1 12 Tf (Hi) Tj ET"); auto at=bytes.find("/Length "); bytes.replace(at+8,2,"99"); expect_error(bytes,ErrorCode::invalid_input);
        ResourceLimits limits; pdf::Budget b{limits}; pdf::Parser p("999999999999999999999999999999",b); bool caught=false; try { p.value(); } catch(const Failure&) { caught=true; } check(caught,"extreme number");
      }},
      {"multiple content streams and generation",[] {
        std::string first="BT /F1 12 Tf 10 20 Td (First) Tj", second=" 0 -30 Td (Second) Tj ET";
        auto bytes=pdf_file({"<< /Type /Catalog /Pages 2 0 R >>", "<< /Type /Pages /Kids [3 0 R] /Count 1 /MediaBox [0 0 612 792] >>",
          "<< /Type /Page /Parent 2 0 R /Resources << /Font << /F1 4 0 R >> >> /Contents [5 0 R 6 0 R] >>",
          "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
          "<< /Length "+std::to_string(first.size())+" >>\nstream\n"+first+"\nendstream",
          "<< /Length "+std::to_string(second.size())+" >>\nstream\n"+second+"\nendstream"});
        auto result=pdf::read(bytes,{}); check(static_cast<bool>(result),"contents array");
        check(result.value().document.pages[0].fragments.back().origin.object==6,"stream provenance");
        auto at=bytes.find("/F1 4 0 R"); bytes[at+6]='1'; expect_error(bytes,ErrorCode::invalid_input);
      }},
      {"bounded mutation fuzz smoke",[] {
        const auto seed=fixture("BT /F1 12 Tf 10 700 Td (Fuzz \\351) Tj ET");
        std::mt19937 random(0x464f5247);
        ConversionOptions options; options.limits.steps=10000; options.limits.depth=16; options.limits.string_bytes=4096;
        for(int i=0;i<1000;++i) {
            auto mutated=seed;
            for(unsigned j=0,count=1+random()%8;j<count;++j) mutated[random()%mutated.size()]=static_cast<char>(random()&255);
            if(i%3==0) mutated.resize(random()%mutated.size());
            auto result=pdf::read(mutated,options);
            if(!result) check(result.error().code!=ErrorCode::internal,"Unexpected internal error under mutation");
        }
      }},
    };
    int failed=0;
    for(const auto& [name,test]:tests) { try { test(); std::cout<<"PASS "<<name<<'\n'; } catch(const std::exception& e) { ++failed; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; } }
    return failed?1:0;
}
