// Focused behavior tests for @flighthq/video.
//
// The package is partial at a627849: four of five modules emit and `videoResourceFrom` refuses, so this
// exercises the two whole modules -- `videoFormat` and `videoResource` -- against the cases their own
// upstream suites state. Nothing here supplies the refused module; that refusal is filed as
// docs/compiler-requests/image-source-union-domain-discriminator.md.
//
// What the two modules promise, and what is checked:
//
//   * `detectVideoMimeType` reads magic bytes, needs four bytes to answer at all and eight to recognize
//     `ftyp`, accepts either an ArrayBuffer or a Uint8Array, and answers null rather than guessing;
//   * `inferVideoMimeType` maps the extension table, drops the query string, ignores case, and answers
//     null for anything it does not know;
//   * `canPlayVideoType` refuses an empty media type without consulting the host, and a host that throws
//     is a refusal, not a crash;
//   * `selectVideoResourceUrl` prefers the declared type over the inferred one, skips a source it cannot
//     name, returns the *same* source object, and answers null when nothing probes;
//   * a video resource's element is only released when it owns it, its object URL is revoked on both
//     teardown paths and only once, and every read answers 0 with no element attached.
#include <flight/video/video_format.hpp>
#include <flight/video/video_resource.hpp>

#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using flight::String;
using flight::types::HostVideoCapability;
using flight::types::VideoResource;
using flight::types::VideoResourceUrl;

std::string text(const std::optional<flight::String>& value) {
  return value.has_value() ? value.value().to_utf8() : std::string("<none>");
}

flight::ArrayBuffer buffer_of(std::initializer_list<unsigned char> bytes) {
  auto buffer = flight::ArrayBuffer::allocate(bytes.size());
  std::memcpy(buffer.writable_data(), bytes.begin(), bytes.size());
  return buffer;
}

flight::host_sdl::ImageSource element_value() {
  return flight::host_sdl::ImageSource::rgba8(2, 2,
                                             flight::Uint8ClampedArray(flight::Array{
                                                 0.0, 0.0, 0.0, 255.0, 0.0, 0.0, 0.0, 255.0,
                                                 0.0, 0.0, 0.0, 255.0, 0.0, 0.0, 0.0, 255.0}),
                                             flight::host_sdl::ImageSourceKind::video_element);
}

flight::Ref<VideoResourceUrl> source_url(const char* url, std::optional<const char*> type = std::nullopt) {
  auto source = flight::make_ref<VideoResourceUrl>();
  source->url = String(url);
  if (type.has_value()) source->type = String(type.value());
  return source;
}

flight::Ref<HostVideoCapability> video_host(bool answer) {
  auto host = flight::make_ref<HostVideoCapability>();
  host->can_play_type = [answer](String) { return answer; };
  return host;
}

}  // namespace

