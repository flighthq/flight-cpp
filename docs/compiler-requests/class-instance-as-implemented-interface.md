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
