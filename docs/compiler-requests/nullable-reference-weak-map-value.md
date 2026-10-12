# A WeakMap value may be a nullable reference carrier

flight-cpp's `WeakMap` constrains its key because the key must be weak and identity-based. Its value is
ordinary represented storage. The compiler nevertheless rejects a nullable object value even though
both the object reference and its nullable carrier have exact C++ representations.

## Minimal TypeScript

```ts
interface Key {
  id: number;
}

interface Failure {
  kind: string;
}

export function make(): void {
  const warned = new WeakMap<Key, Failure | null>();
  void warned;
}
```

At flight-compiler `2b687ea` this refuses with rule
`cpp-weak-map-value-representation-unproven`:

```text
flight-cpp WeakMap value requires a proven C++ representation
```

## Pinned boundary

| WeakMap value | result |
| --- | --- |
| `Failure` | emits |
| `Failure | null` | refused |
| `Failure | undefined` | refused |
| `{ kind: string } | null` | refused |
| `string` | emits |
| `string | null` | emits |
| `Key['failure']` where `failure: Failure | null` | refused |

The direct nullable spelling and the indexed spelling reach the same rule, so nested indexed-access
resolution is not required to reproduce it. A function-local WeakMap and a module-scope WeakMap also
behave the same. The useful boundary is a nullable or optional **object** carrier; scalar nullable
storage already emits.

`Record<string, string>` is separately refused as a WeakMap value even without nullability. That shows
the planner has a broader represented-value boundary, but it is not needed to explain this P-3D2 site
and is not folded into this request.

## Runtime contract already represents the distinction

The required C++ value type is:

```cpp
flight::WeakMap<flight::Ref<Key>, std::optional<flight::Ref<Failure>>>
```

`WeakMap::get` then returns a nested optional. The outer optional distinguishes an absent map entry;
the inner optional distinguishes a stored source `null` from a stored `Failure`. A pinned native probe
compiled and ran this exact carrier, storing and recovering both null and a reference while keeping a
missing key distinct. No WeakMap runtime addition is required.

## P-3D2 reach and distinct requests

`packages/scene3d-resources/src/enableScene3DResourceFailureGuards.ts` declares a function-local:

```ts
new WeakMap<Scene3DResourceEvent['ref'], Scene3DResourceEvent['ref']['failure']>()
```

The value resolves to `ImageResourceFailure | null`, and the module is a committed first refusal. Its
later module-scope `_guards` map has a non-null local interface value, matching the passing control; it
does not demonstrate the failure.

The `lifecycle` and `media` modules share the generic diagnostic but use different value domains, so
this page does not claim that a nullable-reference fix clears them. Likewise,
[structural rows as WeakMap keys](compiler-structural-row-weak-keys.md) concerns the weak key policy,
while [checked erased WeakMap views](compiler-erased-ref-weak-map-view.md) concerns recovery from
`WeakMap<object, object>`. This request changes neither.

No source narrowing or downstream sentinel substitution is carried.
