// Focused behavior tests for @flighthq/useragent.
//
// The package is `assisted` at a627849: the emitter writes three of its four modules and refuses one
// function (parseUserAgentOsVersion, `cpp-member-projection-without-present-storage`), and the platform
// module it writes whole carries a function it emits in a form that cannot compile while marking nothing.
// Both are supplied by overrides/include/flight/useragent/, and this test drives the supplied surface.
//
// Every case is ported from the package's own upstream suites (packages/useragent/src/userAgent.test.ts
// and userAgentParse.test.ts), which are the specification, and the emphasis is the two things those
// suites exist for:
//
//   * PARSING / NORMALIZATION: the ordered precedence each parser depends on -- arm64 before arm, iOS
//     before any product token (EdgiOS/CriOS/FxiOS/OPiOS are all WebKit), legacy EdgeHTML `Edge/18`
//     before the blink family's `edg` alternative, CrOS before Linux, the desktop-mode-iPad touch rule
//     before the desktop branch -- and the two normalizations the source performs (underscores in iOS
//     and macOS versions become dots; a missing capture is '' rather than a throw).
//   * HOST IDENTITY: detectEndianness is cross-checked against this process's own byte order read
//     natively, parseUserAgentRuntime answers from the window-like object it was given (Electron's
//     process.versions.electron ahead of Tauri's __TAURI__ ahead of Capacitor's Capacitor, with
//     ECMAScript truthiness on every probe and `unknown` only for nullish input), and a member read on
//     an absent key or a non-object answers absence rather than throwing.
//
// NOT covered: the pointer-width and architecture tokens a UA cannot express ('wasm' and friends) are
// asserted as values, and the shape of a real host's window object is the host adapter's business.
//
// One of my own cases was fixed here after a mutation check: the CrOS-before-Linux ordering was first
// asserted with the upstream suite's `(X11; CrOS x86_64 14541.0.0)` UA, which carries no `linux` token at
// all -- so reordering the chain left the suite green and the case proved nothing. It now also asserts a UA
// that holds both tokens, which is the case that fails when the order is wrong.
#include <flight/useragent/user_agent.hpp>
#include <flight/useragent/user_agent_parse.hpp>

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
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

using flight::Any;
using flight::String;
using DynamicRecord = flight::Record<String, Any>;
using Runtime = flight::types::PlatformRuntime;
using FormFactor = flight::types::DeviceFormFactor;

DynamicRecord record_of(std::initializer_list<std::pair<const char*, Any>> entries) {
  DynamicRecord record;
  for (const auto& entry : entries) record.set(String(entry.first), entry.second);
  return record;
}


}  // namespace

