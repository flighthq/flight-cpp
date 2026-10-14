// Bounded native evidence for @flighthq/clipboard.
//
// The package had no native test and no override at this pin, and its committed slice compiles 3/3, so the
// whole module is callable. Everything asserted below is the emitter's own lowering of
// packages/clipboard/src/clipboard.ts, and every expectation is taken from that file and its upstream suite
// (packages/clipboard/src/clipboard.test.ts), which is the specification: the suite's four behavioural
// cases -- routing every operation through its supplied capability slot, never consulting another host when
// two providers coexist, pinning a watch's subscription to the host it was attached to, and being
// idempotent per watch -- are the ones ported here. The rest of that suite only checks that names are
// exported.
//
// The emphasis is the three oracles the module actually has:
//
//   * HOST IDENTITY -- every operation reaches the capability it was handed and nothing else. That is also
//     structural here: each parameter is a readonly row view of one capability type, and the watch's is
//     `RowRequired<RowOf<subscribe_...>>`, so the module cannot reach a member its caller did not supply.
//     Two providers coexisting is asserted directly.
//   * PAYLOAD -- arguments arrive unaltered (a format string, a write's exact data, a bookmark's title and
//     url, a list of formats with a missing one omitted) and results come back as the provider produced
//     them. Two of these are derived rather than forwarded: `hasClipboardHtml` and `hasClipboardRTF`
//     substitute the canonical format constants ("text/html", "text/rtf"), and `hasClipboardBookmark`
//     answers a boolean about a payload its provider resolves as null.
//   * LIFECYCLE -- a watch's subscription is released before a new one is taken, even when the new host is
//     different; the released listener never delivers again; detach releases once and is safe when never
//     attached; dispose detaches and deliberately does NOT clear the signal; and two watches do not cross.
//
// ENUMERATED BLOCKERS AND BOUNDS, stated rather than covered or hidden:
//
//   1. `_watchSubscriptions` is a `WeakMap` in the source and `flight::Map` in the emission, so an attached
//      watch is retained strongly by the module and a dropped watch is never collected. No observable
//      behaviour in this module differs -- routing is identity-keyed either way -- so nothing here asserts
//      against it, and it is a lifetime divergence rather than a missing capability: it is filed as
//      docs/compiler-requests/reference-keyed-weak-map-as-strong-map.md, which records this site, the
//      connectivity one that took the weak container for the same source shape, and the eight isolated
//      fixtures in which the pinned emitter elects the weak container for this very shape -- so the
//      election is reproducible everywhere except in this module, and the request stays open with that
//      pair as its reproduction.
//   2. A provider that rejects is not caught: the module forwards the task, and the "returns false when the
//      host denies access" in the source is the host's resolved-value contract, not a catch this module
//      performs. A rejecting capability therefore rejects the caller's task, which is asserted below rather
//      than left implicit.
//   3. Nothing here drives a real system clipboard: the host capabilities are the seam, and every case is
//      a fake capability this file states the contract for.
#include <flight/clipboard/clipboard.hpp>
#include <flight/signals/slot.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
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
using Text = flight::types::HostClipboardTextCapability;
using Formats = flight::types::HostClipboardFormatsCapability;
using Image = flight::types::HostClipboardImageCapability;
using Bookmark = flight::types::HostClipboardBookmarkCapability;
using WriteItem = flight::types::ClipboardWriteItem;

// The shape the watch attach takes: a row requiring exactly `subscribe`.
using SubscribeCapability = flight::clipboard::subscribe_93ebd993676ca20f;

// A text/format/image/bookmark host that records what it was asked and answers with a scripted value, so
// "the argument arrived unaltered" and "the result is the provider's" are both observable.
struct Backend {
  String text;
  bool text_present = false;
  bool clear_result = true;
  std::vector<String> formats;
  bool formats_denied = false;

  std::vector<String> format_queries;
  std::vector<std::pair<String, String>> format_writes;
  std::vector<String> format_reads;
  std::vector<String> item_queries;
  String html;
  String rtf;

