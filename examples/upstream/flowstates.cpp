// Native SDL port of @flighthq/example-flowstates.
// Source: .dependencies/flight/examples/packages/flowstates/src/app.ts

#include "sdl_host.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using flight::examples::Canvas;
using flight::examples::Frame;

enum class State { boot, menu, play, pause, game_over };

[[nodiscard]] std::string_view name(State state) {
  switch (state) {
    case State::boot: return "Boot";
    case State::menu: return "Menu";
    case State::play: return "Play";
    case State::pause: return "Pause";
    case State::game_over: return "GameOver";
  }
  return "?";
}

class FlowStatesExample final {
 public:
  void frame(const Frame& frame) {
    handle_keys(frame);
    if (!stack_.empty() && stack_.back() == State::boot) {
      boot_timer_ += frame.delta_time * 1000.0;
      if (boot_timer_ >= 1500.0) stack_.back() = State::menu;
    }
    if (!stack_.empty() && stack_.back() == State::play) play_time_ += frame.delta_time;

    if (!stack_.empty()) {
      if (stack_.back() == State::pause && stack_.size() >= 2) draw_state(frame.canvas, stack_[stack_.size() - 2]);
      draw_state(frame.canvas, stack_.back());
    }
    draw_hud(frame.canvas);
  }

 private:
  void handle_keys(const Frame& frame) {
    if (stack_.empty()) return;
    const State active = stack_.back();
    if (active == State::menu && frame.input.key_pressed(SDLK_RETURN)) {
      score_ = 0;
      play_time_ = 0.0;
      stack_.push_back(State::play);
      return;
    }
    if (active == State::play && frame.input.key_pressed(SDLK_SPACE)) {
      score_ += 10;
      return;
    }
    if (active == State::play && frame.input.key_pressed(SDLK_ESCAPE)) {
      stack_.push_back(State::pause);
      return;
    }
    if (active == State::pause && frame.input.key_pressed(SDLK_ESCAPE)) {
      stack_.pop_back();
      return;
    }
    if (active == State::play && frame.input.key_pressed(SDLK_G)) {
      stack_.back() = State::game_over;
      return;
    }
    if (active == State::game_over && frame.input.key_pressed(SDLK_R)) {
      score_ = 0;
      play_time_ = 0.0;
      stack_.back() = State::play;
      return;
    }
    if (active == State::game_over && frame.input.key_pressed(SDLK_M)) {
      stack_ = {State::menu};
      return;
    }
    if (active == State::menu && frame.input.key_pressed(SDLK_Q)) stack_ = {State::menu};
  }

  void draw_state(Canvas& canvas, State state) const {
    switch (state) {
      case State::boot:
        canvas.fill_rectangle(0.0, 0.0, 600.0, 400.0, 0x111111ffU);
        canvas.text("FLIGHT SDK", 200.0, 140.0, 0xffffffffU, 32.0);
        canvas.text("Loading...", 240.0, 190.0, 0x888888ffU, 18.0);
        break;
      case State::menu:
        canvas.fill_rectangle(0.0, 0.0, 600.0, 400.0, 0x1a3a5cffU);
        canvas.text("FLOW STATES", 170.0, 80.0, 0xffffffffU, 36.0);
        canvas.text("A @flighthq/flow demo", 190.0, 130.0, 0xaaaaaaffU, 16.0);
        canvas.text("Press ENTER to play", 195.0, 220.0, 0x44cc88ffU, 20.0);
        canvas.text("Press Q to clear stack", 195.0, 260.0, 0x888888ffU, 16.0);
        break;
      case State::play:
        canvas.fill_rectangle(0.0, 0.0, 600.0, 400.0, 0x2d5016ffU);
        canvas.text("PLAYING", 230.0, 30.0, 0xffffffffU, 28.0);
        canvas.text("Score: " + std::to_string(score_), 30.0, 100.0, 0xeedd44ffU, 22.0);
        canvas.text("Time: " + std::to_string(static_cast<int>(play_time_)) + "s", 30.0, 140.0, 0xccccccffU, 16.0);
        canvas.text("SPACE - score points", 30.0, 220.0, 0x88bb88ffU, 16.0);
        canvas.text("ESCAPE - pause", 30.0, 250.0, 0x88bb88ffU, 16.0);
        canvas.text("G - game over", 30.0, 280.0, 0x88bb88ffU, 16.0);
        break;
      case State::pause:
        canvas.fill_rectangle(0.0, 0.0, 600.0, 400.0, 0x000000ffU, 0.6);
        canvas.text("PAUSED", 225.0, 150.0, 0xffffffffU, 40.0);
        canvas.text("Press ESCAPE to resume", 185.0, 210.0, 0xccccccffU, 18.0);
        break;
      case State::game_over:
        canvas.fill_rectangle(0.0, 0.0, 600.0, 400.0, 0x5c1a1affU);
        canvas.text("GAME OVER", 185.0, 100.0, 0xff4444ffU, 36.0);
        canvas.text("Final Score: " + std::to_string(score_), 200.0, 160.0, 0xffffffffU, 24.0);
        canvas.text("Press R to restart", 205.0, 240.0, 0xccccccffU, 18.0);
        canvas.text("Press M for menu", 215.0, 275.0, 0x888888ffU, 18.0);
        break;
    }
  }

  void draw_hud(Canvas& canvas) const {
    const std::string active = stack_.empty() ? "none" : std::string(name(stack_.back()));
    std::string names;
    for (std::size_t index = 0; index < stack_.size(); ++index) {
      if (index != 0) names += " > ";
      names += name(stack_[index]);
    }
    canvas.text("Depth: " + std::to_string(stack_.size()), 400.0, 10.0, 0xffff88ffU, 14.0);
    canvas.text("Active: " + active, 400.0, 30.0, 0xffff88ffU, 14.0);
    canvas.text("Stack: " + names, 400.0, 50.0, 0xffff88ffU, 11.0);
  }

  std::vector<State> stack_{State::boot};
  int score_{};
  double boot_timer_{};
  double play_time_{};
};

} // namespace

int main(int argc, char** argv) {
  try {
    FlowStatesExample example;
    return flight::examples::SdlExample::run(
        argc,
        argv,
        "Flight flow states - SDL",
        [&](const Frame& frame) { example.frame(frame); });
  } catch (const std::exception& error) {
    std::cerr << "flowstates example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
