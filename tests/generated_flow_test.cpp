// The generated @flighthq/flow package at a627849, exercised natively. Ported from packages/flow/src/flow.test.ts
// and flowGuards.test.ts:
// - state progression and ordering: enter on push, pause-then-enter, exit-then-resume on pop, exit-then-enter on
//   replace without touching the state beneath, and clear exiting top-to-bottom;
// - branching through the overlay flags: updateBelow chains and renderBelow visibility runs, each stopping at
//   the first opaque state;
// - lifecycle and identity: fresh independent stacks, the active top by reference, depth, null sentinels, and
//   states that omit every callback;
// - failure (not in upstream, which never throws from a callback): the source's ordering decides what a throwing
//   callback leaves behind -- push adds the state before entering it, pop removes it before exiting it, and
//   both restore the transition depth through their `finally`, so a throw neither corrupts the stack nor leaves
//   later transitions reported as nested.
//
// The guards report through flight::log::log_once. At this base the log package is partial
// (overrides/manifest.json, log-module-qualification-and-operators): merge_span_fields is a stub on every
// emission path, so a report throws instead of reaching a sink. A report is therefore observed as either a
// captured entry or that stub's error, and the absence of a report as neither -- which is what each guard
// assertion below checks.
#include <flight/flow/flow.hpp>
#include <flight/flow/flow_guards.hpp>
#include <flight/log/log.hpp>

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace fl = flight::flow;
using flight::Ref;
using flight::types::FlowStack;
using flight::types::FlowState;
using Log = std::vector<std::string>;

struct Flags {
  bool renderBelow = false;
  bool updateBelow = false;
};

Ref<FlowState> state(const char* name, Flags flags = {}) {
  auto out = flight::make_ref<FlowState>();
  out->name = flight::String(name);
  if (flags.renderBelow) out->render_below = true;
  if (flags.updateBelow) out->update_below = true;
  return out;
}

// upstream's labeledState: every callback appends "<name>.<event>" to the log.
Ref<FlowState> labeled(const char* name, Log& log, Flags flags = {}) {
  auto out = state(name, flags);
  const std::string n = name;
  out->on_enter = std::function<void()>([&log, n] { log.push_back(n + ".enter"); });
  out->on_exit = std::function<void()>([&log, n] { log.push_back(n + ".exit"); });
  out->on_pause = std::function<void()>([&log, n] { log.push_back(n + ".pause"); });
  out->on_resume = std::function<void()>([&log, n] { log.push_back(n + ".resume"); });
  out->on_update = std::function<void(double)>([&log, n](double dt) {
    std::string value = std::to_string(dt);
    value.erase(value.find_last_not_of('0') + 1);
    log.push_back(n + ".update:" + value);
  });
  return out;
}

void push(const Ref<FlowStack>& stack, const Ref<FlowState>& s) { fl::push_flow_state(stack, view(s)); }
void replace(const Ref<FlowStack>& stack, const Ref<FlowState>& s) { fl::replace_flow_state(stack, view(s)); }

bool states_are(const Ref<FlowStack>& stack, const std::vector<Ref<FlowState>>& expected) {
  if (stack->states.size() != expected.size()) return false;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (stack->states.element(static_cast<double>(i)) != expected[i]) return false;
  }
  return true;
}

