# An interface assertion between flattened owners must read the same owner's recorded cells

`@flighthq/loader`'s `getResourceLoadProgress` binds `const internal = loader as ResourceLoaderInternal`
and reads only member cells: `started`, `reports`, `pending`, `inFlight`, `total`, `totalWeight`,
`weightLoaded`, `loaded`. `ResourceLoaderInternal extends ResourceLoader`, but flight-cpp flattens
interface heritage into independent structs and `flight::Ref` is a shared pointer with no
base-to-derived constructor, so the emitter refuses the assertion with
`cpp-reference-assertion-without-heritage`. The header that holds the progress body is unusable, and
the two retained uses of the refused function keep it that way even though the loader's
initializer, factory, and reject boxing already emit.

The runtime already retains what the assertion needs: an object constructed through a construction
row keeps the same `RowOwner`, and `flight::detail::find_row_owner` is the non-creating lookup for
it. `flight::StructuralMemberView<Base>(Ref<Base>)` reads that owner's named cells by name and
returns `flight::MemberValue<T>`, which keeps ABSENCE distinct from a stored false/0: absence is
falsy, converts to NaN in numeric use so comparisons are false and arithmetic propagates, and
throws at a member call or iteration — the source's own `undefined` failing at its use. Only the
present value's Array/Set handle is copied, so live identity and order are preserved.

## Lowering

A cast that asserts one interface to another it transitively extends is lowered to the same-owner
view instead of a pointer cast:

```cpp
const internal = loader as ResourceLoaderInternal;
// flight::StructuralMemberView<flight::Ref<ResourceLoader>>(loader)
internal.started;                 // (internal).read<bool>("started")
internal.reports.filter(...)      // (internal).read<flight::Array<flight::Ref<Report>>>("reports")->filter(...)
for (const entry of internal.inFlight) { ... }   // range-for over the live member handle
```

The source reference is never cast, copied, or reinterpreted, and no replacement owner is
materialized. The lowering is restricted on three axes:

- **Source shape**: the asserted value must be an existing identifier reference (a parameter or
  local). A factory call result is not lowered: the factory allocated and returned the base owner
  itself, so the target's cells were never recorded on it, and a view would read absence the
  construction never wrote. That remainder keeps the existing refusal (and its tests).
- **Heritage**: the target declaration must be an interface that transitively extends the source
  interface. An unrelated interface or a class keeps the refusal.
- **Uses**: the view is readable through member reads, calls through those reads, and iteration over
  an array member. Passing, returning, assigning, or mutating the view refuses with
  `cpp-structural-member-view-escape`, because no carrier survives that escape.

## Disposition

Carried by source patch `compiler-structural-member-view-assertion`, with focused tests in
`cppStructuralMemberView.test.ts` (view emission, escape refusal, no-heritage refusal) and the
runtime absence/owner behavior already covered by `runtime_test.cpp`'s
`test_structural_member_view_absence`. It expires when the C++ emitter lowers this interface
assertion itself, or when the runtime contract offers a checked dynamic-owner recovery that the
emitter elects instead.
