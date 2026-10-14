# @flighthq/permissions -- upstream fixture coverage

`@flighthq/permissions` is `assisted` at a627849: 3 of 3 modules emitted, 0 refusals, no override, all
headers compiling; the `assisted` tier is the pre-existing `conditional-absent-branch-optional` emission
repair, not a gap. **Five** upstream suites specify it, with **49 cases** between them.

This package was **re-audited** because its earlier verification was affected by the
success-banner-before-final-block defect: the pass summary used to be printed about ninety lines before the
end, so every assertion below it could fail while the run still emitted `generated permissions: all checks
passed`. The region below the banner was exactly the coverage the previous arc had ADDED, which means its
evidence was worthless -- a grep for the banner passes either way. The banner is now last, and that region is
pinned by mutations that fail only it.

## Inventory

Brace-depth scanner, expanding `it.each` rows. Two of these suites are parameterized: `%#` rows in the
storage snapshot tables (6 rows each) and the three `Notification %s` state rows. A `^\s*it\(` count
undercounts both.

| fixture | cases |
| --- | --- |
| `getPermissionStates.test.ts` | 6 |
| `requestPermission.test.ts` | 17 |
| `getPermissionState.test.ts` | 14 |
| `geolocationDelegation.test.ts` | 9 |
| `permission.test.ts` | 3 (the named exports) |
| **total** | **49** |

The previous header named only three of these five suites and counted none of them, so 17 cases were
unmentioned; this doc is the correction.

## All 49 cases

