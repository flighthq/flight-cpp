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
#include <variant>

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

int main() {
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

  if (failures == 0) std::cout << "generated video: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