void progression() {
  {
    Log log;
    auto stack = fl::create_flow_stack();
    push(stack, labeled("A", log));
    check(log == Log{"A.enter"}, "push enters a state on an empty stack");
    log.clear();
    push(stack, labeled("B", log));
    check(log == Log{"A.pause", "B.enter"} && stack->states.size() == 2, "push pauses the previous top before entering");
  }
  {
    Log log;
    auto stack = fl::create_flow_stack();
    auto a = labeled("A", log), b = labeled("B", log);
    push(stack, a);
    push(stack, b);
    log.clear();
    const auto popped = fl::pop_flow_state(stack);
    check(log == Log{"B.exit", "A.resume"} && popped.has_value() && popped.value() == b && states_are(stack, {a}),
          "pop exits the top, resumes the revealed state, and returns the popped state");
    log.clear();
    static_cast<void>(fl::pop_flow_state(stack));
    check(log == Log{"A.exit"} && stack->states.size() == 0, "popping the last state resumes nothing");
    check(!fl::pop_flow_state(stack).has_value(), "popping an empty stack returns null");
  }
  {
    Log log;
    auto stack = fl::create_flow_stack();
    auto a = labeled("A", log);
    push(stack, a);
    push(stack, labeled("B", log));
    log.clear();
    auto c = labeled("C", log);
    replace(stack, c);
    check(log == Log{"B.exit", "C.enter"} && states_are(stack, {a, c}), "replace exits the top and enters the replacement, leaving the state below");
    log.clear();
    auto empty = fl::create_flow_stack();
    replace(empty, labeled("D", log));
    check(log == Log{"D.enter"} && empty->states.size() == 1, "replace on an empty stack just pushes and enters");
  }
  {
    Log log;
    auto stack = fl::create_flow_stack();
    push(stack, labeled("A", log));
    push(stack, labeled("B", log));
    push(stack, labeled("C", log));
    log.clear();
    fl::clear_flow_stack(stack);
    check(log == Log{"C.exit", "B.exit", "A.exit"} && stack->states.size() == 0, "clear exits top-to-bottom and empties the stack");
    fl::clear_flow_stack(stack);
    check(stack->states.size() == 0, "clearing an empty stack is a no-op");
  }
}

void branching() {
  const auto updates = [](std::vector<std::pair<const char*, bool>> layers, double dt) {
    Log log;
    auto stack = fl::create_flow_stack();
    for (const auto& [name, below] : layers) push(stack, labeled(name, log, {.updateBelow = below}));
    log.clear();
    fl::update_flow_stack(view(stack), dt);
    return log;
  };
  check(updates({{"A", false}, {"B", false}}, 0.016) == Log{"B.update:0.016"}, "only the top ticks, with the frame delta");
  check(updates({{"A", false}, {"B", true}}, 0.02) == Log{"B.update:0.02", "A.update:0.02"}, "an updateBelow overlay ticks the state beneath");
  check(updates({{"A", false}, {"B", false}, {"C", true}, {"D", true}}, 0.5) == Log{"D.update:0.5", "C.update:0.5", "B.update:0.5"},
        "an updateBelow chain stops at the first opaque state");
  auto empty = fl::create_flow_stack();
  fl::update_flow_stack(view(empty), 0.016);
  auto silent = fl::create_flow_stack();
  silent->states.push(state("over", {.updateBelow = true}));
  silent->states.push(state("bare"));
  fl::update_flow_stack(view(silent), 0.016);
  check(true, "updating an empty stack, or states without onUpdate, does not throw");

  const auto visible = [](std::vector<std::pair<const char*, bool>> layers) {
    auto stack = fl::create_flow_stack();
    std::vector<Ref<FlowState>> pushed;
    for (const auto& [name, below] : layers) {
      pushed.push_back(state(name, {.renderBelow = below}));
      push(stack, pushed.back());
    }
    flight::Array<Ref<FlowState>> out;
    out.push(state("stale"));
    out.push(state("leftover"));
    fl::get_flow_stack_visible_states(view(stack), out);
    std::vector<std::string> names;
    for (const auto& s : out) names.push_back(s->name.value().to_utf8());
    // The visible run is a suffix of what was pushed, so compare it against that suffix by reference.
    bool identity = names.size() <= pushed.size();
    for (std::size_t i = 0; identity && i < names.size(); ++i) identity = out.element(static_cast<double>(i)) == pushed[pushed.size() - names.size() + i];
    return std::pair{names, identity};
  };
  check(visible({{"A", false}, {"B", false}}) == std::pair{std::vector<std::string>{"B"}, true}, "an opaque top is the only visible state");
  check(visible({{"A", false}, {"B", true}, {"C", true}}) == std::pair{std::vector<std::string>{"A", "B", "C"}, true},
        "a renderBelow run is visible bottom-to-top, by reference");
  check(visible({{"A", false}, {"B", false}, {"C", true}}) == std::pair{std::vector<std::string>{"B", "C"}, true},
        "visibility stops at the first opaque state, which is itself visible");
  check(visible({}).first.empty(), "an empty stack clears out to empty");

  // An overlay flag set explicitly to false is opaque, exactly as an absent one: the source tests truthiness.
  // The emitter tested the optional's presence inside `&&` (docs/compiler-requests/logical-and-optional-operand-truthiness.md);
  // the flow-optional-overlay-flags-explicit patch spells both walks `=== true`.
  {
    Log log;
    auto stack = fl::create_flow_stack();
    push(stack, labeled("A", log));
    auto top = labeled("B", log);
    top->update_below = false;
    top->render_below = false;
    push(stack, top);
    log.clear();
    fl::update_flow_stack(view(stack), 1.0);
    flight::Array<Ref<FlowState>> out;
    fl::get_flow_stack_visible_states(view(stack), out);
    check(log == Log{"B.update:1."} && out.size() == 1 && out.element(0.0) == top,
          "explicit false overlay flags are opaque: nothing beneath ticks or shows");
  }
}