int main() {
  namespace ua = flight::useragent;

  // detectEndianness is a host probe: it must agree with this process's own byte order, read natively
  // rather than through the runtime's buffer path it uses.
  {
    const std::uint16_t probe = 0x0102;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&probe);
    const String host_order = bytes[0] == 0x01 ? String("big") : String("little");
    const auto detected = ua::detect_endianness();
    check(detected == host_order, "the endianness probe agrees with the host's own byte order");
    check(detected == String("little") || detected == String("big") || detected == String("unknown"),
          "and it answers one of the canonical values");
  }

  // Architecture: the UA-CH hint first when it is conclusive, then the tokens in the source's order.
  {
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (Linux; aarch64)")) == String("arm64"),
          "aarch64 is arm64");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (iPhone; CPU arm64)")) == String("arm64"),
          "an arm64 token is arm64");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (Linux; armv7l)")) == String("arm"),
          "plain arm is arm, and arm64 is tested before it");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (Windows NT 10.0; Win64; x64)")) == String("x64"),
          "Win64/x64 is x64");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (Windows NT 6.1; WOW64)")) == String("x64"),
          "WOW64 is x64");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (X11; Linux x86_64)")) == String("x64"),
          "x86_64 is x64");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (X11; Linux i686; rv:109.0)")) == String("x86"),
          "i686 is x86");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (Linux; riscv64)")) == String("riscv64") &&
              ua::parse_user_agent_arch(String("Mozilla/5.0 (Linux; mips64)")) == String("mips64") &&
              ua::parse_user_agent_arch(String("Mozilla/5.0 (Linux; mips)")) == String("mips"),
          "riscv64, mips64 and mips are each detected, mips64 before mips");
    check(ua::parse_user_agent_arch(String("SomeCustomBrowser/1.0")) == String(""),
          "an undetectable architecture is the empty string, not a guess");

    check(ua::parse_user_agent_arch(String(""), String("arm")) == String("arm64") &&
              ua::parse_user_agent_arch(String(""), String("Windows")) == String("x64") &&
              ua::parse_user_agent_arch(String(""), String("macOS")) == String("x64") &&
              ua::parse_user_agent_arch(String(""), String("Linux")) == String("x64") &&
              ua::parse_user_agent_arch(String(""), String("Chrome OS")) == String("x64"),
          "the UA-CH platform hint is preferred when it is conclusive");
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (X11; Linux x86_64)"), String("SomeCustomPlatform")) ==
              String("x64"),
          "and an inconclusive hint falls back to the UA string");
  }

  // Engine: iOS decides before any product token, legacy EdgeHTML before the blink family, and the
  // version follows the same precedence.
  {
    const String ios_edge("Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 "
                          "(KHTML, like Gecko) EdgiOS/120.0.0.0 Mobile/15E148 Safari/605.1.15");
    check(ua::parse_user_agent_engine(ios_edge) == String("webkit"),
          "Edge on iOS is WebKit: every browser there runs the system engine");
    check(ua::parse_user_agent_engine(String("Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
                                            "AppleWebKit/605.1.15 CriOS/120.0.6099.119 Safari/604.1")) ==
              String("webkit"),
          "so is Chrome on iOS");
    check(ua::parse_user_agent_engine(String("Mozilla/5.0 (iPad; CPU OS 17_4 like Mac OS X) "
                                            "AppleWebKit/605.1.15 FxiOS/121.0 Safari/605.1.15")) ==
              String("webkit"),
          "and Firefox on an iPad");
    check(ua::parse_user_agent_engine_version(ios_edge, String("webkit")).length() > 0.0,
          "whose version comes from the WebKit tokens rather than the absent Edg/ one");

    const String legacy_edge("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                             "Gecko) Chrome/64.0.3282.140 Safari/537.36 Edge/18.17763");
    check(ua::parse_user_agent_engine(legacy_edge) == String("unknown"),
          "legacy EdgeHTML Edge is not Blink");
    check(ua::parse_user_agent_engine_version(legacy_edge, String("unknown")) == String(""),
          "and its embedded Chrome version is not reported as its engine version");
    check(ua::parse_user_agent_engine(String("Mozilla/5.0 (Windows NT 10.0) AppleWebKit/537.36 Edge/18.17763")) ==
              String("unknown"),
          "which holds without a Chrome token too");

    const String modern_edge("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                             "Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0");
    check(ua::parse_user_agent_engine(modern_edge) == String("blink"),
          "Chromium Edge is Blink");
    check(ua::parse_user_agent_engine_version(modern_edge, String("blink")) == String("120.0.0.0"),
          "and its version is the Edg/ token, ahead of Chrome/");
    check(ua::parse_user_agent_engine(String("Mozilla/5.0 (Windows NT 10.0) AppleWebKit/537.36 (KHTML, like "
                                            "Gecko) Chrome/120.0.0.0 Safari/537.36 OPR/106.0.0.0")) ==
              String("blink"),
          "Opera is Blink");
    check(ua::parse_user_agent_engine_version(String("Mozilla/5.0 Chrome/120.0.0.0 OPR/106.0.5249.91"),
                                             String("blink")) == String("106.0.5249.91"),
          "with its own OPR/ token ahead of the Chrome/ one");
    check(ua::parse_user_agent_engine(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 13_0) "
                                            "AppleWebKit/605.1.15 Version/16.0 Safari/605.1.15")) ==
              String("webkit"),
          "Safari is WebKit");
    check(ua::parse_user_agent_engine_version(String("Mozilla/5.0 (Macintosh) AppleWebKit/605.1.15 "
                                                     "Version/16.0 Safari/605.1.15"),
                                             String("webkit")) == String("16.0"),
          "whose version is the Version/ token");
    check(ua::parse_user_agent_engine_version(String("Mozilla/5.0 AppleWebKit/605.1.15 Safari/605.1.15"),
                                             String("webkit")) == String("605.1.15"),
          "or AppleWebKit/ when there is none");
    check(ua::parse_user_agent_engine(String("Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 "
                                            "Firefox/120.0")) == String("gecko"),
          "Firefox is Gecko");
    check(ua::parse_user_agent_engine_version(String("Mozilla/5.0 Gecko/20100101 Firefox/120.0"),
                                             String("gecko")) == String("120.0"),
          "with the Firefox/ version");
    check(ua::parse_user_agent_engine(String("CustomBot/1.0")) == String("unknown") &&
              ua::parse_user_agent_engine(String("")) == String("unknown"),
          "an unrecognized or empty UA is unknown");
    check(ua::parse_user_agent_engine_version(String("any UA string"), String("unknown")) == String(""),
          "and asking for the version of an unknown engine answers nothing");
    const String safari("Mozilla/5.0 (Macintosh; Intel Mac OS X 13_0) AppleWebKit/605.1.15 "
                        "(KHTML, like Gecko) Version/16.0 Safari/605.1.15");
    check(ua::parse_user_agent_engine(safari) == String("webkit"),
          "a Safari UA is WebKit even though it contains no blink token");
  }

  // Platform name and kind, and the pointer width they imply.
  {
    check(ua::parse_user_agent_name(String("Mozilla/5.0 (Linux; Android 14; Pixel 8)")) == String("android") &&
              ua::parse_user_agent_name(String("Mozilla/5.0 (iPhone; CPU iPhone OS 17_4_1 like Mac OS X)")) ==
                  String("ios") &&
              ua::parse_user_agent_name(String("Mozilla/5.0 (iPad; CPU OS 17_0 like Mac OS X)")) ==
                  String("ios") &&
              ua::parse_user_agent_name(String("Mozilla/5.0 (Windows NT 10.0; Win64; x64)")) ==
                  String("windows") &&
              ua::parse_user_agent_name(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 13_0)")) ==
                  String("macos") &&
              ua::parse_user_agent_name(String("Mozilla/5.0 (X11; Linux x86_64)")) == String("linux") &&
              ua::parse_user_agent_name(String("CustomBot/1.0")) == String("web"),
          "platform names, with the web fallback for an unrecognized UA");
    check(ua::parse_user_agent_kind(String("ios")) == String("mobile") &&
              ua::parse_user_agent_kind(String("android")) == String("mobile"),
          "mobile OSes are kind mobile");
    check(ua::parse_user_agent_kind(String("windows")) == String("web") &&
              ua::parse_user_agent_kind(String("macos")) == String("web") &&
              ua::parse_user_agent_kind(String("linux")) == String("web") &&
              ua::parse_user_agent_kind(String("unknown")) == String("web"),
          "and every other name is kind web");
    check(ua::parse_user_agent_pointer_width(String("x64")) == 64.0 &&
              ua::parse_user_agent_pointer_width(String("arm64")) == 64.0 &&
              ua::parse_user_agent_pointer_width(String("x86")) == 32.0 &&
              ua::parse_user_agent_pointer_width(String("arm")) == 32.0 &&
              ua::parse_user_agent_pointer_width(String("wasm")) == -1.0 &&
              ua::parse_user_agent_pointer_width(String("")) == -1.0,
          "pointer width follows the canonical arch tokens, -1 when unknown");
  }

  // Form factor: the source's chain, including the desktop-mode-iPad rule the touch hint exists for.
  {
    check(ua::parse_user_agent_form_factor(String("Android Auto"), -1.0) == String("Car") &&
              ua::parse_user_agent_form_factor(String("Mozilla/5.0 (car browser)"), -1.0) == String("Car"),
          "automotive UAs are Car");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (SmartTV; Linux)"), -1.0) == String("TV") &&
              ua::parse_user_agent_form_factor(String("Mozilla/5.0 (SMART-TV; LINUX)"), -1.0) == String("TV") &&
              ua::parse_user_agent_form_factor(String("Dalvik/2.1.0 (Linux; Android; GoogleTV)"), -1.0) ==
                  String("TV"),
          "smart TV UAs are TV, in any casing");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (Watch OS 10.0)"), -1.0) == String("Watch") &&
              ua::parse_user_agent_form_factor(String("wearable/1.0"), -1.0) == String("Watch"),
          "watch UAs are Watch");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (iPad; CPU OS 17_0 like Mac OS X)"), 5.0) ==
              String("Tablet"),
          "an iPad UA is Tablet");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (Linux; Android 13; SM-T870) AppleWebKit/537.36"),
                                           5.0) == String("Tablet"),
          "an Android UA with no Mobile token is Tablet");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X)"),
                                           5.0) == String("Phone"),
          "an iPhone UA is Phone");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (Linux; Android 14; Pixel 8) Mobile Safari/537.36"),
                                           5.0) == String("Phone"),
          "an Android mobile UA is Phone");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"),
                                           0.0) == String("Desktop") &&
              ua::parse_user_agent_form_factor(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 13_0)"), 0.0) ==
                  String("Desktop"),
          "desktop OS tokens are Desktop");
    check(ua::parse_user_agent_form_factor(String("SomeBrowser/1.0"), 0.0) == String("Desktop"),
          "and zero touch points is the weak desktop signal for an inconclusive UA");
    check(ua::parse_user_agent_form_factor(String(""), -1.0) == String("Unknown"),
          "an empty UA with no touch hint is Unknown");

    const String ipad_desktop_mode(
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) "
        "Version/17.4 Safari/605.1.15");
    const String real_mac(
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36");
    check(ua::parse_user_agent_form_factor(ipad_desktop_mode, 5.0) == String("Tablet"),
          "a touch-capable Macintosh UA is a desktop-mode iPad, not a Mac");
    check(ua::parse_user_agent_form_factor(real_mac, 0.0) == String("Desktop"),
          "and a real Mac is still a desktop");
    check(ua::parse_user_agent_form_factor(ipad_desktop_mode, -1.0) == String("Desktop"),
          "an unavailable touch hint (-1, not zero) does not tip the answer");
    check(ua::parse_user_agent_form_factor(real_mac, 1.0) == String("Desktop"),
          "one touch point is not more than one, so a single-touch digitiser stays a desktop");
    check(ua::parse_user_agent_form_factor(String("Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
                                                  "Version/17.0 Mobile/15E148 Safari/604.1"),
                                           5.0) == String("Phone"),
          "an iPhone stays a phone even though its UA says 'like Mac OS X'");
  }

  // OS name and OS version: the ordered chain, and the two normalizations.
  {
    check(ua::parse_user_agent_os_name(String("Mozilla/5.0 (Linux; Android 14; Pixel 8)")) == String("Android") &&
              ua::parse_user_agent_os_name(String("Mozilla/5.0 (iPad; CPU OS 17_0 like Mac OS X)")) ==
                  String("iPadOS") &&
              ua::parse_user_agent_os_name(String("Mozilla/5.0 (iPod touch; CPU iPhone OS 16_0 like Mac OS X)")) ==
                  String("iOS") &&
              ua::parse_user_agent_os_name(String("Mozilla/5.0 (Windows NT 10.0; Win64; x64)")) == String("Windows") &&
              ua::parse_user_agent_os_name(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 13_0)")) ==
                  String("macOS"),
          "the first five OS names");
    check(ua::parse_user_agent_os_name(String("Mozilla/5.0 (X11; CrOS x86_64 14541.0.0)")) == String("ChromeOS"),
          "CrOS is ChromeOS");
    // ...and still ChromeOS for a UA that carries the Linux token too, which is what makes the chain's
    // order observable: Linux is tested last, after every BSD and after CrOS.
    check(ua::parse_user_agent_os_name(String("Mozilla/5.0 (X11; CrOS x86_64 14541.0.0; Linux)")) == String("ChromeOS"),
          "a UA holding both tokens is still ChromeOS, because CrOS is tested before Linux");
    check(ua::parse_user_agent_os_name(String("Mozilla/5.0 (X11; Linux x86_64)")) == String("Linux"),
          "plain Linux is Linux");
    check(ua::parse_user_agent_os_name(String("Mozilla/5.0 (X11; FreeBSD amd64)")) == String("FreeBSD") &&
              ua::parse_user_agent_os_name(String("Mozilla/5.0 (X11; OpenBSD amd64)")) == String("OpenBSD") &&
              ua::parse_user_agent_os_name(String("Mozilla/5.0 (X11; NetBSD amd64)")) == String("NetBSD"),
          "the BSDs are named individually");
    check(ua::parse_user_agent_os_name(String("CustomBot/1.0")) == String("") &&
              ua::parse_user_agent_os_name(String("")) == String(""),
          "an unknown OS name is empty, not a fallback");

    check(ua::parse_user_agent_os_version(String("Mozilla/5.0 (Linux; Android 14.0; Pixel)")) == String("14.0") &&
              ua::parse_user_agent_os_version(String("Mozilla/5.0 (Linux; Android 9; SM-G950F)")) == String("9") &&
              ua::parse_user_agent_os_version(String("Mozilla/5.0 (Linux; Android   14; SM-X710)")) ==
                  String("14"),
          "Android versions, and the extra whitespace variant the shared extractor accepts");
    check(ua::parse_user_agent_os_version(String("Mozilla/5.0 (iPhone; CPU iPhone OS 17_0_1 like Mac OS X)")) ==
              String("17.0.1") &&
              ua::parse_user_agent_os_version(String("Mozilla/5.0 (iPad; CPU OS 16_0 like Mac OS X)")) ==
                  String("16.0"),
          "iOS versions, with underscores normalized to dots");
    check(ua::parse_user_agent_os_version(String("Mozilla/5.0 (Windows NT 10.0; Win64; x64)")) == String("10.0") &&
              ua::parse_user_agent_os_version(String("Mozilla/5.0 (Windows NT 6.1; WOW64)")) == String("6.1"),
          "Windows NT versions");
    check(ua::parse_user_agent_os_version(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)")) ==
              String("10.15.7") &&
              ua::parse_user_agent_os_version(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 13.0)")) ==
                  String("13.0"),
          "macOS versions, in both spellings the runtime's FROZEN value arrives in");
    check(ua::parse_user_agent_os_version(String("Mozilla/5.0 (X11; CrOS x86_64 14541.0.0)")) == String("14541.0.0"),
          "ChromeOS versions, skipping the architecture token");
    check(ua::parse_user_agent_os_version(String("CustomBot/1.0")) == String("") &&
              ua::parse_user_agent_os_version(String("")) == String(""),
          "an absent version is empty");
  }

  // The PlatformName vocabulary view: it delegates to the one extractor, so the two must agree.
  {
    check(ua::parse_user_agent_version(String("Mozilla/5.0 (Windows NT 10.0; Win64; x64)"), String("windows")) ==
              String("10.0"),
          "a Windows version is answered for windows");
    check(ua::parse_user_agent_version(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"), String("macos")) ==
              String("10.15.7") &&
              ua::parse_user_agent_version(String("Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15.7)"),
                                           String("macos")) == String("10.15.7"),
          "and a macOS version in both spellings");
    check(ua::parse_user_agent_version(String("Mozilla/5.0 (iPhone; CPU iPhone OS 17_4_1 like Mac OS X)"),
                                       String("ios")) == String("17.4.1") &&
              ua::parse_user_agent_version(String("Mozilla/5.0 (iPad; CPU OS 17_0 like Mac OS X)"), String("ios")) ==
                  String("17.0"),
          "an iOS version, with and without the iPhone token");
    check(ua::parse_user_agent_version(String("Mozilla/5.0 (Linux; Android 14; Pixel 8)"), String("android")) ==
              String("14"),
          "an Android version");
    check(ua::parse_user_agent_version(String("Mozilla/5.0 (X11; Linux x86_64)"), String("linux")) == String(""),
          "linux has no version to answer: its UA carries no kernel version");
    check(ua::parse_user_agent_version(String("Mozilla/5.0 (Windows NT 10.0; Win64; x64)"), String("ios")) ==
              String(""),
          "and asking a Windows UA for its iOS version answers nothing rather than a wrong version");
    check(ua::parse_user_agent_version(String("any UA"), String("unknown")) == String(""),
          "an unknown name has no version");

    // The regression the source records: the two families once carried copies that had drifted, one
    // requiring exactly one space. These pin the agreement, not just the fix.
    const String android_two_spaces("Mozilla/5.0 (Linux; Android  14; SM-X710) AppleWebKit/537.36");
    const String mac_two_spaces("Mozilla/5.0 (Macintosh; Intel Mac OS X  10_15_7)");
    check(ua::parse_user_agent_version(android_two_spaces, String("android")) == String("14") &&
              ua::parse_user_agent_version(android_two_spaces, String("android")) ==
                  ua::parse_user_agent_os_version(android_two_spaces),
          "the whitespace variant parses, and both vocabularies agree on it");
    check(ua::parse_user_agent_version(mac_two_spaces, String("macos")) == String("10.15.7") &&
              ua::parse_user_agent_version(mac_two_spaces, String("macos")) ==
                  ua::parse_user_agent_os_version(mac_two_spaces),
          "the same for a macOS version with two spaces");
  }

  // Host identity: the runtime probes answer from the window-like object they were given.
  {
    const auto runtime_of = [](const DynamicRecord& window) {
      return ua::parse_user_agent_runtime(window);
    };
    check(ua::parse_user_agent_runtime(flight::Null{}) == String("unknown") &&
              ua::parse_user_agent_runtime(flight::Undefined{}) == String("unknown"),
          "null and undefined are unknown, and only those are");
    check(runtime_of(DynamicRecord{}) == String("web"),
          "an empty window has none of the host shells and answers web, without throwing on the absent keys");

    const DynamicRecord versions = record_of({{"electron", Any(String("28.0.0"))}});
    const DynamicRecord process = record_of({{"versions", Any::external(versions)}});
    check(runtime_of(record_of({{"process", Any::external(process)}})) == String("electron"),
          "process.versions.electron is Electron");
    check(runtime_of(record_of({{"__TAURI__", Any::external(DynamicRecord{})}})) == String("tauri"),
          "an object-valued __TAURI__ is Tauri");
    check(runtime_of(record_of({{"Capacitor", Any::external(DynamicRecord{})}})) == String("capacitor"),
          "and an object-valued Capacitor is Capacitor");
    check(runtime_of(record_of({{"__TAURI__", Any::external(DynamicRecord{})},
                                {"process", Any::external(process)}})) == String("electron"),
          "Electron is tested before Tauri when both are present");

    check(runtime_of(record_of({{"Capacitor", Any(false)}})) == String("web") &&
              runtime_of(record_of({{"__TAURI__", Any(String(""))}})) == String("web"),
          "a present but falsy probe does not count: the source tests truthiness");
    check(runtime_of(record_of({{"Capacitor", Any(true)}})) == String("capacitor"),
          "while a true-valued probe does");
    check(runtime_of(record_of({{"process", Any(String("not a record"))}})) == String("web") &&
              runtime_of(record_of({{"process", Any::external(record_of({{"versions", Any(true)}}))}})) ==
                  String("web"),
          "a non-record process, and a versions member that is not a record, probe as absent rather than "
          "reinterpreting the value");
  }

  // THE REMAINING UPSTREAM CASES. Each uses the fixture's own input, because these are ports rather than
  // new probes: an assertion that only resembles the case is how a fixture case quietly stops being
  // covered.
  {
    // A Linux UA carrying NO architecture token is undetectable -- distinct from a UA that is not Linux at
    // all, and the case the empty answer actually has to hold for.
    check(ua::parse_user_agent_arch(String("Mozilla/5.0 (X11; Linux) AppleWebKit/537.36")) == String("") &&
              ua::parse_user_agent_arch(String("SomeCustomBrowser/1.0")) == String(""),
          "a Linux UA with no arch token is undetectable, like any other unknown UA");

    // Plain desktop Chrome is Blink, with no Edge or Opera token in sight.
    const String plain_chrome("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                              "Gecko) Chrome/120.0.0.0 Safari/537.36");
    check(ua::parse_user_agent_engine(plain_chrome) == String("blink"),
          "a plain Chrome UA with no Edge or Opera token is Blink");
    check(ua::parse_user_agent_engine_version(plain_chrome, String("blink")) == String("120.0.0.0"),
          "and its version is the Chrome/ token");

    // The Chrome version WITHOUT an OPR/ token ahead of it -- the OPR/ case pins the precedence, this one
    // pins that the Chrome/ fallback is reachable at all.
    const String chrome_only("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                             "Gecko) Chrome/120.0.6099.109 Safari/537.36");
    check(ua::parse_user_agent_engine_version(chrome_only, String("blink")) == String("120.0.6099.109"),
          "and the Chrome/ version is read when nothing outranks it");
    check(ua::parse_user_agent_engine_version(String(""), String("gecko")) == String(""),
          "an empty UA has no engine version, even for a known engine");

    // Opera's own iOS tokens: OPiOS and OPT are two more WebKit-in-a-product-token spellings, and neither
    // contains the blink family's `edg`.
    const String ios_prefix("Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) AppleWebKit/605.1.15 "
                            "(KHTML, like Gecko) ");
    const String op_ios = ios_prefix +
                          String("OPiOS/9.2.0.11256 Mobile/15E148 Safari/9537.53");
    const String op_opt = ios_prefix + String("Version/17.0 OPT/3.6.1 Mobile/15E148 Safari/604.1");
    check(ua::parse_user_agent_engine(op_ios) == String("webkit") &&
              ua::parse_user_agent_engine_version(op_ios, String("webkit")).length() > 0.0,
          "Opera on iOS under OPiOS is WebKit, with a version from the WebKit tokens");
    check(ua::parse_user_agent_engine(op_opt) == String("webkit") &&
              ua::parse_user_agent_engine_version(op_opt, String("webkit")).length() > 0.0,
          "and so is Opera on iOS under the OPT token");

    // An iPad UA hosting a Chrome token: the iPad branch is what decides, so the embedded CriOS token
    // never reaches the blink family.
    check(ua::parse_user_agent_engine(String("Mozilla/5.0 (iPad; CPU OS 17_4 like Mac OS X) "
                                             "AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/120.0.0.0 "
                                             "Mobile/15E148 Safari/604.1")) == String("webkit"),
          "and a CriOS token inside an iPad UA is WebKit, decided by the iPad token");
  }

  // parseUserAgentVersion's own edge cases: the vocabulary answers per requested name, so a UA that
  // describes one platform answers nothing when asked for another.
  {
    const String win("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    check(ua::parse_user_agent_version(win, String("windows")) == String("10.0") &&
              ua::parse_user_agent_version(win, String("macos")) == String("") &&
              ua::parse_user_agent_version(win, String("ios")) == String(""),
          "a Windows UA answers its own version and nothing for the names it does not describe");

    // The iPad UA says "like Mac OS X", which is exactly the string a macOS pattern would match -- so this
    // is the case that fails if the two vocabularies share a pattern that is not name-gated.
    const String ipad("Mozilla/5.0 (iPad; CPU OS 17_4 like Mac OS X) AppleWebKit/605.1.15");
    check(ua::parse_user_agent_version(ipad, String("macos")) == String("") &&
              ua::parse_user_agent_version(ipad, String("ios")) == String("17.4"),
          "an iPad UA that says 'like Mac OS X' answers ios, never macos");

    check(ua::parse_user_agent_version(String(""), String("windows")) == String(""),
          "an empty UA has no version even for a known name");
    check(ua::parse_user_agent_version(String("some-native-host/1.0"), String("web")) == String(""),
          "and the web fallback name has no version either");
  }

  {
    // A classic iPad UA, longer than the tablet case above and carrying its own Version/ and Mobile/
    // tokens: the iPad token still wins, so the extra tokens do not tip it to a phone.
    const String classic_ipad(
        "Mozilla/5.0 (iPad; CPU OS 17_4 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) "
        "Version/17.4 Mobile/15E148 Safari/604.1");
    check(ua::parse_user_agent_form_factor(classic_ipad, 5.0) == String("Tablet"),
          "a classic iPad UA with its own Version/ and Mobile/ tokens is still a Tablet");
  }

  if (failures == 0) std::cout << "generated useragent: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
