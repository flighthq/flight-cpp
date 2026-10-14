// Behavioural parity for @flighthq/audio, driven by the upstream Vitest fixtures in
// .dependencies/flight/packages/audio/src/.
//
// The package is `assisted` at a627849 and is the only shippable package in its scope: 8/8 modules
// emitted and 8/8 headers compiled. Two of its headers are supplied by hand-written overrides
// (audio_resource_from.hpp, audio_resource_reference.hpp), so the sections below that exercise those
// modules pin the OVERRIDE rather than the emitter's output -- which is the point of pinning them.
//
// This file covers the three modules whose whole surface is reachable without a host: audioResource,
// audioDecodeSlot and audioFormat. The remaining upstream fixtures need more than a pure call --
// decodeAudioResourceBytes is a Promise/AbortSignal task, and audioResourceFrom/audioResourceReference
// pull a decoding host -- and are covered separately.
//
// Host seams are always SUPPLIED by the test, never faked in the runtime: the codec capability below is
// a plain object the fixture hands in, exactly as the upstream fixtures do.

#include <flight/audio/audio_decode_slot.hpp>
#include <flight/audio/audio_format.hpp>
#include <flight/abort.hpp>
// RE-AUDIT FINDINGS (this suite is otherwise unchanged).
//
// REACHABILITY: a mechanical export map put 6 exports at zero call sites, and tracing the MODULE showed all
// six are called internally (create_audio_resource_failure, initialize_audio_resource,
// initialize_embedded_audio_resource_reference, initialize_external_audio_resource_reference,
// resolve_audio_decode_slot, select_audio_resource_source). So NO emitted export is uncovered -- the trace
// turned six apparent gaps into zero. The map ran with a positive control (120 flight::audio:: call sites).
//
// NOT AFFECTED BY THE LOG DEFECT: `@flighthq/log`'s retained loggers call an omitted `merge_span_fields`
// that makes logging THROW at run time on this profile, which makes any log-observed behaviour unobservable.
// This suite has NO log dependence at all -- audio's headers never include or call `flight::log` -- so no
// assertion here is affected. That is stated as a measured absence rather than assumed.
//
// ONE SURVIVOR, KILLED: the upstream `findAudioResourceReferenceByName` case looks up only the SECOND entry,
// so a scan starting at index 1 satisfied it -- and a mutation that skipped the first entry SURVIVED the
// whole suite. Finding the first entry is the same contract and is now pinned; the mutation is re-run
// against the fix and caught.
//
// The two override modules' repaired state writes were mutated directly (clearing the recorded failure, and
// inverting the failed-state test) and both are caught, which is what makes the earlier repair load-bearing
// rather than merely present.
#include <flight/audio/audio_resource.hpp>
#include <flight/audio/audio_resource_from.hpp>
#include <flight/array_buffer.hpp>
#include <flight/blob.hpp>
#include <flight/types/net.hpp>
#include <flight/audio/audio_resource_reference.hpp>
#include <flight/audio/decode_audio_resource_bytes.hpp>
#include <flight/types/host_audio_codec.hpp>
#include <flight/types/host_audio_decode.hpp>
#include <flight/types/audio_resource.hpp>
#include <flight/types/audio_resource_reference.hpp>

#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double got, double want, const char* message) {
  if (std::abs(got - want) < 1e-9) return;
  std::cerr << "FAIL: " << message << "\n  want " << want << "\n  got  " << got << '\n';
  ++failures;
}

// The upstream fixture's createMockAudioBuffer: 2 channels x 100 frames at 44100Hz.
flight::AudioBuffer mock_buffer(double length = 100.0, double channels = 2.0, double rate = 44100.0) {
  return flight::AudioBuffer(flight::AudioBufferOptions{
      .length = length, .number_of_channels = channels, .sample_rate = rate});
}

flight::Ref<flight::types::AudioResource> resource_with(
    std::optional<flight::AudioBuffer> buffer = std::nullopt) {
  return flight::audio::create_audio_resource(buffer);
}

// A distinct slot object per format, so an assertion about WHICH slot answered cannot be satisfied by
// any other -- the upstream fixture's namedSlots().
struct NamedSlots {
  flight::Ref<flight::types::HostAudioDecodeFormatCapability> aac;
  flight::Ref<flight::types::HostAudioDecodeFormatCapability> flac;
  flight::Ref<flight::types::HostAudioDecodeFormatCapability> mp3;
  flight::Ref<flight::types::HostAudioDecodeFormatCapability> mp4;
  flight::Ref<flight::types::HostAudioDecodeFormatCapability> ogg;
  flight::Ref<flight::types::HostAudioDecodeFormatCapability> wav;
  flight::Ref<flight::types::HostAudioDecodeFormatCapability> webm;

  flight::Ref<flight::types::HostAudioDecodeCapabilities> capabilities() const {
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    capabilities->aac = aac;
    capabilities->flac = flac;
    capabilities->mp3 = mp3;
    capabilities->mp4 = mp4;
    capabilities->ogg = ogg;
    capabilities->wav = wav;
    capabilities->webm = webm;
    return capabilities;
  }
};

NamedSlots named_slots() {
  return NamedSlots{flight::make_ref<flight::types::HostAudioDecodeFormatCapability>(),
                    flight::make_ref<flight::types::HostAudioDecodeFormatCapability>(),
                    flight::make_ref<flight::types::HostAudioDecodeFormatCapability>(),
                    flight::make_ref<flight::types::HostAudioDecodeFormatCapability>(),
                    flight::make_ref<flight::types::HostAudioDecodeFormatCapability>(),
                    flight::make_ref<flight::types::HostAudioDecodeFormatCapability>(),
                    flight::make_ref<flight::types::HostAudioDecodeFormatCapability>()};
}

bool slot_is(const std::optional<flight::Ref<flight::types::HostAudioDecodeFormatCapability>>& got,
             const flight::Ref<flight::types::HostAudioDecodeFormatCapability>& want) {
  return got.has_value() && got.value() == want;
}


// The host net seam, supplied by the fixture exactly as the upstream fixtures supply theirs: a real
// HostNetCapability whose sendNetRequest resolves the response the test described. No production
// backend is invented -- an unresolved request is simply never returned.
struct MockNet {
  int requests = 0;
  flight::String last_method;
  flight::String last_url;
  flight::String last_response_type;
  std::function<std::optional<flight::Ref<flight::types::NetResponse>>()> respond;
};

flight::Ref<flight::types::HostNetCapability> mock_net(MockNet& state) {
  const auto net = flight::make_ref<flight::types::HostNetCapability>();
  net->send_net_request = [&state](flight::StructuralRef<
                                       flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::NetRequest>>>>,
                                   auto) {
    state.requests += 1;
    return flight::Task<flight::Ref<flight::types::NetResponse>>::resolve(
        state.respond ? state.respond().value_or(nullptr) : nullptr);
  };
  return net;
}

// A NetResponse carrying an arraybuffer body and a content-type header.
flight::Ref<flight::types::NetResponse> net_response(double status, bool ok, const char* content_type,
                                                     std::optional<flight::ArrayBuffer> body) {
  const auto response = flight::make_ref<flight::types::NetResponse>();
  response->status = status;
  response->ok = ok;
  response->status_text = ok ? flight::String("OK") : flight::String("Not Found");
  response->headers = flight::Record<flight::String, flight::String>{};
  if (content_type != nullptr) response->headers.set(flight::String("content-type"), flight::String(content_type));
  if (body.has_value()) {
    response->body = flight::types::NetResponseBody{
        std::variant<flight::ArrayBuffer, flight::Blob, flight::String, flight::Any>{
            std::in_place_type<flight::ArrayBuffer>, body.value()}};
  }
  response->url = flight::String("sound.mp3");
  return response;
}

// A decode slot that counts its calls and answers with one buffer.
flight::Ref<flight::types::HostAudioDecodeFormatCapability> decoding_slot(
    std::optional<flight::AudioBuffer> buffer, int* calls = nullptr) {
  const auto slot = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
  slot->decode = [buffer, calls](flight::Uint8Array, flight::AbortSignal) {
    if (calls != nullptr) *calls += 1;
    return flight::Task<std::optional<flight::AudioBuffer>>::resolve(buffer);
  };
  return slot;
}

}  // namespace

