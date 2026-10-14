# @flighthq/video -- upstream fixture coverage

`@flighthq/video` is `partial` at a627849: 4 of 5 modules emit, 2 of 5 headers compile, and the package is
**not shippable**. This records every upstream case, what covers it, and -- for the blocked ones -- the exact
boundary that stops it.

`tests/generated_video_test.cpp` exercises the two modules that compile. It was EXTENDED, not replaced.

## Inventory

Three upstream suites. Enumerated with a brace-depth scanner that skips comments and string bodies and
expands `it.each` -- none of these three files has one, so the count is unaffected here, but it is the
scanner that is right when it matters.

| fixture | module | header | cases | state |
| --- | --- | --- | --- | --- |
| `videoFormat.test.ts` | `videoFormat.ts` | `video_format.hpp` | 32 | reachable |
| `videoResource.test.ts` | `videoResource.ts` | `video_resource.hpp` | 35 | reachable |
| `videoResourceFrom.test.ts` | `videoResourceFrom.ts` | `video_resource_from.hpp` | 12 | blocked |
| | | **total** | **79** | 67 reachable, 12 blocked |

## Why the 12 are blocked, at the exact boundary

`video_resource_from.hpp` omits `loadVideoResourceFromUrl` and its two retained siblings
(`loadVideoResourceFromBlob`, `loadVideoResourceFromUrls`) still call it. The header therefore fails at a
name that never existed, and `contract.hpp` and `_internal_index.hpp` fail with it because they re-export
it. Both halves are already filed: `image-source-union-domain-discriminator.md` (why the function cannot be
lowered) and `retained-declaration-calls-omitted-callee.md` (the retained call to an omitted callee --
`@flighthq/video` is that request's FIRST measured site, measured against this exact header). **No request
was filed here**: I checked the directory for this defect before writing one, and a duplicate would have
been the easiest thing to produce and the least useful.

## All 79 cases, in fixture order

State: `reachable` = the module compiles and the behaviour is asserted. `blocked` = the module's header does not compile. `boundary` = the case cannot be expressed on this binding.

| # | fixture | describe :: case | state |
| --- | --- | --- | --- |
| 1 | `videoFormat` | `canPlayVideoType` :: delegates to the backend | reachable -- asserted in `tests/generated_video_test.cpp` |
| 2 | `videoFormat` | `canPlayVideoType` :: returns false with a non-playing backend | reachable -- asserted in `tests/generated_video_test.cpp` |
| 3 | `videoFormat` | `canPlayVideoType` :: rejects an empty MIME type without invoking the backend | reachable -- asserted in `tests/generated_video_test.cpp` |
| 4 | `videoFormat` | `canPlayVideoType` :: accepts only primitive true from a backend | boundary -- the hook is `function<bool(String)>`, so a non-boolean result cannot cross the binding at all; the claim is enforced by the type, not this body |
| 5 | `videoFormat` | `canPlayVideoType` :: normalizes backend exceptions to false | reachable -- asserted in `tests/generated_video_test.cpp` |
| 6 | `videoFormat` | `detectVideoMimeType` :: returns null for a buffer that is too small | reachable -- asserted in `tests/generated_video_test.cpp` |
| 7 | `videoFormat` | `detectVideoMimeType` :: returns null for an unrecognised header | reachable -- asserted in `tests/generated_video_test.cpp` |
| 8 | `videoFormat` | `detectVideoMimeType` :: detects an mp4 ftyp box | reachable -- asserted in `tests/generated_video_test.cpp` |
| 9 | `videoFormat` | `detectVideoMimeType` :: detects a Matroska/WebM EBML header | reachable -- asserted in `tests/generated_video_test.cpp` |
| 10 | `videoFormat` | `detectVideoMimeType` :: detects an Ogg stream | reachable -- asserted in `tests/generated_video_test.cpp` |
| 11 | `videoFormat` | `detectVideoMimeType` :: accepts an ArrayBuffer as well as a Uint8Array | reachable -- asserted in `tests/generated_video_test.cpp` |
| 12 | `videoFormat` | `inferVideoMimeType` :: returns "video/mp4" for .mp4 files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 13 | `videoFormat` | `inferVideoMimeType` :: returns "video/mp4" for .m4v files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 14 | `videoFormat` | `inferVideoMimeType` :: returns "video/webm" for .webm files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 15 | `videoFormat` | `inferVideoMimeType` :: returns "video/x-matroska" for .mkv files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 16 | `videoFormat` | `inferVideoMimeType` :: returns "video/ogg" for .ogv files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 17 | `videoFormat` | `inferVideoMimeType` :: returns "video/ogg" for .ogg files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 18 | `videoFormat` | `inferVideoMimeType` :: returns "video/quicktime" for .mov files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 19 | `videoFormat` | `inferVideoMimeType` :: returns "video/3gpp" for .3gp files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 20 | `videoFormat` | `inferVideoMimeType` :: returns the HLS playlist type for .m3u8 files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 21 | `videoFormat` | `inferVideoMimeType` :: returns the DASH manifest type for .mpd files | reachable -- asserted in `tests/generated_video_test.cpp` |
| 22 | `videoFormat` | `inferVideoMimeType` :: returns null for unrecognized extensions | reachable -- asserted in `tests/generated_video_test.cpp` |
| 23 | `videoFormat` | `inferVideoMimeType` :: strips query parameters before matching | reachable -- asserted in `tests/generated_video_test.cpp` |
| 24 | `videoFormat` | `inferVideoMimeType` :: returns null for a URL with no extension | reachable -- asserted in `tests/generated_video_test.cpp` |
| 25 | `videoFormat` | `selectVideoResourceUrl` :: returns null when no source is playable | reachable -- asserted in `tests/generated_video_test.cpp` |
| 26 | `videoFormat` | `selectVideoResourceUrl` :: returns null for an empty source list | reachable -- asserted in `tests/generated_video_test.cpp` |
| 27 | `videoFormat` | `selectVideoResourceUrl` :: picks the first source whose inferred type is playable | reachable -- asserted in `tests/generated_video_test.cpp` |
| 28 | `videoFormat` | `selectVideoResourceUrl` :: honours an explicit type over the URL extension | reachable -- asserted in `tests/generated_video_test.cpp` |
| 29 | `videoFormat` | `selectVideoResourceUrl` :: treats an explicit empty type as authoritative and does not read the URL | reachable -- asserted through its only observable consequence: an empty-typed source whose URL would infer a playable type is still not selected, and the backend is not asked |
| 30 | `videoFormat` | `selectVideoResourceUrl` :: skips unknown inferred types without invoking the backend | reachable -- asserted in `tests/generated_video_test.cpp` |
| 31 | `videoFormat` | `selectVideoResourceUrl` :: continues after invalid truthy results and backend exceptions | reachable -- asserted in `tests/generated_video_test.cpp` |
| 32 | `videoFormat` | `selectVideoResourceUrl` :: does not swallow caller property-access exceptions | boundary -- `VideoResourceUrl::url`/`type` are plain members, so there is no throwing property access to preserve |
| 33 | `videoResource` | `createVideoResource` :: returns a resource with null element when called with no arguments | reachable -- asserted in `tests/generated_video_test.cpp` |
| 34 | `videoResource` | `createVideoResource` :: stores the provided host video source as borrowed by default | reachable -- asserted in `tests/generated_video_test.cpp` |
| 35 | `videoResource` | `createVideoResource` :: marks the source as owned when explicitly requested | reachable -- asserted in `tests/generated_video_test.cpp` |
| 36 | `videoResource` | `destroyVideoResource` :: asks the host to release an owned source, detaching a live stream with it | reachable -- asserted in `tests/generated_video_test.cpp` |
| 37 | `videoResource` | `destroyVideoResource` :: does not release a borrowed source — the caller manages its lifecycle | reachable -- asserted in `tests/generated_video_test.cpp` |
| 38 | `videoResource` | `destroyVideoResource` :: revokes an owned object URL after the host has released the element | reachable -- asserted in `tests/generated_video_test.cpp` |
| 39 | `videoResource` | `destroyVideoResource` :: revokes an object URL even when the source is borrowed | reachable -- asserted in `tests/generated_video_test.cpp` |
| 40 | `videoResource` | `destroyVideoResource` :: is idempotent — a second destruction is a no-op | reachable -- asserted in `tests/generated_video_test.cpp` |
| 41 | `videoResource` | `destroyVideoResource` :: still clears the resource when the host carries no release capability | reachable -- asserted in `tests/generated_video_test.cpp` |
| 42 | `videoResource` | `disposeVideoResource` :: releases the source regardless of ownership and drops it | reachable -- asserted in `tests/generated_video_test.cpp` |
| 43 | `videoResource` | `disposeVideoResource` :: is a no-op on an already element-less resource | reachable -- asserted in `tests/generated_video_test.cpp` |
| 44 | `videoResource` | `disposeVideoResource` :: revokes an owned object URL and clears it | reachable -- asserted in `tests/generated_video_test.cpp` |
| 45 | `videoResource` | `disposeVideoResource` :: does not revoke again when disposed twice | reachable -- asserted in `tests/generated_video_test.cpp` |
| 46 | `videoResource` | `disposeVideoResource` :: revokes nothing when the resource owns no object URL | reachable -- asserted in `tests/generated_video_test.cpp` |
| 47 | `videoResource` | `disposeVideoResource` :: revokes the object URL only after the host has released the element | reachable -- asserted in `tests/generated_video_test.cpp` |
| 48 | `videoResource` | `getVideoResourceDuration` :: returns 0 when there is no element | reachable -- asserted in `tests/generated_video_test.cpp` |
| 49 | `videoResource` | `getVideoResourceDuration` :: reads duration from the host | reachable -- asserted in `tests/generated_video_test.cpp` |
| 50 | `videoResource` | `getVideoResourceDuration` :: returns 0 when the host cannot report a duration | reachable -- asserted in `tests/generated_video_test.cpp` |
| 51 | `videoResource` | `getVideoResourceHeight` :: returns 0 when there is no element | reachable -- asserted in `tests/generated_video_test.cpp` |
| 52 | `videoResource` | `getVideoResourceHeight` :: reads the decoded height from the host | reachable -- asserted in `tests/generated_video_test.cpp` |
| 53 | `videoResource` | `getVideoResourceHeight` :: returns 0 when the host cannot report dimensions | reachable -- asserted in `tests/generated_video_test.cpp` |
| 54 | `videoResource` | `getVideoResourceWidth` :: returns 0 when there is no element | reachable -- asserted in `tests/generated_video_test.cpp` |
| 55 | `videoResource` | `getVideoResourceWidth` :: reads the decoded width from the host | reachable -- asserted in `tests/generated_video_test.cpp` |
| 56 | `videoResource` | `getVideoResourceWidth` :: returns 0 when the host cannot report dimensions | reachable -- asserted in `tests/generated_video_test.cpp` |
| 57 | `videoResource` | `hasVideoResourceElement` :: is false without an element and true with one | reachable -- asserted in `tests/generated_video_test.cpp` |
| 58 | `videoResource` | `initializeVideoResource` :: is the construction initializer of createVideoResource | reachable -- asserted in `tests/generated_video_test.cpp` |
| 59 | `videoResource` | `isVideoResourceEmpty` :: is true when there is no element | reachable -- asserted in `tests/generated_video_test.cpp` |
| 60 | `videoResource` | `isVideoResourceEmpty` :: is true when the host reports no decoded dimensions | reachable -- asserted in `tests/generated_video_test.cpp` |
| 61 | `videoResource` | `isVideoResourceEmpty` :: is false once the host reports dimensions | reachable -- asserted in `tests/generated_video_test.cpp` |
| 62 | `videoResource` | `isVideoResourceEmpty` :: is true when only one dimension is zero | reachable -- asserted in `tests/generated_video_test.cpp` |
| 63 | `videoResource` | `isVideoResourceEmpty` :: is true when the host cannot report dimensions at all | reachable -- asserted in `tests/generated_video_test.cpp` |
| 64 | `videoResource` | `isVideoResourceReady` :: is false when there is no element | reachable -- asserted in `tests/generated_video_test.cpp` |
| 65 | `videoResource` | `isVideoResourceReady` :: is false while the host reports no decoded frame | reachable -- asserted in `tests/generated_video_test.cpp` |
| 66 | `videoResource` | `isVideoResourceReady` :: is true once the host reports a decoded frame | reachable -- asserted in `tests/generated_video_test.cpp` |
| 67 | `videoResource` | `isVideoResourceReady` :: is false when the host cannot answer readiness | reachable -- asserted in `tests/generated_video_test.cpp` |
| 68 | `videoResourceFrom` | `loadVideoResourceFromBlob` :: returns an owned resource that keeps its object URL live after loading | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 69 | `videoResourceFrom` | `loadVideoResourceFromBlob` :: forwards loading options and the abort signal through the URL loader | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 70 | `videoResourceFrom` | `loadVideoResourceFromBlob` :: revokes the object URL when loading fails because no resource takes ownership | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 71 | `videoResourceFrom` | `loadVideoResourceFromBlob` :: revokes the object URL when loading is aborted because no resource takes ownership | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 72 | `videoResourceFrom` | `loadVideoResourceFromUrl` :: returns a Promise and wraps the provider result as an owned resource | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 73 | `videoResourceFrom` | `loadVideoResourceFromUrl` :: forwards the URL, options, and abort signal to the provider | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 74 | `videoResourceFrom` | `loadVideoResourceFromUrl` :: rejects when the provider has no URL loader | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 75 | `videoResourceFrom` | `loadVideoResourceFromUrl` :: preserves provider load failures | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 76 | `videoResourceFrom` | `loadVideoResourceFromUrl` :: preserves provider abort failures | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 77 | `videoResourceFrom` | `loadVideoResourceFromUrls` :: resolves immediately with a null-element resource when sources is empty | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 78 | `videoResourceFrom` | `loadVideoResourceFromUrls` :: resolves to a null-element resource when no source is playable | blocked -- omitted `loadVideoResourceFromUrl` (see below) |
| 79 | `videoResourceFrom` | `loadVideoResourceFromUrls` :: loads the first playable source through the provider | blocked -- omitted `loadVideoResourceFromUrl` (see below) |

## Three cases that are NOT reachable, and are not the compiler's fault

These are boundaries of the host binding, not compiler defects, and are recorded as such rather than
asserted against something a caller cannot express:

| # | case | why it cannot be expressed here |
| --- | --- | --- |
| 4 | `canPlayVideoType` "accepts only primitive true from a backend" | the upstream backend returns the string `'probably'` cast past the type. The emitted hook is `function<bool(String)>`, so a non-boolean cannot cross the boundary at all -- the claim is enforced by the binding's type rather than by this function's body. |
| 29 | `selectVideoResourceUrl` "does not read the URL" | upstream proves it with a `url` getter that throws when read. `VideoResourceUrl::url` is a plain `String` member, so a read cannot throw. The claim IS asserted, through its only observable consequence: a source with an explicit empty type whose URL would infer a playable type is still not selected. |
| 32 | `selectVideoResourceUrl` "does not swallow caller property-access exceptions" | same reason as 29 -- there is no property access to fail. |

## Cases added to the test, and the claim each makes

The pre-existing assertions were sound, but several did not test the claim in their case's title:

| case | title claims | what was asserted before | now asserted |
| --- | --- | --- | --- |
| `destroyVideoResource` "revokes ... **after** the host has released the element" | ORDER | the counts of releases and revokes | an ordered event log: `release` then `revoke` |
| `disposeVideoResource` "only after the host has released the element" | ORDER | the counts | the same ordered log |
| `selectVideoResourceUrl` "treats an explicit **empty** type as authoritative" | the URL is never read | nothing | an empty-typed source whose URL would infer a playable type is still not selected, and the backend is not asked |
| `selectVideoResourceUrl` "returns null for an empty source list" | the loop never runs | nothing | an empty list selects nothing |
| `selectVideoResourceUrl` "skips unknown inferred types **without invoking the backend**" | no probe | that the source is skipped | that it cost exactly zero probes |
| `selectVideoResourceUrl` "continues after invalid truthy results and backend exceptions" | the search continues | a throwing backend selects nothing | a backend that throws for the first source still selects the second |
| `disposeVideoResource` "is a no-op on an already element-less resource" | nothing is released | nothing | a resource that never had an element releases nothing |
| `disposeVideoResource` "revokes nothing when the resource owns no object URL" | nothing is revoked | nothing | a resource with no URL revokes nothing |
| `getVideoResourceDuration` "returns 0 when the host **cannot report**" | absent hook, not a hook answering 0 | NaN pass-through and the no-element case | a host with no duration hook at all answers 0 |
| `getVideoResourceHeight` "returns 0 when the host cannot report dimensions" | absent hook | width only | a host with no height hook answers 0 |
| `isVideoResourceEmpty` "is true when **only one** dimension is zero" | either side | both dimensions zero | width-only and height-only zero are each empty |
| `initializeVideoResource` "is the construction initializer" | it exists and builds | nothing | driving it directly builds the same resource |

## Mutations

Eleven, all verified to hit exactly one site before the result was read. Nine caught, two are EQUIVALENT
mutants and are recorded rather than chased.

| # | mutation | result |
| --- | --- | --- |
| M1 | destroy revokes before releasing | caught -- the order assertion |
| M2 | dispose revokes before releasing | caught -- the order assertion |
| M3 | `isEmpty` requires BOTH dimensions zero | caught (2 assertions) |
| M4 | the no-URL early return is removed | caught -- named failure: `bad_optional_access` |
| M5 | dispose no longer releases a present element | caught (2 assertions) |
| M6 | an absent duration hook answers -1 | caught |
| M7 | the empty-type `continue` in `selectVideoResourceUrl` is removed | **EQUIVALENT** -- see below |
| M8 | a backend exception is allowed to escape the search | caught -- named failure |
| M9 | the empty-type guard in `probe_selected_backend` is removed | **EQUIVALENT** -- see below |
| M10 | the 4-byte length gate becomes 8 | caught (2 assertions) |
| M11 | `ftyp` is read at offset 0 | caught |

### The two equivalent mutants

The empty-type refusal is enforced at three sites: `canPlayVideoType` refuses it, `selectVideoResourceUrl`
skips such a source with `continue`, and `probe_selected_backend` refuses it before calling the host. Any
ONE of the three is sufficient for every upstream case, so removing any single one changes no observable.
M7 and M9 are therefore equivalent mutants, not coverage gaps -- no upstream case distinguishes them, and
asserting the guard in the emitted helper directly would be asserting emitter structure rather than the
source's contract. Recorded so the next reader does not spend the time I did discovering it.

### A crash is a coarser signal than a failure

M4 and M8 originally aborted the process (`terminating due to uncaught exception`) -- caught, but with no
statement of which assertion was being passed through. The checks now run inside `run_checks()`, called from
a `main` that catches and names anything escaping, so both report
`FAIL: an unexpected exception escaped: bad_optional_access` and the assertions before them still stand.

## What would make the other 12 reachable

Nothing on this side. Making the header compile needs the omitted `loadVideoResourceFromUrl` to exist, and
that is the refusal in `image-source-union-domain-discriminator.md`. A hand-written override could supply
the three functions, and is the only path to shippable -- but it is the last-resort mechanism, it carries a
copy of a module rather than a seam, and the decision is the user's rather than one to take unilaterally in
a parity arc.
