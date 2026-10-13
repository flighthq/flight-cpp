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
#include <flight/audio/audio_resource.hpp>
#include <flight/types/host_audio_codec.hpp>
#include <flight/types/host_audio_decode.hpp>
#include <flight/types/audio_resource.hpp>

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

  return failures == 0 ? 0 : 1;
}
