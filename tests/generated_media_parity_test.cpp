// Behavioural parity for @flighthq/media, driven by the upstream Vitest fixtures in
// .dependencies/flight/packages/media/src/.
//
// SCOPE, and why it is what it is. @flighthq/media is at `partial`: 5 of 7 modules emit, and two are
// refused at this pin -- audioMixer.ts (contextual C++ union conversion over a Set carrier) and
// videoChannel.ts (a WeakMap value that is a bound external carrier). Both refusals are filed
// (media-audio-mixer-runtime-set-union-conversion.md, media-weakmap-value-representation.md) and
// neither is reachable from the source: naming the collection's element type does not clear either,
// including at the site the message itself prescribes.
//
// THE SCOPE NOTE THAT STOOD HERE WAS WRONG IN TWO PLACES, and a re-audit found it by mutation. It said
// audioMixer, enableAudioMixerGuards and videoChannel "cannot be covered here at all". True of videoChannel
// (its header does not compile); FALSE of the other two. enableAudioMixerGuards emits all three of its
// functions and compiles, and audioMixer emits seven -- including the four bus setters. Seven mutations of
// those functions SURVIVED this suite before the coverage below existed. Reachability per fixture, measured
// rather than assumed:
//
//   mediaChannelSignals   8 cases  reachable (covered)
//   audioChannel         55 cases  reachable (covered, extended below)
//   audioMixer           32 cases  PARTLY reachable: 7 functions emit and their surface is covered below;
//                                  the refused mixer surface is not
//   enableAudioMixer*    3 cases   2 reachable in principle but NOT observable here -- see the guard block
//   videoChannel         29 cases  unreachable: video_channel.hpp does not compile (filed defect)
//
// Everything below drives the emitted code through a mock HostAudioDeviceCapability, which is how the
// upstream fixtures drive it too. No decoder, no browser, no SDL: the host seam is supplied by the
// test and the package never invents one.

#include <flight/audio/audio_resource.hpp>
#include <flight/log/log.hpp>
#include <flight/media/audio_channel.hpp>
#include <flight/media/audio_mixer.hpp>
#include <flight/media/enable_audio_mixer_guards.hpp>
#include <flight/media/media_channel_signals.hpp>
#include <flight/types/audio_resource.hpp>
#include <flight/types/video_resource.hpp>
#include <flight/types/host_audio_device.hpp>
#include <flight/types/media_channel_signals.hpp>

#include <cmath>
#include <functional>
#include <iostream>
#include <map>
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

// The host the upstream fixtures record against: every method is a stub the test can inspect, and the
// ended-callback map is what lets a fixture drive a loop or a completion.
struct MockDevice {
  double device_time = 0.0;
  double next_source = 1.0;
  int create_source_calls = 0;
  int destroy_source_calls = 0;
  int start_source_calls = 0;
  int stop_source_calls = 0;
  std::map<double, std::function<void()>> on_ended;
};

flight::Ref<flight::types::HostAudioDeviceCapability> mock_backend(MockDevice& state) {
  const auto backend = flight::make_ref<flight::types::HostAudioDeviceCapability>();
  backend->create_buffer = [](double, double, double, double, flight::Array<flight::Float32Array>) {
    return 1.0;
  };
  backend->create_device = [](double) { return 1.0; };
  backend->create_source = [&state](double, double) {
    state.create_source_calls += 1;
    return state.next_source++;
  };
  backend->destroy_buffer = [](double) {};
  backend->destroy_device = [](double) {};
  backend->destroy_source = [&state](double source) {
    state.destroy_source_calls += 1;
    state.on_ended.erase(source);
  };
  backend->get_device_time = [&state](double) { return state.device_time; };
  backend->on_source_ended = [&state](double source, std::optional<std::function<void()>> callback) {
    if (callback.has_value()) {
      state.on_ended[source] = callback.value();
    } else {
      state.on_ended.erase(source);
    }
  };
  backend->resume_device = [](double) {};
  backend->set_source_gain = [](double, double) {};
  backend->set_source_pan = [](double, double) {};
  backend->set_source_playback_rate = [](double, double) {};
  backend->start_source = [&state](double, double, double) { state.start_source_calls += 1; };
  backend->stop_source = [&state](double) { state.stop_source_calls += 1; };
  return backend;
}