| # | fixture | describe :: case | state |
| --- | --- | --- | --- |
| 1 | `geolocationDelegation` | `requestPermission` :: projects a granted geolocation prompt to granted | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 2 | `geolocationDelegation` | `requestPermission` :: projects a denied geolocation prompt to denied | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 3 | `geolocationDelegation` | `requestPermission` :: projects a dismissed geolocation prompt to prompt | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 4 | `geolocationDelegation` | `requestPermission` :: preserves a geolocation timeout as a reason-only outcome with no state | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 5 | `geolocationDelegation` | `requestPermission` :: does not report a timeout as denied, dismissed or granted | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 6 | `geolocationDelegation` | `requestPermission` :: keeps the granted decision when cleanup fails | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 7 | `geolocationDelegation` | `requestPermission` :: asks the geolocation provider exactly once per request | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 8 | `geolocationDelegation` | `requestPermission` :: does not consult the geolocation provider for another permission name | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 9 | `geolocationDelegation` | `requestPermission` :: reports runtime-unavailable when no geolocation provider is installed | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 10 | `getPermissionState` | `getPermissionState` :: projects Notification granted to the common %s state | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 11 | `getPermissionState` | `getPermissionState` :: projects Notification denied to the common %s state | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 12 | `getPermissionState` | `getPermissionState` :: projects Notification default to the common %s state | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 13 | `getPermissionState` | `getPermissionState` :: preserves an owner query failure instead of returning a plausible state | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 14 | `getPermissionState` | `getPermissionState` :: projects the Storage owner snapshot { outcome: 'persistent', without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 15 | `getPermissionState` | `getPermissionState` :: projects the Storage owner snapshot { outcome: 'operation-fa without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 16 | `getPermissionState` | `getPermissionState` :: projects the Storage owner snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 17 | `getPermissionState` | `getPermissionState` :: projects the Storage owner snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 18 | `getPermissionState` | `getPermissionState` :: projects the Storage owner snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 19 | `getPermissionState` | `getPermissionState` :: projects the Storage owner snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 20 | `getPermissionState` | `getPermissionState` :: reports an absent persistence-query owner structurally | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 21 | `getPermissionState` | `getPermissionState` :: projects MIDI from only Host.midi.permission without touching Web globals or access | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 22 | `getPermissionState` | `getPermissionState` :: delegates native queries to the explicit permissions provider without reading Web globals | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 23 | `getPermissionState` | `getPermissionState` :: requires explicit providers at the caller boundary | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 24 | `getPermissionStates` | `getPermissionStates` :: pins repeated names to the direct provider passed by the caller | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 25 | `getPermissionStates` | `getPermissionStates` :: pins native queries to the permissions provider captured before work starts | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 26 | `getPermissionStates` | `getPermissionStates` :: keeps repeated MIDI queries pinned to the direct provider | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 27 | `getPermissionStates` | `getPermissionStates` :: keeps input order and repeated entries when work resolves out of order | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 28 | `getPermissionStates` | `getPermissionStates` :: does not resolve an owner for an empty batch | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 29 | `getPermissionStates` | `getPermissionStates` :: captures the persistence-query owner once and preserves repeated entries and order | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 30 | `permission` | `getPermissionState` :: remains a named export with its permanent behavior suite colocated | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 31 | `permission` | `getPermissionStates` :: remains a named export with its permanent behavior suite colocated | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 32 | `permission` | `requestPermission` :: remains a named export with its permanent behavior suite colocated | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 33 | `requestPermission` | `requestPermission` :: projects the Notification granted outcome without erasing its owner reason | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 34 | `requestPermission` | `requestPermission` :: projects the Notification denied outcome without erasing its owner reason | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 35 | `requestPermission` | `requestPermission` :: projects the Notification dismissed outcome without erasing its owner reason | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 36 | `requestPermission` | `requestPermission` :: preserves an owner request failure instead of translating it to denial | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 37 | `requestPermission` | `requestPermission` :: delegates media acquisition and preserves its cleanup failure outcome | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 38 | `requestPermission` | `requestPermission` :: delegates wake-lock acquisition and preserves its cleanup failure outcome | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 39 | `requestPermission` | `requestPermission` :: projects a thrown host permission request as an operational failure | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 40 | `requestPermission` | `requestPermission` :: reports an absent request route without silently degrading to a read | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 41 | `requestPermission` | `requestPermission` :: projects the Storage request snapshot { outcome: 'persistent', without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 42 | `requestPermission` | `requestPermission` :: projects the Storage request snapshot { outcome: 'operation-fa without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 43 | `requestPermission` | `requestPermission` :: projects the Storage request snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 44 | `requestPermission` | `requestPermission` :: projects the Storage request snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 45 | `requestPermission` | `requestPermission` :: projects the Storage request snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 46 | `requestPermission` | `requestPermission` :: projects the Storage request snapshot { outcome: 'best-effort' without inferring field consistency | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 47 | `requestPermission` | `requestPermission` :: reports an absent persistence-request owner without crossing into query | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 48 | `requestPermission` | `requestPermission` :: makes MIDI query-only here and never acquires ambient access | reachable -- asserted in `tests/generated_permissions_test.cpp` |
| 49 | `requestPermission` | `requestPermission` :: requires explicit providers at the caller boundary | reachable -- asserted in `tests/generated_permissions_test.cpp` |

All 49 are reachable: the package has no refused module, so nothing is blocked by a compiler boundary. The
three `permission.test.ts` cases are export claims, mapped mechanically to call sites
(`get_permission_state` 4, `get_permission_states` 3, `request_permission` 11).

## Defects the audit found

**1. The pass banner was printed before the last ~90 lines.** Measured, not argued: with the old placement,
a mutation failing only that region produced the banner AND its FAIL lines. Since the region was the coverage
the previous arc added, its verification could not have failed. The banner is now the last statement.

**2. The storage persistence REQUEST projection had no coverage at all.** Its QUERY sibling was covered; the
request side was not. This was found by writing two mutations of its body and watching BOTH SURVIVE the whole
suite -- the gap was discovered by mutation, not by reading. Its six upstream rows are now asserted.

**3. An absent geolocation provider was unasserted.** Upstream has a case for it; a mutation rewriting that
arm's reason to `operation-failed` survived. Now covered.

## Mutation battery

Ten mutations, each scoped to a single function body so a pattern that repeats across sibling projections
still targets one site, and each verified to hit exactly one site before its result was read.

| # | mutation | result |
| --- | --- | --- |
| Q1 | the query persistent arm reports denied where the owner said denied | caught |
| Q2 | the query best-effort arm drops the state it carried through | caught |
| Q3 | the query persistent arm reports its own name as the reason | caught |
| Q4 | the query operation-failed arm gains a state key it must not have | caught |
| R1 | the request persistent arm reports denied instead of granted | **caught only after the new coverage** |
| R2 | the request best-effort arm drops the state it carried through | **caught only after the new coverage** |
| G1 | a dismissed prompt is reported with the prompt reason | caught |
| G2 | a timeout is rewritten as an operational failure | caught |
| G3 | an installed provider is reported as runtime-unavailable | **caught only after the new coverage** |
| G4 | a cleanup failure loses its granted state | caught |

Q1-Q4 are the mutations that specifically fail the FORMER FINAL BLOCK. They are caught, which is what makes
the banner move verifiable rather than cosmetic: the block is load-bearing and now provably so.

## Method note

The first attempt at this battery mis-targeted every mutation: the arm text repeats across sibling
projections, so a substring anchored on `.reason = flight::String("granted")` hit two to five sites. The
harness refused to interpret any of them and reported `MIS-TARGETED` instead. The fix was to scope each
replacement to one function's body by name. A mutation harness that silently replaced the wrong occurrence
would have produced confident nonsense.
