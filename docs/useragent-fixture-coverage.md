# @flighthq/useragent -- upstream fixture coverage

The package ships four source modules; the emitter writes three and refuses `parseUserAgentOsVersion`
(`cpp-member-projection-without-present-storage`), and `userAgent.ts`'s platform module is supplied whole by
`overrides/include/flight/useragent/`. `tests/generated_useragent_test.cpp` drives that supplied surface.

## How the inventory was taken

Two upstream suites specify this package: `userAgent.test.ts` and `userAgentParse.test.ts`.

The count was taken with a **brace-depth scanner**, not a regex over lines: it advances through the source
skipping comments and string/template bodies so that braces inside them cannot shift the depth, attributes
each `it` to its enclosing `describe` chain by depth, and **expands `it.each([...])` into one case per row**.
That last part is why a regex pass is not enough: `it.each` in `userAgent.test.ts` hides the two Opera-on-iOS
cases behind `it.each([...])('keeps the %s product token on WebKit', ...)`, whose call site does not look like
`it(` at all. A `^\s*it\(` count reports 73 sites for that file; the true case count is 74.

| fixture | cases |
| --- | --- |
| `userAgent.test.ts` | 74 (72 `it`/`test` sites, one of which is an `it.each` over 2 rows) |
| `userAgentParse.test.ts` | 32 |
| **total** | **106** |

## Accounting

Every case below is listed in fixture order. The `covered by` column names the assertion's failure message —
the string the suite prints, which is the identity of the assertion rather than a line number that moves.

`scripts/` has no gate for this; the check is run directly. It is two-directional: every case below must
appear in the fixture enumeration, and every cited message must appear in the test source.