// The mixer host the audioMixer fixture records against. Every member is present so the emitted bus
// setters have an operation to reach; nothing here is a real mixer graph.
struct MockMixer {
  int gain_nodes = 0;
  int pan_nodes = 0;
  double last_gain = -1.0;
  double last_pan = -99.0;
};

flight::Ref<flight::types::HostAudioMixerCapability> mock_mixer(MockMixer& state) {
  const auto host = flight::make_ref<flight::types::HostAudioMixerCapability>();
  host->create_mixer_graph = [](double, double) { return 1.0; };
  host->destroy_mixer_graph = [](double) {};
  host->create_bus_node = [&state](double, double, double) {
    state.gain_nodes += 1;
    return 1.0;
  };
  host->destroy_bus_node = [](double, double) {};
  host->set_bus_node_gain = [&state](double, double, double value) { state.last_gain = value; };
  host->set_bus_node_pan = [&state](double, double, double value) { state.last_pan = value; };
  host->fade_bus_node_gain = [](double, double, double, double) {};
  host->set_master_gain = [](double, double) {};
  host->route_source_to_bus = [](double, double, double) {};
  host->unroute_source = [](double, double) {};
  host->route_source_to_default = [](double, double) {};
  return host;
}

// A one-second mono buffer. The upstream fixtures' mockBuffer() uses 8Hz because it is a plain object
// literal the runtime never validates; the real AudioBuffer does validate its rate, so this uses the
// same 44100Hz/44100-sample shape the audio-channel fixture declares, which is also one second long.
flight::AudioBuffer mock_buffer() {
  return flight::AudioBuffer(flight::AudioBufferOptions{
      .length = 44100.0, .number_of_channels = 1.0, .sample_rate = 44100.0});
}

flight::Ref<flight::types::AudioChannel> play(const flight::Ref<flight::types::HostAudioDeviceCapability>& backend,
                                              std::optional<double> loops = std::nullopt) {
  std::optional<flight::Ref<flight::types::AudioPlayOptions>> options;
  if (loops.has_value()) {
    auto play_options = flight::make_ref<flight::types::AudioPlayOptions>();
    play_options->loops = loops;
    options = play_options;
  }
  const auto channel = flight::media::play_audio_resource(backend, 1.0,
                                                          flight::audio::create_audio_resource(mock_buffer()),
                                                          options);
  return channel.value_or(nullptr);
}

}  // namespace

