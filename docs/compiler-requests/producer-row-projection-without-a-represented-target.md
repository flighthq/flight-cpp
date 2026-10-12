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
