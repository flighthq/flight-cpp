# Optional index/member flow must preserve the present payload

An optional chain supplies its own presence branch. The flight-cpp backend preserves that evidence for
an ordinary optional property and for an indexed object followed by a property read, but not consistently
when the indexed payload is the receiver of a member call. An inferred `optional ?? {}` local loses the
same present-object fact before a later optional member read.

## Indexed member-call reduction

```ts
export function read(values: readonly string[], index: number): string | undefined {
  return values[index]?.trim();
}
```

At flight-compiler `2b687ea` this refuses:

```text
member trim on optional C++ storage requires proven present payload
```

Binding the same indexed result and writing the guard explicitly emits correctly:

```ts
const value = values[index];
if (value === undefined) return undefined;
return value.trim();
```

A `Record<string, string>` indexed optional chain does not reproduce the refusal. It instead emits an
invalid pre-projection:

```cpp
auto optional_chain_receiver = values.get(key).value();
if (!optional_chain_receiver.has_value()) return std::nullopt;
return optional_chain_receiver.value().trim();
```

`Record::get` returns `std::optional<flight::String>`, so the first `.value()` makes the local a
`flight::String`; the following `has_value()` and `value()` cannot compile. This is a negative reduction,
not a passing control.

## Ordinary member and coalesced-object controls

Pinned fixtures give this boundary:

| source shape | result |
| --- | --- |
| `row.text?.trim()` for `text?: string` | emits a correct presence check and one unwrap |
| `rows[index]?.name` for required `name: string` | emits a correct indexed presence check |
| bind `rows[index]`, guard `=== undefined`, then call a member | emits correctly |
| `strings[index]?.trim()` | refused before the call |
| `record[key]?.trim()` | emits the invalid pre-unwrap above |

One related ordinary-member shape reaches the same evidence family as the current `gltfParse.ts` first
refusal:

```ts
interface Pbr { readonly baseColorFactor?: readonly number[] }
interface Material { pbr?: Pbr }

export function red(material: Material): number {
  const pbr = material.pbr ?? {};
  return pbr.baseColorFactor?.[0] ?? 1;
}
```

The inferred local refuses at `pbr.baseColorFactor` because its C++ carrier still admits absence. Adding
the source annotation `const pbr: Readonly<Pbr> = material.pbr ?? {}` emits, proving that the optional
member and fallback are representable; the inferred union has lost the present structural owner. This
small fixture deliberately omits the full module's imported outer `Readonly<GltfMaterial>` view. Adding
an equivalent `Readonly<Material>` wrapper makes the reduction stop one stage earlier on "property
baseColorFactor on a C++ variant requires proven union member access". It does not reproduce the
committed module's exact winning diagnostic.

## Expected lowering

For an indexed optional-chain receiver, test the lookup carrier before projecting it:

```cpp
return ([&]() -> std::optional<flight::String> {
  auto receiver = values.get(index);
  if (!receiver.has_value()) return std::nullopt;
  return receiver.value().trim();
}());
```

For `optional ?? {}`, either retain a non-optional structural view whose cells are optional, or make an
absent internal shortcut behave as the empty object's missing-property result. It must not be treated as
an absent receiver when the source has already selected a present fallback object. Writes or escapes
that observe the fallback object's identity need a real object; a read-only shortcut is not a general
replacement.

## P-3D2 reach and non-reproductions

The source lane has nine indexed optional-chain occurrences. Six string member calls are in
`scene3d-formats/stlFeatures.ts`, and the indexed `attributes[attributeName]?.trim()` in
`colladaMaterial.ts` is the Record form above. Those seven sites need the indexed member-call rule.
The remaining two — `skeleton3d/enableSkeleton3DGuards.ts` and
`scene3d-formats/gltfAnimations.ts` — read a property from an indexed object and match the passing
minimal control; they are not evidence for this request.

`colladaMaterial.ts` is recorded as refusing on `trim`, but its ordinary local Record reduction emits
invalid C++ instead of reproducing that refusal. `gltfParse.ts` is recorded with the absence-carrying
`baseColorFactor` refusal; its outer-`Readonly` local facsimile instead reaches the adjacent variant-member
refusal described above. Both full imported-owner/package contexts therefore need a rerun after a fix;
this page does not claim the minimal fixtures explain which diagnostic wins there.

