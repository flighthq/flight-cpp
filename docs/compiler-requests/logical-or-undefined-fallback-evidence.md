# An `undefined` logical-OR fallback loses the present value domain

A value logical OR whose final operand is `undefined` is refused while constructing the contextual
`T | undefined` carrier, even when every value-bearing operand has one concrete represented type. The
same value expression with `null` or a concrete fallback already emits.

## Minimal TypeScript

```ts
export function optionalName(value: string): string | undefined {
  return value || undefined;
}
```

At flight-compiler `2b687ea`:

```text
an optional logical-OR result requires concrete type evidence for every value-bearing operand; narrow
or assert an erased operand before applying ||
```

There is no erased value-bearing operand in the fixture. `value` is a concrete string and `undefined`
is only the result's absence sentinel.

The same refusal reproduces in all three P-3D2 contexts that matter: an object-literal optional field,
an assignment to an optional field, and a local explicitly declared `string | undefined`.

## Negative controls

Pinned minimal fixtures give this boundary:

| expression | contextual result | result |
| --- | --- | --- |
| `value || undefined` | `string | undefined` | refused |
| `entry?.text.trim() || undefined` | `string | undefined` | refused |
| `value || null` | `string | null` | emitted |
| `entry?.text.trim() || null` | `string | null` | emitted and covered by the compiler's C++ syntax fixture |
| `value || ''` | `string` | emitted |

This is not the existing erased-operand restriction: that rule correctly refuses `any || null` because
`any` supplies no represented present domain. It is also distinct from negated-truthiness branch
narrowing, dual-sentinel optional storage, and optional-property call-result evidence. None of those
requests determines the result carrier of a value-returning `||` expression.

## Expected behavior

`undefined` contributes no present domain. Retain the string evidence from the left operand and use the
same single-evaluation truthiness lowering already emitted for the `null` control:

```cpp
return ([&]() -> std::optional<flight::String> {
  auto logical_or_value = value;
  if (flight::to_boolean(logical_or_value)) return logical_or_value;
  return std::nullopt;
}());
```

For an optional-chain left operand, return its payload in the truthy branch and the same `nullopt` in
the fallback branch. Empty strings remain falsy; no nullish-coalescing substitution is equivalent.

## P-3D2 reach

Seven production sites in `scene3d-formats` use this exact spelling:

- one light name in `awd2LightingHandler.ts`;
- one skeleton joint name in `awd2SkeletonHandler.ts`;
- four container or mesh-instance names in `awd2SceneStructureHandler.ts`; and
- the current OBJ group name in `objParse.ts`.

Minimal reductions of the nearby `colladaXml.ts` and `colladaMaterial.ts` `|| null` spellings are
passing controls at this compiler pin, not evidence for this request. Those full modules can still
stop on unrelated imported-owner or dependency findings.

## Carrying workaround

None. Rewriting `value || undefined` as `value ? value : undefined` solely for C++ repeats the value in
source and obscures the ordinary JavaScript intent; `value ?? undefined` would incorrectly retain an
empty string. The compiler already has the correct single-evaluation logical-OR machinery and needs to
preserve its present-domain evidence when the final sentinel is `undefined`.
