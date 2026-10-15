# Interaction runtime membership needs property presence through the prototype chain

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, `interactionManager.ts` declares:

```ts
function hasTransform2DRuntime(
  runtime: Readonly<NodeRuntime>,
): runtime is Readonly<NodeRuntime & HasTransform2DRuntime> {
  return 'worldMatrix' in runtime;
}
```

The Interaction header retains the body as:

```cpp
return (flight::String("worldMatrix") in runtime);
```

C++ cannot parse `in`. This also exists on the pre-migration `c676af54` and refreshed
`7341b7f0` floors; the Interaction pin move did not introduce it. The current emitted
parameter is the readonly structural row for `NodeRuntime<NodeTraits>`, not a Record.

The pinned source function was extracted with the pinned TypeScript parser and run
unchanged. It returns false for an absent property, true for an own property whose
value is undefined, and true for an inherited property whose value is undefined.
An inherited getter is not invoked. A replacement that tests the property's value
would lose the present-undefined case; own-key presence would lose inheritance.

`include/flight/structural_ref.hpp` explicitly defines `NamedProperties::has` for own
string keys only. `StructuralRef` itself has no `.has` method. Consequently the
existing `respell-membership-test` Record repair cannot represent this source
operation, and spelling it as `named_properties(runtime).has(...)` would narrow it.

The compiler/runtime contract needs a presence operation which preserves these
observations on the actual row owner, including inherited properties, before this
body can be lowered faithfully. The migration leaves this body and its compile
failure visible. It neither manufactures a transform runtime nor substitutes a
constant result, value read, own-key predicate or throwing replacement.

See [literal membership lowering](membership-test-lowered-literally.md) and
[Record membership and prototypes](record-in-operator-lowered-verbatim.md). This
Interaction instance remains a distinct runtime carrier requirement; no runtime
implementation or broader membership repair is included here.