This is distinct from [optional property-call result evidence](optional-property-call-result-evidence.md),
where the callable result type is missing, and from [narrowed union presence](narrowed-union-presence-proof.md),
where an explicit guard has not projected a dual-level carrier. Here the indexed optional chain or
nullish fallback already provides the present-flow fact.

The scene-document YAML-subset parser now carries one source rewrite in this family: its private
`tokenAt` helper preserves the nominal token owner while two sequential guards spell the same line and
first-token absence branches as `line?.tokens[0]`. No generated repair is carried.

## Third and fourth measured sites: `@flighthq/useragent`

Both halves of this request's pattern turn up in one package, at the same pin, and the second one shows the
invalid pre-lowering being emitted on a line whose *other* half does it correctly.

**The refusal, from a bound match result** (`packages/useragent/src/userAgentParse.ts:86`,
`cpp-member-projection-without-present-storage [compiler-restriction]`):

```ts
const ios = ua.match(/(?:iphone|ipad|ipod).*?os\s+([\d_]+)/i);
if (ios) return ios[1].replace(/_/g, '.');
```

```
member replace on optional C++ storage requires proven present payload: nothing proved the payload present,
so the read would unpack storage that may be holding absence. Guard the receiver, read it through an optional
chain, or narrow it into a present local first
```

The guard is present in the source (`if (ios)`), so this is the same evidence family as the indexed
member-call reduction above with one more step in it: the guarded value is indexed (`ios[1]`) before the
member call. The module's other two parsers emit, so one function's refusal withholds the whole module from
the package's callers (see the fourth site in
[retained-declaration-calls-omitted-callee.md](retained-declaration-calls-omitted-callee.md)).

**The invalid pre-lowering, from a dynamic member through an optional chain**
(`userAgent.ts`, `parseUserAgentRuntime`, emitted with no refusal marker at all):

```ts
const proc = win.process as Record<string, unknown> | undefined;
if (proc?.versions && (proc.versions as Record<string, unknown>).electron) return 'electron';
```

```cpp
auto optional_chain_receiver = proc;
if (!optional_chain_receiver.has_value()) return std::nullopt;
return optional_chain_receiver.value().versions;        // a STATIC member on a Record carrier
...
flight::named_properties(proc.value().get(flight::String("versions")).value())   // the same read, dynamic
```

`Record` has no `versions` member, so the first read does not compile while the second, on the same line, is
the dynamic read the first should have been. This is the same invalid pre-projection as the `record[key]?.trim()`
reduction above, on a member read rather than an indexed one, and it is the shape that makes the whole module
uncompilable -- which matters here because the module carries **no** PARTIAL banner: nothing in the emitted
header records that its own body cannot compile.

Carrying workaround: `@flighthq/useragent` supplies the refused function and replaces this one body in
`overrides/include/flight/useragent/`, keeping every other emitted line verbatim.

## Disjunctive absence guard: `a === undefined || b === undefined`

A guard that returns when *either* of two indexed locals is absent proves both present afterwards. The
emitter keeps the presence of neither. Each optional is then passed as itself where a present value is
required: no refusal, and a header that does not compile.

Measured at flight-compiler `2b687ea` with this repository's 44 compiler patches applied. Both modules emit.
The call site differs only in the guard:

```ts
export interface Item { weight: number; }
export interface Holder { items: Item[]; }
function total(a: Readonly<Item>, b: Readonly<Item>): number { return a.weight + b.weight; }

export function pairWeight(holder: Holder, i: number, j: number): number {
  const a = holder.items[i];
  const b = holder.items[j];
  if (a === undefined || b === undefined) return -1;
  return total(a, b);
}
```

```cpp
// disjunctive guard: the optionals themselves
return total(… StructuralRef<RowWritable<RowOf<Ref<Item>>>>(a) …, … (b) …);
// two guards, `if (a === undefined) return -1; if (b === undefined) return -1;`: the payloads
return total_c(… StructuralRef<RowWritable<RowOf<Ref<ItemC>>>>(a.value()) …, … (b.value()) …);
```

`@flighthq/physics2d` `step.ts` `resolveEarliestPhysics2DImpact` (line 1084) has this guard over two colliders
read by index. It is carried by `physics2d-earliest-impact-split-presence-guards`, which writes the two guards
separately, in the same order, with the same early return.
