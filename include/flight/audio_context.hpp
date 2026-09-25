#pragma once

#include <functional>
#include <memory>
#include <utility>

#include <flight/array_buffer.hpp>
#include <flight/audio_buffer.hpp>
#include <flight/error.hpp>
#include <flight/string.hpp>
#include <flight/task.hpp>

namespace flight {

// The decode seam. `decodeAudioData` turns encoded bytes -- mp3, ogg, flac, wav -- into sample
// data, and flight-cpp has no codecs to do it with: SDL3's core audio decodes WAV and nothing
// else, and everything past that is a codec library this runtime deliberately does not depend on.
//
// So the decoder is supplied by the host, exactly like the font-face loader, and an
// `AudioContext` is a carrier for one rather than an implementation of one. A host that has
// codecs installs a real decoder; a host that does not installs nothing, and a decode REJECTS
// rather than handing back silence. Silence is the dangerous answer here: it is indistinguishable
// from a correctly decoded quiet passage, so a missing codec would surface as an audio bug
// somewhere far away instead of as a failed decode at the call site.
using AudioDecoder = std::function<Task<AudioBuffer>(ArrayBuffer)>;

// `AudioContext` as Flight actually uses it.
//
// The surface is ONE METHOD, and that is not an abbreviation: across the whole SDK every use is
// `context: AudioContext | null` and the only member reached is `decodeAudioData`.
// `audioResourceFrom.ts` says so in as many words -- it builds buffers through the `AudioBuffer`
// constructor "not context.createBuffer" -- and the test fixtures spell an entire context as
// `{ decodeAudioData } as unknown as AudioContext`. Declaring the rest of the Web Audio graph
// here would be inventing a contract nothing asks for and then having to keep it true.
class AudioContext final {
 public:
  AudioContext() = default;

  explicit AudioContext(AudioDecoder decoder)
      : state_(std::make_shared<State>(State{std::move(decoder)})) {}

  // Decodes encoded audio bytes. The returned task carries the host decoder's own settlement
  // unchanged, rejection reason included, for the reason `FontFace::load` does: the caller reads
  // that reason, and replacing it with one this runtime invented loses what actually went wrong.
  [[nodiscard]] Task<AudioBuffer> decode_audio_data(ArrayBuffer bytes) const {
    if (!state_ || !state_->decoder) {
      return Task<AudioBuffer>::reject(Error(
          String("flight::AudioContext has no audio decoder installed in this profile")));
    }
    return state_->decoder(std::move(bytes));
  }

  [[nodiscard]] bool has_decoder() const noexcept {
    return state_ != nullptr && static_cast<bool>(state_->decoder);
  }

  // Reference identity, as every non-primitive has in JavaScript: two copies of one context are
  // one context, and two separately constructed ones are not.
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }

  [[nodiscard]] friend bool operator==(const AudioContext& left, const AudioContext& right) noexcept {
    return left.state_ == right.state_;
  }

 private:
  struct State final {
    AudioDecoder decoder;
  };

  std::shared_ptr<State> state_;
};

} // namespace flight