// The body is a function so that an exception escaping it is NAMED rather than left to abort the
// process. That matters for mutation checking: an implementation that reaches for an absent object URL
// throws where the real one returns early, and without this the run ends as "terminating due to uncaught
// exception" with no indication of which assertion it was passing through.
int run_checks() {
  // detectVideoMimeType: magic bytes, the length gate, and both carrier arms.
  {
    const auto webm = flight::video::detect_video_mime_type(
        std::variant<flight::ArrayBuffer, flight::Uint8Array>{std::in_place_type<flight::Uint8Array>,
                                                              flight::Uint8Array(flight::Array{26.0, 69.0, 223.0, 163.0})});
    check(text(webm) == "video/webm", "the EBML magic identifies WebM");

    const auto ogg = flight::video::detect_video_mime_type(
        std::variant<flight::ArrayBuffer, flight::Uint8Array>{std::in_place_type<flight::Uint8Array>,
                                                              flight::Uint8Array(flight::Array{79.0, 103.0, 103.0, 83.0})});
    check(text(ogg) == "video/ogg", "the OggS magic identifies Ogg");

    const auto short_buffer = flight::video::detect_video_mime_type(
        std::variant<flight::ArrayBuffer, flight::Uint8Array>{std::in_place_type<flight::Uint8Array>,
                                                              flight::Uint8Array(flight::Array{26.0, 69.0, 223.0})});
    check(!short_buffer.has_value(), "three bytes cannot identify anything");

    // `ftyp` is read at offset four, so four bytes are not enough even when they spell it.
    const auto not_quite_ftyp = flight::video::detect_video_mime_type(
        std::variant<flight::ArrayBuffer, flight::Uint8Array>{std::in_place_type<flight::Uint8Array>,
                                                              flight::Uint8Array(flight::Array{102.0, 116.0, 121.0, 112.0})});
    check(!not_quite_ftyp.has_value(), "ftyp at offset zero is not the ISO-BMFF signature");

    const auto mp4 = flight::video::detect_video_mime_type(
        std::variant<flight::ArrayBuffer, flight::Uint8Array>{
            std::in_place_type<flight::ArrayBuffer>,
            buffer_of({0, 0, 0, 16, 'f', 't', 'y', 'p'})});
    check(text(mp4) == "video/mp4", "ftyp at offset four identifies MP4 through the ArrayBuffer arm");

    const auto unknown = flight::video::detect_video_mime_type(
        std::variant<flight::ArrayBuffer, flight::Uint8Array>{
            std::in_place_type<flight::ArrayBuffer>, buffer_of({1, 2, 3, 4, 5, 6, 7, 8})});
    check(!unknown.has_value(), "unrecognized bytes answer null rather than guessing");
  }

  // inferVideoMimeType: the table, the query string, case, and the unknown case.
  {
    check(text(flight::video::infer_video_mime_type(String("clip.mp4"))) == "video/mp4", "mp4");
    check(text(flight::video::infer_video_mime_type(String("clip.m4v"))) == "video/mp4", "m4v shares MP4");
    check(text(flight::video::infer_video_mime_type(String("clip.webm"))) == "video/webm", "webm");
    check(text(flight::video::infer_video_mime_type(String("clip.mkv"))) == "video/x-matroska", "mkv");
    check(text(flight::video::infer_video_mime_type(String("clip.ogv"))) == "video/ogg", "ogv");
    check(text(flight::video::infer_video_mime_type(String("clip.ogg"))) == "video/ogg", "ogg");
    check(text(flight::video::infer_video_mime_type(String("clip.mov"))) == "video/quicktime", "mov");
    check(text(flight::video::infer_video_mime_type(String("clip.3gp"))) == "video/3gpp", "3gp");
    check(text(flight::video::infer_video_mime_type(String("stream.m3u8"))) ==
              "application/vnd.apple.mpegurl",
          "m3u8");
    check(text(flight::video::infer_video_mime_type(String("stream.mpd"))) == "application/dash+xml",
          "mpd");
    check(text(flight::video::infer_video_mime_type(String("clip.MP4?token=abc"))) == "video/mp4",
          "the extension is lower-cased and the query string is dropped");
    check(!flight::video::infer_video_mime_type(String("clip")).has_value(), "no extension is null");
    check(!flight::video::infer_video_mime_type(String("clip.avi")).has_value(), "an unknown extension is null");
    check(!flight::video::infer_video_mime_type(String("")).has_value(), "an empty URL is null");
    check(!flight::video::infer_video_mime_type(String("clip.")).has_value(), "a trailing dot is null");
  }

  // canPlayVideoType: the empty type short-circuits, and a throwing host is a refusal.
  {
    int probes = 0;
    auto host = flight::make_ref<HostVideoCapability>();
    host->can_play_type = [&probes](String) {
      ++probes;
      return true;
    };
    check(!flight::video::can_play_video_type(host, String("")),
          "an empty media type is refused without asking");
    check(probes == 0, "and the host was never consulted");
    check(flight::video::can_play_video_type(host, String("video/mp4")), "a supported type reports true");
    check(probes == 1, "exactly once");

    auto refusing = video_host(false);
    check(!flight::video::can_play_video_type(refusing, String("video/mp4")),
          "an unsupported type reports false");

    auto throwing = flight::make_ref<HostVideoCapability>();
    throwing->can_play_type = [](String) -> bool { throw std::runtime_error("probe failed"); };
    check(!flight::video::can_play_video_type(throwing, String("video/mp4")),
          "a host that throws is a refusal, not a crash");
  }

  // selectVideoResourceUrl: declared type first, inferred second, same object back, null otherwise.
  {
    const auto declared = source_url("clip.unknown", "video/mp4");
    const auto inferred_only = source_url("clip.webm");
    const auto unnamed = source_url("clip.unknown");

    auto host = video_host(true);
    flight::Array<flight::Ref<VideoResourceUrl>> sources = flight::Array<flight::Ref<VideoResourceUrl>>{};
    sources.push(unnamed);
    sources.push(declared);
    sources.push(inferred_only);
    const auto selected = flight::video::select_video_resource_url(host, sources);
    check(selected.has_value(), "a source that probes is selected");
    check(selected.has_value() && selected.value() == declared,
          "the source that declares a playable type wins over the one before it");
    check(selected.has_value() && selected.value()->url == String("clip.unknown"),
          "and the very same source object comes back, not a copy");

    // With only the inferable source present, its inferred type is what gets probed.
    flight::Array<flight::Ref<VideoResourceUrl>> only_inferred = flight::Array<flight::Ref<VideoResourceUrl>>{};
    only_inferred.push(unnamed);
    only_inferred.push(inferred_only);
    const auto inferred_selected = flight::video::select_video_resource_url(host, only_inferred);
    check(inferred_selected.has_value() && inferred_selected.value() == inferred_only,
          "a source without a declared type is probed through its inferred type");
    check(inferred_selected.has_value() && inferred_selected.value() != unnamed,
          "and the source nothing can name is skipped rather than selected");

    auto refusing = video_host(false);
    check(!flight::video::select_video_resource_url(refusing, sources).has_value(),
          "nothing is selected when no source probes");
  }

  // A video resource's carrier defaults, and every read with no element attached.
  {
    const auto empty = flight::video::create_video_resource();
    check(!empty->element.has_value(), "a resource starts with no element");
    check(!empty->object_url.has_value(), "and no object URL");
    check(!empty->owns_element, "and owns nothing");
    check(!flight::video::has_video_resource_element(empty), "so it has no element");

    int host_reads = 0;
    auto host = flight::make_ref<HostVideoCapability>();
    host->get_width = [&host_reads](flight::types::HostImageSource) {
      ++host_reads;
      return 640.0;
    };
    check(flight::video::get_video_resource_width(host, empty) == 0.0,
          "width is 0 with no element, even when the host can report one");
    check(flight::video::get_video_resource_duration(host, empty) == 0.0,
          "duration is 0 with no element");
    check(flight::video::get_video_resource_height(host, empty) == 0.0,
          "height is 0 with no element");
    check(host_reads == 0, "and the host hooks are never consulted");
    check(flight::video::is_video_resource_empty(host, empty), "a resource with no extent is empty");
    check(!flight::video::is_video_resource_ready(host, empty), "and is not ready");
  }

  // With an element attached, the host's own answer is what comes back.
  {
    const auto element = element_value();
    auto resource = flight::video::create_video_resource(element, std::nullopt, false);
    check(resource->element.has_value(), "the element is retained");
    check(flight::video::has_video_resource_element(resource), "and observable");

    auto host = flight::make_ref<HostVideoCapability>();
    host->get_width = [](flight::types::HostImageSource) { return 640.0; };
    host->get_height = [](flight::types::HostImageSource) { return 360.0; };
    host->is_ready = [](flight::types::HostImageSource) { return true; };
    check(flight::video::get_video_resource_width(host, resource) == 640.0, "width comes from the host");
    check(flight::video::get_video_resource_height(host, resource) == 360.0, "height comes from the host");
    check(!flight::video::is_video_resource_empty(host, resource), "a sized resource is not empty");
    check(flight::video::is_video_resource_ready(host, resource), "and a ready host makes it ready");

    // One dimension at a time. The check above zeroes BOTH dimensions when it means "no extent", so an `||`
    // that tested only one of the two would pass it -- which is exactly what the upstream suite warns about,
    // and what a mutation to a strict zero test on either operand went undetected by until this pair existed.
    {
      auto wide = flight::make_ref<HostVideoCapability>();
      wide->get_width = [](flight::types::HostImageSource) { return 640.0; };
      wide->get_height = [](flight::types::HostImageSource) { return 0.0; };
      check(flight::video::is_video_resource_empty(wide, resource),
            "a resource with no height is empty even when its width is 640");

      auto tall = flight::make_ref<HostVideoCapability>();
      tall->get_width = [](flight::types::HostImageSource) { return 0.0; };
      tall->get_height = [](flight::types::HostImageSource) { return 480.0; };
      check(flight::video::is_video_resource_empty(tall, resource),
            "and one with no width is empty even when its height is 480");
    }

    // The host may report NaN before metadata and Infinity for a live stream; both pass through.
    auto live = flight::make_ref<HostVideoCapability>();
    live->get_duration = [](flight::types::HostImageSource) {
      return std::numeric_limits<double>::infinity();
    };
    check(flight::video::get_video_resource_duration(live, resource) ==
              std::numeric_limits<double>::infinity(),
          "an open-ended live stream keeps its infinite duration");

    auto unknown_duration = flight::make_ref<HostVideoCapability>();
    unknown_duration->get_duration = [](flight::types::HostImageSource) {
      return std::numeric_limits<double>::quiet_NaN();
    };
    check(std::isnan(flight::video::get_video_resource_duration(unknown_duration, resource)),
          "a host that has not read metadata yet keeps its NaN");

    // A host with no hook at all answers 0 / not ready rather than failing.
    auto bare = flight::make_ref<HostVideoCapability>();
    check(flight::video::get_video_resource_width(bare, resource) == 0.0,
          "a host without a width hook reports 0");
    check(flight::video::is_video_resource_empty(bare, resource), "which makes the resource empty");
    check(!flight::video::is_video_resource_ready(bare, resource),
          "and a host without a readiness hook is not ready");

    auto not_ready = flight::make_ref<HostVideoCapability>();
    not_ready->is_ready = [](flight::types::HostImageSource) { return false; };
    check(!flight::video::is_video_resource_ready(not_ready, resource), "a host reporting false is not ready");
  }

  // destroyVideoResource honours ownership; disposeVideoResource does not; both revoke once.
  {
    const auto element = element_value();
    int releases = 0;
    int revokes = 0;
    flight::types::HostImageSource released;
    auto host = flight::make_ref<HostVideoCapability>();
    host->release_element = [&](flight::types::HostImageSource value) {
      ++releases;
      released = value;
    };
    host->revoke_object_url = [&revokes](String) { ++revokes; };

    // Borrowed: the caller owns the element, so destroy must not release it.
    auto borrowed = flight::video::create_video_resource(element, String("blob:borrowed"), false);
    flight::video::destroy_video_resource(host, borrowed);
    check(releases == 0, "a borrowed element is not released");
    check(revokes == 1, "but the resource's own object URL is revoked");
    check(!borrowed->object_url.has_value(), "and the URL is cleared");
    check(!borrowed->element.has_value() && !borrowed->owns_element, "the element is dropped");

    // Owned: destroy releases the exact element.
    auto owned = flight::video::create_video_resource(element, String("blob:owned"), true);
    flight::video::destroy_video_resource(host, owned);
    check(releases == 1, "an owned element is released");
    check(released == element, "and it is the element that was attached");
    check(revokes == 2, "and the object URL is revoked");

    // A second teardown has nothing left to do.
    flight::video::destroy_video_resource(host, owned);
    check(releases == 1 && revokes == 2, "tearing down twice releases and revokes nothing more");

    // dispose releases regardless of ownership.
    auto disposed = flight::video::create_video_resource(element, String("blob:disposed"), false);
    flight::video::dispose_video_resource(host, disposed);
    check(releases == 2, "dispose releases a borrowed element too");
    check(revokes == 3, "and revokes the URL");

    // A host without the hooks still clears the resource rather than failing.
    auto bare = flight::make_ref<HostVideoCapability>();
    auto bare_resource = flight::video::create_video_resource(element, String("blob:bare"), true);
    flight::video::destroy_video_resource(bare, bare_resource);
    check(!bare_resource->element.has_value() && !bare_resource->object_url.has_value(),
          "a host with no teardown hooks still leaves the resource cleared");
  }

  // THE REMAINING REACHABLE CASES. Each uses the fixture's own input, because these are ports rather than
  // new probes: an assertion that only resembles the case is how a case quietly stops being covered.
  {
    // An EMPTY source list selects nothing. This is a different claim from "nothing in the list is
    // playable" -- it never enters the loop at all.
    auto host = video_host(true);
    flight::Array<flight::Ref<VideoResourceUrl>> nothing = flight::Array<flight::Ref<VideoResourceUrl>>{};
    check(!flight::video::select_video_resource_url(host, nothing).has_value(),
          "an empty source list selects nothing");
  }

  {
    // An explicit EMPTY type is AUTHORITATIVE: the URL behind it is never read. The URL here would infer
    // video/webm, which this backend plays, so a source probed through its URL would be selected -- and it
    // must not be, which is the only way to observe "the URL is not read" on a surface where a member read
    // cannot throw.
    int probes = 0;
    auto host = flight::make_ref<HostVideoCapability>();
    host->can_play_type = [&probes](String) {
      ++probes;
      return true;
    };
    flight::Array<flight::Ref<VideoResourceUrl>> sources = flight::Array<flight::Ref<VideoResourceUrl>>{};
    sources.push(source_url("clip.webm", ""));
    check(!flight::video::select_video_resource_url(host, sources).has_value(),
          "an explicit empty type is authoritative and selects nothing");
    check(probes == 0, "and the backend is never asked about it");
  }

  {
    // A source nothing can name costs NO probe, and a later playable one is still reached.
    int probes = 0;
    auto host = flight::make_ref<HostVideoCapability>();
    host->can_play_type = [&probes](String) {
      ++probes;
      return true;
    };
    flight::Array<flight::Ref<VideoResourceUrl>> sources = flight::Array<flight::Ref<VideoResourceUrl>>{};
    sources.push(source_url("clip.avi"));
    sources.push(source_url("clip.ogv"));
    const auto selected = flight::video::select_video_resource_url(host, sources);
    check(selected.has_value() && selected.value()->url == String("clip.ogv"),
          "an unnameable source is skipped and the next playable source is selected");
    check(probes == 1, "and the unnameable source cost no probe");
  }

  {
    // A backend that throws for one source does not end the search: the exception is a refusal for THAT
    // source, and the next one is still tried.
    int calls = 0;
    auto host = flight::make_ref<HostVideoCapability>();
    host->can_play_type = [&calls](String) {
      ++calls;
      if (calls == 1) throw std::runtime_error("unavailable");
      return true;
    };
    flight::Array<flight::Ref<VideoResourceUrl>> sources = flight::Array<flight::Ref<VideoResourceUrl>>{};
    sources.push(source_url("first.mp4"));
    sources.push(source_url("second.webm"));
    // The escape is caught here rather than left to abort the process: an exception that leaks out of the
    // search is exactly the failure this case names, and a crash would hide every assertion after it.
    std::optional<flight::Ref<VideoResourceUrl>> selected;
    bool escaped = false;
    try {
      selected = flight::video::select_video_resource_url(host, sources);
    } catch (...) {
      escaped = true;
    }
    check(!escaped, "a backend exception does not escape the search");
    check(selected.has_value() && selected.value()->url == String("second.webm"),
          "a backend exception is a refusal for that source, not the end of the search");
    check(calls == 2, "and the next source was tried after it");
  }

  {
    // Teardown ORDER. The source is explicit about why it matters -- the element must let go of its src
    // before the URL behind it is revoked -- and the counts asserted in the blocks above cannot see it: an
    // implementation that revoked first and released second passes every one of them.
    std::vector<std::string> order;
    const auto element = element_value();
    auto host = flight::make_ref<HostVideoCapability>();
    host->release_element = [&order](flight::types::HostImageSource) { order.push_back("release"); };
    host->revoke_object_url = [&order](String) { order.push_back("revoke"); };

    auto owned = flight::video::create_video_resource(element, String("blob:ordered"), true);
    flight::video::destroy_video_resource(host, owned);
    check(order == std::vector<std::string>{"release", "revoke"},
          "destroy releases the element BEFORE revoking the URL behind it");

    order.clear();
    auto disposed = flight::video::create_video_resource(element, String("blob:ordered"), false);
    flight::video::dispose_video_resource(host, disposed);
    check(order == std::vector<std::string>{"release", "revoke"},
          "and dispose holds the same order for a borrowed element");
  }

  {
    // dispose on a resource that never had an element, and one that owns no object URL.
    int releases = 0;
    int revokes = 0;
    auto host = flight::make_ref<HostVideoCapability>();
    host->release_element = [&releases](flight::types::HostImageSource) { ++releases; };
    host->revoke_object_url = [&revokes](String) { ++revokes; };

    const auto never_had = flight::video::create_video_resource();
    const auto no_url = flight::video::create_video_resource(element_value());
    // Both of these teardown a resource carrying no object URL. Upstream returns early for exactly that
    // case, so an implementation that reaches for the absent URL throws instead -- caught here so it is a
    // recorded failure rather than an abort that hides the assertions after it.
    bool threw = false;
    try {
      flight::video::dispose_video_resource(host, never_had);
    } catch (...) {
      threw = true;
    }
    check(!threw, "disposing a resource that never had an element does not throw");
    check(releases == 0, "and it releases nothing");

    try {
      flight::video::dispose_video_resource(host, no_url);
    } catch (...) {
      threw = true;
    }
    check(!threw, "disposing a resource with no object URL does not throw");
    check(releases == 1, "a present element is released");
    check(revokes == 0, "and a resource owning no object URL revokes nothing");
  }

  {
    // The reads that answer 0 for an ABSENT hook, which is a different host from one whose hook answers 0.
    const auto resource = flight::video::create_video_resource(element_value());
    const auto bare = flight::make_ref<HostVideoCapability>();
    check(flight::video::get_video_resource_duration(bare, resource) == 0.0,
          "duration is 0 when the host carries no duration hook");
    check(flight::video::get_video_resource_height(bare, resource) == 0.0,
          "height is 0 when the host carries no height hook");
    check(flight::video::get_video_resource_width(bare, resource) == 0.0,
          "width is 0 when the host carries no width hook");
  }

  {
    // Exactly ONE dimension reported zero, on either side.
    const auto resource = flight::video::create_video_resource(element_value());
    auto wide = flight::make_ref<HostVideoCapability>();
    wide->get_width = [](flight::types::HostImageSource) { return 640.0; };
    wide->get_height = [](flight::types::HostImageSource) { return 0.0; };
    auto tall = flight::make_ref<HostVideoCapability>();
    tall->get_width = [](flight::types::HostImageSource) { return 0.0; };
    tall->get_height = [](flight::types::HostImageSource) { return 480.0; };
    check(flight::video::is_video_resource_empty(wide, resource),
          "a resource with a width but no height is empty");
    check(flight::video::is_video_resource_empty(tall, resource),
          "and one with a height but no width is empty too");
  }

  {
    // initializeVideoResource is a required export and is the construction initializer createVideoResource
    // calls -- so it must build the same resource when driven directly.
    auto construction = flight::entity::allocate_entity<flight::Ref<flight::types::VideoResource>>();
    flight::video::initialize_video_resource(construction, element_value(),
                                             std::optional<String>{String("blob:init")}, true);
    const auto built = flight::entity::finish_entity<flight::Ref<flight::types::VideoResource>>(construction);
    check(built->element.has_value() && built->object_url.has_value() &&
              built->object_url.value() == String("blob:init") && built->owns_element,
          "initializeVideoResource is the construction initializer and sets all three members");
  }

  if (failures == 0) std::cout << "generated video: all checks passed\n";
  return failures == 0 ? 0 : 1;
}

int main() {
  try {
    return run_checks();
  } catch (const std::exception& error) {
    std::cerr << "FAIL: an unexpected exception escaped: " << error.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "FAIL: an unexpected non-standard exception escaped\n";
    return 1;
  }
}
