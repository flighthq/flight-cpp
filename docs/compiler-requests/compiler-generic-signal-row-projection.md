# A callee's type parameter must not survive into an emitted structural row

Passing a `Signal<T>` through a generic helper leaves the helper's own type-parameter name in the
emitted C++, where it names nothing.

## Minimal TypeScript

The helper (`@flighthq/signals`, `packages/signals/src/slot.ts:64`):

```ts
export function hasSignalSlots<T extends (...args: any[]) => void>(signal: Readonly<Signal<T>>): boolean;
```

and a caller (`@flighthq/power`, `packages/power/src/power.ts`):

```ts
if (power.onIdleStateChange !== null && hasSignalSlots(power.onIdleStateChange)) { … }
```

## Expected behavior

The projection the emitter builds for the argument must name the type argument it was called with —
`std::function<void()>` here — because that is the type the value has. The callee's type-parameter
*identifier* is a source-level name and has no meaning at the call site.

## Observed at `2b687ea` / `a627849`

`generated/include/flight/power/power.hpp:220`, emitted in full:

```cpp
flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<std::shared_ptr<flight::types::Signal<T>>>>>>(
    flight::StructuralRef<flight::RowWritable<flight::RowOf<std::shared_ptr<flight::types::Signal<std::function<void()>>>>>>(idle_signal.value()))
```

The **source** side of the cast spells the concrete `Signal<std::function<void()>>` correctly; the
**target** side spells `Signal<T>`, with `T` undeclared, so the whole translation unit fails:

```
flight/power/power.hpp:220: error: use of undeclared identifier 'T'
```

All three of `@flighthq/power`'s headers fail on it (`power.hpp`, `contract.hpp`, `_internal_index.hpp`),
and the package emits cleanly otherwise: **3/3 modules emitted, 0 refused, 0/3 headers compiling**.

## Affected modules

`packages/power/src/power.ts` today. `hasSignalSlots` has one other SDK caller —
`packages/preferences/src/storage.ts` — so `@flighthq/preferences` is expected to meet the same defect
and was not measured separately here.

## The cause, and the carried patch

The defect is in type-parameter **deduction**, not in row projection. The caller's argument has type
evidence `Ref<Signal<() => void>>`; the callee's parameter is `Readonly<Signal<T>>`, which the
representation planner resolves to `StructuralRef<RowReadonly<RowOf<Ref<Signal<T>>>>>`. Deduction walks
the parameter pattern against the argument evidence, reaches `named` vs `named`, compares target
names — `Readonly` against `Signal` — and stops. Nothing is substituted, `T` survives to emission, and
the row is spelled with it.

The representation already treats the two as one value: `getCppStructuralProjectionRowCpp` answers a
`readonly` row for `Readonly<T>` over the *same* owner, which is why the source side of the cast spells
the concrete `Signal<std::function<void()>>` correctly while the target side does not. The patch makes
deduction agree with that — `collectCppResultTypeSubstitutionsCpp` looks through an ambient
`Readonly<T>` on either side before the nominal comparison, so the type argument is deduced from the
argument's own evidence:

[`compiler-readonly-view-argument-substitution.patch`](../../source-patches/compiler-readonly-view-argument-substitution.patch)

Only `Readonly` is unwrapped; `NoInfer`, unions, and every other wrapper keep the comparison they had,
and an argument whose owner differs from the parameter's still fails deduction — the wrapper is looked
through, the owner is not.

## The reduction

A fixture that writes the helper as an ambient declaration —

```ts
declare function hasSignalSlots<T extends (...args: any[]) => void>(signal: Readonly<Signal<T>>): boolean;
```

— refuses *earlier* and for a different reason (`cpp-runtime-external-symbol-binding-incomplete`), so it
never reaches this defect. The fixture has to **import** the helper from the module that defines it, the
way the real caller does, which is what the harness supports by lowering a fixture inside a package
closure with an explicit resolution edge:

```ts
{ specifier: '@flighthq/signals/contract',
  target: { packageName: '@flighthq/signals', source: 'packages/signals/src/contract.ts' } }
```

`lowerGenericReadonlyViewArgument(owner, argumentType)` in `cppCompilerBackend.test.ts` is that
two-module graph: `@flighthq/signals` declares `Signal<T>` and the helper, `@flighthq/power` imports
both and calls the helper with an `onIdleStateChange: Signal<() => void> | null` — once passing the
parameter straight through, once reaching it through a local read of a property, which is the shape
`power.ts` actually writes. The `owner` parameter reuses the same graph for the negative case.

Three tests carry it:

- **substitutes a generic parameter through a readonly structural view from argument evidence** — both
  call sites emit `flight::RowReadonly<flight::RowOf<flight::Ref<flighthq_signals::Signal<std::function<void()>>>>>`,
  and the consumer contains no `Signal<T>`;
- **does not fill a readonly view parameter with an owner the argument does not share** — with an
  `Emitter<() => void>` argument the call retains `flighthq_signals::Emitter<std::function<void()>>` and
  does not acquire `Signal<std::function<void()>>`;
- **compiles a generic parameter substituted through a readonly view** (skipped when no C++ toolchain is
  configured) — syntax-checks the substituted pair.

## Verification and what it does not cover

The substituted pair syntax-checks against the runtime's installed headers with the pinned C++20
toolchain (`cxx-zig`, clang 22.1.8 / libc++): the emitted consumer compiles, where before the patch the
same translation unit failed on `use of undeclared identifier 'T'`. The C++ backend suite is unchanged
by the patch — 24 failures before and after on this checkout (all pre-existing), 1461 → 1463 passing,
32 → 33 skipped — and the package typechecks clean.

**Residual boundary, recorded rather than hidden:** when *no* argument determines the type parameter,
the emitter still spells it. A structurally compatible argument whose owner is declared under a
different name (`Emitter<() => void>` into `Readonly<Signal<T>>`) is the reproduction; deduction
correctly declines to conflate the owners, and emission writes `Signal<T>` rather than refusing. That is
a separate change — refusing an undetermined type parameter at emission instead of emitting it — and no
SDK module reaches it today: `hasSignalSlots` is called with a `Signal` in both of its callers.

**The residual is shared, the causes are not.** Two other SDK sites reach this same emission step, where
an undetermined parameter is written out by name, through deduction gaps the readonly-view patch does not
cover:
- [an inferred `any` argument](node-any-generic-traits-leak.md) (`@flighthq/command`);
- [an argument that reaches the parameter only through interface heritage](heritage-argument-generic-deduction.md)
  (`@flighthq/movieclip`).

Each keeps its own request because each needs a different deduction fix, and the controls in
[node-any-generic-traits-leak](node-any-generic-traits-leak.md#related-leaks-and-how-they-differ) separate
them. The residual change above would turn both into refusals. It would not make either compile.
