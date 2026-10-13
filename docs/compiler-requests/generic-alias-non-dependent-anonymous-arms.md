# A generic union alias must not template an arm that does not use its type parameter

The emitter disagrees with itself about a generic alias. The alias declaration and the representation plan for
the same alias choose two different C++ types for one arm, so a value can never reach its own declared arm.

## Minimal TypeScript

```ts
export interface E { id?: number }
export type PF = (E & { readonly outcome: 'cancelled' }) | (E & { readonly error?: unknown; readonly outcome: 'invalid-icon' });
export interface S<T> extends E { readonly outcome: 'created'; readonly tray: T }
export type CR<T = number> = S<T> | PF;
export type CRN = S<number> | PF;

export function make(): PF { return { id: 1, outcome: 'cancelled' }; }
export function f(): CR { const out: PF = make(); return out; }
```

## Emitted (flight-compiler 2b687ea with every carried patch)

```cpp
struct id_outcome_9612b089dd0085bd : public flight::ReferenceEnabled { /* id, outcome */ };
struct id_error_outcome_f7b658aa63d217ee : public flight::ReferenceEnabled { /* id, error, outcome */ };
using PF = std::variant<flight::Ref<id_error_outcome_f7b658aa63d217ee>, flight::Ref<id_outcome_9612b089dd0085bd>>;

template <typename T>
struct id_outcome_452723bda593d7b1 : public flight::ReferenceEnabled { /* id, outcome -- no T */ };
template <typename T>
struct id_error_outcome_d5d06eb6dafe282a : public flight::ReferenceEnabled { /* id, error, outcome -- no T */ };
template <typename T = double>
using CR = std::variant<flight::Ref<id_error_outcome_d5d06eb6dafe282a<T>>, flight::Ref<id_outcome_452723bda593d7b1<T>>, flight::Ref<S<T>>>;
using CRN = std::variant<flight::Ref<id_error_outcome_f7b658aa63d217ee>, flight::Ref<id_outcome_9612b089dd0085bd>, flight::Ref<S<double>>>;

inline CR<> f() {
  PF out = make();
  return ([&]() -> std::variant<flight::Ref<id_error_outcome_f7b658aa63d217ee>, flight::Ref<id_outcome_9612b089dd0085bd>, flight::Ref<S<double>>> { /* get_if per PF arm */ }());
}
```

- **The generic alias `CR<T>`** re-emits `PF`'s two arms as new structs templated on `T`, which neither uses.
- **The non-generic `CRN`** reuses `PF`'s own arms.
- **The representation plan for `CR<>`** also reuses `PF`'s arms. So `f` builds its result as
  `variant<f7b6…, 9612…, S<double>>`, but its declared return type is `CR<>`, which is
  `variant<d5d06…<double>, 4527…<double>, S<double>>`. In a single module this emits code that does not compile.

## How it shows in `@flighthq/tray`

With the source patch `tray-create-result-and-runtime-owner`, `TrayCreateResult<Tray>` is
`TrayCreateSuccessResult<Tray> | TrayCreateFailedResult | TrayCreateProviderFailureResult`. The emitted
`flight/types/tray.hpp` declares:
- `TrayCreateProviderFailureResult` over `entity_runtime_key_error_outcome_4ce1…`, `…_b8e6…` and
  `entity_runtime_key_outcome_1729…`;
- `TrayCreateResult<Tray>` over templated twins of the same shapes: `…_0f47…<Tray>`, `…_63dd…<Tray>` and
  `…_outcome_8ec9…<Tray>`.

In `createTrayIcon`, `return finishEntity(out)` with `out: Entity & TrayCreateProviderFailureResult` is refused with
`cpp-contextual-union-inequivalent`. The target carriers are reported from the `types` module's own plan
(`entity_runtime_key_error_outcome_4ce1…`, unqualified); the source carriers are the same names qualified as
`flight::types::`. The unqualified spelling is a symptom: even with the spelling matched, the declared return
type's alternatives are the templated twins, and no alternative of it is the struct the value is.
`startTrayIconAnimation` refuses with the same code, but for a different reason, and this request does not cover
it. `TrayAnimationStartResult = { started } | { empty } | Exclude<TrayImageUpdateResult, { outcome: 'updated' }>`
is emitted as a nested variant:

```cpp
std::variant<std::variant<Ref<error_outcome_cfa3…>, Ref<outcome_bd96…>>, Ref<outcome_release_35b9…>, Ref<outcome_bd96…>>
```

The `Exclude<>` arm is kept as an inner variant rather than flattened, and `outcome_bd96…` appears twice. The source
value (a `TrayImageUpdateResult` narrowed by `!== 'updated'`) has no single destination alternative. No generic alias
is involved.

## Why no conversion is carried

`id_error_outcome_f7b6…` and `id_error_outcome_d5d06…<double>` are distinct nominal C++ owners with byte-identical
bodies:
- a pointer cast between them retypes the object;
- copying into the other struct replaces the object's identity;
- a structural view changes the declared result carrier.

None of these may be traded for a compile, so the fix is in emission, not conversion.

## Required behavior

An anonymous arm of a generic alias that names none of the alias's type parameters is the same declaration as the
non-generic arm of that shape. It is emitted once, untemplated, under the hash the non-generic use gets, so the
alias, its representation plan, and every value of that shape agree on one C++ type. An arm that does use a type
parameter (`S<T>`, or `{ tray: T }`) stays templated.

## Scope of the fix (why it is not carried here)

The fix is one rule in alias emission, but it changes emitted rows across the SDK. At flight a627849, the
committed tree has 91 templated anonymous structs whose template parameters appear nowhere in their bodies, in
23 headers:

| Package | Structs |
| --- | --- |
| types | 39 |
| node | 29 |
| midi | 10 |
| notification | 7 |
| scene3d | 2 |
| skeleton2d, render, scene2d_gl, interaction | 1 each |

Every one of them changes name, and each declaring package's consumers would need regenerating. That is a
coordinated `@flighthq/types`-rooted regeneration, not a package-scoped change, so it is recorded here and not
patched in a Tray unit.

## Affected modules

`@flighthq/tray`: `createTrayIcon`, after the
`compiler-coalesced-union-member-narrowing` patch moves `createTrayIcon` past its earlier refusal. The other
packages above carry the same twins; whether any of their declarations depend on a conversion like this one was
not measured.
