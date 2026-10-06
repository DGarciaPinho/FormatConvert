#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>
namespace forgeconvert {
struct Matrix { double a=1, b=0, c=0, d=1, e=0, f=0; };
struct Origin { std::size_t page=0, object=0, byte_offset=0, sequence=0; };
struct Fragment {
    std::string text; // UTF-8, no byte == Unicode assumption
    double x=0, y=0, advance=0, size=12;
    std::string font="Helvetica";
    std::array<double,3> color{0,0,0};
    Origin origin;
};
struct Run { std::string text, font="Arial"; double size=12; std::array<double,3> color{0,0,0}; };
struct Paragraph { std::vector<Run> runs; std::string heuristic="single-column-v1"; };
struct Page {
    std::array<double,4> media_box{0,0,612,792};
    std::vector<Fragment> fragments;
    std::vector<Paragraph> paragraphs;
};
struct Document { std::vector<Page> pages; };
void reconstruct(Document& document);
}
