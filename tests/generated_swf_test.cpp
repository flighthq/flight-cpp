// Native parity test for @flighthq/swf's reachable non-document surface at a627849.
//
// The package has a native test already -- tests/generated_swf_reader_test.cpp covers SwfReader -- so this file adds
// only what is genuinely absent, and only for modules that emit and compile. Three of them, each with upstream
// behaviour no native test asserts:
//
//   * expandSwfTagHandlerDispatch -- the flat tag-code table a tag walk dispatches through, from
//     packages/swf/src/expandSwfTagHandlerDispatch.test.ts, whose cases that build a dispatch from the package's own
//     handler families cannot run here (swfShapeTagFamily and swfAllTagHandlers are both refused). What is testable is
//     the function's own contract with CALLER-named handlers, which is also the reason the module was written with no
//     handler import at all: every tag a handler claims maps to that handler, nothing is claimed for an empty array,
//     and a tag claimed twice resolves to the last handler named, so a caller overrides a stock handler by appending.
//   * swfKnownTags -- the code-to-known membership the parser's diagnostics depend on: a ten-bit code space bounded at
//     94, answered from three authored 32-bit masks rather than from the tag names. The mask VALUES are format facts
//     and are asserted as such; the membership is asserted for codes either side of the boundary and for the codes the
//     masks select.
//   * swfImageMimeType -- the two MIME strings a lossless SWF bitmap is carried under.
//
// Not covered here, because the modules refuse: swfDocument and everything that reads a real document through it, the
// shape/text/bitmap handler families, and swfAppearance. Those are refused for the reasons their entries in
// docs/compiler-requests/ record, not for want of a test.
#include <flight/swf/expand_swf_tag_handler_dispatch.hpp>
#include <flight/swf/swf_image_mime_type.hpp>
#include <flight/swf/swf_known_tags.hpp>

#include <cmath>
#include <iostream>
#include <initializer_list>
#include <string>

namespace {

using flight::Array;
using flight::Ref;
using flight::String;
namespace swf = flight::swf;
namespace types = flight::types;

using Handler = Ref<types::SwfTagHandler>;
using HandlerView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Handler>>>;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::string utf8(const String& value) { return value.to_utf8(); }

Handler handler(std::initializer_list<double> tags) {
  return flight::make_ref<types::SwfTagHandler>(types::SwfTagHandler{.tags = Array<double>(tags)});
}

// The same projection the emitted dispatch stores, so identity can be compared rather than assumed.
HandlerView view_of(const Handler& value) {
  return flight::structural_ref_cast<HandlerView>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<Handler>>>(value));
}

// Every tag a handler claims maps to that handler; nothing is claimed for an empty array.
void test_dispatch_maps_every_claimed_tag() {
  const auto first = handler({2.0, 22.0, 87.0});
  const auto second = handler({26.0});
  const auto dispatch = swf::expand_swf_tag_handler_dispatch(Array<Handler>{first, second});

  check(dispatch.size() == 4.0, "one entry per distinct tag claimed");
  check(dispatch.get(2.0).has_value() && dispatch.get(2.0).value() == view_of(first),
        "a tag claimed by the first handler resolves to that handler");
  check(dispatch.get(87.0).has_value() && dispatch.get(87.0).value() == view_of(first),
        "and so does its third tag");
  check(dispatch.get(26.0).has_value() && dispatch.get(26.0).value() == view_of(second),
        "a tag claimed by the second handler resolves to the second handler");
  check(!dispatch.get(3.0).has_value(), "a tag no handler claims resolves to nothing");

  const auto empty = swf::expand_swf_tag_handler_dispatch(Array<Handler>{});
  check(empty.size() == 0.0, "an empty handler array claims nothing");
}

// A tag claimed more than once resolves to the LAST handler named, in either order -- the rule is order, not a
// preference for one of the two.
void test_dispatch_resolves_contested_tags_by_order() {
  const auto stock = handler({2.0, 22.0});
  const auto override_ = handler({2.0});

  const auto appended = swf::expand_swf_tag_handler_dispatch(Array<Handler>{stock, override_});
  check(appended.get(2.0).value() == view_of(override_), "a handler appended later wins the contested tag");
  check(appended.get(22.0).value() == view_of(stock), "and does not disturb the tags it does not claim");

  const auto prepended = swf::expand_swf_tag_handler_dispatch(Array<Handler>{override_, stock});
  check(prepended.get(2.0).value() == view_of(stock), "named first, the other handler wins the same tag");
}

// The authored masks are the format facts the membership answer rests on, and the two MIME strings are facts too.
void test_tag_vocabulary_and_mime_constants() {
  check(swf::swf_known_tag_masks.size() == 3.0 && swf::swf_known_tag_masks[0] == 369033207.0 &&
            swf::swf_known_tag_masks[1] == 4278282943.0 && swf::swf_known_tag_masks[2] == 1876721639.0,
        "the three known-tag masks are the authored values");
  check(utf8(swf::swf_lossless_mime_type) == "image/x-swf-lossless" &&
            utf8(swf::swf_lossless_alpha_mime_type) == "image/x-swf-lossless-alpha",
        "the lossless MIME types are the authored strings");
}

// Membership: the code space is bounded at 94, and inside it the answer follows the masks.
void test_known_tag_membership() {
  check(!swf::is_known_swf_tag(-1.0), "a negative code is not a tag");
  check(!swf::is_known_swf_tag(95.0), "a code above the ten-bit vocabulary's end is not known");
  check(!swf::is_known_swf_tag(1023.0), "nor is the largest ten-bit code");
  check(swf::is_known_swf_tag(0.0), "code 0 is known");
  check(swf::is_known_swf_tag(1.0), "code 1 is known");
  check(swf::is_known_swf_tag(94.0), "the last code of the vocabulary is known");

  // The masks say which codes are known; the function must agree with them everywhere in the bounded range. The
  // arithmetic here is the masks' own bit test, so what this catches is the emitted INDEXING -- a shift or mask
  // off by one would move the answer for many codes at once.
  int mismatches = 0;
  int known = 0;
  for (int code = 0; code <= 94; ++code) {
    const double word = swf::swf_known_tag_masks[static_cast<std::size_t>(code / 32)];
    const double bit = std::fmod(std::floor(word / std::pow(2.0, code % 32)), 2.0);
    const bool expected = bit >= 1.0;
    if (swf::is_known_swf_tag(code)) ++known;
    if (swf::is_known_swf_tag(code) != expected) ++mismatches;
  }
  check(mismatches == 0, "every code in the bounded range agrees with the masks");
  // Measured at this pin, and stable as long as the masks are: 69 of the 95 codes in the vocabulary are known.
  check(known == 69, "69 of the vocabulary's 95 codes are known");
}

}  // namespace

int main() {
  test_dispatch_maps_every_claimed_tag();
  test_dispatch_resolves_contested_tags_by_order();
  test_tag_vocabulary_and_mime_constants();
  test_known_tag_membership();
  if (failures == 0) std::cout << "generated swf: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