  String image;
  std::optional<flight::Ref<flight::types::ClipboardBookmark>> bookmark;
  int bookmark_reads = 0;

  int clears = 0;
  bool reject_text = false;
};

flight::Ref<Text> text_of(const std::shared_ptr<Backend>& state) {
  auto host = flight::make_ref<Text>();
  host->clear = [state]() {
    ++state->clears;
    return flight::Task<bool>::resolve(state->clear_result);
  };
  host->has_text = [state]() { return flight::Task<bool>::resolve(state->text_present); };
  host->read_text = [state]() {
    if (state->reject_text) {
      return flight::Task<String>::reject(flight::Error(String("the host refused the read")));
    }
    return flight::Task<String>::resolve(state->text);
  };
  host->write_text = [state](String value) {
    state->text = value;
    state->text_present = true;
    return flight::Task<bool>::resolve(true);
  };
  return host;
}

flight::Ref<Formats> formats_of(const std::shared_ptr<Backend>& state) {
  auto host = flight::make_ref<Formats>();
  host->get_formats = [state]() {
    return flight::Task<flight::Array<String>>::resolve(
        state->formats_denied ? flight::Array<String>{} : [&] {
          flight::Array<String> listed = flight::Array<String>{};
          for (const auto& format : state->formats) listed.push(format);
          return listed;
        }());
  };
  host->has_format = [state](String format) {
    state->format_queries.push_back(format);
    for (const auto& present : state->formats) {
      if (present == format) return flight::Task<bool>::resolve(true);
    }
    return flight::Task<bool>::resolve(false);
  };
  host->read_format = [state](String format) {
    state->format_reads.push_back(format);
    for (std::size_t i = 0; i < state->formats.size(); ++i) {
      if (state->formats[i] == format && i < state->format_writes.size()) {
        return flight::Task<String>::resolve(state->format_writes[i].second);
      }
    }
    return flight::Task<String>::resolve(String(""));
  };
  host->read_html = [state]() { return flight::Task<String>::resolve(state->html); };
  host->read_rtf = [state]() { return flight::Task<String>::resolve(state->rtf); };
  host->read_items = [state](flight::Array<String> wanted) {
    flight::Record<String, String> found{};
    for (const auto& format : wanted) {
      for (std::size_t i = 0; i < state->formats.size(); ++i) {
        if (state->formats[i] == format && i < state->format_writes.size()) {
          found.set(format, state->format_writes[i].second);
        }
      }
    }
    return flight::Task<flight::Record<String, String>>::resolve(found);
  };
  host->write_format = [state](String format, String data) {
    state->format_writes.emplace_back(format, data);
    return flight::Task<bool>::resolve(true);
  };
  host->write_html = [state](String value) {
    state->html = value;
    return flight::Task<bool>::resolve(true);
  };
  host->write_rtf = [state](String value) {
    state->rtf = value;
    return flight::Task<bool>::resolve(true);
  };
  host->write_items = [state](
                          flight::SequenceView<flight::StructuralRef<
                              flight::RowReadonly<flight::RowOf<flight::Ref<WriteItem>>>>> items) {
    for (const auto& item : items) {
      state->format_writes.emplace_back(item->format, item->data);
      state->item_queries.push_back(item->format);
    }
    return flight::Task<bool>::resolve(true);
  };
  return host;
}

flight::Ref<Image> image_of(const std::shared_ptr<Backend>& state) {
  auto host = flight::make_ref<Image>();
  host->has_image = [state]() { return flight::Task<bool>::resolve(!state->image.empty()); };
  host->read_image = [state]() { return flight::Task<String>::resolve(state->image); };
  host->write_image = [state](String value) {
    state->image = value;
    return flight::Task<bool>::resolve(true);
  };
  return host;
}

flight::Ref<Bookmark> bookmark_of(const std::shared_ptr<Backend>& state) {
  auto host = flight::make_ref<Bookmark>();
  host->read_bookmark = [state]() {
    ++state->bookmark_reads;
    return flight::Task<std::optional<flight::Ref<flight::types::ClipboardBookmark>>>::resolve(
        state->bookmark);
  };
  host->write_bookmark = [state](String title, String url) {
    auto written = flight::make_ref<flight::types::ClipboardBookmark>();
    written->title = title;
    written->url = url;
    state->bookmark = written;
    return flight::Task<bool>::resolve(true);
  };
  return host;
}

