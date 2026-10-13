# Dependent indexed access must read through the reference carrier

A TypeScript type parameter constrained to a generated interface denotes that interface value. In
native C++, an interface value is carried by `flight::Ref<T>` (or another reference-shaped carrier),
not by the generated object struct itself. A dependent indexed access must therefore inspect the
object behind that carrier.

At Flight revision `a627849`, the materials discriminant helper exposes the invalid spelling:

```ts
export function getMaterialOfKind<T extends Material>(
  material: Readonly<Material> | null,
  kind: T['kind'],
): T | null;
```

```cpp
template <typename T>
std::optional<T> get_material_of_kind(
    /* ... */,
    std::remove_cvref_t<decltype(std::declval<T&>().kind)> kind);
```

Every useful `T` is a `flight::Ref<ConcreteMaterial>`, so instantiation fails because
`std::shared_ptr` has no `kind` member. The same backend spelling affects generic aliases over
interface constraints even though the emitted instantiations also use reference carriers.

The carried compiler repair changes only the unevaluated dependent member expression to
`std::declval<T&>()->kind`. The constraint proof remains unchanged: emission still refuses a key
that is not present on every inhabited constraint arm. No object is dereferenced at runtime,
allocated, copied, or cast; `decltype` only recovers the concrete member type behind the carrier.
A backend fixture pins the carrier spelling, and the native materials test instantiates matching,
mismatching, and null `getMaterialOfKind` paths while asserting exact returned owner identity.

The request expires when dependent indexed access consults the selected C++ representation and
uses its native member-access form, rather than assuming the type parameter is stored by value.
