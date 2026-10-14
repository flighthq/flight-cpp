# @flighthq/clipboard -- upstream fixture coverage

`@flighthq/clipboard` is fully emitted at a627849: 3 of 3 modules, ZERO refusals, no override, and every
header compiles. One upstream suite specifies it, `clipboard.test.ts`, with 31 cases.

`tests/generated_clipboard_test.cpp` was written earlier in this session and is EXTENDED here, not replaced.

## How the inventory was taken

Brace-depth scanner: skips comments and string/template bodies so braces inside them cannot shift depth,
attributes each `it` to its enclosing describe by depth, and expands `it.each` into one case per row. This
fixture has no `it.each`, so the count is unaffected -- the scanner is used because it is the one that is
right when it matters.

Case mix: **26 of the 31 are `is exported`** (`expect(name).toBeTypeOf('function')`), one per exported name.
The other five are the behavioural cases.

## All 31 cases

State: `reachable` = the module compiles and the name or behaviour is exercised from the test.

| # | describe :: case | state |
| --- | --- | --- |
| 1 | `attachClipboardWatch` :: is exported | reachable -- `attach_clipboard_watch(` is called in the test |
| 2 | `clearClipboard` :: is exported | reachable -- `clear_clipboard(` is called in the test |
| 3 | `clipboard` :: routes each operation through its supplied clipboard capability slot | reachable -- asserted in `tests/generated_clipboard_test.cpp` |
| 4 | `clipboard` :: never consults another host when two clipboard providers coexist | reachable -- asserted in `tests/generated_clipboard_test.cpp` |
| 5 | `ClipboardWatch` :: pins the subscription to the supplied host until explicitly attached elsewhere | reachable -- asserted in `tests/generated_clipboard_test.cpp` |
| 6 | `ClipboardWatch` :: is idempotent per watch and dispose releases the active subscription | reachable -- asserted in `tests/generated_clipboard_test.cpp` |
| 7 | `createClipboardWatch` :: is exported | reachable -- `create_clipboard_watch(` is called in the test |
| 8 | `detachClipboardWatch` :: is exported | reachable -- `detach_clipboard_watch(` is called in the test |
| 9 | `disposeClipboardWatch` :: is exported | reachable -- `dispose_clipboard_watch(` is called in the test |
| 10 | `getClipboardFormats` :: is exported | reachable -- `get_clipboard_formats(` is called in the test |
| 11 | `hasClipboardBookmark` :: is exported | reachable -- `has_clipboard_bookmark(` is called in the test |
| 12 | `hasClipboardFormat` :: is exported | reachable -- `has_clipboard_format(` is called in the test |
| 13 | `hasClipboardHtml` :: is exported | reachable -- `has_clipboard_html(` is called in the test |
| 14 | `hasClipboardImage` :: is exported | reachable -- `has_clipboard_image(` is called in the test |
| 15 | `hasClipboardRTF` :: is exported | reachable -- `has_clipboard_rtf(` is called in the test |
| 16 | `hasClipboardText` :: is exported | reachable -- `has_clipboard_text(` is called in the test |
| 17 | `initializeClipboardWatch` :: is the construction initializer of createClipboardWatch | reachable -- asserted in `tests/generated_clipboard_test.cpp` |
| 18 | `readClipboard` :: is exported | reachable -- `read_clipboard(` is called in the test |
| 19 | `readClipboardBookmark` :: is exported | reachable -- `read_clipboard_bookmark(` is called in the test |
| 20 | `readClipboardFormat` :: is exported | reachable -- `read_clipboard_format(` is called in the test |
| 21 | `readClipboardHtml` :: is exported | reachable -- `read_clipboard_html(` is called in the test |
| 22 | `readClipboardImage` :: is exported | reachable -- `read_clipboard_image(` is called in the test |
| 23 | `readClipboardRTF` :: is exported | reachable -- `read_clipboard_rtf(` is called in the test |
| 24 | `readClipboardText` :: is exported | reachable -- `read_clipboard_text(` is called in the test |
| 25 | `writeClipboard` :: is exported | reachable -- `write_clipboard(` is called in the test |
| 26 | `writeClipboardBookmark` :: is exported | reachable -- `write_clipboard_bookmark(` is called in the test |
| 27 | `writeClipboardFormat` :: is exported | reachable -- `write_clipboard_format(` is called in the test |
| 28 | `writeClipboardHtml` :: is exported | reachable -- `write_clipboard_html(` is called in the test |
| 29 | `writeClipboardImage` :: is exported | reachable -- `write_clipboard_image(` is called in the test |
| 30 | `writeClipboardRTF` :: is exported | reachable -- `write_clipboard_rtf(` is called in the test |
| 31 | `writeClipboardText` :: is exported | reachable -- `write_clipboard_text(` is called in the test |

