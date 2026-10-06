#pragma once
#include <map>
#include <set>
#include <string_view>
#include "forgeconvert/conversion/convert.hpp"
namespace forgeconvert::pdf {
enum class Kind { null, boolean, number, name, string, array, dictionary, reference, keyword };
struct Value {
    Kind kind=Kind::null;
    double number=0;
    std::string text;
    std::vector<Value> array;
    std::map<std::string,Value> dict;
    std::size_t id=0, generation=0;
};
struct Budget {
    const ResourceLimits& limits;
    std::size_t steps=0, stream_bytes=0;
    void tick(std::size_t offset=0) { if(++steps>limits.steps) fail(ErrorCode::limit,"steps exceeded",offset); }
};
class Parser {
public:
    std::string_view data;
    std::size_t pos;
    Budget& budget;
    Parser(std::string_view bytes, Budget& b,std::size_t offset=0):data(bytes),pos(offset),budget(b) {}
    void whitespace();
    bool consume(std::string_view token);
    void expect(std::string_view token);
    std::string token();
    Value value(std::size_t depth=0, bool references=true);
};
std::size_t integer(const Value& v, std::string_view label);
double numeric(const Value& v);
const Value& field(const Value& v, std::string_view key);
struct Object { Value value; std::string stream; bool has_stream=false; std::size_t stream_offset=0; };
class Reader {
    struct Xref { std::size_t offset, generation; bool active; };
    std::map<std::size_t,Xref> xref_;
    std::map<std::size_t,Object> cache_;
    std::set<std::size_t> resolving_;
    Value trailer_;
public:
    std::string_view bytes;
    const ConversionOptions& options;
    Budget budget;
    ConversionReport report;
    Reader(std::string_view data,const ConversionOptions& opts):bytes(data),options(opts),budget{opts.limits} {}
    void xrefs();
    const Object& object(const Value& ref);
    Value resolve(Value v);
    Document read();
    void loss(std::string message);
};
struct Content { std::string bytes; std::size_t object=0, offset=0; };
void interpret(Reader& reader, Page& page, const Value& resources, const std::vector<Content>& streams, std::size_t page_number);
Result<Inspection> read(std::string_view bytes,const ConversionOptions& options);
}