| # | fixture | describe :: case | covered by |
| --- | --- | --- | --- |
| 1 | `userAgent.test.ts` | `detectEndianness` :: returns a known canonical value | the endianness probe agrees with the host's own byte order / and it answers one of the canonical values |
| 2 | `userAgent.test.ts` | `detectEndianness` :: returns little on Node.js (x64 host) | the endianness probe agrees with the host's own byte order (on this x64 host that is 'little') |
| 3 | `userAgent.test.ts` | `legacy EdgeHTML` :: is not reported as blink | legacy EdgeHTML Edge is not Blink |
| 4 | `userAgent.test.ts` | `legacy EdgeHTML` :: does not report the embedded Chrome version as its engine version | and its embedded Chrome version is not reported as its engine version |
| 5 | `userAgent.test.ts` | `legacy EdgeHTML` :: is still not blink without a Chrome token | which holds without a Chrome token too |
| 6 | `userAgent.test.ts` | `legacy EdgeHTML` :: leaves modern Chromium Edge reported as blink | Chromium Edge is Blink + and its version is the Edg/ token, ahead of Chrome/ |
| 7 | `userAgent.test.ts` | `legacy EdgeHTML` :: leaves Edge on iOS reported as webkit | Edge on iOS is WebKit: every browser there runs the system engine |
| 8 | `userAgent.test.ts` | `parseUserAgentArch` :: detects arm64 from aarch64 token | aarch64 is arm64 |
| 9 | `userAgent.test.ts` | `parseUserAgentArch` :: detects arm64 from arm64 token | an arm64 token is arm64 |
| 10 | `userAgent.test.ts` | `parseUserAgentArch` :: detects x64 from Win64 token | Win64/x64 is x64 |
| 11 | `userAgent.test.ts` | `parseUserAgentArch` :: detects x64 from WOW64 token | WOW64 is x64 |
| 12 | `userAgent.test.ts` | `parseUserAgentArch` :: detects x64 from x86_64 token | x86_64 is x64 |
| 13 | `userAgent.test.ts` | `parseUserAgentArch` :: detects x86 from i686 token | i686 is x86 |
| 14 | `userAgent.test.ts` | `parseUserAgentArch` :: detects arm before arm64 does not fire on plain arm token | plain arm is arm, and arm64 is tested before it |
| 15 | `userAgent.test.ts` | `parseUserAgentArch` :: detects riscv64, mips64, and mips tokens | riscv64, mips64 and mips are each detected, mips64 before mips |
| 16 | `userAgent.test.ts` | `parseUserAgentArch` :: prefers the UA-CH platform hint over the UA string | the UA-CH platform hint is preferred when it is conclusive |
| 17 | `userAgent.test.ts` | `parseUserAgentArch` :: falls back to the UA string when the platform hint is inconclusive | and an inconclusive hint falls back to the UA string |
| 18 | `userAgent.test.ts` | `parseUserAgentArch` :: returns empty string when arch is undetectable | a Linux UA with no arch token is undetectable, like any other unknown UA |
| 19 | `userAgent.test.ts` | `parseUserAgentEngine` :: returns gecko for Firefox | Firefox is Gecko |
| 20 | `userAgent.test.ts` | `parseUserAgentEngine` :: returns blink for Chrome | a plain Chrome UA with no Edge or Opera token is Blink |
| 21 | `userAgent.test.ts` | `parseUserAgentEngine` :: returns blink for Edge (Edg/) | Chromium Edge is Blink |
| 22 | `userAgent.test.ts` | `parseUserAgentEngine` :: returns webkit for Safari | Safari is WebKit / a Safari UA is WebKit even though it contains no blink token |
| 23 | `userAgent.test.ts` | `parseUserAgentEngine` :: returns unknown for unrecognized UA | an unrecognized or empty UA is unknown |
| 24 | `userAgent.test.ts` | `parseUserAgentEngine for Opera on iOS` :: keeps the OPiOS product token on WebKit | Opera on iOS under OPiOS is WebKit, with a version from the WebKit tokens |
| 25 | `userAgent.test.ts` | `parseUserAgentEngine for Opera on iOS` :: keeps the OPT product token on WebKit | and so is Opera on iOS under the OPT token |
| 26 | `userAgent.test.ts` | `parseUserAgentEngine on iOS` :: reports webkit for Edge on iOS, which contains the blink-family token "edg" | Edge on iOS is WebKit: every browser there runs the system engine |
| 27 | `userAgent.test.ts` | `parseUserAgentEngine on iOS` :: reports webkit for Chrome and Firefox on iOS | so is Chrome on iOS + and Firefox on an iPad |
| 28 | `userAgent.test.ts` | `parseUserAgentEngine on iOS` :: reports webkit for an iPad UA | and a CriOS token inside an iPad UA is WebKit, decided by the iPad token |
| 29 | `userAgent.test.ts` | `parseUserAgentEngine on iOS` :: still reports blink for desktop Chrome and Edge, which are not on iOS | Chromium Edge is Blink + a plain Chrome UA with no Edge or Opera token is Blink |
| 30 | `userAgent.test.ts` | `parseUserAgentEngine on iOS` :: still reports gecko for desktop Firefox | Firefox is Gecko |
| 31 | `userAgent.test.ts` | `parseUserAgentEngineVersion` :: extracts Firefox version | with the Firefox/ version |
| 32 | `userAgent.test.ts` | `parseUserAgentEngineVersion` :: extracts Chrome version | and the Chrome/ version is read when nothing outranks it |
| 33 | `userAgent.test.ts` | `parseUserAgentEngineVersion` :: extracts Edge version (Edg/ token takes priority over Chrome/) | and its version is the Edg/ token, ahead of Chrome/ |
| 34 | `userAgent.test.ts` | `parseUserAgentEngineVersion` :: extracts Safari Version/ token | whose version is the Version/ token |
| 35 | `userAgent.test.ts` | `parseUserAgentEngineVersion` :: returns empty string for unknown engine | and asking for the version of an unknown engine answers nothing |
| 36 | `userAgent.test.ts` | `parseUserAgentEngineVersion` :: returns empty string when version is absent | an empty UA has no engine version, even for a known engine |
| 37 | `userAgent.test.ts` | `parseUserAgentKind` :: returns mobile for ios | mobile OSes are kind mobile |
| 38 | `userAgent.test.ts` | `parseUserAgentKind` :: returns mobile for android | mobile OSes are kind mobile |
| 39 | `userAgent.test.ts` | `parseUserAgentKind` :: returns web for desktop names | and every other name is kind web |
| 40 | `userAgent.test.ts` | `parseUserAgentKind` :: returns web for unknown | and every other name is kind web |
| 41 | `userAgent.test.ts` | `parseUserAgentName` :: detects android | platform names, with the web fallback for an unrecognized UA |
| 42 | `userAgent.test.ts` | `parseUserAgentName` :: detects ios from iPhone | platform names, with the web fallback for an unrecognized UA |
| 43 | `userAgent.test.ts` | `parseUserAgentName` :: detects ios from iPad | platform names, with the web fallback for an unrecognized UA |
| 44 | `userAgent.test.ts` | `parseUserAgentName` :: detects windows | platform names, with the web fallback for an unrecognized UA |
| 45 | `userAgent.test.ts` | `parseUserAgentName` :: detects macos | platform names, with the web fallback for an unrecognized UA |
| 46 | `userAgent.test.ts` | `parseUserAgentName` :: detects linux | platform names, with the web fallback for an unrecognized UA |
| 47 | `userAgent.test.ts` | `parseUserAgentName` :: returns web when no OS is detected | platform names, with the web fallback for an unrecognized UA |
| 48 | `userAgent.test.ts` | `parseUserAgentPointerWidth` :: returns 64 for x64 | pointer width follows the canonical arch tokens, -1 when unknown |
| 49 | `userAgent.test.ts` | `parseUserAgentPointerWidth` :: returns 64 for arm64 | pointer width follows the canonical arch tokens, -1 when unknown |
| 50 | `userAgent.test.ts` | `parseUserAgentPointerWidth` :: returns 32 for x86 | pointer width follows the canonical arch tokens, -1 when unknown |
| 51 | `userAgent.test.ts` | `parseUserAgentPointerWidth` :: returns 32 for arm | pointer width follows the canonical arch tokens, -1 when unknown |
| 52 | `userAgent.test.ts` | `parseUserAgentPointerWidth` :: returns -1 for wasm | pointer width follows the canonical arch tokens, -1 when unknown |
| 53 | `userAgent.test.ts` | `parseUserAgentPointerWidth` :: returns -1 for empty string | pointer width follows the canonical arch tokens, -1 when unknown |
| 54 | `userAgent.test.ts` | `parseUserAgentRuntime` :: returns unknown when window is null | null and undefined are unknown, and only those are |
| 55 | `userAgent.test.ts` | `parseUserAgentRuntime` :: returns unknown when window is undefined | null and undefined are unknown, and only those are |
| 56 | `userAgent.test.ts` | `parseUserAgentRuntime` :: returns web when no host shell globals are present | an empty window has none of the host shells and answers web, without throwing on the absent keys |
| 57 | `userAgent.test.ts` | `parseUserAgentRuntime` :: returns electron when process.versions.electron is present | process.versions.electron is Electron |
| 58 | `userAgent.test.ts` | `parseUserAgentRuntime` :: returns tauri when __TAURI__ is present | an object-valued __TAURI__ is Tauri |
| 59 | `userAgent.test.ts` | `parseUserAgentRuntime` :: returns capacitor when Capacitor is present | and an object-valued Capacitor is Capacitor |
| 60 | `userAgent.test.ts` | `parseUserAgentRuntime` :: prioritizes electron over tauri when both globals are present | Electron is tested before Tauri when both are present |
| 61 | `userAgent.test.ts` | `parseUserAgentVersion` :: parses Windows NT version | a Windows version is answered for windows |
| 62 | `userAgent.test.ts` | `parseUserAgentVersion` :: parses macOS version with underscore separators | and a macOS version in both spellings |
| 63 | `userAgent.test.ts` | `parseUserAgentVersion` :: parses macOS version with dot separators | and a macOS version in both spellings |
| 64 | `userAgent.test.ts` | `parseUserAgentVersion` :: parses iOS version | an iOS version, with and without the iPhone token |
| 65 | `userAgent.test.ts` | `parseUserAgentVersion` :: parses iOS version without iPhone token | an iOS version, with and without the iPhone token |
| 66 | `userAgent.test.ts` | `parseUserAgentVersion` :: parses Android version | an Android version |
| 67 | `userAgent.test.ts` | `parseUserAgentVersion` :: returns empty string for linux (no kernel in UA) | linux has no version to answer: its UA carries no kernel version |
| 68 | `userAgent.test.ts` | `parseUserAgentVersion` :: returns empty string for unknown name | an unknown name has no version |
| 69 | `userAgent.test.ts` | `parseUserAgentVersion` :: returns empty string when version is absent | an empty UA has no version even for a known name |
| 70 | `userAgent.test.ts` | `parseUserAgentVersion shares one extractor with parseUserAgentOsVersion` :: parses the whitespace variants that used to return empty | the whitespace variant parses, and both vocabularies agree on it |
| 71 | `userAgent.test.ts` | `parseUserAgentVersion shares one extractor with parseUserAgentOsVersion` :: agrees with parseUserAgentOsVersion on every case | the same for a macOS version with two spaces |
| 72 | `userAgent.test.ts` | `parseUserAgentVersion shares one extractor with parseUserAgentOsVersion` :: returns empty when the requested platform is not the one the UA describes | a Windows UA answers its own version and nothing for the names it does not describe |
| 73 | `userAgent.test.ts` | `parseUserAgentVersion shares one extractor with parseUserAgentOsVersion` :: does not read a macOS version off an iPad UA that says "like Mac OS X" | an iPad UA that says 'like Mac OS X' answers ios, never macos |
| 74 | `userAgent.test.ts` | `parseUserAgentVersion shares one extractor with parseUserAgentOsVersion` :: still returns empty for linux and the web fallback | linux has no version to answer: its UA carries no kernel version + and the web fallback name has no version either |
| 75 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Automotive for Android Auto UA | automotive UAs are Car |
| 76 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns TV for smart TV UAs | smart TV UAs are TV, in any casing |
| 77 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Watch for WatchOS UA | watch UAs are Watch |
| 78 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Tablet for iPad UA | an iPad UA is Tablet |
| 79 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Tablet for Android tablet UA (no Mobile token) | an Android UA with no Mobile token is Tablet |
| 80 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Phone for iPhone UA | an iPhone UA is Phone |
| 81 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Phone for Android mobile UA | an Android mobile UA is Phone |
| 82 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Desktop for Windows UA | desktop OS tokens are Desktop |
| 83 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Desktop for macOS UA | desktop OS tokens are Desktop |
| 84 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Desktop when maxTouchPoints is 0 and UA is inconclusive | and zero touch points is the weak desktop signal for an inconclusive UA |
| 85 | `userAgentParse.test.ts` | `parseUserAgentFormFactor` :: returns Unknown for empty UA with touch points available | an empty UA with no touch hint is Unknown |
| 86 | `userAgentParse.test.ts` | `parseUserAgentFormFactor desktop-mode iPad` :: classifies a touch-capable Macintosh UA as a tablet | a touch-capable Macintosh UA is a desktop-mode iPad, not a Mac |
| 87 | `userAgentParse.test.ts` | `parseUserAgentFormFactor desktop-mode iPad` :: still classifies a real Mac as a desktop | and a real Mac is still a desktop |
| 88 | `userAgentParse.test.ts` | `parseUserAgentFormFactor desktop-mode iPad` :: does not guess from an unavailable touch hint | an unavailable touch hint (-1, not zero) does not tip the answer |
| 89 | `userAgentParse.test.ts` | `parseUserAgentFormFactor desktop-mode iPad` :: needs more than one touch point, so a single-touch desktop digitiser stays a desktop | one touch point is not more than one, so a single-touch digitiser stays a desktop |
| 90 | `userAgentParse.test.ts` | `parseUserAgentFormFactor desktop-mode iPad` :: leaves an iPhone a phone even though its UA also says "like Mac OS X" | an iPhone stays a phone even though its UA says 'like Mac OS X' |
| 91 | `userAgentParse.test.ts` | `parseUserAgentFormFactor desktop-mode iPad` :: leaves a classic iPad UA a tablet | a classic iPad UA with its own Version/ and Mobile/ tokens is still a Tablet |
| 92 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns Android for Android UA | the first five OS names |
| 93 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns iPadOS for iPad UA | the first five OS names |
| 94 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns iOS for iPhone/iPod UA | the first five OS names |
| 95 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns Windows for Windows UA | the first five OS names |
| 96 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns macOS for Macintosh UA | the first five OS names |
| 97 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns ChromeOS for CrOS UA | CrOS is ChromeOS |
| 98 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns Linux for Linux UA | plain Linux is Linux |
| 99 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns FreeBSD for FreeBSD UA | the BSDs are named individually |
| 100 | `userAgentParse.test.ts` | `parseUserAgentOsName` :: returns empty string for unknown UA | an unknown OS name is empty, not a fallback |
| 101 | `userAgentParse.test.ts` | `parseUserAgentOsVersion` :: parses Android version | Android versions, and the extra whitespace variant the shared extractor accepts |
| 102 | `userAgentParse.test.ts` | `parseUserAgentOsVersion` :: parses iOS version with underscore notation | iOS versions, with underscores normalized to dots |
| 103 | `userAgentParse.test.ts` | `parseUserAgentOsVersion` :: parses Windows NT version | Windows NT versions |
| 104 | `userAgentParse.test.ts` | `parseUserAgentOsVersion` :: parses macOS version with underscore and dot notation | macOS versions, in both spellings the runtime's FROZEN value arrives in |
| 105 | `userAgentParse.test.ts` | `parseUserAgentOsVersion` :: parses ChromeOS version | ChromeOS versions, skipping the architecture token |
| 106 | `userAgentParse.test.ts` | `parseUserAgentOsVersion` :: returns empty string for unknown UA | an absent version is empty |

## Not ported

None of the 106 cases is left out. Two upstream *assertion styles* have no direct counterpart and are not
faked:

* `detectEndianness`'s `returns little on Node.js` asserts a literal `'little'` on the strength of the CI
  host being x64/arm64. The port asserts the stronger, host-independent claim — that the probe agrees with
  **this** process's own byte order, read natively — so it holds on a big-endian host too rather than
  passing for the wrong reason.
* `parseUserAgentRuntime`'s cases are driven through a `flight::Record<String, Any>` standing in for the
  window object, which is the contract-defined shape the probe reads. No browser or Electron host is
  fabricated; the probe's own truthiness and absence semantics are what is asserted.

## Known limit in the emitter, recorded rather than patched

`parseUserAgentOsVersion` is the one refused function, supplied by the override; the emission refusal is
`cpp-member-projection-without-present-storage` and is already filed upstream as
`optional-index-member-present-flow.md`. No source patch, repair or override beyond the two existing ones was
needed for this work.