int main() {
  // ---- mediaChannelSignals.test.ts ---------------------------------------------------------------
  //
  // The contract is opt-in: nothing is allocated until a caller asks, and the same object comes back
  // on every later call so a caller's listeners are not silently replaced.

  {
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    check(channel != nullptr, "signals: a channel plays against the supplied host");

    // Absent until asked for -- the whole reason the map is consulted rather than the channel.
    check(!flight::media::get_audio_channel_signals(channel).has_value(),
          "signals: an un-opted channel has no signals");

    const auto enabled = flight::media::enable_audio_channel_signals(channel);
    check(enabled != nullptr, "signals: enabling allocates a signals object");
    check(enabled->on_complete != nullptr && enabled->on_loop != nullptr && enabled->on_pause != nullptr &&
              enabled->on_play != nullptr && enabled->on_stop != nullptr,
          "signals: the object carries the channel signals");

    check(flight::media::get_audio_channel_signals(channel).has_value() &&
              flight::media::get_audio_channel_signals(channel).value() == enabled,
          "signals: after enabling, the query returns the enabled object");

    // Identity, not merely equality: a second enable must not replace the object a caller's
    // listeners are attached to.
    check(flight::media::enable_audio_channel_signals(channel) == enabled,
          "signals: enabling twice returns the same object rather than replacing listeners");

    // An un-opted channel runs its whole lifecycle without producing anything.
    MockDevice quiet_state;
    const auto quiet_backend = mock_backend(quiet_state);
    const auto quiet = play(quiet_backend);
    flight::media::pause_audio_channel(quiet);
    flight::media::resume_audio_channel(quiet);
    flight::media::stop_audio_channel(quiet);
    check(!flight::media::get_audio_channel_signals(quiet).has_value(),
          "signals: an un-opted channel records nothing across its whole lifecycle");
  }

  {
    // Video and audio channels are independent: enabling one does not enable the other, and two
    // channels never share one signals object.
    const auto first = flight::make_ref<flight::types::VideoChannel>();
    const auto second = flight::make_ref<flight::types::VideoChannel>();
    // Asked before anything is enabled, so this really is the un-opted state.
    check(!flight::media::get_video_channel_signals(first).has_value(),
          "signals: a video channel has no signals until asked");
    check(flight::media::enable_video_channel_signals(first) !=
              flight::media::enable_video_channel_signals(second),
          "signals: video channels are independent per channel");
    const auto enabled = flight::media::enable_video_channel_signals(first);
    check(enabled != nullptr && flight::media::get_video_channel_signals(first).has_value() &&
              flight::media::get_video_channel_signals(first).value() == enabled,
          "signals: enabling a video channel makes its signals reachable");
  }

  // ---- audioChannel.test.ts: the lifecycle the signals are delivered through ----------------------

  {
    // Playing allocates a source through the host and starts it; the channel records the buffer's
    // duration in milliseconds and the requested loop count.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend, 3.0);
    check(channel != nullptr, "audio: a buffer-backed resource plays");
    check_close(channel->length, 1000.0, "audio: the channel length is the buffer duration in ms");
    check_close(channel->loops, 3.0, "audio: the requested loop count is recorded");
    check(channel->state == flight::String("playing"), "audio: the channel starts playing");
    check(state.create_source_calls == 1, "audio: playing asks the host for one source");
    check(state.start_source_calls > 0, "audio: playing starts the host source");

    // Pause and resume move the state and drive the host.
    flight::media::pause_audio_channel(channel);
    check(channel->state == flight::String("paused"), "audio: pausing marks the channel paused");
    flight::media::resume_audio_channel(channel);
    check(channel->state == flight::String("playing"), "audio: resuming marks the channel playing");

    // Stop tears the source down and returns the channel to the start.
    flight::media::stop_audio_channel(channel);
    check(channel->state == flight::String("stopped"), "audio: stopping marks the channel stopped");
    check_close(channel->current_time, 0.0, "audio: stopping rewinds the channel");
    check(state.destroy_source_calls > 0, "audio: stopping releases the host source");
  }

  {
    // Pausing twice is idempotent: the state is already paused, so the host is not told again.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    flight::media::pause_audio_channel(channel);
    flight::media::pause_audio_channel(channel);
    check(channel->state == flight::String("paused"),
          "audio: pausing an already-paused channel is inert");
  }

  {
    // A channel with no buffer cannot play: the resource carries absence and the seam says so rather
    // than fabricating a silent channel.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto empty = flight::audio::create_audio_resource(std::nullopt);
    check(!flight::media::play_audio_resource(backend, 1.0, empty, std::nullopt).has_value(),
          "audio: a resource with no buffer does not play");
    check(state.create_source_calls == 0, "audio: a resource with no buffer never reaches the host");
  }

  {
    // The source route is a seam the caller supplies and can clear; clearing leaves the channel
    // usable rather than tearing it down.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    std::vector<double> routed;
    flight::media::set_audio_channel_source_route(
        channel, std::optional<std::function<void(double)>>{
                     std::function<void(double)>([&routed](double handle) { routed.push_back(handle); })});
    flight::media::set_audio_channel_source_route(channel, std::nullopt);
    check(channel->state == flight::String("playing"),
          "audio: clearing the source route leaves the channel playing");
    check(routed.size() <= 1, "audio: the route is invoked at most once per source");
  }

  {
    // Gain, pan and playback rate are read back from the channel and clamped to their domains.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    check_close(channel->gain, 1.0, "audio: the default gain is unity");
    check_close(channel->playback_rate, 1.0, "audio: the default playback rate is unity");

    flight::media::set_audio_channel_gain(channel, 0.25);
    check_close(channel->gain, 0.25, "audio: setting the gain stores it");
    check_close(flight::media::set_audio_channel_pan(channel, 5.0), 1.0,
                "audio: pan clamps to the hard-right bound");
    check_close(flight::media::set_audio_channel_pan(channel, -5.0), -1.0,
                "audio: pan clamps to the hard-left bound");
    check_close(channel->pan, -1.0, "audio: the clamped pan is what the channel keeps");

    // Muting is a flag, not a pause: it must not disturb the playing state.
    check(flight::media::set_audio_channel_muted(channel, true), "audio: muting reports the new state");
    check(flight::media::is_audio_channel_muted(channel), "audio: a muted channel reads as muted");
    check(channel->state == flight::String("playing"), "audio: muting does not pause the channel");
  }

  {
    // Seeking is bounded by the channel's own length, the way the upstream fixture asserts.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    check_close(flight::media::set_audio_channel_current_time(channel, 250.0), 250.0,
                "audio: seeking forward stores the requested time");
    check_close(flight::media::get_audio_channel_current_time(channel), 250.0,
                "audio: the current time reads back");
    flight::media::set_audio_channel_current_time(channel, -10.0);
    check(channel->current_time >= 0.0, "audio: seeking before the start clamps to zero");
  }

  {
    // The loop region is inclusive of a valid span and refused when it is not one, and clearing it
    // returns the channel to playing the whole buffer.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    check(flight::media::set_audio_channel_loop_region(channel, 100.0, 400.0),
          "audio: a well-formed loop region is accepted");
    check_close(channel->loop_start, 100.0, "audio: the loop start is stored");
    check_close(channel->loop_end, 400.0, "audio: the loop end is stored");
    check(!flight::media::set_audio_channel_loop_region(channel, 400.0, 100.0),
          "audio: an inverted loop region is refused");
    flight::media::clear_audio_channel_loop_region(channel);
    check_close(channel->loop_start, 0.0, "audio: clearing the loop region drops the start");
    check_close(channel->loop_end, 0.0, "audio: clearing the loop region drops the end");
  }

  {
    // Destroying a channel releases the host source and leaves it stopped and rewound.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    const int destroys_before = state.destroy_source_calls;
    flight::media::destroy_audio_channel(channel);
    check(state.destroy_source_calls > destroys_before,
          "audio: destroying a channel releases its host source");
    check(channel->state == flight::String("stopped"), "audio: a destroyed channel is stopped");
  }

  {
    // The duration accessor reports the channel's own length, and an unplayed channel reports zero.
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    check_close(flight::media::get_audio_channel_duration(channel), channel->length,
                "audio: the duration is the channel length");
    check_close(flight::media::get_audio_channel_duration(flight::make_ref<flight::types::AudioChannel>()),
                0.0, "audio: a channel that never played has no duration");
  }

  // ---- THE ROWS THIS SUITE'S OWN SCOPE NOTE CALLED UNREACHABLE ---------------------------------
  //
  // The note at the top of this file says audioMixer, enableAudioMixerGuards and videoChannel "cannot be
  // covered here at all". That is true of videoChannel (its header does not compile) and FALSE for the
  // other two: enableAudioMixerGuards emits all three of its functions and compiles, and audioMixer emits
  // seven, including the four bus setters. Seven mutations of the functions below SURVIVED the whole suite
  // before this block existed, which is how the overstated note was found.
  {
    MockDevice state;
    const auto backend = mock_backend(state);
    const auto channel = play(backend);
    check(channel != nullptr, "channel: plays against the supplied host");

    check(flight::media::is_audio_channel_playing(channel),
          "isAudioChannelPlaying reports true while the channel is playing");
    flight::media::pause_audio_channel(channel);
    check(!flight::media::is_audio_channel_playing(channel), "and false once it is paused");

    check_close(flight::media::get_audio_channel_source_handle(channel), 0.0,
                "getAudioChannelSourceHandle reports the invalid sentinel once the source is inactive");
  }

  {
    MockDevice state;
    const auto backend = mock_backend(state);
    check(!flight::media::has_audio_channel_fade(backend),
          "hasAudioChannelFade is false when the host declares no fadeSourceGain");

    double faded_source = -1.0;
    double faded_target = -1.0;
    double faded_ms = -1.0;
    backend->fade_source_gain = [&faded_source, &faded_target, &faded_ms](double source, double target,
                                                                         double ms) {
      faded_source = source;
      faded_target = target;
      faded_ms = ms;
    };
    check(flight::media::has_audio_channel_fade(backend),
          "and true once the host declares it");

    const auto channel = play(backend);
    check(channel != nullptr, "channel: plays against the supplied host");
    flight::media::fade_audio_channel_gain(backend, channel, 0.5, 500.0);
    check_close(faded_target, 0.5, "fadeAudioChannelGain hands the host the target gain");
    check_close(faded_ms, 500.0, "in MILLISECONDS, converting nothing itself");
    check_close(channel->gain, 0.5, "and stores the target on the channel");
    check(faded_source >= 1.0, "against the channel's own source handle");
  }

  {
    MockDevice state;
    const auto backend = mock_backend(state);
    double rate_forwarded = -1.0;
    backend->set_source_playback_rate = [&rate_forwarded](double, double value) { rate_forwarded = value; };
    const auto channel = play(backend);

    check_close(flight::media::set_audio_channel_playback_rate(channel, 2.0), 2.0,
                "setAudioChannelPlaybackRate answers the value it was given");
    check_close(channel->playback_rate, 2.0, "stores it on the channel");
    check_close(rate_forwarded, 2.0, "and forwards it to the selected host once");
  }

  {
    MockMixer mixer_state;
    const auto mixer = mock_mixer(mixer_state);
    const auto bus = flight::media::create_audio_bus();
    check(bus != nullptr, "createAudioBus creates a bus");
    check_close(bus->gain, 1.0, "with the documented default gain");
    check(!bus->muted, "unmuted by default");

    check_close(flight::media::set_audio_bus_gain(mixer, bus, 0.25), 0.25,
                "setAudioBusGain answers the value it was given");
    check_close(bus->gain, 0.25, "stores the gain on the bus");

    check(flight::media::set_audio_bus_muted(mixer, bus, true), "setAudioBusMuted answers the value");
    check(bus->muted, "and stores it");

    check_close(flight::media::set_audio_bus_pan(mixer, bus, 9.0), 1.0,
                "setAudioBusPan answers the CLAMPED value");
    check_close(bus->pan, 1.0, "and stores the clamped pan, not the raw one");
    check_close(flight::media::set_audio_bus_pan(mixer, bus, -9.0), -1.0, "clamped at the other end too");
  }

  // ---- THE GUARD PAIR, AND WHY THEY ARE NOT OBSERVABLE HERE ---------------------------------------
  //
  // enableAudioMixerGuards / disableAudioMixerGuards differ in exactly one observable: with the guard
  // installed, a gain change on a bus that belongs to no mixer emits a WARNING through `@flighthq/log`.
  // That warning cannot be observed on this profile, and the reason is precise rather than "the fixture is
  // unreachable": `generated/include/flight/log/log.hpp` is itself PARTIAL -- 23 omissions -- and its
  // retained `log_info`/`log_error`/`log_debug` call `merge_span_fields`, which is NOT GENERATED (the
  // source's `_mergeSpanFields`, log.ts line 773). Calling any of them therefore throws
  // "flight::log::merge_span_fields was not generated for this profile" rather than logging.
  //
  // So the guard's own effect is unobservable here through no fault of the media module: the seam it warns
  // through does not run. Two mutations of these functions (enable never installing the guard, disable never
  // clearing it) survive this suite, and they survive because NOTHING on this profile can distinguish them --
  // an equivalent pair under this boundary, not an oversight. The boundary is a measured defect in the log
  // package, recorded with the other measured sites of the same defect in
  // docs/compiler-requests/retained-declaration-calls-omitted-callee.md.

  return failures == 0 ? 0 : 1;
}