void lifecycle() {
  auto a = fl::create_flow_stack(), b = fl::create_flow_stack();
  check(a->states.size() == 0 && a != b && a->states.identity() != b->states.identity(), "each stack is fresh and independent");
  check(!fl::get_active_flow_state(view(a)).has_value() && fl::get_flow_stack_depth(view(a)) == 0, "an empty stack has no active state and depth 0");
  auto first = state("A"), second = state("B");
  push(a, first);
  push(a, second);
  check(fl::get_active_flow_state(view(a)).value() == second && fl::get_flow_stack_depth(view(a)) == 2, "the active state is the top, by reference");
  static_cast<void>(fl::pop_flow_state(a));
  check(fl::get_flow_stack_depth(view(a)) == 1 && fl::get_active_flow_state(view(a)).value() == first, "pop reduces the depth");
  auto bare = fl::create_flow_stack();
  push(bare, flight::make_ref<FlowState>());
  push(bare, flight::make_ref<FlowState>());
  replace(bare, flight::make_ref<FlowState>());
  static_cast<void>(fl::pop_flow_state(bare));
  fl::clear_flow_stack(bare);
  check(bare->states.size() == 0, "states that omit every callback push, replace, pop and clear without throwing");
}

// A report is a captured entry, or, while log is partial, its merge_span_fields stub error.
struct Reported {
  bool reported = false;
  std::vector<std::string> kinds;
};

Reported capture_reports(const std::function<void()>& run) {
  Reported result;
  auto sink = flight::make_ref<flight::log::LogSink>([&result](Ref<flight::types::LogEntry> entry) {
    result.reported = true;
    if (const auto* record = std::get_if<flight::Record<flight::String, flight::Any>>(&entry->data)) {
      const auto kind = record->get(flight::String("kind"));
      if (kind.has_value() && kind->kind() == flight::AnyKind::string) result.kinds.push_back(kind->as_string().to_utf8());
    }
  });
  flight::log::clear_log_once_keys();
  flight::log::add_log_sink(sink);
  struct Remove {
    Ref<flight::log::LogSink> sink;
    ~Remove() { static_cast<void>(flight::log::remove_log_sink(sink)); }
  } remove{sink};
  try {
    run();
  } catch (const flight::Error& error) {
    if (std::string(error.what()).find("merge_span_fields") == std::string::npos) throw;
    result.reported = true;
  }
  return result;
}

