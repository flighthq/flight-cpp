# `new Set(collection.values())` must lower without a contextual optional construction

A refusal, not a miscompile. `new Set(iterable)` with a `values()` or `keys()` iterator argument is refused:

```
contextual optionalSingle construction requires expression type evidence
```

The Set constructor's parameter is `Iterable<T> | null | undefined`, so the emitter lowers the argument as a
contextual optional union. It needs type evidence for the argument expression to do that, and an iterator call
(`set.values()`, `map.keys()`, `array.values()`) has none. The construction is unnecessary anyway: these calls
never produce `null` or `undefined`.

`new Map(map.entries())` is not affected, because the Map constructor already gives its argument a dedicated
expected type.

## Reproduction

Measured with flight-compiler `2b687ea` and every carried patch:

| Form | Before |
| --- | --- |
| `new Set(set.values())`, `new Set(set.keys())`, `ReadonlySet`, `new Set<string>(set.values())` | refused |
| `new Set(map.keys())`, `new Set(map.values())` | refused |
| `new Set(array.values())` | refused |
| `new Set(set.entries())` | refused |
| `new Set(set.values())` with `set: ReadonlySet<T>` (generic) | emitted |
| `const it = set.values(); new Set(it)` | refused: `contextual union value type flight::Any is not a represented runtime domain` |

## Required C++ shape

A `values()` or `keys()` call on a Set, Map or Array receiver is passed to the range constructor as it is. The
range constructor reads every value in order into new storage:

```cpp
return flight::Set<flight::String>(set.values());
```

Node semantics this must keep (node 22):
- `new Set(s.values())` is independent of `s` and keeps insertion order;
- `new Set(map.values())` de-duplicates in first-seen order;
- iterating a Set while adding to it visits the added value.

## Boundaries, deliberately not changed

- **`new Set(x.entries())`:** `flight::Set` has no `entries()`. Also, a Set built from entries holds a fresh
  `[value, value]` array per element, with its own reference identity. That is a runtime capability, not a
  lowering detail. It stays refused.
- **A stored iterator (`const it = set.values(); new Set(it)`):** the local's type reaches C++ as `flight::Any`,
  so it fails on a different, earlier rule. Fixing it means preserving iterator types through locals, which is a
  broader inference change. It stays refused.

## Affected modules

None in the committed SDK at flight a627849. This is a preventive fix for a refusal reproduced with focused
compiler fixtures, where the receiver is a parameter.

The committed refusal ledger has 12 refusals with this message (`cpp-contextual-union-missing-expression-type`).
None of them is a `new Set(<iterator>)` site; they are other constructs that reach the same rule.

The two SDK sources that build a Set from a Map's `keys()` behave as follows:
- `scene2d-formats` `riveRequirements.ts:65`, `new Set(registry.handlers.keys())`, already emits in package context
  as `flight::Set<double>(registry->handlers.keys())`, because its receiver is a typed property with evidence;
- `scene3d-formats` `awd2LightingHandler.ts:403` is in a module that is not emitted at all, for other reasons.

## Carried compiler patch

[`compiler-new-set-from-iterator.patch`](../../source-patches/compiler-new-set-from-iterator.patch) emits the
single argument of a flight-cpp `new Set(...)` as itself when it is a non-optional, argument-free `values()` or
`keys()` call on a Set, ReadonlySet, Map, ReadonlyMap or Array receiver. It then reaches the range constructor.
`entries()` and stored iterators are not matched and stay refused. Fixtures are in `cppNewSetIterator.test.ts`.
Native behavior matches node 22 for every matched form. No SDK row was regenerated.
