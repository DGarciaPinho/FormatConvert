#include "forgeconvert/document/model.hpp"
#include <algorithm>
#include <cmath>
namespace forgeconvert {
void reconstruct(Document& document) {
    for(auto& page:document.pages) {
        page.paragraphs.clear();
        std::vector<const Fragment*> ordered;
        for(const auto& f:page.fragments) ordered.push_back(&f);
        std::stable_sort(ordered.begin(),ordered.end(),[](auto a,auto b) { return a->y>b->y; });
        struct Line { double y,size; std::vector<const Fragment*> fragments; };
        std::vector<Line> lines;
        for(auto f:ordered) {
            // Compare to fixed representative baseline, avoiding a non-transitive sort comparator.
            if(lines.empty()||std::abs(lines.back().y-f->y)>0.25*std::min(lines.back().size,f->size)) lines.push_back({f->y,f->size,{}});
            lines.back().fragments.push_back(f);
        }
        double previous_y=0,previous_size=0,previous_indent=0;
        for(const auto& line:lines) {
            auto pieces=line.fragments;
            std::stable_sort(pieces.begin(),pieces.end(),[](auto a,auto b) { return a->x<b->x; });
            double indent=pieces.front()->x;
            bool new_paragraph=page.paragraphs.empty() || previous_y-line.y>1.6*std::max(previous_size,line.size) || std::abs(indent-previous_indent)>line.size || std::abs(previous_size-line.size)>1;
            if(new_paragraph) page.paragraphs.push_back({});
            auto& paragraph=page.paragraphs.back();
            if(!new_paragraph&&!paragraph.runs.empty()) paragraph.runs.back().text+=' ';
            const Fragment* previous=nullptr;
            for(auto f:pieces) {
                if(previous && f->x-(previous->x+previous->advance)>0.18*f->size && previous->text!=" " && f->text!=" ")
                    paragraph.runs.back().text+=' ';
                if(paragraph.runs.empty()||paragraph.runs.back().size!=f->size||paragraph.runs.back().color!=f->color)
                    paragraph.runs.push_back({{},"Arial",f->size,f->color});
                paragraph.runs.back().text+=f->text; previous=f;
            }
            previous_y=line.y; previous_size=line.size; previous_indent=indent;
        }
    }
}
}
