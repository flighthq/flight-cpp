// Include complete original emitted caller and stroke module, not extracted cap/join replacements.
#include <flight/scene2d_formats/lottie_gradient_shape_items.hpp>
#include <flight/scene2d_formats/lottie_stroke_shape_item.hpp>
#include <iostream>
#include <limits>
#include <optional>
struct SourceCase { std::optional<double> value; const char* caps; const char* joints; };
#include "lottie-cap-join-source-cases.inc"
int main(){int checks=0,failures=0;for(const auto& c:source_cases){
 ++checks;if(flight::scene2d_formats::map_lottie_line_cap(c.value)!=flight::String(c.caps))++failures;
 ++checks;if(flight::scene2d_formats::map_lottie_line_join(c.value)!=flight::String(c.joints))++failures;
}std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}
