#include "forgeconvert/conversion/convert.hpp"
#include <iostream>
#include <string_view>
using namespace forgeconvert;
namespace {
int error(const Error& e) {
    std::cerr<<"error["<<static_cast<int>(e.code)<<"] "<<e.message;
    if(e.byte_offset) std::cerr<<" (byte "<<*e.byte_offset<<")";
    if(e.page) std::cerr<<" (page "<<*e.page<<")";
    std::cerr<<'\n'; return static_cast<int>(e.code);
}
void report(const ConversionReport& r) {
    std::cout<<"Pages processed: "<<r.pages_processed<<"\nQuality: "<<r.quality<<'\n';
    for(const auto& w:r.warnings) std::cerr<<"warning: "<<w<<'\n';
    for(const auto& f:r.font_substitutions) std::cerr<<"font: "<<f<<'\n';
}
int usage() {
    std::cerr<<"Usage:\n  forgeconvert capabilities\n  forgeconvert inspect INPUT.pdf [--allow-lossy]\n  forgeconvert convert INPUT.pdf --to docx|txt --output OUTPUT [--allow-lossy] [--overwrite]\n"; return 2;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string_view(argv[1])=="capabilities") { std::cout<<capabilities(); return 0; }
        if(argc<3) return usage();
        std::string command=argv[1]; if(command!="inspect"&&command!="convert") return usage();
        ConversionOptions options; std::filesystem::path input=argv[2],output;
        OutputFormat format=OutputFormat::docx; bool has_format=false,has_lossy=false,has_overwrite=false;
        for(int i=3;i<argc;++i) {
            std::string flag=argv[i];
            if(flag=="--allow-lossy"&&!has_lossy) { options.strict=false; has_lossy=true; }
            else if(flag=="--overwrite"&&command=="convert"&&!has_overwrite) { options.overwrite=true; has_overwrite=true; }
            else if(flag=="--output"&&command=="convert"&&output.empty()&&i+1<argc) output=argv[++i];
            else if(flag=="--to"&&command=="convert"&&!has_format&&i+1<argc) {
                std::string to=argv[++i]; if(to=="docx") format=OutputFormat::docx; else if(to=="txt") format=OutputFormat::txt; else return usage(); has_format=true;
            } else return usage();
        }
        if(command=="inspect") {
            auto result=inspect(input,options); if(!result) return error(result.error()); report(result.value().report);
            for(std::size_t i=0;i<result.value().document.pages.size();++i) {
                const auto& page=result.value().document.pages[i]; std::cout<<"Page "<<i+1<<": "<<page.fragments.size()<<" glyphs, "<<page.paragraphs.size()<<" paragraphs\n";
                for(const auto& p:page.paragraphs) { for(const auto& run:p.runs) std::cout<<run.text; std::cout<<'\n'; }
            }
            return 0;
        }
        if(!has_format||output.empty()) return usage();
        auto result=convert(input,output,format,options); if(!result) return error(result.error()); report(result.value()); std::cout<<"Written: "<<output.string()<<'\n'; return 0;
    } catch(const std::exception& e) { return error({ErrorCode::internal,e.what()}); }
}
