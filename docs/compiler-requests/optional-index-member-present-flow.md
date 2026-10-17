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


## G2 glTF punctual lights lose an indexed payload after a loop guard

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, cluster `variant-cast-37507e07` has two recorded
failures at `flight/scene3d_formats/gltf_punctual_lights.hpp:216`. Despite the historical cluster
name, the first rejected value is `optional<Ref<GltfPunctualLight>>`, not a multi-arm variant.
Source `packages/scene3d-formats/src/gltfPunctualLights.ts:18-26` binds an indexed definition,
then explicitly continues when it is undefined, before passing the present object to a readonly helper:

```ts
const source = definitions[lightIndex];
if (source === undefined) {
  tallyLightDrop(lightDrops, ImportDiagnosticSeverity.Drop, 'gltf.light-missing', {
    firstLight: lightIndex, firstNode: node,
  });
  continue;
}
const descriptor = buildGltfPunctualLight(source, lightIndex, lightDrops);
```

Native208 retains the optional Array::get result and native209 tests its presence. Native216 nevertheless
constructs the writable structural row from the entire optional, then casts that row readonly:

```cpp
std::optional<flight::Ref<flight::types::GltfPunctualLight>> source = definitions.get(light_index.value());
if (!source.has_value()) { /* record missing definition, then continue */ }
// The actual conversion's reduced type aliases:
build_gltf_punctual_light(structural_ref_cast<ReadView>(WriteView(source)),
                         light_index.value(), light_drops);
```

ReadView/WriteView above abbreviate the emitted RowReadonly/RowWritable RowOf<Ref<GltfPunctualLight>>
carriers; they are not new declarations. StructuralRef accepts an exact shared reference or a proved row,
not optional<Ref<T>>. The loop guard proves this lookup present, so faithful emission projects
source.value() and creates a readonly view of that same source owner, for example ReadView(source.value()).
No source mutation, copied light definition or unchecked pointer cast is needed or permitted. Native221
already projects source.value()->name, showing that the same local's member read and helper conversion
are inconsistent. This is the existing indexed-present-flow request at a loop continue guard: the
ordinary explicit-return controls and disjunctive-guard reductions earlier in this page are not claimed
to reproduce this complete imported-owner/handler context. This G2 audit does not rerun those reductions.

Adding an optional-taking StructuralRef constructor would have to invent the absent case for a required
helper parameter and still would not repair the omitted builder. A source guard is already present;
repeating an annotation/guard is not evidence of corrected emission. Declaration-only repair cannot
replace this retained conversion expression. No source/compiler/runtime prototype or normal generation
is carried for this documentation-only disposition.

### The independent light-owner refusal remains open

The module explicitly omits buildGltfPunctualLight47 with
`cpp-contextual-union-interface-heritage-carrier-unrepresented`. Its declared Light|null result
first returns the exact createDirectionalLight owner; point and spot branches return their own exact
owners. [The existing nullable-heritage request already records this exact source site](compiler-interface-heritage-owner-widening.md#same-defect-in-p-3d2).
Light, DirectionalLight, PointLight and SpotLight are independently flattened generated structs with
ReferenceEnabled, not a declared native inheritance chain. Extracting source.value() cannot supply that
missing algorithm or relation. Neither copying a Light prefix nor casting an unrelated shared pointer
can preserve the real factory result's owner, kind and live cells. No declaration candidate or positive
unused compile/link/run or qualified negative-link gate was attempted or claimed here.

Distinguish the helper result from its eventual sink: unpatched Scene3DDocumentLight.descriptor declares
open Light, while the existing scene3d-document-light-values source patch changes document storage to
LightValue. The generated sink retains eight exact reference arms: DirectionalLight, AreaLight, Light,
Environment, AmbientLight, HemisphereLight, SpotLight and PointLight. The open Light arm preserves vendor
kinds, and each built-in arm retains its original allocation. That patch does not change this helper's
Light|null signature or make the directional nullable return emit. It is unchanged here; document storage
must not be narrowed to only the three punctual kinds, reduced to base copies or replaced with fake nodes.

### Full handler, kind and placement contract

- The handler is opt-in, kind KHR_lights_punctual. Its family registration adds that exact handler to
  the caller-owned list, replacing a prior same-kind entry in place. It is not a global install.
  Missing top-level definitions/nodes use their source empty-array fallbacks. Iterate source nodes in
  order and skip only an undefined light reference. An unresolved definition records Drop
  gltf.light-missing with firstLight/firstNode; it must not become a default light or abort other nodes.
- Build a new descriptor for each successful node placement, even when multiple nodes reference the
  same definition. Preserve source name, context.nodeIndices[node], and the exact transform returned
  by context.buildNodeTransform(node), then append to the original document.lights array. No node
  fabrication, index renumbering, descriptor deduplication/cache or replacement document is justified.
  Descriptor direction is local -Z for directional/spot, and a point starts at its local origin;
  the stored Transform3D places it. Do not bake world transforms into the descriptor and compose again.
- Preserve color's white fallback and individual missing RGB component defaults, alpha1, and the real
  packLinearToColor sRGB packing. Intensity defaults to1 and rejects !(intensity>=0), including NaN;
  zero/-0 are valid and +Infinity is not rejected by this predicate. Directional returns before range
  validation. For other kinds, range defaults to -1 and accepts exactly that sentinel or a positive
  value; do not add a finite-only rule or reinterpret intensity units. Use the actual factory defaults,
  including their UnitlessLightUnit, rather than replacing photometric or shadow behavior.
- Point and spot remain separate real factories. Spot defaults inner=0/outer=pi/4 and requires inner>=0,
  outer>inner and outer<=pi/2, then converts radians to factory degrees. Unsupported dynamic type data
  reaches Skip gltf.light-unsupported-type (firstType), after the source intensity/range checks; it is
  not coerced into a point light. The source schema's three literal types and native String carrier
  do not remove this fallback: existing tests deliberately exercise an unsupported type.
- When diagnostics is absent, tallying is a no-op. Otherwise keep one local Map keyed by diagnostic
  kind: preserve the first severity/detail object, increment count for each later occurrence (including
  repeated node references), and flush one crumb per kind in insertion order after traversal. Spread
  the first detail into a fresh record with count and report origin buildGltfPunctualLight to the same
  context diagnostics array. Do not report per definition, drop first-offender detail or swallow factory/
  transform exceptions. This handler owns its aggregation, separate from core parser tally state.

The existing gltfPunctualLights tests specify three real placed light kinds, transform/name/index values,
opt-in/required-extension behavior, missing/negative/range/cone/unsupported diagnostics, repeat aggregation
and absent collectors. The family-registration test specifies same-kind replacement by the exact handler.
These were read as source requirements only. Inspected extension Record/object recovery and diagnostics
construction are not newly measured next errors or native behavior/link evidence.

**Recorded 0/2 pass, 2 fail is retained.** The owned headers are
`flight/scene3d_formats/{gltf_punctual_lights,register_gltf_lighting_extension_handlers}.hpp`.
Their two rows and cluster link this existing request and the already-filed nullable-heritage request.
Only this append-only note, worklist96 and three compact owned product lines change. C++, owner types,
source patches, repairs, generation inventory and pins are unchanged; unchanged C++ is not rerun.
Foreign compact product lines/worklist entries and historical/G1 totals remain intact. No native
header-availability, light import/placement, parser, rendering or link success is claimed.
