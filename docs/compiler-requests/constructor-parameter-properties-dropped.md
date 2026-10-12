# Constructor parameter properties are declared but never assigned

A TypeScript parameter property, `constructor(readonly source: Uint8Array, public position: number)`,
declares a field AND assigns the argument to it. The C++ emitter keeps the declaration and drops the
assignment:

```ts
export class Reader {
  offset = 0;
  constructor(readonly source: Uint8Array, public position: number, start: number) {
    this.offset = start;
  }
  read(): number { return this.source[this.position++]; }
}
```

```cpp
struct Reader : public flight::ReferenceEnabled {
  double offset = 0.0;
  flight::Uint8Array source;   // never assigned: stays empty
  double position;             // never assigned: an uninitialized double, so reads are undefined behaviour
  Reader(flight::Uint8Array source, double position, double start) {
    (this->offset = start);
  }
  ...
};
```

The header compiles, and nothing at compile time catches it. The object then reads an empty buffer from a
garbage position. This is a silent miscompilation, not a refusal.

Measured at flight-compiler `2b687ea`, with every carried patch applied, against flight `a627849`. The
repro above emits exactly as shown.

## Expected C++

Each parameter property is initialized from its argument before the constructor body runs, in parameter
order, as TypeScript does:

```cpp
Reader(flight::Uint8Array source, double position, double start) : source(source), position(position) {
  (this->offset = start);
}
```

Both `readonly` and the access modifiers (`public`, `private`, `protected`) create the property; plain
parameters such as `start` do not.

## Affected

These are the only three constructors in the Flight source at a627849 that use parameter properties, and
all three are emitted this way:
- `@flighthq/compression` `deflate.ts` `InflateState(input, position, outputLimit)`. Measured natively,
  `read_bit()` on any input throws "deflate: read past end of stream". So `decompressDeflate` returns
  null for every input, including Node-zlib fixtures, and `compressDeflate` cannot be verified by round
  trip. The package still tiers as ready, because every header compiles.
- `@flighthq/swf` `swfReader.ts` `SwfReader(source, start, end)`: `source` and `end` are never set.
- `@flighthq/abc` `abcFile.ts` `AbcReader(source)`: the constructor body is empty, so `source` is never set.

## Carrying workaround

[`compiler-constructor-parameter-property-assignment.patch`](../../source-patches/compiler-constructor-parameter-property-assignment.patch)
emits `this->field = parameter;` for each parameter property at constructor body entry, from the shared
`createIrClassInitializationPlan` (the plan the Haxe and Rust backends already consume). The assignments
come after default parameter values are resolved, so a defaulted parameter property is read as `.value()`,
and before the constructor's own statements. A derived class's body runs after its base initializer, which
is TypeScript's assign-after-`super()` order.

## Edge cases, stated rather than widened

- A NON-parameter field initializer that reads a parameter property (`len = this.source.length`) would
  run as a C++ default member initializer, before the body assigns the parameter property. TypeScript
  rejects that source under `useDefineForClassFields` (TS2729, "used before its initialization"), and
  none of the three affected classes does it, so the patch does not reorder field initialization.
- A parameter property that also redeclares an abstract base field is assigned through `this->` into
  the base storage, the same field TypeScript writes.
- A parameter captured by a closure that mutates it is still read from the parameter at entry, which is
  the value the property receives in TypeScript.
- SWF and ABC are fixed by the same patch when their packages are next regenerated. This change does
  not move them.
