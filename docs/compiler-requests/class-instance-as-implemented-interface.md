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

## Prepared call-site mapping (compiler side, pending the shared primitive's contract)

Two call positions need the view, and the emitter knows both from the argument/parameter types it
already threads (`getIrInvocationArgumentExpectedTypeCpp` / `getIrCallArgumentExpectedTypeCpp`):

- **forward** — the argument's evidence is a class declaration and the parameter's expected type is
  an interface the class structurally satisfies (every declared interface member exists on the
  class with a representation-compatible type: data cell or method member). Emitted at that
  argument position, the view adopts the argument's exact owner as the interface.
- **reverse** — the argument's evidence is the interface and the parameter's expected type is the
  class whose members the interface was written from. Same adoption, opposite direction.

Both positions occur in the SWF module (`read_swf_matrix`/`read_swf_color_transform` take the
interface; `read_swf_filter_list` takes the class, and the placement handler passes its
interface-typed `body` to it). The structural satisfaction check is emitted from declarations
already resolved for the argument and parameter; no name-based or shape-guessing rule is involved.
The exact call text is the primitive's adoption entry point, taken from builder2's contract — this
request fixes only WHERE the adoption is emitted and WHEN it is legal, not the family's own API.
No copy, downcast or empty fallback is emitted where the satisfaction check fails; that stays the
existing refusal.

## Measured reproduction of the emitter side

A focused fixture with `interface R { pos: number; read(): number }`, `class C { pos = 0;
read(): number { ... } }` and `use(c)` emits `return use(c);` — a `Ref<C>` passed where `Ref<R>` is
declared, with no refusal and no conversion. The emitter has no lane for a class value at a
structurally satisfied interface parameter; C++ rejects it later, which is exactly how the SWF
headers fail. This is the call-site the bounded view must occupy (see the prepared mapping above).

## Measured constraint on the existing declared-carrier mechanism

`applyCppStructuralValueCarriersCpp` retypes a declared parameter/field by SUBSTITUTING the nominal
type with the carrier's `target`, an existing C++ type. Measured against the SWF pair:

- `flight::NominalOrStructuralView<Nominal, Base>` requires `Nominal` to be a `Base` object (its
  constructor stores `Ref<Nominal>` into `Ref<Base>`, and `of` recovers `Nominal` from a `Ref<Base>`).
  `SwfReader` is not a `SwfTagReader` object in C++, so the template cannot be instantiated for this
  pair; builder2's contract confirms this and recommends an emitted site-specific view instead.
- The retyping path can therefore name only a type that already exists. A class-to-interface view is
  a NEW generated type (accessor cells mirroring the interface's data members over the class's own
  fields, plus method thunks calling the class's methods), which the declared-carrier mechanism does
  not emit.

Implementation increments for the bounded view (authorized shape b):

1. a declared-site kind naming `interface` and `class` plus the parameter/callback sites, validated
   against the pinned source like the existing carriers;
2. emit the view struct once, in the package that declares the interface, with `name()` /
   `set_name(value)` accessors for each interface data member and a thunk per interface method;
3. retype the declared sites to the view type, and lower member access on a view-typed receiver:
   a property read to `receiver.name()`, a property write to `receiver.set_name(value)`, a method
   call to the thunk of the same spelling;
4. at a call argument where a class reference reaches a view-typed parameter, construct the view
   from the class reference and wrap it in an aliasing `Ref<View>` sharing the class's control block;
   at the reverse positions emit `flight::require_owner<Class>(interfaceRef)`.

## Landed: the reverse half (interface value at a class parameter)

`compiler-interface-to-class-owner-adoption` emits
`flight::require_owner<Class>(argument)` at a call argument whose value is an interface the expected
class structurally satisfies (every interface property exists on the class by field or method name).
The concrete owner is recovered from the control-block record of the SAME allocation, or the call
throws `NativeScalarEscapeError` when the value was not allocated as that class — never a copy,
downcast or empty reference. Arguments already declared as the class, and pairs the class does not
satisfy, are untouched.

Measured: the focused test emits the adoption (and leaves a same-class argument alone); the emitted
fixture compiles as C++20 against the pinned runtime; the compiler builds with all 115 patches and
the stack applies/reverts clean. This covers the SWF placement helpers that take the concrete
reader; the forward half (a class instance at an interface parameter) remains the declared-site view.
