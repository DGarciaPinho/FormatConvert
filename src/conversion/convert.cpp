#include "forgeconvert/conversion/convert.hpp"
#include "forgeconvert/docx/writer.hpp"
#include "pdf/internal.hpp"
#include <fstream>
#include <random>
#include <system_error>
namespace forgeconvert {
namespace {
std::string input_bytes(const std::filesystem::path& path,const ResourceLimits& limits) {
    if(!std::filesystem::is_regular_file(path)) fail(ErrorCode::io,"Input is not a readable regular file");
    auto size=std::filesystem::file_size(path);
    if(size>limits.input_bytes) fail(ErrorCode::limit,"input_bytes exceeded");
    std::ifstream stream(path,std::ios::binary); if(!stream) fail(ErrorCode::io,"Cannot open input");
    std::string data(static_cast<std::size_t>(size),'\0');
    if(!stream.read(data.data(),static_cast<std::streamsize>(data.size()))) fail(ErrorCode::io,"Input changed or read failed");
    char extra; if(stream.get(extra)) fail(ErrorCode::io,"Input grew during read");
    if(!stream.eof()) fail(ErrorCode::io,"Input read error");
    return data;
}
bool same_file(const std::filesystem::path& a,const std::filesystem::path& b) {
    if(std::filesystem::weakly_canonical(a)==std::filesystem::weakly_canonical(b)) return true;
    return std::filesystem::exists(b) && std::filesystem::equivalent(a,b);
}
struct Temporary {
    std::filesystem::path dir,file;
    ~Temporary() { std::error_code error; if(!file.empty()) std::filesystem::remove(file,error); if(!dir.empty()) std::filesystem::remove(dir,error); }
};
void publish(const std::filesystem::path& input,const std::filesystem::path& output,const std::string& data,bool overwrite) {
    auto parent=output.parent_path(); if(parent.empty()) parent=".";
    if(!std::filesystem::is_directory(parent)) fail(ErrorCode::io,"Output directory does not exist");
    Temporary temporary;
    std::random_device random;
    for(int attempt=0;attempt<32;++attempt) {
        auto dir=parent/(".forgeconvert-"+std::to_string(random())+"-"+std::to_string(random()));
        if(std::filesystem::create_directory(dir)) { temporary.dir=std::move(dir); break; }
    }
    if(temporary.dir.empty()) fail(ErrorCode::io,"Cannot reserve temporary output directory");
    temporary.file=temporary.dir/"output.tmp";
    {
        std::ofstream stream(temporary.file,std::ios::binary|std::ios::trunc); if(!stream) fail(ErrorCode::io,"Cannot create temporary output");
        stream.write(data.data(),static_cast<std::streamsize>(data.size())); stream.flush();
        if(!stream) fail(ErrorCode::io,"Cannot write temporary output");
        stream.close(); if(stream.fail()) fail(ErrorCode::io,"Cannot close temporary output");
    }
    if(same_file(input,output)) fail(ErrorCode::arguments,"Never overwrite the input, including aliases/hard links");
    if(overwrite) std::filesystem::rename(temporary.file,output);
    else std::filesystem::create_hard_link(temporary.file,output); // atomic no-clobber publication, even against a competing writer
}
}
Result<Inspection> inspect(const std::filesystem::path& input,const ConversionOptions& options) {
    try { auto bytes=input_bytes(input,options.limits); return pdf::read(bytes,options); }
    catch(const Failure& e) { return e.error; }
    catch(const std::filesystem::filesystem_error& e) { return Error{ErrorCode::io,e.what()}; }
    catch(const std::bad_alloc&) { return Error{ErrorCode::limit,"Memory allocation failed"}; }
    catch(const std::exception& e) { return Error{ErrorCode::internal,e.what()}; }
}
Result<ConversionReport> convert(const std::filesystem::path& input,const std::filesystem::path& output,OutputFormat format,const ConversionOptions& options) {
    try {
        if(output.empty()||output.filename().empty()) fail(ErrorCode::arguments,"Output filename required");
        if(same_file(input,output)) fail(ErrorCode::arguments,"Never overwrite the input, including aliases/hard links");
        if(std::filesystem::exists(output)&&!options.overwrite) fail(ErrorCode::io,"Output exists; use --overwrite");
        auto inspection=inspect(input,options); if(!inspection) return inspection.error();
        std::string bytes;
        if(format==OutputFormat::docx) {
            auto result=docx::write(inspection.value().document,options.limits.output_bytes); if(!result) return result.error(); bytes=std::move(result.value());
        } else if(format==OutputFormat::txt) {
            for(std::size_t i=0;i<inspection.value().document.pages.size();++i) {
                if(i) bytes+="\f\n";
                for(const auto& paragraph:inspection.value().document.pages[i].paragraphs) {
                    for(const auto& run:paragraph.runs) { if(run.text.size()>options.limits.output_bytes || bytes.size()>options.limits.output_bytes-run.text.size()) fail(ErrorCode::limit,"output_bytes exceeded"); bytes+=run.text; }
                    bytes+='\n';
                }
                if(bytes.size()>options.limits.output_bytes) fail(ErrorCode::limit,"output_bytes exceeded");
            }
        } else fail(ErrorCode::unsupported,"Unknown output format");
        publish(input,output,bytes,options.overwrite);
        return std::move(inspection.value().report);
    } catch(const Failure& e) { return e.error; }
    catch(const std::filesystem::filesystem_error& e) { return Error{ErrorCode::io,e.what()}; }
    catch(const std::bad_alloc&) { return Error{ErrorCode::limit,"Memory allocation failed"}; }
    catch(const std::exception& e) { return Error{ErrorCode::internal,e.what()}; }
}
std::string capabilities() {
    return "ForgeConvert 0.1.0\n"
           "Input: unencrypted PDF, classic xref (including Prev revisions), indirect Length\n"
           "Streams: unfiltered only; FlateDecode/DEFLATE not yet implemented\n"
           "Fonts: Type1 Helvetica, WinAnsi; StandardEncoding ASCII subset; optional Widths\n"
           "Text: BT ET Tf Tm Td TD T* Tj TJ Tc Tw Tz TL Ts ' \"; q Q cm; g rg; Tr=0\n"
           "Layout: positive axis-aligned horizontal, single column, heuristic paragraphs\n"
           "Output: editable OOXML Transitional DOCX (own ZIP STORE/XML), UTF-8 TXT\n"
           "Unsupported: OCR, images, forms, Type0/CMaps, xref/object streams, encryption, columns/tables\n"
           "Default strict; --allow-lossy reports omitted resources; Helvetica -> Arial\n";
}
}
