# One frozen constant serving two outcome unions needs one carrier per union

A module-level frozen value can be structurally valid in two different unions at once, and the source relies on
that: `OPERATION_FAILED` is returned from a function whose result is `AppUpdateCheckOutcome` and from one whose
result is `AppUpdateInstallOutcome`. The emitter gives the constant a single carrier -- the first union's
readonly structural row -- and then returns it from the function of the *second* union, whose emitted carrier is
a nominal `Ref`. The retained body therefore cannot compile, and the header says it did.

Measured at flight `a627849` / flight-compiler `2b687ea` against `@flighthq/updater`, slice regenerated under
binding profile set `070fadeb2a07`. The same body has a second failure -- it names the refused constant
`install_ok` -- filed as
[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md).

## Minimal TypeScript

```ts
interface CheckOk { reason: 'ok'; update: DownloadedUpdate }
interface CheckProgress { reason: 'check-in-progress' }
type AppUpdateCheckOutcome = CheckOk | CheckProgress | { reason: 'operation-failed' };
type AppUpdateInstallOutcome = { reason: 'ok' } | { reason: 'operation-failed' };

const OPERATION_FAILED = Object.freeze({ reason: 'operation-failed' });

export async function check(): Promise<AppUpdateCheckOutcome> {
  return OPERATION_FAILED;
}
export async function install(): Promise<AppUpdateInstallOutcome> {
  return OPERATION_FAILED;
}
```

Both functions are well typed and the constant is one object: TypeScript's structural typing lets the same
frozen literal inhabit both unions, and `Object.freeze` is what keeps it from being mutated through either.

## Observed

`generated/include/flight/updater/updater.hpp` declares the two unions with unrelated carriers -- the check
outcome is a `std::variant` of readonly structural rows (`flight/types/updater.hpp:57`), the install outcome is a
nominal struct (`:59`) reached through `flight::Ref` -- and gives the shared constant the *check* union's
carrier:

```cpp
inline flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<reason_21a7169b3eff3d17>>>>
    operation_failed = flight::structural_ref_cast<...>(...);
```

The retained install body then returns it:

```cpp
    if ((outcome->reason != flight::String("ok"))) { co_return operation_failed; }
    ...
    co_return operation_failed;
```

Emission alone, no override, that header gives five errors:

```
generated/include/flight/updater/updater.hpp:138:7: error: no matching member function for call to 'return_value'
  ... no matching constructor for initialization of 'std::shared_ptr<flight::types::AppUpdateInstallOutcome>'
generated/include/flight/updater/updater.hpp:141:15: error: use of undeclared identifier 'install_ok'
generated/include/flight/updater/updater.hpp:144:5: error: no matching member function for call to 'return_value'
```

`contract.hpp` and `_internal_index.hpp` report the same five, because they re-export the module. The check
side cannot use the constant either, and that is the sharper half of the measurement: the emitted readonly row
does **not** convert into the check union's reason-only arm, whose declared struct is a different type --
`is_constructible_v<AppUpdateCheckOutcome, StructuralRef<RowReadonly<RowOf<Ref<reason_21a7169b3eff3d17>>>>>` is
false (measured; the compiler's own diagnostic when the supplied check function tried to return it). So the one
carrier the emitter chose serves *neither* union, while the module's own banner above it reads

```
// PARTIAL: the C++ emitter refused declarations in this module. Everything else compiled, and each
// omission is marked NOT GENERATED below with the reason. This file is NOT complete.
```

Everything else did not compile; two of the omissions are enough to take the header down.

## Required C++ shape

A frozen value that two unions share has to be materialized once per union, each as that union's own arm, and
neither may be a cast from the other's carrier -- the refusal for the sibling constant states why in its own
words: "a Readonly structural view does not become a mutable nominal Ref". For the module above:

```cpp
inline const flight::types::AppUpdateCheckOutcome check_operation_failed = <the union's reason-only arm>;
inline const flight::Ref<flight::types::AppUpdateInstallOutcome> install_operation_failed =
    flight::make_ref<flight::types::AppUpdateInstallOutcome>(
        flight::types::AppUpdateInstallOutcome{.reason = flight::String("operation-failed")});
```

The previous slice of this package (binding profile set `0a2183634245`) built the install-side constant exactly
this way -- `structural_ref_cast<flight::Ref<AppUpdateInstallOutcome>>(make_ref<reason_5d202cf6c17c5b52>(...))`
for `INSTALL_OK` -- so a per-union carrier is the shape the emitter already reaches for; what regressed is
which constant gets it. If a per-union materialization is not something the emitter wants to infer, refusing
the *shared* constant (as it already refuses the two `as`-asserted siblings) and saying so in the banner would
be honest and would leave the module's other bodies compilable.

## Distinguish from the requests already filed

[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md) is this same body
failing at a *name* the module omitted. [`named-union-arms-not-canonical.md`](named-union-arms-not-canonical.md)
is about one union whose arm is spelled two ways across two uses; here each union is internally consistent and
the disagreement is *between* two unions that the source shares one value across.
[`distinct-union-domains-one-carrier.md`](distinct-union-domains-one-carrier.md) is the reverse direction: a
union the emitter should have kept as one carrier. This one is a value the emitter should have split into two.

## Carrying workaround

`overrides/include/flight/updater/updater.hpp` -- the module verbatim at this emission, with the shared
constant materialized per union and the emitted install body's two uses renamed to the install-side one.
`@flighthq/updater` compiles 3/3 headers with it and 0/3 without it.