// A change host whose listeners are kept so a released one stops delivering -- the property the pinning
// case turns on.
struct ChangeState {
  std::vector<std::function<void()>> listeners;
  std::vector<bool> registered;
  int releases = 0;
  int subscribes = 0;
};

flight::Ref<SubscribeCapability> change_of(const std::shared_ptr<ChangeState>& state) {
  auto host = flight::make_ref<SubscribeCapability>();
  host->subscribe = [state](std::function<void()> listener) {
    ++state->subscribes;
    state->listeners.push_back(std::move(listener));
    state->registered.push_back(true);
    const auto index = state->registered.size() - 1;
    return std::function<void()>{[state, index]() {
      ++state->releases;
      if (index < state->registered.size()) state->registered[index] = false;
    }};
  };
  return host;
}

void fire(const std::shared_ptr<ChangeState>& state) {
  for (std::size_t index = 0; index < state->listeners.size(); ++index) {
    if (state->registered[index]) state->listeners[index]();
  }
}

}  // namespace

int main() {
  namespace clipboard = flight::clipboard;

  // Host identity and payload: every operation reaches its own slot with unaltered arguments, and the
  // result is what the provider produced.
  {
    const auto state = std::make_shared<Backend>();
    const auto text = text_of(state);
    const auto formats = formats_of(state);
    const auto image = image_of(state);
    const auto bookmark = bookmark_of(state);

    check(clipboard::write_clipboard_text(text, String("plain")).get(), "a text write reports the provider's answer");
    check(state->text == String("plain"), "the exact text reached the provider");
    check(clipboard::read_clipboard_text(text).get() == String("plain"), "and reads it back");

    check(clipboard::write_clipboard_image(image, String("data:image/png;base64,AAAA")).get(),
          "an image write reports the provider's answer");
    check(state->image == String("data:image/png;base64,AAAA"), "in the payload it was given");
    check(clipboard::read_clipboard_image(image).get() == String("data:image/png;base64,AAAA"),
          "and the read returns that payload unchanged");

    check(clipboard::write_clipboard_bookmark(bookmark, String("Flight"), String("https://example.com")).get(),
          "a bookmark write reports the provider's answer");
    const auto written = state->bookmark;
    check(written.has_value() && written.value()->title == String("Flight") &&
              written.value()->url == String("https://example.com"),
          "with both fields reaching the provider");
    const auto read_back = clipboard::read_clipboard_bookmark(bookmark).get();
    check(read_back.has_value() && read_back.value()->url == String("https://example.com"),
          "and the read hands the caller the provider's record");

    check(clipboard::write_clipboard_format(formats, String("application/x-flight"), String("custom")).get(),
          "a format write reports the provider's answer");
    check(clipboard::write_clipboard_html(formats, String("<b>html</b>")).get(), "as does an html write");
    check(state->html == String("<b>html</b>"), "with the html reaching the provider");
    check(clipboard::write_clipboard_rtf(formats, String("{\\rtf1 rich}")).get(), "and an rtf write");
    check(state->rtf == String("{\\rtf1 rich}"), "with the rtf reaching the provider");

    // A multi-format write goes through the sequence view as the provider's own items.
    flight::Array<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<WriteItem>>>>> items =
        flight::Array<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<WriteItem>>>>>{};
    auto item = flight::make_ref<WriteItem>();
    item->format = String("text/plain");
    item->data = String("multi-text");
    items.push(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<WriteItem>>>>(item));
    check(clipboard::write_clipboard(formats, items).get(), "a multi-format write reports the provider's answer");
    check(state->item_queries.size() == 1 && state->item_queries.front() == String("text/plain"),
          "and the provider saw the item's format");
    check(state->format_writes.back().first == String("text/plain") &&
              state->format_writes.back().second == String("multi-text"),
          "and its data");

    // clear is a forwarded sentinel: the provider's false is the caller's false.
    state->clear_result = false;
    check(!clipboard::clear_clipboard(text).get(), "a denied clear is the provider's false");
    check(state->clears == 1, "which was asked once");
    state->clear_result = true;
    check(clipboard::clear_clipboard(text).get(), "and a permitted clear is true");
    check(state->clears == 2, "asked once more");
  }

  // The two derived format constants: hasClipboardHtml and hasClipboardRTF substitute the canonical
  // strings rather than forwarding what the caller said.
  {
    const auto state = std::make_shared<Backend>();
    const auto formats = formats_of(state);
    state->formats.push_back(String("text/html"));
    state->formats.push_back(String("text/rtf"));

    check(clipboard::has_clipboard_html(formats).get(), "hasClipboardHtml asks for text/html");
    check(clipboard::has_clipboard_rtf(formats).get(), "hasClipboardRTF asks for text/rtf");
    check(state->format_queries.size() == 2 && state->format_queries[0] == String("text/html") &&
              state->format_queries[1] == String("text/rtf"),
          "and those are the exact strings the provider was given");
    // A host that carries ONLY rtf. The two assertions above check the ANSWER, so on their own they are
    // satisfied by a module that asks for the wrong constant -- a mutation proved exactly that: swapping
    // hasClipboardHtml's constant to rtf left both of them passing, and only the exact-strings assertion
    // caught it. This block makes each NAME load-bearing: with the host carrying rtf and not html, the
    // html answer is only false if the module really asked for text/html.
    const auto only_rtf = std::make_shared<Backend>();
    const auto rtf_formats = formats_of(only_rtf);
    only_rtf->formats.push_back(String("text/rtf"));

    check(clipboard::has_clipboard_html(rtf_formats).get() == false,
          "hasClipboardHtml asks for text/html and NOT rtf: a host carrying only rtf has no html");
    check(clipboard::has_clipboard_rtf(rtf_formats).get(),
          "hasClipboardRTF asks for text/rtf: a host carrying rtf has it");
    check(clipboard::has_clipboard_format(rtf_formats, String("text/html")).get() == false,
          "the same host reports the html format itself as absent");

    check(clipboard::has_clipboard_format(formats, String("text/plain")).get() == false,
          "a format that is not present is false");
    check(clipboard::has_clipboard_image(image_of(std::make_shared<Backend>())).get() == false,
          "and an empty image host reports false");

    // getFormats answers the provider's list, and the denied sentinel is an empty list rather than a throw.
    const auto listed = clipboard::get_clipboard_formats(formats).get();
    check(listed.size() == 2.0, "the format list is the provider's");
    state->formats_denied = true;
    check(clipboard::get_clipboard_formats(formats).get().size() == 0.0,
          "and a denied host answers the empty-list sentinel");
  }

  // hasClipboardBookmark is a boolean about a payload: null means false, not a rejection.
  {
    const auto state = std::make_shared<Backend>();
    const auto bookmark = bookmark_of(state);
    check(!clipboard::has_clipboard_bookmark(bookmark).get(), "no bookmark is false");
    auto present = flight::make_ref<flight::types::ClipboardBookmark>();
    present->title = String("Flight");
    present->url = String("https://example.com");
    state->bookmark = present;
    check(clipboard::has_clipboard_bookmark(bookmark).get(), "a bookmark is true");
    check(state->bookmark_reads == 2, "each answer is one provider read");
  }

  // A multi-format read omits the formats the provider does not have.
  {
    const auto state = std::make_shared<Backend>();
    const auto formats = formats_of(state);
    state->formats.push_back(String("text/plain"));
    state->formats.push_back(String("application/x-flight"));
    state->format_writes.emplace_back(String("text/plain"), String("plain"));
    state->format_writes.emplace_back(String("application/x-flight"), String("custom"));

    flight::Array<String> wanted = flight::Array<String>{};
    wanted.push(String("text/plain"));
    wanted.push(String("application/x-flight"));
    wanted.push(String("missing"));
    const auto found = clipboard::read_clipboard(formats, wanted).get();
    check(found.size() == 2.0, "a missing format is omitted rather than empty");
    check(found.get(String("text/plain")).value_or(String("")) == String("plain"),
          "and the present ones carry the provider's data");
  }

  // Two providers coexist: neither reads the other's clipboard.
  {
    const auto first = std::make_shared<Backend>();
    const auto second = std::make_shared<Backend>();
    static_cast<void>(clipboard::write_clipboard_text(text_of(first), String("first")).get());
    static_cast<void>(clipboard::write_clipboard_text(text_of(second), String("second")).get());
    // The host objects are rebuilt per call here, so the state each one carries is what is compared: the
    // assertion is that the module forwarded to the host it was given, not to a shared slot.
    check(clipboard::read_clipboard_text(text_of(first)).get() == String("first"),
          "the first host answers with its own value");
    check(clipboard::read_clipboard_text(text_of(second)).get() == String("second"),
          "and the second with its own");
  }

  // A rejecting provider is not caught: the module forwards the task.
  {
    const auto state = std::make_shared<Backend>();
    state->reject_text = true;
    const auto text = text_of(state);
    bool propagated = false;
    try {
      static_cast<void>(clipboard::read_clipboard_text(text).get());
    } catch (const std::exception&) {
      propagated = true;
    }
    check(propagated, "a rejecting capability rejects the caller's task, which the module does not catch");
  }

  // FOUR EXPORTED FUNCTIONS THE EARLIER PASS NEVER CALLED. The upstream suite has one "is exported" case
  // per name, and a name exercised nowhere proves nothing about it -- these four were referenced by no line
  // of this file, so every claim about them was unwitnessed. `readClipboardRTF` is the one worth calling
  // out: it reads the row key `readRTF`, which is a DIFFERENT key from the `hasFormat`-based `has*` pair,
  // so a module that wired it to the html key or to `hasFormat` would have gone unnoticed.
  {
    const auto state = std::make_shared<Backend>();
    const auto formats = formats_of(state);
    const auto text = text_of(state);

    check(clipboard::write_clipboard_html(formats, String("<b>html</b>")).get(), "an html write lands");
    check(clipboard::write_clipboard_rtf(formats, String("{\\rtf1 rich}")).get(), "and an rtf write lands");
    // The fake keeps the SET of present formats apart from the writes, so a format has to be present before
    // it can be read back -- that is the fake's own contract, matching the upstream backend's.
    state->formats.push_back(String("application/x-flight"));
    check(clipboard::write_clipboard_format(formats, String("application/x-flight"), String("custom")).get(),
          "and a custom-format write lands");

    check(clipboard::read_clipboard_html(formats).get() == String("<b>html</b>"),
          "readClipboardHtml reads the provider's html through the formats slot");
    check(clipboard::read_clipboard_rtf(formats).get() == String("{\\rtf1 rich}"),
          "readClipboardRTF reads the provider's rtf through its own readRTF key");
    check(clipboard::read_clipboard_format(formats, String("application/x-flight")).get() == String("custom"),
          "readClipboardFormat reads the provider's data for the format it was given");
    check(!state->format_reads.empty() && state->format_reads.back() == String("application/x-flight"),
          "and passes the requested format through unaltered");

    // hasClipboardText, in both directions: a provider that has text and one that does not.
    check(clipboard::write_clipboard_text(text, String("plain")).get(), "a text write lands");
    check(clipboard::has_clipboard_text(text).get(), "hasClipboardText reports the provider's yes");
    state->text_present = false;
    check(!clipboard::has_clipboard_text(text).get(), "and its no, rather than answering from the payload");
  }

  // Lifecycle: the watch's subscription is pinned to the host it was attached to, released before a new
  // one is taken even when that host is different, and never delivers from a released listener.
  {
    const auto first = std::make_shared<ChangeState>();
    const auto second = std::make_shared<ChangeState>();
    const auto watch = clipboard::create_clipboard_watch();
    check(static_cast<bool>(watch->on_change), "a created watch carries its signal");

    int changes = 0;
    flight::signals::connect_signal<std::function<void()>>(watch->on_change, std::function<void()>{[&]() { ++changes; }});

    clipboard::attach_clipboard_watch(change_of(first), watch);
    check(first->subscribes == 1, "attach subscribes once");
    fire(first);
    check(changes == 1, "and a host event reaches the watch's signal");

    // Detach is safe when never attached, and the same watch attached elsewhere releases the first host.
    clipboard::attach_clipboard_watch(change_of(second), watch);
    check(first->releases == 1, "re-attaching releases the first host's subscription");
    check(second->subscribes == 1, "and takes exactly one on the second");
    fire(first);
    check(changes == 1, "a released listener never delivers again");
    fire(second);
    check(changes == 2, "while the new host's does");

    // Attaching twice to one host is idempotent per watch: the prior subscription is released first.
    clipboard::attach_clipboard_watch(change_of(second), watch);
    check(second->releases == 1, "a re-attach to the same host releases the prior subscription");
    check(changes == 2, "and does not stack delivery");

    clipboard::detach_clipboard_watch(watch);
    // Exactly two: one for the re-attach above and one for this detach. A weaker bound would let a detach
    // that released nothing pass.
    check(second->releases == 2, "detach releases exactly the subscription it holds");
    fire(first);
    fire(second);
    check(changes == 2, "after which no host delivers");
    // Both of these are the fixture's own idempotency cases, and until now they were CALLED BUT NOT
    // OBSERVED -- the release count was read two lines earlier and never again, so an implementation whose
    // detach forgot the subscription would have released a second time here in silence. A mutation that
    // drops the map erase survived every assertion in this file for exactly that reason.
    clipboard::detach_clipboard_watch(watch);
    check(second->releases == 2,
          "a second detach releases NOTHING more, because the first forgot the subscription it held");
    // A watch that was never attached has nothing to release and must still be safe.
    bool never_attached_threw = false;
    try {
      clipboard::detach_clipboard_watch(clipboard::create_clipboard_watch());
    } catch (...) {
      never_attached_threw = true;
    }
    check(!never_attached_threw, "detaching a watch that was never attached is safe and releases nothing");
    check(second->releases == 2, "and touches no host at all");

    // dispose detaches and deliberately leaves the signal alone: the module has no clearSignal for it.
    const auto third = std::make_shared<ChangeState>();
    clipboard::attach_clipboard_watch(change_of(third), watch);
    clipboard::dispose_clipboard_watch(watch);
    check(third->releases >= 1, "dispose releases the active subscription");
    // dispose detaches and nothing else: the module has no clearSignal, so the slot connected above is
    // still there. A module that cleared the signal would report no slots here.
    check(flight::signals::has_signal_slots<std::function<void()>>(
              flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
                  flight::RowOf<std::shared_ptr<flight::types::Signal<std::function<void()>>>>>>>(watch->on_change)),
          "and does not clear the watch's signal");
  }

  // Two watches do not cross: each is keyed by its own identity.
  {
    const auto first = std::make_shared<ChangeState>();
    const auto second = std::make_shared<ChangeState>();
    const auto one = clipboard::create_clipboard_watch();
    const auto two = clipboard::create_clipboard_watch();
    int one_changes = 0;
    int two_changes = 0;
    flight::signals::connect_signal<std::function<void()>>(one->on_change,
                                                           std::function<void()>{[&]() { ++one_changes; }});
    flight::signals::connect_signal<std::function<void()>>(two->on_change,
                                                           std::function<void()>{[&]() { ++two_changes; }});

    clipboard::attach_clipboard_watch(change_of(first), one);
    clipboard::attach_clipboard_watch(change_of(second), two);
    fire(first);
    fire(second);
    check(one_changes == 1 && two_changes == 1, "each watch hears its own host once");
    clipboard::detach_clipboard_watch(one);
    fire(first);
    fire(second);
    check(one_changes == 1 && two_changes == 2, "and detaching one leaves the other attached");
  }

  if (failures == 0) std::cout << "generated clipboard: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
