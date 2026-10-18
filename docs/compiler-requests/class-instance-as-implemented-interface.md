# A class instance passed as the interface it implements must keep its identity

A class satisfies an interface structurally and is passed wherever that interface is expected. The
callee mutates the interface's data fields, and the caller relies on seeing those mutations. The emitter
lowers the class and the interface to unrelated C++ types, so the call does not compile. A wrapper or
copy would split the mutable state.

## Minimal TypeScript

```ts
interface Reader { pos: number; readUint8(): number }

export class ByteReader {
  constructor(readonly bytes: Uint8Array, public pos = 0) {}
  readUint8(): number { return this.bytes[this.pos++]; }
}

export function consume(read: (reader: Reader) => void, bytes: Uint8Array): number {
  const reader = new ByteReader(bytes);
  read(reader);
  return reader.pos; // sees every advance the callee made
}
```

## Required C++ shape

The class must be usable as the interface without a copy. Either it derives from the emitted interface
struct, or the interface is lowered to a view over the class's exact owner. Field writes through one
must be reads through the other.

## Affected modules

`readSwfTimeline` in `@flighthq/swf` (`swfTimelineParse.ts`) passes a `SwfReader` to each dispatched
`SwfTagHandler.parse(body: SwfTagReader, ...)`. Handlers advance `pos`, `bitPosition` and `valid` on that
reader. `swf_timeline_parse.hpp` and the 8 headers that include it fail with "no known conversion from
`Ref<swf::SwfReader>` to `Ref<types::SwfTagReader>`". No workaround is carried.

## G2 SWF placement handlers join the cause after effect ownership clears

With the effect-owner heritage repaired, `swf_place_object3_handler.hpp` and
`swf_placement_tag_family.hpp` reach this cause: their first failure is
`no viable conversion from 'Ref<flight::types::SwfTagReader>' to 'Ref<flight::swf::SwfReader>'`
at `swf_place_object3_handler.hpp:54`. Two rows joined, taking the current cluster to 12; the
mutable reader owner, parse-callback arguments and placement behavior are unchanged.

## Prepared controls (run once the shared owner-adoption primitive lands)

The SWF shape needs BOTH directions on one owner, and the controls pin each:

1. **source** — a `SwfReader` is constructed and passed to a callback parameterized by the
   structural `SwfTagReader` interface; the callback advances `pos`/`bitPosition`, sets `valid`,
   and calls `readUint8()`/`alignToByte()`. The call must compile and the callback must run.
2. **identity** — the interface reference and the class reference name the SAME object; a pointer
   identity check (`get()` equality) between the forward view and the original class reference, and
   between the reverse view and the original, holds.
3. **live field, both ways** — a write through the interface (`pos = n`, `valid = false`) is read
   through the class instance, and a write through the class is read through the interface; no
   duplicated cell, no copy.
4. **method forward** — `readUint8()` called through the interface reaches the class's
   implementation and mutates the shared cursor (the returned byte and the resulting `pos` match the
   source method).
5. **reverse conversion** — the placement path hands an interface-typed `body` to a helper
   parameterized by the concrete `SwfReader` (`readSwfFilterList`); the reverse view must recover
   the SAME owner (not a downcast of an unrelated object) and keep it alive.
6. **lifetime** — dropping the original class reference while the view is held keeps the owner
   alive; no closure holds a copy of the fields.

A SWF control consumer will drive `readSwfTimeline`-shaped parsing over a small tag stream and the
placement helper through the reverse direction, and assert the cursor and validity outcomes above.
The view family, its names, and its adoption call are taken from the shared primitive; this request
carries no competing primitive, copy closure owner, or downcast-empty fallback.
