#include "pdf/internal.hpp"
#include <algorithm>
#include <functional>
namespace forgeconvert::pdf {
void Reader::loss(std::string message) {
    if(options.strict) fail(ErrorCode::unsupported,message);
    if(std::find(report.warnings.begin(),report.warnings.end(),message)==report.warnings.end()) {
        report.warnings.push_back(message); report.discarded_resources.push_back(message);
    }
}
void Reader::xrefs() {
    if(bytes.substr(0,5)!="%PDF-") fail(ErrorCode::unsupported,"Input is not a PDF");
    auto marker=bytes.rfind("startxref"); if(marker==std::string_view::npos) fail(ErrorCode::invalid_input,"Missing startxref");
    Parser tail(bytes,budget,marker+9); auto offset=integer(tail.value(0,false),"startxref");
    while(tail.pos<bytes.size() && std::string_view(" \t\r\n\f").find(bytes[tail.pos])!=std::string_view::npos) ++tail.pos;
    if(bytes.substr(tail.pos,5)!="%%EOF") fail(ErrorCode::invalid_input,"Missing PDF EOF marker",tail.pos);
    std::set<std::size_t> revisions;
    std::size_t entries=0;
    for(;;) {
        if(offset>=bytes.size()) fail(ErrorCode::invalid_input,"xref offset outside input",offset);
        if(!revisions.insert(offset).second) fail(ErrorCode::invalid_input,"Cyclic Prev chain",offset);
        if(revisions.size()>options.limits.depth) fail(ErrorCode::limit,"revision depth exceeded",offset);
        Parser p(bytes,budget,offset);
        if(!p.consume("xref")) {
            // An actual xref stream is unsupported; an arbitrary wrong offset is invalid input.
            auto first=p.value(0,false);
            if(first.kind==Kind::number) {
                p.value(0,false); p.expect("obj"); auto object=p.value();
                if(object.kind==Kind::dictionary && object.dict.contains("Type") && object.dict.at("Type").text=="XRef")
                    fail(ErrorCode::unsupported,"xref streams unsupported",offset);
            }
            fail(ErrorCode::invalid_input,"startxref/Prev does not point to an xref table",offset);
        }
        while(!p.consume("trailer")) {
            auto first=integer(p.value(0,false),"xref subsection start"), count=integer(p.value(0,false),"xref subsection count");
            if(count>options.limits.objects || entries>options.limits.objects-count || first>options.limits.objects || count>options.limits.objects-first)
                fail(ErrorCode::limit,"objects exceeded",p.pos);
            entries+=count;
            for(std::size_t i=0;i<count;++i) {
                auto at=integer(p.value(0,false),"xref offset"), gen=integer(p.value(0,false),"xref generation");
                auto status=p.token(); if((status!="n"&&status!="f")||gen>65535) fail(ErrorCode::invalid_input,"Invalid xref entry",p.pos);
                if(status=="n"&&at>=bytes.size()) fail(ErrorCode::invalid_input,"Object offset outside input",at);
                xref_.try_emplace(first+i,Xref{at,gen,status=="n"});
            }
        }
        auto trailer=p.value(); if(trailer.kind!=Kind::dictionary) fail(ErrorCode::invalid_input,"Invalid trailer",p.pos);
        auto size=integer(field(trailer,"Size"),"trailer Size"); if(size>options.limits.objects) fail(ErrorCode::limit,"objects exceeded",p.pos);
        if(trailer.dict.contains("Encrypt")) fail(ErrorCode::unsupported,"Encrypted PDFs are unsupported",p.pos);
        if(trailer.dict.contains("XRefStm")) fail(ErrorCode::unsupported,"Hybrid xref PDFs are unsupported",p.pos);
        if(trailer_.kind==Kind::null) trailer_=trailer;
        else for(const auto& [key,v]:trailer.dict) trailer_.dict.try_emplace(key,v);
        auto prev=trailer.dict.find("Prev"); if(prev==trailer.dict.end()) break;
        auto previous=integer(prev->second,"Prev"); if(previous>=offset) fail(ErrorCode::invalid_input,"Prev must refer to an earlier revision",p.pos);
        offset=previous;
    }
}
const Object& Reader::object(const Value& ref) {
    if(ref.kind!=Kind::reference) fail(ErrorCode::invalid_input,"Expected indirect reference");
    budget.tick();
    auto entry=xref_.find(ref.id);
    if(entry==xref_.end() || !entry->second.active || entry->second.generation!=ref.generation) fail(ErrorCode::invalid_input,"Unresolved indirect reference "+std::to_string(ref.id));
    if(auto it=cache_.find(ref.id);it!=cache_.end()) return it->second;
    if(resolving_.size()>=options.limits.depth) fail(ErrorCode::limit,"object resolution depth exceeded");
    if(!resolving_.insert(ref.id).second) fail(ErrorCode::invalid_input,"Cyclic object reference");
    Parser p(bytes,budget,entry->second.offset);
    auto id=integer(p.value(0,false),"object number"), generation=integer(p.value(0,false),"generation"); p.expect("obj");
    if(id!=ref.id||generation!=ref.generation) fail(ErrorCode::invalid_input,"xref/object identity mismatch",entry->second.offset);
    Object result; result.value=p.value();
    if(result.value.kind==Kind::keyword) fail(ErrorCode::invalid_input,"Unexpected object keyword",p.pos);
    if(p.consume("stream")) {
        auto length=integer(resolve(field(result.value,"Length")),"stream Length");
        if(p.pos>=bytes.size()) fail(ErrorCode::invalid_input,"Truncated stream",p.pos);
        if(bytes[p.pos]=='\r') { ++p.pos; if(p.pos<bytes.size()&&bytes[p.pos]=='\n') ++p.pos; }
        else if(bytes[p.pos]=='\n') ++p.pos;
        else fail(ErrorCode::invalid_input,"stream requires EOL",p.pos);
        if(length>bytes.size()-p.pos) fail(ErrorCode::invalid_input,"Truncated stream Length",p.pos);
        if(length>options.limits.stream_bytes) fail(ErrorCode::limit,"stream_bytes exceeded",p.pos);
        if(length>options.limits.total_stream_bytes || budget.stream_bytes>options.limits.total_stream_bytes-length) fail(ErrorCode::limit,"total_stream_bytes exceeded",p.pos);
        budget.stream_bytes+=length; result.stream_offset=p.pos; result.has_stream=true;
        result.stream=std::string(bytes.substr(p.pos,length)); p.pos+=length; p.expect("endstream");
    }
    p.expect("endobj"); resolving_.erase(ref.id);
    return cache_.emplace(ref.id,std::move(result)).first->second;
}
Value Reader::resolve(Value v) {
    std::set<std::size_t> seen;
    while(v.kind==Kind::reference) {
        if(!seen.insert(v.id).second) fail(ErrorCode::invalid_input,"Cyclic reference chain");
        if(seen.size()>options.limits.depth) fail(ErrorCode::limit,"reference depth exceeded");
        v=object(v).value;
    }
    return v;
}
Document Reader::read() {
    xrefs();
    auto catalog=resolve(field(trailer_,"Root"));
    const auto& type=field(catalog,"Type"); if(type.kind!=Kind::name||type.text!="Catalog") fail(ErrorCode::invalid_input,"Root is not a Catalog");
    for(auto key:{"AcroForm","OCProperties","OpenAction","AA","Names"}) if(catalog.dict.contains(key)) loss(std::string("Catalog feature unsupported: ")+key);
    Document document;
    std::set<std::size_t> visited;
    std::function<void(Value,Value,std::size_t)> walk=[&](Value ref,Value inherited,std::size_t depth) {
        budget.tick(); if(depth>=options.limits.depth) fail(ErrorCode::limit,"page tree depth exceeded");
        if(ref.kind!=Kind::reference) fail(ErrorCode::invalid_input,"Page tree nodes must be indirect");
        if(!visited.insert(ref.id).second) fail(ErrorCode::invalid_input,"Cyclic or duplicate page tree node");
        auto node=resolve(ref); auto t=field(node,"Type");
        if(t.kind!=Kind::name) fail(ErrorCode::invalid_input,"Invalid page node Type");
        for(auto key:{"Resources","MediaBox","CropBox","Rotate"}) if(node.dict.contains(key)) inherited.dict[key]=node.dict.at(key);
        inherited.kind=Kind::dictionary;
        if(t.text=="Pages") {
            auto kids=resolve(field(node,"Kids")); if(kids.kind!=Kind::array) fail(ErrorCode::invalid_input,"Kids must be an array");
            auto before=document.pages.size();
            for(auto kid:kids.array) walk(kid,inherited,depth+1);
            if(integer(resolve(field(node,"Count")),"page Count")!=document.pages.size()-before) fail(ErrorCode::invalid_input,"Page Count mismatch");
            return;
        }
        if(t.text!="Page") fail(ErrorCode::invalid_input,"Unexpected page tree Type");
        if(document.pages.size()>=options.limits.pages) fail(ErrorCode::limit,"pages exceeded");
        auto page_number=document.pages.size()+1;
        try {
            Page page;
            auto box=resolve(field(inherited,"MediaBox")); if(box.kind!=Kind::array||box.array.size()!=4) fail(ErrorCode::invalid_input,"Invalid MediaBox");
            for(std::size_t i=0;i<4;++i) page.media_box[i]=numeric(resolve(box.array[i]));
            if(page.media_box[2]<=page.media_box[0]||page.media_box[3]<=page.media_box[1]) fail(ErrorCode::invalid_input,"Empty MediaBox");
            if(inherited.dict.contains("Rotate") && numeric(resolve(inherited.dict.at("Rotate")))!=0) loss("Page rotation unsupported");
            if(inherited.dict.contains("CropBox")) {
                auto crop=resolve(inherited.dict.at("CropBox")); if(crop.kind!=Kind::array||crop.array.size()!=4) fail(ErrorCode::invalid_input,"Invalid CropBox");
                for(std::size_t i=0;i<4;++i) if(numeric(resolve(crop.array[i]))!=page.media_box[i]) { loss("CropBox different from MediaBox unsupported"); break; }
            }
            if(node.dict.contains("UserUnit")&&numeric(resolve(node.dict.at("UserUnit")))!=1) loss("UserUnit scaling unsupported");
            for(auto key:{"Annots","AA","Group","Trans"}) if(node.dict.contains(key)) loss(std::string("Page feature unsupported: ")+key);
            Value resources; resources.kind=Kind::dictionary;
            if(inherited.dict.contains("Resources")) resources=resolve(inherited.dict.at("Resources"));
            std::vector<Content> streams;
            auto add=[&](const Value& r) {
                const auto& obj=object(r); if(!obj.has_stream) fail(ErrorCode::invalid_input,"Contents is not a stream");
                if(obj.value.dict.contains("Filter")) {
                    auto filter=resolve(obj.value.dict.at("Filter"));
                    if(filter.kind!=Kind::null && !(filter.kind==Kind::array&&filter.array.empty())) {
                        loss("Filtered stream unsupported (including FlateDecode); stream omitted"); return;
                    }
                }
                if(obj.value.dict.contains("F")) fail(ErrorCode::unsupported,"External stream unsupported");
                streams.push_back({obj.stream,r.id,obj.stream_offset});
            };
            if(node.dict.contains("Contents")) {
                auto contents=node.dict.at("Contents");
                if(contents.kind==Kind::reference && !object(contents).has_stream) contents=resolve(contents);
                if(contents.kind==Kind::array) for(const auto& r:contents.array) add(r);
                else if(contents.kind!=Kind::null) add(contents);
            }
            interpret(*this,page,resources,streams,page_number);
            document.pages.push_back(std::move(page));
        } catch(Failure& e) { e.error.page=page_number; throw; }
    };
    walk(field(catalog,"Pages"),{},0);
    if(document.pages.empty()) fail(ErrorCode::invalid_input,"PDF has no pages");
    bool text=false;
    for(std::size_t i=0;i<document.pages.size();++i) {
        if(!document.pages[i].fragments.empty()) text=true;
        else report.warnings.push_back("Page "+std::to_string(i+1)+": nenhuma camada de texto detectada; pode exigir OCR ou conter texto desenhado como formas");
    }
    if(!text) fail(ErrorCode::unsupported,"Nenhuma camada de texto detectada; o arquivo pode exigir OCR ou conter texto desenhado como formas");
    report.pages_processed=document.pages.size();
    report.font_substitutions.push_back("Helvetica -> Arial (font not embedded; final metrics depend on viewer)");
    reconstruct(document); return document;
}
Result<Inspection> read(std::string_view bytes,const ConversionOptions& options) {
    try { Reader reader(bytes,options); auto document=reader.read(); return Inspection{std::move(document),std::move(reader.report)}; }
    catch(const Failure& e) { return e.error; }
    catch(const std::bad_alloc&) { return Error{ErrorCode::limit,"Memory allocation failed"}; }
    catch(const std::exception& e) { return Error{ErrorCode::internal,e.what()}; }
}
}
