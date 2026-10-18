// Expected results come from the WHOLE original pinned TypeScript modules, including actual reader and diagnostic callees.
#include <flight/skeleton2d_formats/spine_binary_bones_handler.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
#include <type_traits>

namespace {
namespace spine = flight::skeleton2d_formats;
namespace types = flight::types;
using Bytes = std::vector<std::uint8_t>;
struct Bone { const char* name; int preset; double length,parent,rotation,scale_x,scale_y,shear_x,shear_y,x,y; };
struct Diagnostic { const char* severity; const char* kind; const char* origin; const char* bone; double declared,read; };
struct Case { const char* name; Bytes bytes; bool nonessential,skip,collect; double offset; std::vector<Bone> bones; std::vector<Diagnostic> diagnostics; };
const std::vector<Case> cases = {
  {"empty", {0}, false, false, true, 1.0, {}, {}},
  {"short-count", {}, false, false, true, 1.0, {}, {}},
  {"mode-0", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1}, false, false, true, 40.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-1", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,1,1}, false, false, true, 40.0, {{"root", 1, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-2", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,2,1}, false, false, true, 40.0, {{"root", 2, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-3", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,3,1}, false, false, true, 40.0, {{"root", 3, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-4", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,4,1}, false, false, true, 40.0, {{"root", 4, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-5", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,5,1}, false, false, true, 40.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-127", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,127,1}, false, false, true, 40.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-128", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,128,1,1}, false, false, true, 41.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"mode-4294967295", {1,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,255,255,255,255,15,1}, false, false, true, 44.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"null-name", {1,0,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1}, false, false, true, 36.0, {{nullptr, 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"empty-name", {1,1,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,1,1}, false, false, true, 36.0, {{"", 1, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"unicode", {1,8,233,170,168,240,159,153,130,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,2,1}, false, false, true, 43.0, {{"骨🙂", 2, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"two-bones", {2,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1,6,99,104,105,108,100,0,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,3,1}, false, false, true, 81.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0},{"child", 3, 18.0, 0.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"invalid-parent", {2,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1,4,98,97,100,1}, false, false, true, 46.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {{"Drop", "spine.binary-bone-parent-out-of-range", "parseSpineBinaryBones", "bad", 1.0, 1.0}}},
  {"invalid-large-parent", {2,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1,4,98,97,100,231,7}, false, false, true, 47.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {{"Drop", "spine.binary-bone-parent-out-of-range", "parseSpineBinaryBones", "bad", 999.0, 1.0}}},
  {"nonessential", {1,6,99,111,108,111,114,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,4,1,1,2,3,4}, true, false, true, 45.0, {{"color", 4, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"numeric-domain", {1,7,100,111,109,97,105,110,128,0,0,0,127,128,0,0,255,128,0,0,127,192,0,0,192,64,0,0,0,0,0,0,62,128,0,0,113,73,242,202,0,1}, false, false, true, 42.0, {{"domain", 0, 1.0000000150474662e+30, -1.0, -0.0, std::numeric_limits<double>::quiet_NaN(), -3.0, 0.0, 0.25, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}}, {}},
  {"prefix-1", {1}, false, false, true, 2.0, {{nullptr, 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-2", {1,4}, false, false, true, 3.0, {{nullptr, 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-3", {1,4,99}, false, false, true, 4.0, {{nullptr, 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-4", {1,4,99,117}, false, false, true, 5.0, {{nullptr, 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-5", {1,4,99,117,116}, false, false, true, 6.0, {{"cut", 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-6", {1,4,99,117,116,65}, false, false, true, 7.0, {{"cut", 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-7", {1,4,99,117,116,65,48}, false, false, true, 8.0, {{"cut", 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-8", {1,4,99,117,116,65,48,0}, false, false, true, 9.0, {{"cut", 0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-9", {1,4,99,117,116,65,48,0,0}, false, false, true, 10.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-10", {1,4,99,117,116,65,48,0,0,65}, false, false, true, 11.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-11", {1,4,99,117,116,65,48,0,0,65,64}, false, false, true, 12.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-12", {1,4,99,117,116,65,48,0,0,65,64,0}, false, false, true, 13.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}}, {}},
  {"prefix-13", {1,4,99,117,116,65,48,0,0,65,64,0,0}, false, false, true, 14.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 0.0}}, {}},
  {"prefix-14", {1,4,99,117,116,65,48,0,0,65,64,0,0,65}, false, false, true, 15.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 0.0}}, {}},
  {"prefix-15", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80}, false, false, true, 16.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 0.0}}, {}},
  {"prefix-16", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0}, false, false, true, 17.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 0.0}}, {}},
  {"prefix-17", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0}, false, false, true, 18.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-18", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65}, false, false, true, 19.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-19", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96}, false, false, true, 20.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-20", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0}, false, false, true, 21.0, {{"cut", 0, 0.0, -1.0, 11.0, 0.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-21", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0}, false, false, true, 22.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-22", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65}, false, false, true, 23.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-23", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112}, false, false, true, 24.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-24", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0}, false, false, true, 25.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 0.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-25", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0}, false, false, true, 26.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-26", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65}, false, false, true, 27.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-27", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128}, false, false, true, 28.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-28", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0}, false, false, true, 29.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 0.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-29", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0}, false, false, true, 30.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-30", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65}, false, false, true, 31.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-31", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136}, false, false, true, 32.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-32", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0}, false, false, true, 33.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 0.0, 12.0, 13.0}}, {}},
  {"prefix-33", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0}, false, false, true, 34.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"prefix-34", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65}, false, false, true, 35.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"prefix-35", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144}, false, false, true, 36.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"prefix-36", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0}, false, false, true, 37.0, {{"cut", 0, 0.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"prefix-37", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0}, false, false, true, 38.0, {{"cut", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"prefix-38", {1,4,99,117,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,2}, false, false, true, 39.0, {{"cut", 2, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
  {"skip-valid", {2,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1,6,99,104,105,108,100,0,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,1,1}, false, true, true, 81.0, {}, {}},
  {"skip-invalid", {2,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1,4,98,97,100,1}, false, true, true, 46.0, {}, {}},
  {"no-sink-invalid", {2,5,114,111,111,116,65,48,0,0,65,64,0,0,65,80,0,0,65,96,0,0,65,112,0,0,65,128,0,0,65,136,0,0,65,144,0,0,0,1,4,98,97,100,1}, false, false, false, 46.0, {{"root", 0, 18.0, -1.0, 11.0, 14.0, 15.0, 16.0, 17.0, 12.0, 13.0}}, {}},
};
int failures = 0;
void check(bool condition, const char* name, const char* message) {
  if (!condition) { std::cerr << name << ": " << message << '\n'; ++failures; }
}
bool same(double a,double b) { return std::isnan(b) ? std::isnan(a) : a==b && (b!=0 || std::signbit(a)==std::signbit(b)); }
template <typename Pointer> auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}
void run(const Case& c) {
  Bytes raw{0xaa,0xbb,0xcc}; raw.insert(raw.end(),c.bytes.begin(),c.bytes.end()); raw.push_back(0xdd);raw.push_back(0xee);
  flight::Uint8Array backing(raw);
  auto input=backing.subarray(3,3+c.bytes.size());
  auto reader=spine::create_spine_binary_reader(input);
  const auto view_owner=reader->view.identity(), buffer_owner=reader->view.buffer.identity();
  const auto seed=flight::make_ref<types::Bone2D>(); seed->name=flight::String("seed"); seed->transform_mode=types::transform_mode_2d->no_scale;
  const flight::Array<flight::Ref<types::Bone2D>> original{seed};
  const flight::Array<flight::Ref<types::ImportDiagnostic>> diagnostics;
  auto context=flight::make_ref<types::SpineBinarySectionContext>();
  context->bones=original;context->reader=reader;context->nonessential=c.nonessential;
  context->diagnostics=c.collect ? std::optional{diagnostics}:std::nullopt;
  context->registry=view(flight::make_ref<types::SpineBinaryRegistry>());
  if(c.skip) spine::skip_spine_binary_bones_section(context); else spine::spine_binary_bones_section_handler(context);
  check(context->reader==reader && context->bones.identity()==original.identity(),c.name,"same reader and caller bone array");
  check(reader->view.identity()==view_owner && reader->view.buffer.identity()==buffer_owner && reader->view.byte_offset==3 && reader->view.byte_length==c.bytes.size(),c.name,"original view and subarray window");
  check(reader->offset==c.offset,c.name,"exact cursor / sticky overrun");
  check(context->bones.size()==c.bones.size()+1 && context->bones[0]==seed,c.name,"seed, append order and output size");
  const std::array modes{types::transform_mode_2d->normal,types::transform_mode_2d->only_translation,types::transform_mode_2d->no_rotation_or_reflection,types::transform_mode_2d->no_scale,types::transform_mode_2d->no_scale_or_reflection};
  for(std::size_t i=0;i<c.bones.size() && i+1<context->bones.size();++i) {
    const auto& got=context->bones[i+1]; const auto& want=c.bones[i];
    const auto text=std::get_if<flight::String>(&got->name);
    check(want.name ? text && *text==flight::String(want.name) : std::holds_alternative<flight::Null>(got->name),c.name,"null/empty/UTF16 bone name");
    check(got->transform_mode==modes[want.preset] && !got->transform_mode.owner_before(modes[want.preset]) && !modes[want.preset].owner_before(got->transform_mode),c.name,"original preset object and control block");
    const double actual[]={got->length,got->parent_index,got->rotation,got->scale_x,got->scale_y,got->shear_x,got->shear_y,got->x,got->y};
    const double expected[]={want.length,want.parent,want.rotation,want.scale_x,want.scale_y,want.shear_x,want.shear_y,want.x,want.y};
    for(int k=0;k<9;++k) check(same(actual[k],expected[k]),c.name,"exact original numeric domain/field order");
  }
  check(diagnostics.size()==c.diagnostics.size(),c.name,"diagnostic count / absent sink / skip");
  for(std::size_t i=0;i<c.diagnostics.size() && i<diagnostics.size();++i) {
    const auto& got=diagnostics[i];const auto& want=c.diagnostics[i];
    check(got->severity==flight::String(want.severity) && got->kind==flight::String(want.kind) && got->origin==flight::String(want.origin),c.name,"original Drop diagnostic fields");
    for(const auto& entry:std::array<std::pair<const char*,double>,2>{{{"declared",want.declared},{"read",want.read}}}) {
      const auto value=got->detail ? got->detail->get(flight::String(entry.first)):std::nullopt;
      check(value && std::holds_alternative<double>(*value) && std::get<double>(*value)==entry.second,c.name,"diagnostic numeric detail");
    }
    const auto bone=got->detail ? got->detail->get(flight::String("bone")):std::nullopt;
    check(bone && std::holds_alternative<flight::String>(*bone) && std::get<flight::String>(*bone)==flight::String(want.bone),c.name,"diagnostic bone detail");
  }
  for(std::size_t i=0;i<raw.size();++i) check(backing[i]==raw[i],c.name,"input bytes unchanged");
}
void public_domain() {
  static_assert(std::is_same_v<decltype(types::Bone2D::transform_mode),flight::Ref<types::TransformInherit2D>>);
  for(int mask=0;mask<16;++mask) {
    auto flags=flight::make_ref<types::TransformInherit2D>();
    flags->reflection=mask&1;flags->rotation=mask&2;flags->scale=mask&4;flags->translation=mask&8;
    auto bone=flight::make_ref<types::Bone2D>();bone->transform_mode=flags;
    check(bone->transform_mode==flags && flags->reflection==bool(mask&1) && flags->rotation==bool(mask&2) && flags->scale==bool(mask&4) && flags->translation==bool(mask&8),"public-domain","all independent four-axis combinations retain owner");
  }
  const std::array presets{types::transform_mode_2d->normal,types::transform_mode_2d->only_translation,types::transform_mode_2d->no_rotation_or_reflection,types::transform_mode_2d->no_scale,types::transform_mode_2d->no_scale_or_reflection};
  const bool expected[][4] = {{true,true,true,true},{false,false,false,true},{false,false,true,true},{true,true,false,true},{false,true,false,true}};
  for(int i=0;i<5;++i) {
    check(presets[i]->reflection==expected[i][0] && presets[i]->rotation==expected[i][1] && presets[i]->scale==expected[i][2] && presets[i]->translation==expected[i][3],"preset-flags","original four flags of each shared preset");
    for(int j=i+1;j<5;++j) check(presets[i]!=presets[j],"preset-flags","distinct original singleton owners");
  }
  const auto normal=types::transform_mode_2d->normal;
  auto context=flight::make_ref<types::SpineBinarySectionContext>();context->nonessential=false;
  context->reader=spine::create_spine_binary_reader(flight::Uint8Array(cases.at(2).bytes));
  context->registry=view(flight::make_ref<types::SpineBinaryRegistry>());
  spine::spine_binary_bones_section_handler(context);
  check(context->bones.size()==1 && context->bones[0]->transform_mode==normal,"live-preset","public parser retains Normal");
  const bool before=normal->reflection;normal->reflection=!before;
  check(context->bones.size()==1 && context->bones[0]->transform_mode->reflection==!before,"live-preset","later writes visible through bone and table");
  normal->reflection=before;
}
}
int main() {
  using Handler = decltype(&spine::spine_binary_bones_section_handler);
  const auto target = spine::spine_binary_bones_section_reader.target<Handler>();
  check(target && *target==&spine::spine_binary_bones_section_handler,"public-alias","original reader alias");
  for(const auto& c:cases) run(c);
  public_domain();
  std::cout << cases.size() << " whole-source binary bone cases; all16 public flag combinations\n";
  return failures ? 1:0;
}
