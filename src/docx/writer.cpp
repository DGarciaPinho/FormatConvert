#include "forgeconvert/docx/writer.hpp"
#include "forgeconvert/codecs/zip.hpp"
#include "forgeconvert/xml/writer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace forgeconvert::docx {
Result<std::string> write(const Document& document, std::size_t max_bytes) {
    try {
        auto escaped=[](const std::string& s) { auto r=xml::escape(s); if(!r) throw Failure(r.error()); return r.value(); };
        std::string main="<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>";
        for(std::size_t i=0;i<document.pages.size();++i) {
            if(i) main+="<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>";
            for(const auto& p:document.pages[i].paragraphs) {
                main+="<w:p>";
                for(const auto& r:p.runs) {
                    if(!std::isfinite(r.size) || r.size<=0 || r.size>1000) fail(ErrorCode::invalid_input,"Invalid DOCX font size");
                    char color[7];
                    for(double c:r.color) if(!std::isfinite(c) || c<0 || c>1) fail(ErrorCode::invalid_input,"Invalid DOCX color");
                    std::snprintf(color,sizeof(color),"%02X%02X%02X",static_cast<unsigned>(std::lround(r.color[0]*255)),static_cast<unsigned>(std::lround(r.color[1]*255)),static_cast<unsigned>(std::lround(r.color[2]*255)));
                    main+="<w:r><w:rPr><w:rFonts w:ascii=\""+escaped(r.font)+"\" w:hAnsi=\""+escaped(r.font)+"\"/><w:color w:val=\""+color+"\"/><w:sz w:val=\""+std::to_string(std::max(1L,std::lround(r.size*2)))+"\"/></w:rPr><w:t xml:space=\"preserve\">"+escaped(r.text)+"</w:t></w:r>";
                    if(main.size()>max_bytes) fail(ErrorCode::limit,"output_bytes exceeded");
                }
                main+="</w:p>";
            }
        }
        main+="<w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/><w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\"/></w:sectPr></w:body></w:document>";
        return codecs::zip_store({
            {"[Content_Types].xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/></Types>"},
            {"_rels/.rels", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>"},
            {"word/document.xml",std::move(main)}}, max_bytes);
    } catch(const Failure& e) { return e.error; }
}
}