void guards() {
  fl::disable_flow_guards();
  fl::disable_flow_guards();
  fl::enable_flow_guards();
  fl::disable_flow_guards();
  check(true, "enable and disable toggle without throwing, and disable is idempotent");

  auto repeated = state("A");
  const auto quiet = capture_reports([&] {
    auto stack = fl::create_flow_stack();
    push(stack, repeated);
    push(stack, repeated);
  });
  check(!quiet.reported, "guards are opt-in: a duplicate push reports nothing while disabled");

  const auto duplicate = capture_reports([&] {
    fl::enable_flow_guards();
    struct Disable {
      ~Disable() { fl::disable_flow_guards(); }
    } disable;
    auto stack = fl::create_flow_stack();
    push(stack, repeated);
    push(stack, repeated);
  });
  check(duplicate.reported && (duplicate.kinds.empty() || duplicate.kinds == std::vector<std::string>{"duplicate-state-push"}),
        "an enabled guard reports pushing a state already on the stack");

  const auto nested = capture_reports([&] {
    fl::enable_flow_guards();
    struct Disable {
      ~Disable() { fl::disable_flow_guards(); }
    } disable;
    auto stack = fl::create_flow_stack();
    auto outer = state("outer");
    outer->on_enter = std::function<void()>([stack] { push(stack, state("inner")); });
    push(stack, outer);
  });
  check(nested.reported && (nested.kinds.empty() || nested.kinds == std::vector<std::string>{"transition-during-transition"}),
        "an enabled guard reports a push made from inside another transition");

  const auto distinct = capture_reports([&] {
    fl::enable_flow_guards();
    struct Disable {
      ~Disable() { fl::disable_flow_guards(); }
    } disable;
    auto stack = fl::create_flow_stack();
    push(stack, state("A"));
    push(stack, state("B"));
    replace(stack, state("C"));
    static_cast<void>(fl::pop_flow_state(stack));
  });
  check(!distinct.reported, "well-formed transitions report nothing even with guards enabled");
}

void failure() {
  {
    auto stack = fl::create_flow_stack();
    auto throwing = state("boom");
    throwing->on_enter = std::function<void()>([] { throw std::runtime_error("enter failed"); });
    bool threw = false;
    try {
      push(stack, throwing);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    check(threw && states_are(stack, {throwing}), "a throwing onEnter propagates, and the state was pushed before entering");
    const auto after = capture_reports([&] {
      fl::enable_flow_guards();
      struct Disable {
        ~Disable() { fl::disable_flow_guards(); }
      } disable;
      push(stack, state("next"));
    });
    check(!after.reported, "push's finally restores the transition depth, so the next push is not reported as nested");
  }
  {
    Log log;
    auto stack = fl::create_flow_stack();
    auto below = labeled("A", log);
    auto throwing = state("boom");
    throwing->on_exit = std::function<void()>([] { throw std::runtime_error("exit failed"); });
    push(stack, below);
    push(stack, throwing);
    log.clear();
    bool threw = false;
    try {
      static_cast<void>(fl::pop_flow_state(stack));
    } catch (const std::runtime_error&) {
      threw = true;
    }
    check(threw && states_are(stack, {below}) && log.empty(),
          "a throwing onExit propagates from pop after the state is removed, and nothing is resumed");
  }
  {
    Log log;
    auto stack = fl::create_flow_stack();
    auto throwing = state("boom");
    throwing->on_exit = std::function<void()>([] { throw std::runtime_error("exit failed"); });
    push(stack, throwing);
    auto replacement = labeled("C", log);
    bool threw = false;
    try {
      replace(stack, replacement);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    check(threw && stack->states.size() == 0 && log.empty(), "a throwing onExit aborts replace after removing the old top, before the new one enters");
    const auto after = capture_reports([&] {
      fl::enable_flow_guards();
      struct Disable {
        ~Disable() { fl::disable_flow_guards(); }
      } disable;
      push(stack, state("next"));
    });
    check(!after.reported, "replace's finally restores the transition depth too");
  }
}

}  // namespace

int main() {
  progression();
  branching();
  lifecycle();
  guards();
  failure();
  return failures == 0 ? 0 : 1;
}
