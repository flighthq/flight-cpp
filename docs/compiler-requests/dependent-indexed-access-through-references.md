# `T['key']` on a type parameter must read the member through the type argument's carrier

Measured at flight-compiler `2b687ea` with every carried patch, against flight `a627849`.

## Minimal TypeScript

```ts
export type U = { readonly outcome: 'updated' } | { readonly error?: unknown; readonly outcome: 'image-update-failed' };
export interface Obj { readonly outcome: string; readonly n: number }

export function f<Result extends { readonly outcome: string }>(failure: Result['outcome']): Result['outcome'] { return failure; }
export function useU(): string { return f<U>('image-update-failed'); }
export function useObj(): string { return f<Obj>('x'); }
```

## Emitted

```cpp
template <typename Result>
inline std::remove_cvref_t<decltype(std::declval<Result&>().outcome)> f(std::remove_cvref_t<decltype(std::declval<Result&>().outcome)> failure);
inline flight::String use_u()   { return f<U>(flight::String("image-update-failed")); }
inline flight::String use_obj() { return f<flight::Ref<Obj>>(flight::String("x")); }
```

Both calls fail natively. `Result` is never the struct itself:
- for `use_u`, it is a `std::variant` of `Ref`s: `substitution failure [with Result = U]: no member named 'outcome' in 'std::variant<...>'`;
- for `use_obj`, it is a `flight::Ref<Obj>`, which is a `std::shared_ptr`, with the same substitution failure.

The header gate never instantiates the template, so the declaration compiles and every caller fails.

## Where it shows

- **`@flighthq/tray`:** `invokeRead` (`failure: Result['outcome']`). Its 3 callers fail natively in a scratch
  regeneration with `no matching function for call to 'invoke_read'`. `invokeUpdate` has the same parameter.
- **`@flighthq/materials` (committed tree):** `get_material_of_kind<T>(…, std::remove_cvref_t<decltype(std::declval<T&>().kind)> kind)`.
  Every documented use passes `T = Ref<BlinnPhongMaterial>`, which cannot instantiate.

## Required behavior

The emitter's gate is right and stays: it emits the dependent form only where the constraint proves the member on
every branch, and it keeps the instantiation's own member type rather than the constraint's. Only the spelling has
to read the member the way the type argument stores it:
- **through a `Ref`:** the referenced struct's member;
- **through a structural row:** the row schema's member;
- **through a variant:** the member every alternative stores. If the alternatives disagree on its C++ type, or
  one lacks it, the instantiation is a compile-time error rather than an answer from one alternative.

## Proposed lowering

The runtime trait `flight::indexed_member_t<T, flight::RowKey<"key">>` (in `flight/structural_ref.hpp`) resolves the
member through the generated key table that rows already read members through. The emitter spells a dependent
`T['key']` with it.

The table is derived from the `RowKey<"…">` spellings the emitter writes. The keys these sites need, `outcome` and
`kind`, are already in it, so the table does not change.