## What the earlier pass left, and what this audit found

The four behavioural cases were already ported in depth. This arc asked specifically for ordering,
negative-observation and input-sequence audit, and it found two real defects in the test itself:

**1. Four exported names were referenced by no line of the file.** `hasClipboardText`,
`readClipboardFormat`, `readClipboardHtml` and `readClipboardRTF` were named by the upstream suite and
exercised nowhere here. A name that is never called proves nothing -- the "is exported" cases for those four
were not covered at all. All four are now called, with their results asserted. `readClipboardRTF` is the one
worth the attention: it reads the row key `readRTF`, a DIFFERENT key from the `hasFormat`-based `has*` pair,
so a module that wired it to the html key would have gone unnoticed.

**2. The fixture's detach-idempotency calls were MADE BUT NEVER OBSERVED.** Two lines called
`detachClipboardWatch` (once on an already-detached watch, once on a never-attached one) and asserted nothing
afterwards -- the release count was read two lines earlier and never again. A mutation that dropped the
`watch_subscriptions.erase(watch)` in `detach` survived every assertion in the file for exactly that reason:
the first detach released, the second released AGAIN, and nothing looked. Both calls now assert their
outcome, and the mutation is caught.

Neither was a wrong assertion -- both were missing observations, which is the failure mode a mutation that
"passes" cannot show you on its own.

## Mutations

Fourteen, each verified to hit exactly one site before its result was read. All fourteen caught, zero
survivors.

| # | mutation | caught by |
| --- | --- | --- |
| M1 | attach subscribes on the NEW host before releasing the old subscription | "re-attaching releases the first host's subscription" + "a released listener never delivers again" |
| M2 | detach calls the unsubscribe but never forgets it | "a second detach releases NOTHING more" |
| M3 | `hasClipboardHtml` asks for the RTF constant | "hasClipboardHtml asks for text/html" |
| M4 | `hasClipboardRTF` asks for the HTML constant | "hasClipboardRTF asks for text/rtf" |
| M5 | `hasClipboardBookmark` answers true without asking | "no bookmark is false" |
| M6 | dispose no longer detaches | "dispose releases the active subscription" |
| M7 | a denied clear is ignored in favour of true | "a denied clear is the provider's false" |
| M8 | the requested format list is dropped on the way to the provider | "a missing format is omitted rather than empty" |
| M9 | `hasClipboardImage` answers true without asking | "an empty image host reports false" |
| M10 | `getClipboardFormats` answers an empty list | "the format list is the provider's" |
| M11 | `readClipboardRTF` reads the html key | "readClipboardRTF reads ... its own readRTF key" |
| M12 | `readClipboardHtml` reads the rtf key | "readClipboardHtml reads the provider's html" |
| M13 | `readClipboardFormat` drops the format it was asked for | "readClipboardFormat reads the provider's data" |
| M14 | `hasClipboardText` answers true without asking | "and its no, rather than answering from the payload" |

M1 is the ordering mutation and M2 the negative-observation one; both were run against the pre-audit file,
and M2 SURVIVED it. That is the finding that produced the fix above.

## Stated rather than covered

* **`EntityRuntimeKey in backend`** (the first assertion of the behavioural case) checks that the upstream
  test's fake backend carries an entity runtime key. It is a claim about the fixture, not the module, and has
  no native counterpart: capabilities here are plain reference types with no identity slot to probe.
* **The `clear` sequence** -- upstream's fake clears its own text and format list, then the suite reads `''`
  and `[]` back. Those assertions are about the FAKE's post-clear state, not the module's, which only forwards
  `clear`. This file asserts the forwarding and the provider's answer instead, and states the difference.
* **`_watchSubscriptions` is a `WeakMap` upstream and a `flight::Map` here**, so an attached watch is retained
  strongly. No observable in this module differs (routing is identity-keyed either way); it is filed as
  `docs/compiler-requests/reference-keyed-weak-map-as-strong-map.md`.
* A provider that REJECTS is not caught by the module -- it forwards the task. Asserted as such rather than
  left implicit.
