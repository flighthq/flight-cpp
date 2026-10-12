# Projecting a produced record into a row needs a represented object-reference target

Three module constants in `@flighthq/shortcut` are refused because the emitter will not project a value it
just produced into a structural row: `Object.freeze({ reason: … })` is an object reference, the union arm
it stands for is a row over the declared carrier, and no represented target exists for the projection. The
same projection in the *same file* is emitted correctly for the attach constants, so the rule is applied
inconsistently rather than missing.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/shortcut`.

## Observed

`generated/include/flight/shortcut/shortcut_explicit_dependency.hpp:143`, `:151` and `:159` — three
consecutive omissions, one per constant:

```
cpp emission failed for @flighthq/shortcut/packages/shortcut/src/shortcutExplicitDependency.ts:
structural-row projection requires a represented object-reference target
```

The sources they stood for:

```ts
const NOT_REGISTERED = Object.freeze({ reason: 'not-registered' }) as GlobalShortcutQueryOutcome;
const QUERY_PROVIDER_FAILED = Object.freeze({ reason: 'query-provider-failed' }) as GlobalShortcutQueryOutcome;
const REGISTERED = Object.freeze({ reason: 'registered' }) as GlobalShortcutQueryOutcome;
```

and the arm they belong to:

```cpp
using GlobalShortcutQueryOutcome = std::variant<
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<parse_error_reason_541454762c1ce542>>>>,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<reason_fbb74a314a7a269d>>>>>;
```

## What the same file already proves is lowerable

The attach constants beside them are emitted with exactly this projection, against their own arm:

```cpp
inline flight::types::GlobalShortcutAttachOutcome already_attached =
    flight::structural_ref_cast<flight::types::GlobalShortcutAttachOutcome>(
        flight::make_ref<reason_e78bc865cc377071>(reason_e78bc865cc377071{.reason = flight::String("already-attached")}));
```

The object is produced, the row is its own subject, and the cast projects one to the other without copying,
retyping or re-homing anything. The refused form differs only in that the target is a *variant* whose
alternative is that row, which the emitter could construct exactly as it constructs the attach arm.

## Where it fires

| constant | source line | consequence |
| --- | --- | --- |
| `NOT_REGISTERED` | 31 | the query path's three non-error answers are unavailable |
| `QUERY_PROVIDER_FAILED` | 32 | |
| `REGISTERED` | 33 | |

The emitted `queryGlobalShortcutRegistration` returns all three, so the module does not compile until they
exist — the shape filed as
[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md).

## Carrying workaround

`overrides/include/flight/shortcut/shortcut_explicit_dependency.hpp` supplies the three constants with the
projection the emitted attach constants already use; `@flighthq/shortcut` compiles 4/4 headers on that
basis.

## Third measured site: `@flighthq/updater`

The same rule, and the sharpest form of the same inconsistency: four sibling constants of one file are the
same kind of frozen object literal, and the emitter treats them three different ways.

`packages/updater/src/updater.ts` lines 8-11:

```ts
const CHECK_IN_PROGRESS = Object.freeze({ reason: 'check-in-progress' }) as AppUpdateCheckOutcome;
const NOT_AVAILABLE = Object.freeze({ reason: 'not-available' }) as AppUpdateCheckOutcome;
const OPERATION_FAILED: Readonly<{ reason: 'operation-failed' }> = Object.freeze({ reason: 'operation-failed' });
const INSTALL_OK = Object.freeze({ reason: 'ok' }) as AppUpdateInstallOutcome;
```

Lines 8 and 9 are refused with exactly this rule:

```
cpp emission failed for @flighthq/updater/packages/updater/src/updater.ts: structural-row projection
requires a represented object-reference target
```

Line 11 is refused under a different rule
([`readonly-mesh-to-writable-node-authority.md`](readonly-mesh-to-writable-node-authority.md)). **Line 10 is
emitted** -- the one whose type is a declared readonly row rather than an `as` assertion to a union -- and what
the emitter emits for it is a module-local anonymous struct plus its readonly view:

```cpp
struct reason_21a7169b3eff3d17 : public flight::ReferenceEnabled { flight::String reason; };
inline flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<reason_21a7169b3eff3d17>>>>
    operation_failed = flight::structural_ref_cast<...>(...);
```

So the projection these constants need exists and is applied to the sibling spelling. What it does not do is
land on the *union's* arm: `reason_21a7169b3eff3d17` is a second, module-local spelling of the shape
`flight::types` already declares as `reason_01523507f7b21613`, and the emitted union cannot hold it
(`is_constructible_v<AppUpdateCheckOutcome, StructuralRef<RowReadonly<RowOf<Ref<reason_21a7169b3eff3d17>>>>>`
is false; measured). The derived structural-alias repair does not fold it either: the slice committed before
this one (binding profile set `0a2183634245`) *did* alias this module's copy of that shape
(`using reason_01523507f7b21613 = flight::types::reason_01523507f7b21613;`), and the slice regenerated under
`070fadeb2a07` does not.

The three constants even change places between the two slices: under `0a2183634245` lines 8 and 9 were emitted
as `flight::Ref<AppUpdateCheckOutcome>` erased references and `_downloadOwners` (line 12) was refused; under
`070fadeb2a07` lines 8 and 9 are refused and the WeakMap is emitted. Neither slice is self-consistent.

Carrying workaround: `@flighthq/updater`'s override supplies the two refused constants as the union's own
reason-only arm, built from the `flight::types` struct that arm is declared with.