int main() {
  // ---- audioResource.test.ts ---------------------------------------------------------------------

  {
    // A clone is a NEW resource over the SAME buffer: the identity is independent, the decoded PCM is
    // shared by reference. That pair is the contract -- duplicating the samples here would silently
    // give two playback subsystems two different buffers.
    const auto buffer = mock_buffer();
    const auto resource = resource_with(buffer);
    const auto copy = flight::audio::clone_audio_resource(resource);
    check(copy != resource, "resource: a clone is a distinct resource identity");
    check(copy->buffer.has_value() && copy->buffer.value().length == buffer.length,
          "resource: a clone carries the same buffer");

    // Cloning a resource with no buffer copies the absence rather than inventing one.
    check(!flight::audio::clone_audio_resource(resource_with())->buffer.has_value(),
          "resource: cloning a bufferless resource copies the absence");
  }

  {
    check(!resource_with()->buffer.has_value(), "resource: a resource with no buffer has none");
    check(resource_with(mock_buffer())->buffer.has_value(), "resource: a resource wraps the buffer");
  }

  {
    // Disposing releases the reference so the buffer can be collected; it does not free anything
    // native, because an AudioBuffer is plain GC-managed memory.
    const auto resource = resource_with(mock_buffer());
    check(flight::audio::has_audio_resource_buffer(resource), "resource: a wrapped buffer is present");
    flight::audio::dispose_audio_resource(resource);
    check(!resource->buffer.has_value(), "resource: disposing releases the buffer reference");
  }

  {
    // Byte size is channels x length x 4 (Float32 samples), and 0 without a buffer.
    check_close(flight::audio::get_audio_resource_byte_size(resource_with()), 0.0,
                "resource: a bufferless resource has no byte size");
    check_close(flight::audio::get_audio_resource_byte_size(resource_with(mock_buffer())), 2.0 * 100.0 * 4.0,
                "resource: byte size is channels x length x 4");
  }

  {
    check_close(flight::audio::get_audio_resource_channel_count(resource_with()), 0.0,
                "resource: a bufferless resource has no channels");
    check_close(flight::audio::get_audio_resource_channel_count(resource_with(mock_buffer())), 2.0,
                "resource: the channel count is the buffer's");
  }

  {
    // Channel data is returned by reference and is bounded by the buffer's own channel count.
    const auto resource = resource_with(mock_buffer());
    const auto data = flight::audio::get_audio_resource_channel_data(resource, 1.0);
    check(data.has_value(), "resource: an in-range channel has data");
    check_close(static_cast<double>(data.value().length()), 100.0,
                "resource: the channel data has the buffer's frame count");

    check(!flight::audio::get_audio_resource_channel_data(resource_with(), 0.0).has_value(),
          "resource: a bufferless resource has no channel data");
    check(!flight::audio::get_audio_resource_channel_data(resource, -1.0).has_value(),
          "resource: a negative channel index is out of range");
    check(!flight::audio::get_audio_resource_channel_data(resource, 2.0).has_value(),
          "resource: a channel at the count is out of range");
  }

  {
    check_close(flight::audio::get_audio_resource_duration(resource_with()), 0.0,
                "resource: a bufferless resource has no duration");
    check_close(flight::audio::get_audio_resource_duration(resource_with(mock_buffer())), 100.0 / 44100.0,
                "resource: the duration is the buffer's");
    check_close(flight::audio::get_audio_resource_sample_rate(resource_with()), 0.0,
                "resource: a bufferless resource has no sample rate");
    check_close(flight::audio::get_audio_resource_sample_rate(resource_with(mock_buffer())), 44100.0,
                "resource: the sample rate is the buffer's");
  }

  {
    // Empty means "no buffer OR no frames". Only the no-buffer half is reachable here: the runtime's
    // AudioBuffer REFUSES a length of zero ("flight::AudioBuffer length is outside the supported
    // range" -- AudioBufferOptions::validate requires length > 0), so the zero-length cell the upstream
    // fixture reaches with a plain object cast cannot be reached through the public API. Asserting it
    // would mean constructing a buffer the runtime says cannot exist.
    check(flight::audio::is_audio_resource_empty(resource_with()), "resource: no buffer is empty");
    check(!flight::audio::is_audio_resource_empty(resource_with(mock_buffer())),
          "resource: a buffer with frames is not empty");
  }

  // ---- audioDecodeSlot.test.ts -------------------------------------------------------------------

  {
    const auto slots = named_slots();
    const auto capabilities = slots.capabilities();

    // Each standard container reaches its OWN slot: the fixture gives every format a distinct object
    // precisely so an answer from the wrong slot cannot pass.
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/aac")), slots.aac),
          "slot: audio/aac reaches the aac slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/flac")), slots.flac),
          "slot: audio/flac reaches the flac slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/mpeg")), slots.mp3),
          "slot: audio/mpeg reaches the mp3 slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/mp4")), slots.mp4),
          "slot: audio/mp4 reaches the mp4 slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/ogg")), slots.ogg),
          "slot: audio/ogg reaches the ogg slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/wav")), slots.wav),
          "slot: audio/wav reaches the wav slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/webm")), slots.webm),
          "slot: audio/webm reaches the webm slot");
  }

  {
    // One codec under several registered spellings, so the slot set does not grow per synonym.
    const auto slots = named_slots();
    const auto capabilities = slots.capabilities();
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/mp3")), slots.mp3),
          "slot: audio/mp3 is the mp3 slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/x-mpeg")), slots.mp3),
          "slot: audio/x-mpeg is the mp3 slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/x-wav")), slots.wav),
          "slot: audio/x-wav is the wav slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/vnd.wave")), slots.wav),
          "slot: audio/vnd.wave is the wav slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/x-flac")), slots.flac),
          "slot: audio/x-flac is the flac slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/x-m4a")), slots.mp4),
          "slot: audio/x-m4a is the mp4 slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("application/ogg")), slots.ogg),
          "slot: application/ogg is the ogg slot");
  }

  {
    // The slot is chosen by the ESSENCE: parameters describe the stream, not which decoder reads it, and
    // the type arrives from servers with arbitrary casing and spacing.
    const auto slots = named_slots();
    const auto capabilities = slots.capabilities();
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("audio/mp4; codecs=\"mp4a.40.2\"")),
                  slots.mp4),
          "slot: MIME parameters are ignored when choosing the slot");
    check(slot_is(flight::audio::get_audio_decode_slot(capabilities, flight::String("  AUDIO/OGG ; rate=48000")),
                  slots.ogg),
          "slot: casing and spacing are ignored when choosing the slot");
  }

  {
    // A format nothing can read and a format nobody supplied a reader for are deliberately ONE answer:
    // from the decode path's side they lead to the same place.
    const auto slots = named_slots();
    check(!flight::audio::get_audio_decode_slot(slots.capabilities(),
                                                flight::String("audio/vnd.adobe.swf-adpcm"))
               .has_value(),
          "slot: a type outside the standard set has no slot");
    const auto empty = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    check(!flight::audio::get_audio_decode_slot(empty, flight::String("audio/mpeg")).has_value(),
          "slot: a standard type whose slot the host left empty has no slot");
  }

  {
    // hasAudioDecodeSlot is exactly "the slot resolves", for both the filled and unfilled cases.
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    capabilities->ogg = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    check(flight::audio::has_audio_decode_slot(capabilities, flight::String("audio/ogg")),
          "slot: a filled slot reports as present");
    check(!flight::audio::has_audio_decode_slot(capabilities, flight::String("audio/mpeg")),
          "slot: an unfilled standard slot reports as absent");
    check(!flight::audio::has_audio_decode_slot(capabilities, flight::String("audio/vnd.acme.custom")),
          "slot: an unrecognised type reports as absent");
  }

  // ---- audioFormat.test.ts -----------------------------------------------------------------------

  {
    // canPlayAudioType asks the host, and answers false for the empty string without probing it.
    int probes = 0;
    const auto codec = flight::make_ref<flight::types::HostAudioCodecCapability>();
    codec->can_play_type = [&probes](flight::String type) {
      ++probes;
      return type == flight::String("audio/mpeg");
    };
    check(!flight::audio::can_play_audio_type(codec, flight::String("")),
          "format: the empty type is not playable");
    check(probes == 0, "format: the empty type is refused without asking the host");
    check(flight::audio::can_play_audio_type(codec, flight::String("audio/mpeg")),
          "format: a type the host reports playable is playable");
    check(!flight::audio::can_play_audio_type(codec, flight::String("audio/x-unknown")),
          "format: a type the host cannot play is not playable");
  }

  {
    // Container detection reads the bytes: each magic number names its own container, a short or
    // unrecognised header names none.
    check(!flight::audio::detect_audio_mime_type(flight::Uint8Array{0x00, 0x01, 0x02}).has_value(),
          "format: a too-short buffer has no detected type");
    check(!flight::audio::detect_audio_mime_type(flight::Uint8Array{0x00, 0x01, 0x02, 0x03}).has_value(),
          "format: an unrecognised header has no detected type");

    const auto detected = [](flight::Uint8Array bytes) {
      const auto found = flight::audio::detect_audio_mime_type(bytes);
      return found.has_value() ? found.value() : flight::String("");
    };
    check(detected(flight::Uint8Array{0x52, 0x49, 0x46, 0x46, 0, 0, 0, 0, 0x57, 0x41, 0x56, 0x45}) ==
              flight::String("audio/wav"),
          "format: RIFF....WAVE is detected as WAV");
    check(detected(flight::Uint8Array{0x66, 0x4c, 0x61, 0x43}) == flight::String("audio/flac"),
          "format: fLaC is detected as FLAC");
    check(detected(flight::Uint8Array{0x4f, 0x67, 0x67, 0x53}) == flight::String("audio/ogg"),
          "format: OggS is detected as Ogg");
    check(detected(flight::Uint8Array{0x49, 0x44, 0x33, 0x04}) == flight::String("audio/mpeg"),
          "format: an ID3 tag is detected as MPEG");
    check(detected(flight::Uint8Array{0xff, 0xfb, 0x90, 0x00}) == flight::String("audio/mpeg"),
          "format: an MPEG frame sync is detected as MPEG");
    check(detected(flight::Uint8Array{0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70}) ==
              flight::String("audio/mp4"),
          "format: an ftyp box is detected as MP4");
    check(detected(flight::Uint8Array{0x1a, 0x45, 0xdf, 0xa3}) == flight::String("audio/webm"),
          "format: the EBML magic is detected as WebM");
  }

  {
    // The essence strips parameters and lowercases; a parameterless type is returned unchanged.
    check(flight::audio::get_audio_mime_type_essence(
              flight::String("audio/vnd.adobe.swf-adpcm; rate=22050; channels=1")) ==
              flight::String("audio/vnd.adobe.swf-adpcm"),
          "format: the essence strips every parameter");
    check(flight::audio::get_audio_mime_type_essence(flight::String("  AUDIO/MPEG  ")) ==
              flight::String("audio/mpeg"),
          "format: the essence lowercases and trims");
    check(flight::audio::get_audio_mime_type_essence(flight::String("audio/mpeg")) ==
              flight::String("audio/mpeg"),
          "format: a parameterless type is unchanged");
    check(flight::audio::get_audio_mime_type_essence(flight::String("")) == flight::String(""),
          "format: the empty type has an empty essence");
  }

  {
    // Parameters are read case-insensitively, a quoted value is unwrapped, and the type itself is never
    // mistaken for one of its own parameters.
    const flight::String mime("audio/vnd.adobe.swf-pcm; rate=22050; channels=2; bits=16");
    const auto parameter = [&mime](const char* name) {
      const auto found = flight::audio::get_audio_mime_type_parameter(mime, flight::String(name));
      return found.has_value() ? found.value() : flight::String("");
    };
    check(parameter("rate") == flight::String("22050"), "format: a rate parameter is read");
    check(parameter("channels") == flight::String("2"), "format: a channels parameter is read");
    check(parameter("BITS") == flight::String("16"),
          "format: a parameter name is matched case-insensitively");

    const auto quoted =
        flight::audio::get_audio_mime_type_parameter(flight::String("audio/x-thing; codec=\"a;b\""),
                                                     flight::String("codec"));
    check(quoted.has_value() && quoted.value() == flight::String("a;b"),
          "format: a quoted parameter value is unwrapped");

    check(!flight::audio::get_audio_mime_type_parameter(flight::String("audio/mpeg"), flight::String("rate"))
               .has_value(),
          "format: a parameter the type does not carry is absent");
    check(!flight::audio::get_audio_mime_type_parameter(flight::String("audio/mpeg; rate=1"),
                                                        flight::String("channels"))
               .has_value(),
          "format: another parameter's name does not match");
    check(!flight::audio::get_audio_mime_type_parameter(flight::String("audio/mpeg"),
                                                        flight::String("audio/mpeg"))
               .has_value(),
          "format: the type itself is never read as a parameter");
  }

  {
    // Extension inference, including the query string a cache-buster adds and the no-extension case.
    const auto inferred = [](const char* url) {
      const auto found = flight::audio::infer_audio_mime_type(flight::String(url));
      return found.has_value() ? found.value() : flight::String("");
    };
    check(inferred("track.mp3") == flight::String("audio/mpeg"), "format: .mp3 infers audio/mpeg");
    check(inferred("track.ogg") == flight::String("audio/ogg"), "format: .ogg infers audio/ogg");
    check(inferred("track.wav") == flight::String("audio/wav"), "format: .wav infers audio/wav");
    check(inferred("track.aac") == flight::String("audio/aac"), "format: .aac infers audio/aac");
    check(inferred("track.flac") == flight::String("audio/flac"), "format: .flac infers audio/flac");
    check(inferred("track.webm") == flight::String("audio/webm"), "format: .webm infers audio/webm");
    check(inferred("track.m4a") == flight::String("audio/mp4"), "format: .m4a infers audio/mp4");
    check(inferred("track.mid") == flight::String(""), "format: an unknown extension infers nothing");
    check(inferred("track.mp3?cb=1") == flight::String("audio/mpeg"),
          "format: a query string is stripped before matching");
    check(inferred("audio") == flight::String(""), "format: a URL with no extension infers nothing");
  }


  // ---- decodeAudioResourceBytes.test.ts ----------------------------------------------------------
  //
  // This is the package's one asynchronous surface, and it is exercised through the REAL runtime
  // primitives -- flight::Task and flight::AbortSignal -- rather than a stand-in. The emitted body is a
  // coroutine (co_await/co_return), so settlement, rejection and cancellation below are the runtime's
  // own, not an imitation of them.

  {
    const auto slot_with = [](std::optional<flight::AudioBuffer> buffer, int* calls) {
      const auto slot = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
      slot->decode = [buffer, calls](flight::Uint8Array, flight::AbortSignal) {
        if (calls != nullptr) *calls += 1;
        return flight::Task<std::optional<flight::AudioBuffer>>::resolve(buffer);
      };
      return slot;
    };

    // A parameterized MIME type reaches the decoder the caller registered for the BARE type, and the
    // decoder is handed the bytes, the FULL declared type and the signal.
    {
      const auto buffer = mock_buffer();
      flight::String seen_mime;
      std::size_t seen_bytes = 0;
      auto registry = flight::types::AudioDecoderRegistry{};
      registry.set(flight::String("audio/vnd.acme.custom"),
                   flight::types::AudioDecoder(
                       [&buffer, &seen_mime, &seen_bytes](flight::Uint8Array bytes, flight::String mime,
                                                          flight::AbortSignal) {
                         seen_mime = mime;
                         seen_bytes = bytes.length();
                         return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
                             flight::audio::create_audio_resource(buffer));
                       }));
      flight::AbortController controller;
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      const auto decoded = flight::audio::decode_audio_resource_bytes(
                              capabilities, flight::Uint8Array{0x01, 0x02},
                              flight::String("audio/vnd.acme.custom; rate=22050"), controller.signal,
                              registry)
                              .get();
      check(decoded.has_value(), "decode: a caller decoder resolves a resource");
      check(decoded.value()->buffer.has_value() &&
                decoded.value()->buffer.value().length == buffer.length,
            "decode: the decoded resource wraps the buffer the decoder returned");
      check(seen_mime == flight::String("audio/vnd.acme.custom; rate=22050"),
            "decode: the decoder receives the FULL declared type, not the essence");
      check(seen_bytes == 2, "decode: the decoder receives the caller's bytes");
    }

    // An expected miss from the caller's decoder is returned unchanged rather than being retried against
    // the host.
    {
      auto registry = flight::types::AudioDecoderRegistry{};
      registry.set(flight::String("audio/vnd.acme.custom"),
                   flight::types::AudioDecoder(
                       [](flight::Uint8Array, flight::String, flight::AbortSignal) {
                         return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
                             std::nullopt);
                       }));
      flight::AbortController controller;
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      check(!flight::audio::decode_audio_resource_bytes(capabilities, flight::Uint8Array{0x01},
                                                        flight::String("audio/vnd.acme.custom"),
                                                        controller.signal, registry)
                 .get()
                 .has_value(),
            "decode: a caller decoder's miss stays a miss");
    }

    // Neither route covers the type: nothing is invented.
    {
      flight::AbortController controller;
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      check(!flight::audio::decode_audio_resource_bytes(capabilities, flight::Uint8Array{0x01},
                                                        flight::String("audio/mpeg"), controller.signal)
                 .get()
                 .has_value(),
            "decode: an uncovered type resolves nothing");
    }

    // A standard container goes through the host slot registered for it.
    {
      const auto buffer = mock_buffer();
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      capabilities->mp3 = slot_with(buffer, nullptr);
      flight::AbortController controller;
      const auto decoded = flight::audio::decode_audio_resource_bytes(
                              capabilities, flight::Uint8Array{0x01}, flight::String("audio/mpeg"),
                              controller.signal)
                              .get();
      check(decoded.has_value() && decoded.value()->buffer.value().length == buffer.length,
            "decode: a standard container resolves through its host slot");
    }

    // The caller's decoder WINS over a host slot claiming the same type: their decoder exists because the
    // platform cannot read that format, so the host must not be tried first.
    {
      const auto host_buffer = mock_buffer(100.0);
      const auto caller_buffer = mock_buffer(200.0);
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      capabilities->mp3 = slot_with(host_buffer, nullptr);
      auto registry = flight::types::AudioDecoderRegistry{};
      registry.set(flight::String("audio/mpeg"),
                   flight::types::AudioDecoder(
                       [&caller_buffer](flight::Uint8Array, flight::String, flight::AbortSignal) {
                         return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
                             flight::audio::create_audio_resource(caller_buffer));
                       }));
      flight::AbortController controller;
      const auto decoded = flight::audio::decode_audio_resource_bytes(
                              capabilities, flight::Uint8Array{0x01}, flight::String("audio/mpeg"),
                              controller.signal, registry)
                              .get();
      check(decoded.has_value() && decoded.value()->buffer.value().length == caller_buffer.length,
            "decode: the caller's decoder is preferred over a host slot for the same type");
    }

    // With no declared type, the container is sniffed from the bytes -- which is what keeps a payload
    // served without a content-type decodable.
    {
      const auto buffer = mock_buffer();
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      capabilities->wav = slot_with(buffer, nullptr);
      flight::AbortController controller;
      const auto decoded = flight::audio::decode_audio_resource_bytes(
                              capabilities,
                              flight::Uint8Array{0x52, 0x49, 0x46, 0x46, 0, 0, 0, 0, 0x57, 0x41, 0x56, 0x45},
                              std::nullopt, controller.signal)
                              .get();
      check(decoded.has_value() && decoded.value()->buffer.value().length == buffer.length,
            "decode: an undeclared type is sniffed from the container magic");
    }

    // ...and the same when the declared type names no standard format.
    {
      const auto buffer = mock_buffer();
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      capabilities->wav = slot_with(buffer, nullptr);
      flight::AbortController controller;
      const auto decoded = flight::audio::decode_audio_resource_bytes(
                              capabilities,
                              flight::Uint8Array{0x52, 0x49, 0x46, 0x46, 0, 0, 0, 0, 0x57, 0x41, 0x56, 0x45},
                              flight::String("application/octet-stream"), controller.signal)
                              .get();
      check(decoded.has_value(), "decode: a non-standard declared type still sniffs the container");
    }

    // A host slot that reports an expected miss resolves nothing.
    {
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      capabilities->mp3 = slot_with(std::nullopt, nullptr);
      flight::AbortController controller;
      check(!flight::audio::decode_audio_resource_bytes(capabilities, flight::Uint8Array{0x01},
                                                        flight::String("audio/mpeg"), controller.signal)
                 .get()
                 .has_value(),
            "decode: a host slot's miss resolves nothing");
    }

    // CANCELLATION, both halves. An abort that is already set refuses BEFORE any decoding, and the host is
    // never asked.
    {
      int calls = 0;
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      capabilities->mp3 = slot_with(mock_buffer(), &calls);
      flight::AbortController controller;
      controller.abort();
      bool rejected = false;
      try {
        static_cast<void>(flight::audio::decode_audio_resource_bytes(
            capabilities, flight::Uint8Array{0x01}, flight::String("audio/mpeg"), controller.signal)
                              .get());
      } catch (...) {
        rejected = true;
      }
      check(rejected, "decode: an already-aborted signal refuses the operation");
      check(calls == 0, "decode: an already-aborted signal never reaches the decoder");
    }

    // An abort that lands MID-decode suppresses the result: a host decode is usually uncancellable once
    // begun, so it cannot be trusted to have stopped anything.
    {
      flight::AbortController controller;
      const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
      const auto slot = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
      const auto buffer = mock_buffer();
      slot->decode = [&controller, &buffer](flight::Uint8Array, flight::AbortSignal) {
        controller.abort();
        return flight::Task<std::optional<flight::AudioBuffer>>::resolve(std::optional<flight::AudioBuffer>{buffer});
      };
      capabilities->mp3 = slot;
      bool rejected = false;
      try {
        static_cast<void>(flight::audio::decode_audio_resource_bytes(
            capabilities, flight::Uint8Array{0x01}, flight::String("audio/mpeg"), controller.signal)
                              .get());
      } catch (...) {
        rejected = true;
      }
      check(rejected,
            "decode: an abort landing mid-decode suppresses the result rather than returning it");
    }
  }


  // ---- audioResourceReference.test.ts ------------------------------------------------------------
  //
  // audio_resource_reference.hpp is a HAND-WRITTEN OVERRIDE, so everything here pins the override. The
  // existing tests/generated_audio_test.cpp already covers createAudioResourceFailure's two branches,
  // and is deliberately not duplicated below.

  {
    const auto no_fetch = flight::types::AudioResourceFetch(nullptr);
    (void)no_fetch;
  }

  {
    // The reference exists BEFORE its samples do, so a cue can hold it while the bytes are undecoded --
    // and it borrows the caller's bytes rather than copying them.
    flight::Uint8Array bytes{1, 2, 3};
    const auto reference = flight::audio::create_embedded_audio_resource_reference(
        bytes, flight::String("audio/mpeg"), flight::String("theme"));
    check(reference->kind == flight::types::audio_resource_reference_kind->embedded,
          "reference: an embedded reference reports the Embedded kind");
    // Borrowing, proved by mutation: writing through the caller's handle after the reference was built
    // is visible through the reference, which a copy would not be.
    check(reference->bytes.length() == 3 && reference->bytes[0] == 1,
          "reference: the reference carries the caller's bytes");
    bytes[0] = 7;
    check(reference->bytes[0] == 7,
          "reference: an embedded reference borrows the caller's bytes rather than copying them");
    check(reference->mime_type == std::optional<flight::String>{flight::String("audio/mpeg")},
          "reference: the declared type is stored");
    check(reference->name == std::optional<flight::String>{flight::String("theme")},
          "reference: the declared name is stored");
    check(!reference->failure.has_value(), "reference: a fresh reference has no failure");
    check(reference->state == flight::types::resource_resolution_state->unresolved,
          "reference: a fresh reference is unresolved");
    check(!reference->resource->buffer.has_value(),
          "reference: the resource exists before its samples do");

    // Absence is copied as absence, not defaulted.
    const auto bare = flight::audio::create_embedded_audio_resource_reference(flight::Uint8Array{0});
    check(!bare->mime_type.has_value() && !bare->name.has_value(),
          "reference: an undeclared type and name stay absent");

    // Each reference owns its OWN resource: sharing one would make every cue resolve into the same place.
    const auto other = flight::audio::create_embedded_audio_resource_reference(flight::Uint8Array{2});
    check(reference->resource != other->resource,
          "reference: each embedded reference has its own resource");
  }

  {
    const auto reference = flight::audio::create_external_audio_resource_reference(
        flight::String("hit.mp3"), flight::String("/sounds"), flight::String("audio/mpeg"),
        flight::String("hit"));
    check(reference->kind == flight::types::audio_resource_reference_kind->external,
          "reference: an external reference reports the External kind");
    check(reference->uri == flight::String("hit.mp3"), "reference: the uri is stored");
    check(reference->base_path == std::optional<flight::String>{flight::String("/sounds")},
          "reference: the base path is stored");
    check(reference->mime_type == std::optional<flight::String>{flight::String("audio/mpeg")},
          "reference: the external type is stored");
    check(!reference->resource->buffer.has_value(),
          "reference: an external resource awaits its fetch");
    check(reference->state == flight::types::resource_resolution_state->unresolved,
          "reference: an external reference starts unresolved");

    const auto bare = flight::audio::create_external_audio_resource_reference(flight::String("hit.mp3"));
    check(!bare->base_path.has_value() && !bare->mime_type.has_value(),
          "reference: an external reference defaults its base path and type to absent");
  }

  {
    // The explanation is a detached summary: a caller cannot reach through it to mutate the reference's
    // own failure, which is the whole reason it is built from a copy.
    const auto unresolved = flight::audio::create_embedded_audio_resource_reference(flight::Uint8Array{1});
    const auto explanation =
        flight::audio::explain_audio_resource_reference_resolution(unresolved);
    check(!explanation->retryable &&
              explanation->kind == flight::types::audio_resource_reference_kind->embedded &&
              explanation->state == flight::types::resource_resolution_state->unresolved &&
              !explanation->failure.has_value(),
          "reference: an unresolved reference explains itself as not retryable");

    const auto failed = flight::audio::create_embedded_audio_resource_reference(flight::Uint8Array{1});
    failed->state = flight::types::resource_resolution_state->failed;
    const auto cause = flight::make_ref<flight::types::AudioResourceFailure>();
    cause->kind = flight::types::audio_resource_failure_kind->error;
    cause->message = flight::String("boom");
    failed->failure = cause;

    const auto failed_explanation = flight::audio::explain_audio_resource_reference_resolution(failed);
    check(failed_explanation->retryable,
          "reference: a failed reference explains itself as retryable");
    check(failed_explanation->failure.has_value() &&
              failed_explanation->failure.value() != failed->failure.value(),
          "reference: the explanation detaches the failure rather than sharing it");
    failed_explanation->failure.value()->message = flight::String("changed");
    check(failed->failure.value()->message == flight::String("boom"),
          "reference: mutating the explanation's failure cannot reach the reference's own");
  }

  {
    const auto intro = flight::audio::create_embedded_audio_resource_reference(
        flight::Uint8Array{1}, flight::String("audio/mpeg"), flight::String("intro"));
    const auto loop = flight::audio::create_embedded_audio_resource_reference(
        flight::Uint8Array{2}, flight::String("audio/mpeg"), flight::String("loop"));
    auto references = flight::Array<flight::types::AudioResourceReference>{};
    references.push(flight::types::AudioResourceReference{intro});
    references.push(flight::types::AudioResourceReference{loop});

    const auto found = flight::audio::find_audio_resource_reference_by_name(references, flight::String("loop"));
    check(found.has_value() && std::get<flight::Ref<flight::types::EmbeddedAudioResourceReference>>(found.value()) ==
                                  loop,
          "reference: a named lookup returns the reference carrying that name");
    // The upstream case looks up only the SECOND entry, so a scan that started at index 1 would satisfy it
    // -- and did: a mutation skipping the first entry SURVIVED the whole suite. "Returns the reference
    // carrying that name" covers the first entry too, so it is pinned here.
    const auto first_found =
        flight::audio::find_audio_resource_reference_by_name(references, flight::String("intro"));
    check(first_found.has_value() &&
              std::get<flight::Ref<flight::types::EmbeddedAudioResourceReference>>(first_found.value()) ==
                  intro,
          "reference: a named lookup finds the FIRST entry too, not only later ones");
    check(!flight::audio::find_audio_resource_reference_by_name(references, flight::String("missing")).has_value(),
          "reference: a name the document does not carry finds nothing");
    check(!flight::audio::find_audio_resource_reference_by_name(
               flight::Array<flight::types::AudioResourceReference>{}, flight::String("anything"))
               .has_value(),
          "reference: an empty document finds nothing");
  }

  {
    // A failed reference can be re-armed; one that never failed is left alone and says so.
    const auto reference = flight::audio::create_embedded_audio_resource_reference(flight::Uint8Array{1});
    reference->state = flight::types::resource_resolution_state->failed;
    const auto cause = flight::make_ref<flight::types::AudioResourceFailure>();
    cause->kind = flight::types::audio_resource_failure_kind->error;
    cause->message = flight::String("boom");
    reference->failure = cause;

    check(flight::audio::reset_failed_audio_resource_reference(reference),
          "reference: resetting a failed reference reports a change");

    check(reference->state == flight::types::resource_resolution_state->unresolved &&
              !reference->failure.has_value(),
          "reference: resetting returns a failed reference to unresolved and clears the failure");

    reference->state = flight::types::resource_resolution_state->resolved;
    check(!flight::audio::reset_failed_audio_resource_reference(reference),
          "reference: resetting a reference that never failed reports no change");
    check(reference->state == flight::types::resource_resolution_state->resolved,
          "reference: a reference that never failed is left untouched");
  }

  {
    // RESOLUTION, through the real Task surface. The decoded samples land in the resource the reference
    // ALREADY HOLDS -- swapping in the decoder's own resource would leave every cue pointing at an empty
    // one.
    const auto decoded_buffer = mock_buffer(100.0);
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    const auto slot = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    slot->decode = [&decoded_buffer](flight::Uint8Array, flight::AbortSignal) {
      return flight::Task<std::optional<flight::AudioBuffer>>::resolve(
          std::optional<flight::AudioBuffer>{decoded_buffer});
    };
    capabilities->mp3 = slot;
    const auto no_fetch = flight::types::AudioResourceFetch(
        [](flight::StructuralRef<
               flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ExternalAudioResourceReference>>>>,
           flight::AbortSignal) {
          return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(std::nullopt);
        });

    const auto reference = flight::audio::create_embedded_audio_resource_reference(
        flight::Uint8Array{1, 2}, flight::String("audio/mpeg"));
    const auto held = reference->resource;
    flight::AbortController controller;
    const auto resolved = flight::audio::resolve_audio_resource_reference(
                              flight::types::AudioResourceReference{reference}, capabilities, no_fetch,
                              controller.signal)
                              .get();
    check(resolved.has_value() && resolved.value() == held,
          "reference: resolution binds into the resource the reference already holds");
    check(held->buffer.has_value() && held->buffer.value().length == decoded_buffer.length,
          "reference: the held resource gains the decoded samples");
    check(reference->state == flight::types::resource_resolution_state->resolved,
          "reference: a resolved reference records the resolved state");
  }

  {
    // Nothing can take the bytes: the reference FAILS rather than throwing, and records why.
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    const auto no_fetch = flight::types::AudioResourceFetch(
        [](flight::StructuralRef<
               flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ExternalAudioResourceReference>>>>,
           flight::AbortSignal) {
          return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(std::nullopt);
        });
    const auto reference = flight::audio::create_embedded_audio_resource_reference(
        flight::Uint8Array{7}, flight::String("audio/vnd.adobe.swf-adpcm"));
    flight::AbortController controller;
    const auto resolved = flight::audio::resolve_audio_resource_reference(
                              flight::types::AudioResourceReference{reference}, capabilities, no_fetch,
                              controller.signal)
                              .get();
    check(!resolved.has_value(), "reference: an unreachable payload resolves nothing, without throwing");
    check(reference->state == flight::types::resource_resolution_state->failed,
          "reference: an unreachable payload fails the reference rather than throwing");
    check(reference->failure.has_value() &&
              reference->failure.value()->kind == flight::types::audio_resource_failure_kind->unavailable &&
              reference->failure.value()->message == flight::String("Audio resource unavailable"),
          "reference: the failure names the unavailable kind and its message");
  }

  {
    // An external reference routes through the fetch seam the caller supplies.
    const auto fetched_buffer = mock_buffer(200.0);
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    int fetches = 0;
    const auto fetch = flight::types::AudioResourceFetch(
        [&fetches, &fetched_buffer](
            flight::StructuralRef<
                flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ExternalAudioResourceReference>>>>,
            flight::AbortSignal) {
          ++fetches;
          return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
              std::optional<flight::Ref<flight::types::AudioResource>>{
                  flight::audio::create_audio_resource(fetched_buffer)});
        });
    const auto reference = flight::audio::create_external_audio_resource_reference(
        flight::String("hit.mp3"), flight::String("/sounds"));
    flight::AbortController controller;
    static_cast<void>(flight::audio::resolve_audio_resource_reference(
                          flight::types::AudioResourceReference{reference}, capabilities, fetch,
                          controller.signal)
                          .get());
    check(fetches == 1, "reference: an external reference goes through the fetch seam exactly once");
    check(reference->resource->buffer.has_value() &&
              reference->resource->buffer.value().length == fetched_buffer.length,
          "reference: the fetched samples land in the reference's own resource");
    check(reference->state == flight::types::resource_resolution_state->resolved,
          "reference: an external reference resolves");
  }

  {
    // An abort is a CANCEL, not a failure: the reference stays requestable instead of being marked failed.
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    const auto no_fetch = flight::types::AudioResourceFetch(
        [](flight::StructuralRef<
               flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ExternalAudioResourceReference>>>>,
           flight::AbortSignal) {
          return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(std::nullopt);
        });
    const auto reference = flight::audio::create_embedded_audio_resource_reference(
        flight::Uint8Array{1}, flight::String("audio/mpeg"));
    flight::AbortController controller;
    controller.abort();
    bool rejected = false;
    try {
      static_cast<void>(flight::audio::resolve_audio_resource_reference(
                            flight::types::AudioResourceReference{reference}, capabilities, no_fetch,
                            controller.signal)
                            .get());
    } catch (...) {
      rejected = true;
    }
    check(rejected, "reference: an aborted resolution reports the abort");
    check(reference->state == flight::types::resource_resolution_state->unresolved &&
              !reference->failure.has_value(),
          "reference: an abort leaves the reference unresolved and unfailed, so it can be retried");
  }

  {
    // IDENTITY AND LIFETIME. The accessors above now hand back references into the POINTEE, so the
    // repair must not have made one reference's state reach another's, and a read must still read. Two
    // independently-created references are driven through the same calls:
    const auto first = flight::audio::create_embedded_audio_resource_reference(
        flight::Uint8Array{1}, flight::String("audio/mpeg"));
    const auto second = flight::audio::create_embedded_audio_resource_reference(
        flight::Uint8Array{2}, flight::String("audio/mpeg"));
    first->state = flight::types::resource_resolution_state->failed;
    second->state = flight::types::resource_resolution_state->failed;

    check(flight::audio::reset_failed_audio_resource_reference(
              flight::types::AudioResourceReference{first}),
          "reference: resetting one failed reference reports a change");
    check(first->state == flight::types::resource_resolution_state->unresolved,
          "reference: the reset reference is the one that changed");
    check(second->state == flight::types::resource_resolution_state->failed,
          "reference: a reset writes ONLY the reference it was given, never another");

    // The variant holds the same object, so a write through it is a write to that object -- which is
    // what makes the accessor sound rather than merely compiling.
    flight::types::AudioResourceReference held{first};
    std::visit([](const auto& value) -> flight::types::ResourceResolutionState& { return value->state; },
               held) = flight::types::resource_resolution_state->failed;
    check(first->state == flight::types::resource_resolution_state->failed,
          "reference: a write through the variant reaches the object the variant holds");

    // And a read taken before a write does not follow it: the accessor returns a reference, not a view
    // that outlives the call in some other way.
    check(flight::audio::find_audio_resource_reference_by_name(
              flight::Array<flight::types::AudioResourceReference>{}, flight::String("x"))
              .has_value() == false,
          "reference: a lookup over an empty document still reads as nothing");
  }

  // ---- audioResourceFrom.test.ts -----------------------------------------------------------------
  //
  // Supplied by the hand-written override audio_resource_from.hpp. The existing
  // tests/generated_audio_test.cpp already covers createAudioResourceFromSamples's two blank cases,
  // so those are not repeated here; the samples-copy path and the selection rules are.

  {
    // Building from samples COPIES them, so the caller's arrays stay independent of the buffer's.
    flight::Float32Array left{0.0, 0.5, 0.25};
    flight::Float32Array right{0.75, -0.5, 0.25};
    flight::Array<flight::Float32Array> channels{left, right};
    const auto resource = flight::audio::create_audio_resource_from_samples(channels, 48000.0);
    check(resource->buffer.has_value(), "samples: a non-empty channel list builds a buffer");
    check_close(resource->buffer.value().number_of_channels, 2.0,
                "samples: the channel count is the list length");
    check_close(resource->buffer.value().length, 3.0, "samples: the frame count is the channel length");
    check_close(resource->buffer.value().sample_rate, 48000.0, "samples: the requested rate is kept");

    const auto copied = resource->buffer.value().get_channel_data(1.0);
    check_close(copied[0], 0.75, "samples: the second channel's first sample round-trips");
    check_close(copied[1], -0.5, "samples: a negative sample round-trips");
    check_close(copied[2], 0.25, "samples: the last sample round-trips");

    // Mutating the caller's array after the fact must NOT reach the buffer -- that is what "copies"
    // means, and it is the difference between this and borrowing.
    right[0] = 100.0;
    check_close(resource->buffer.value().get_channel_data(1.0)[0], 0.75,
                "samples: the buffer copied the samples rather than borrowing the caller's arrays");
  }

  {
    // SELECTION. The gate is canPlayType OR a caller-supplied decoder -- and a host decode slot is
    // deliberately NOT an arm, because a platform-backed host declares all seven slots whatever its
    // element can play, so letting them vote would leave canPlayType with no veto.
    const auto codec = flight::make_ref<flight::types::HostAudioCodecCapability>();
    codec->can_play_type = [](flight::String type) { return type == flight::String("audio/ogg"); };
    const auto source = [](const char* url, std::optional<flight::String> type) {
      const auto entry = flight::make_ref<flight::types::AudioResourceUrl>();
      entry->url = flight::String(url);
      entry->type = type;
      return entry;
    };

    flight::Array<flight::Ref<flight::types::AudioResourceUrl>> sources{source("a.mp3", std::nullopt),
                                                                       source("b.ogg", std::nullopt)};
    const auto picked = flight::audio::select_audio_resource_url(codec, sources, std::nullopt);
    check(picked.has_value() && picked.value() == flight::String("b.ogg"),
          "select: the first source whose INFERRED type is playable wins");

    flight::Array<flight::Ref<flight::types::AudioResourceUrl>> typed{source("stream", flight::String("audio/ogg"))};
    const auto explicit_type = flight::audio::select_audio_resource_url(codec, typed, std::nullopt);
    check(explicit_type.has_value() && explicit_type.value() == flight::String("stream"),
          "select: an explicit type is honoured over the URL extension");

    flight::Array<flight::Ref<flight::types::AudioResourceUrl>> unplayable{source("a.mp3", std::nullopt),
                                                                          source("b.wav", std::nullopt)};
    check(!flight::audio::select_audio_resource_url(codec, unplayable, std::nullopt).has_value(),
          "select: nothing playable selects nothing");

    // The decode arm alone: canPlayType says no to everything, so the source is accepted only because
    // something can decode it.
    const auto refuses = flight::make_ref<flight::types::HostAudioCodecCapability>();
    refuses->can_play_type = [](flight::String) { return false; };
    auto registry = flight::types::AudioDecoderRegistry{};
    registry.set(flight::String("audio/vnd.acme.custom"),
                 flight::types::AudioDecoder(
                     [](flight::Uint8Array, flight::String, flight::AbortSignal) {
                       return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
                           flight::audio::create_audio_resource(std::nullopt));
                     }));
    flight::Array<flight::Ref<flight::types::AudioResourceUrl>> custom{
        source("sound.custom", flight::String("audio/vnd.acme.custom"))};
    const auto by_decoder = flight::audio::select_audio_resource_url(refuses, custom, registry);
    check(by_decoder.has_value() && by_decoder.value() == flight::String("sound.custom"),
          "select: a caller-supplied decoder can carry a source the platform cannot play");

    // ...but a HOST DECODE SLOT is not a selection arm even though the format is standard.
    flight::Array<flight::Ref<flight::types::AudioResourceUrl>> ogg{source("sound.ogg", flight::String("audio/ogg"))};
    check(!flight::audio::select_audio_resource_url(refuses, ogg, std::nullopt).has_value(),
          "select: a host decode slot alone does not make a source playable");
  }

  {
    // LOADING FROM BYTES rejects rather than resolving null -- the loader's contract differs from the
    // raw decode path it is built on, and that difference is the point of the loader.
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    const auto slot = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    const auto buffer = mock_buffer();
    slot->decode = [&buffer](flight::Uint8Array, flight::AbortSignal) {
      return flight::Task<std::optional<flight::AudioBuffer>>::resolve(
          std::optional<flight::AudioBuffer>{buffer});
    };
    capabilities->mp3 = slot;

    flight::Uint8Array caller_bytes{1, 2, 3};
    flight::AbortController controller;
    const auto loaded = flight::audio::load_audio_resource_from_bytes(
                            capabilities, caller_bytes, flight::String("audio/mpeg"), controller.signal)
                            .get();
    check(loaded->buffer.has_value() && loaded->buffer.value().length == buffer.length,
          "loadBytes: bytes decode into a resource");
    check(caller_bytes.length() == 3 && caller_bytes[0] == 1,
          "loadBytes: the loader does not detach the caller's bytes");

    // A caller-supplied decoder is preferred when the MIME type is known.
    const auto caller_buffer = mock_buffer(300.0);
    auto registry = flight::types::AudioDecoderRegistry{};
    registry.set(flight::String("audio/mpeg"),
                 flight::types::AudioDecoder(
                     [&caller_buffer](flight::Uint8Array, flight::String, flight::AbortSignal) {
                       return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
                           flight::audio::create_audio_resource(caller_buffer));
                     }));
    const auto preferred = flight::audio::load_audio_resource_from_bytes(
                               capabilities, caller_bytes, flight::String("audio/mpeg"),
                               controller.signal, registry)
                               .get();
    check(preferred->buffer.value().length == caller_buffer.length,
          "loadBytes: a caller-supplied decoder is preferred when the type is known");

    // An expected miss REJECTS here, where the raw decode path would have resolved null.
    auto missing = flight::types::AudioDecoderRegistry{};
    missing.set(flight::String("audio/mpeg"),
                flight::types::AudioDecoder(
                    [](flight::Uint8Array, flight::String, flight::AbortSignal) {
                      return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
                          std::nullopt);
                    }));
    bool miss_rejected = false;
    try {
      static_cast<void>(flight::audio::load_audio_resource_from_bytes(
                            capabilities, caller_bytes, flight::String("audio/mpeg"), controller.signal,
                            missing)
                            .get());
    } catch (...) {
      miss_rejected = true;
    }
    check(miss_rejected, "loadBytes: a caller decoder's expected miss rejects rather than resolving null");

    // An abort that is already set rejects before the decode is attempted.
    int decodes = 0;
    const auto counting = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    counting->decode = [&decodes, &buffer](flight::Uint8Array, flight::AbortSignal) {
      ++decodes;
      return flight::Task<std::optional<flight::AudioBuffer>>::resolve(std::optional<flight::AudioBuffer>{buffer});
    };
    const auto counting_capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    counting_capabilities->mp3 = counting;
    flight::AbortController aborted;
    aborted.abort();
    bool pre_abort_rejected = false;
    try {
      static_cast<void>(flight::audio::load_audio_resource_from_bytes(
                            counting_capabilities, caller_bytes, flight::String("audio/mpeg"), aborted.signal)
                            .get());
    } catch (...) {
      pre_abort_rejected = true;
    }
    check(pre_abort_rejected, "loadBytes: an already-aborted signal rejects");
    check(decodes == 0, "loadBytes: an already-aborted signal never reaches the decoder");
  }

  // ---- audioResourceFrom.test.ts: the four loaders ----------------------------------------------

  {
    // BASE64. The loader decodes the text to bytes and hands them to the same bytes path, so the slot
    // sees real decoded bytes rather than the base64 text.
    int decodes = 0;
    std::size_t seen_bytes = 0;
    const auto buffer = mock_buffer();
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    const auto base64_slot = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    base64_slot->decode = [&decodes, &seen_bytes, &buffer](flight::Uint8Array bytes, flight::AbortSignal) {
      ++decodes;
      seen_bytes = bytes.length();
      return flight::Task<std::optional<flight::AudioBuffer>>::resolve(std::optional<flight::AudioBuffer>{buffer});
    };
    capabilities->mp3 = base64_slot;
    flight::AbortController controller;
    // "YWJj" is btoa('abc'): three bytes, not the four characters of the encoded text.
    const auto decoded = flight::audio::load_audio_resource_from_base64(
                              capabilities, flight::String("YWJj"), flight::String("audio/mpeg"),
                              controller.signal)
                              .get();
    check(decoded->buffer.has_value() && decoded->buffer.value().length == buffer.length,
          "base64: encoded bytes decode into a resource");
    check(decodes == 1, "base64: the decoder is called once");
    check(seen_bytes == 3,
          "base64: the decoder receives the DECODED bytes, not the encoded text");

    // An abort that lands while the decode is in flight rejects rather than resolving a result.
    flight::AbortController mid;
    const auto aborting = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    aborting->decode = [&mid, &buffer](flight::Uint8Array, flight::AbortSignal) {
      mid.abort();
      return flight::Task<std::optional<flight::AudioBuffer>>::resolve(std::optional<flight::AudioBuffer>{buffer});
    };
    const auto aborting_capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    aborting_capabilities->mp3 = aborting;
    bool rejected = false;
    try {
      static_cast<void>(flight::audio::load_audio_resource_from_base64(
                            aborting_capabilities, flight::String("YWJj"), flight::String("audio/mpeg"),
                            mid.signal)
                            .get());
    } catch (...) {
      rejected = true;
    }
    check(rejected, "base64: an abort landing while the decode is in flight rejects");
  }

  {
    // BLOB. A real flight::Blob is built from parts with a declared type, which is what the loader
    // reads; its bytes go through the same decode path.
    int decodes = 0;
    const auto buffer = mock_buffer();
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    capabilities->wav = decoding_slot(buffer, &decodes);
    flight::Blob blob(std::vector<flight::Uint8Array>{flight::Uint8Array{1, 2, 3, 4}},
                      flight::BlobOptions{.type = flight::String("audio/wav")});
    flight::AbortController controller;
    const auto decoded =
        flight::audio::load_audio_resource_from_blob(capabilities, blob, controller.signal).get();
    check(decoded->buffer.has_value() && decoded->buffer.value().length == buffer.length,
          "blob: a blob's bytes decode into a resource");
    check(decodes == 1, "blob: the declared blob type selects the slot");

    flight::AbortController mid;
    const auto aborting = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    aborting->decode = [&mid, &buffer](flight::Uint8Array, flight::AbortSignal) {
      mid.abort();
      return flight::Task<std::optional<flight::AudioBuffer>>::resolve(std::optional<flight::AudioBuffer>{buffer});
    };
    const auto aborting_capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    aborting_capabilities->wav = aborting;
    bool rejected = false;
    try {
      static_cast<void>(
          flight::audio::load_audio_resource_from_blob(aborting_capabilities, blob, mid.signal).get());
    } catch (...) {
      rejected = true;
    }
    check(rejected, "blob: an abort landing while the decode is in flight rejects");
  }

  {
    // URL. The request goes through the host net seam with the exact shape the loader promises, and the
    // decoded body is what comes back.
    int decodes = 0;
    const auto buffer = mock_buffer();
    MockNet net;
    net.respond = [&buffer] {
      return std::optional<flight::Ref<flight::types::NetResponse>>{
          net_response(200.0, true, "audio/mpeg", flight::ArrayBuffer(8.0))};
    };
    const auto host_net = mock_net(net);
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    capabilities->mp3 = decoding_slot(buffer, &decodes);
    flight::AbortController controller;
    const auto decoded = flight::audio::load_audio_resource_from_url(
                              host_net, capabilities, flight::String("sound.mp3"), controller.signal)
                              .get();
    check(net.requests == 1, "url: loading goes through the host net seam exactly once");
    check(decoded->buffer.has_value() && decoded->buffer.value().length == buffer.length,
          "url: a successful arraybuffer response decodes into a resource");
    check(decodes == 1, "url: the response body reaches the decoder");

    // An error response is REJECTED with its status rather than decoded. Deliberately given a VALID
    // arraybuffer body and a valid content-type: with a null body the separate invalid-body guard would
    // reject it too, and this assertion could then pass without the status check existing at all --
    // which is exactly what a mutation proved before this body was added.
    MockNet failing;
    failing.respond = [] {
      return std::optional<flight::Ref<flight::types::NetResponse>>{
          net_response(404.0, false, "audio/mpeg", flight::ArrayBuffer(8.0))};
    };
    const auto failing_net = mock_net(failing);
    int error_decodes = 0;
    const auto error_capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    error_capabilities->mp3 = decoding_slot(buffer, &error_decodes);
    bool http_rejected = false;
    try {
      static_cast<void>(flight::audio::load_audio_resource_from_url(
                            failing_net, error_capabilities, flight::String("missing.mp3"), controller.signal)
                            .get());
    } catch (...) {
      http_rejected = true;
    }
    check(http_rejected, "url: an error response rejects with its status");
    check(error_decodes == 0, "url: an error response body is never decoded");

    // A 200 with no body is its own rejection: a successful status does not make an absent body
    // decodable, and the guard for it is separate from the status guard.
    MockNet bodiless;
    bodiless.respond = [] {
      return std::optional<flight::Ref<flight::types::NetResponse>>{
          net_response(200.0, true, "audio/mpeg", std::nullopt)};
    };
    const auto bodiless_net = mock_net(bodiless);
    int bodiless_decodes = 0;
    const auto bodiless_capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    bodiless_capabilities->mp3 = decoding_slot(buffer, &bodiless_decodes);
    // The rejection must be the LOADER'S OWN, naming the invalid body. A bare "did it throw" is not
    // enough: with the guard removed the following std::get/.value() still throws, so the assertion
    // would pass without the guard existing -- which a mutation proved before this was tightened.
    bool body_rejected = false;
    try {
      static_cast<void>(flight::audio::load_audio_resource_from_url(
                            bodiless_net, bodiless_capabilities, flight::String("sound.mp3"), controller.signal)
                            .get());
    } catch (const flight::Error& error) {
      const flight::String message = error.message();
      body_rejected = message.index_of(flight::String("invalid body")) != -1;
    } catch (...) {
    }
    check(body_rejected, "url: a successful response with no body is rejected BY THE LOADER, naming it");
    check(bodiless_decodes == 0, "url: a response with no body is never decoded");

    // Cancellation while the decode is in flight.
    MockNet mid;
    mid.respond = [&buffer] {
      return std::optional<flight::Ref<flight::types::NetResponse>>{
          net_response(200.0, true, "audio/mpeg", flight::ArrayBuffer(8.0))};
    };
    const auto mid_net = mock_net(mid);
    flight::AbortController mid_controller;
    const auto aborting = flight::make_ref<flight::types::HostAudioDecodeFormatCapability>();
    aborting->decode = [&mid_controller, &buffer](flight::Uint8Array, flight::AbortSignal) {
      mid_controller.abort();
      return flight::Task<std::optional<flight::AudioBuffer>>::resolve(std::optional<flight::AudioBuffer>{buffer});
    };
    const auto mid_capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    mid_capabilities->mp3 = aborting;
    bool mid_rejected = false;
    try {
      static_cast<void>(flight::audio::load_audio_resource_from_url(
                            mid_net, mid_capabilities, flight::String("sound.mp3"), mid_controller.signal)
                            .get());
    } catch (...) {
      mid_rejected = true;
    }
    check(mid_rejected, "url: an abort landing while the decode is in flight rejects");
  }

  {
    // URLS. Selection first, then the same url path -- so an empty source list resolves the null-buffer
    // resource rather than throwing, and only the SELECTED url is requested.
    const auto codec = flight::make_ref<flight::types::HostAudioCodecCapability>();
    codec->can_play_type = [](flight::String type) { return type == flight::String("audio/ogg"); };
    const auto buffer = mock_buffer();

    MockNet empty_net;
    const auto host_net = mock_net(empty_net);
    const auto capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    capabilities->ogg = decoding_slot(buffer, nullptr);
    flight::AbortController controller;
    const auto none = flight::audio::load_audio_resource_from_urls(
                          host_net, codec, capabilities,
                          flight::Array<flight::Ref<flight::types::AudioResourceUrl>>{}, controller.signal)
                          .get();
    check(!none->buffer.has_value(), "urls: an empty source list resolves the null-buffer resource");
    check(empty_net.requests == 0, "urls: an empty source list requests nothing");

    MockNet net;
    net.respond = [&buffer] {
      return std::optional<flight::Ref<flight::types::NetResponse>>{
          net_response(200.0, true, "audio/ogg", flight::ArrayBuffer(8.0))};
    };
    const auto selecting_net = mock_net(net);
    net.last_url = flight::String("");
    const auto source = [](const char* url) {
      const auto entry = flight::make_ref<flight::types::AudioResourceUrl>();
      entry->url = flight::String(url);
      return entry;
    };
    flight::Array<flight::Ref<flight::types::AudioResourceUrl>> sources{source("sound.mp3"), source("sound.ogg")};
    const auto chosen = flight::audio::load_audio_resource_from_urls(selecting_net, codec, capabilities,
                                                                    sources, controller.signal)
                            .get();
    check(chosen->buffer.has_value(), "urls: the first playable source loads");
    check(net.requests == 1, "urls: only the selected source is requested");

    // The decode arm at the LOADER level: the platform can play nothing, so the source is selected and
    // loaded only because a caller-supplied decoder takes it.
    MockNet custom_net;
    custom_net.respond = [] {
      return std::optional<flight::Ref<flight::types::NetResponse>>{
          net_response(200.0, true, "audio/vnd.acme.custom", flight::ArrayBuffer(8.0))};
    };
    const auto custom_host = mock_net(custom_net);
    const auto refuses = flight::make_ref<flight::types::HostAudioCodecCapability>();
    refuses->can_play_type = [](flight::String) { return false; };
    const auto custom_capabilities = flight::make_ref<flight::types::HostAudioDecodeCapabilities>();
    custom_capabilities->mp3 = decoding_slot(buffer, nullptr);
    auto custom_registry = flight::types::AudioDecoderRegistry{};
    custom_registry.set(flight::String("audio/vnd.acme.custom"),
                        flight::types::AudioDecoder(
                            [&buffer](flight::Uint8Array, flight::String, flight::AbortSignal) {
                              return flight::Task<std::optional<flight::Ref<flight::types::AudioResource>>>::resolve(
                                  flight::audio::create_audio_resource(buffer));
                            }));
    flight::Array<flight::Ref<flight::types::AudioResourceUrl>> only_custom{
        [&] {
          const auto entry = flight::make_ref<flight::types::AudioResourceUrl>();
          entry->url = flight::String("sound.custom");
          entry->type = flight::String("audio/vnd.acme.custom");
          return entry;
        }()};
    const auto by_decoder = flight::audio::load_audio_resource_from_urls(
                                custom_host, refuses, custom_capabilities, only_custom, controller.signal,
                                custom_registry)
                                .get();
    check(by_decoder->buffer.has_value(),
          "urls: a caller-supplied decoder loads a format the platform cannot play");
  }

  // ---- (end of audioResourceFrom) ----------------------------------------------------------------


  return failures == 0 ? 0 : 1;
}
