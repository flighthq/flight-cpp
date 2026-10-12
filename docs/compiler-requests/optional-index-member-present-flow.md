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

No source rewrite or generated repair is carried.
