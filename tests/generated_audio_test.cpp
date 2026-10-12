// Focused behavior tests for the two re-derived @flighthq/audio overrides.
//
// Both modules shadow a generated header that does not compile, so "it compiles" is the weakest thing
// that can be said about them. These check the behavior each re-derivation is responsible for:
// `createAudioResourceFailure`'s two branches, which the copy recovers with a checked downcast where the
// emitter stringified an erased value; and `createAudioResourceFromSamples`'s length path, which the
// presence patch rewrote.
#include <flight/audio/audio_resource_from.hpp>
#include <flight/audio/audio_resource_reference.hpp>

#include <iostream>
#include <stdexcept>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

}  // namespace

int main() {
  using flight::String;
  using namespace flight::types;

  // A caught Error keeps its own message and its own name -- the source's `cause instanceof Error` branch.
  const flight::RangeError range_error(String("index out of range"));
  const auto from_error = flight::audio::create_audio_resource_failure(static_cast<const std::exception&>(range_error));
  check(from_error->kind == flight::types::audio_resource_failure_kind->error, "an Error cause reports the error kind");
  check(from_error->message == String("index out of range"), "an Error cause keeps the error's message");
  check(from_error->name.has_value() && from_error->name.value() == String("RangeError"),
        "an Error cause keeps the subclass name rather than defaulting it");

  // Anything else reports the text the runtime's own Error constructor documents as the message, and no
  // name -- the source's `String(cause)` branch.
  const std::runtime_error plain("plain failure");
  const auto from_plain = flight::audio::create_audio_resource_failure(plain);
  check(from_plain->kind == flight::types::audio_resource_failure_kind->error, "a non-Error cause reports the error kind");
  check(from_plain->message == String("plain failure"), "a non-Error cause reports `what()` as its message");
  check(!from_plain->name.has_value(), "a non-Error cause carries no name");

  // The samples path: no channels, and a zero-length first channel, both produce a bufferless resource
  // rather than a buffer of length zero.
  const auto empty = flight::audio::create_audio_resource_from_samples(flight::Array<flight::Float32Array>{}, 44100.0);
  check(!empty->buffer.has_value(), "no channels yields a bufferless resource");

  flight::Array<flight::Float32Array> zero_length;
  zero_length.push(flight::Float32Array(0));
  const auto zero = flight::audio::create_audio_resource_from_samples(zero_length, 44100.0);
  check(!zero->buffer.has_value(), "a zero-length first channel yields a bufferless resource");

  // A present first channel sizes the buffer, and every channel is copied into it.
  flight::Array<flight::Float32Array> channels;
  channels.push(flight::Float32Array(4));
  channels.push(flight::Float32Array(4));
  const auto sized = flight::audio::create_audio_resource_from_samples(channels, 44100.0);
  check(sized->buffer.has_value(), "a present first channel yields a buffer");
  if (sized->buffer.has_value()) {
    check(sized->buffer.value().length == 4.0, "the buffer takes the first channel's length");
    check(sized->buffer.value().number_of_channels == 2.0, "the buffer takes the channel count");
    check(sized->buffer.value().sample_rate == 44100.0, "the buffer takes the sample rate");
  }

  if (failures == 0) std::cout << "generated audio: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
