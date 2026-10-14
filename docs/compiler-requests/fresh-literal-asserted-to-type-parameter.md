# A fresh object literal asserted to a type parameter has no C++ owner to become

`@flighthq/tray` `invokeUpdate` is refused with `cpp-reference-assertion-without-heritage`. The refusal is correct:
no heritage or widening proof can apply, because the asserted value is a new literal and never an existing owner.
The branch of the same function that is *not* refused emits code that cannot be instantiated.

Measured at flight-compiler `2b687ea` with every carried patch, against flight `a627849`.

## Upstream shape

```ts
async function invokeUpdate<Provider, Result extends { readonly outcome: string }>(
  provider: Readonly<Provider>, tray: TrayIcon, failure: Result['outcome'],
  operation: (provider: Readonly<Provider>) => Promise<Result>,
): Promise<Result> {
  const runtime = getActiveTrayRuntime(tray);
  if (runtime === null) return { outcome: 'tray-destroyed' } as Result;
  try {
    return await operation(provider);
  } catch (error) {
    return { error, outcome: failure } as unknown as Result;
  }
}
```

It is instantiated with result unions such as `TrayImageUpdateResult`, which is
`{ outcome: 'updated' } | { outcome: 'tray-destroyed' } | { error?: unknown; outcome: 'invalid-icon' | 'image-update-failed' }`.

## Minimal reproduction

```ts
export type U = { readonly outcome: 'updated' } | { readonly outcome: 'tray-destroyed' }
  | { readonly error?: unknown; readonly outcome: 'image-update-failed' };
export function inv<Result extends { readonly outcome: string }>(op: () => Result, failure: Result['outcome'], e: unknown): Result {
  if (!active()) return { error: e, outcome: failure } as unknown as Result;   // refused
  return op();
}
export function inv2<Result extends { readonly outcome: string }>(op: () => Result): Result {
  if (!active()) return { outcome: 'tray-destroyed' } as Result;                // emitted, cannot instantiate
  return op();
}
export function use(): U { return inv2<U>(() => ({ outcome: 'updated' })); }
```

- **`{ error, outcome } as unknown as Result`** is refused:
  `a reference assertion from flight::Ref<error_outcome_f5f0…<Result>> to Result has no heritage to cast along`.
  The literal is emitted as an anonymous struct templated on the function's type parameters, which it does not use.
  That is the same templating seen in
  [generic-alias-non-dependent-anonymous-arms.md](generic-alias-non-dependent-anonymous-arms.md).
- **`{ outcome: 'tray-destroyed' } as Result`** emits `return flight::make_ref<Result>(Result{.outcome = flight::String("tray-destroyed")});`.
  Instantiating `use()` natively fails:
  `initialization of non-aggregate type 'std::variant<…error_outcome…, …outcome…>' with a designated initializer`.
  The header compile gate does not instantiate templates, so this is invisible until a caller instantiates it.

## Why no heritage proof applies

The task's admissible fix is to prove the source carrier *is* the asserted owner, or reaches it through a
generated widening. Neither can hold:
- the source is an object created by the literal itself, so there is no existing owner whose identity a cast
  could preserve;
- the target is a type parameter, so which C++ type it names is decided at instantiation, where it is usually a
  `std::variant` that no single struct widens to.

A cast would retype the object, so it stays refused.

## Required behavior

TypeScript returns the literal itself, typed as `Result`. A faithful C++ lowering builds the literal *as* the
alternative of `Result` whose declared shape it is, chosen at instantiation:
- when `Result` is a variant, the one alternative whose members are exactly the literal's keys, at compatible types;
- when `Result` is an object, `Result` itself;
- when no alternative, or more than one, has exactly that shape, the instantiation must be a compile-time error.

The value is new either way, so identity is the new object's.

Two facts make this more than an emitter change:
- **Exact-shape selection needs a member census.** C++20 cannot enumerate an aggregate's members, and
  designated-initializer viability also accepts a literal that omits an optional member. So `{ outcome }` is
  viable for both `{ outcome }` and `{ error?, outcome }`, and selection by viability is ambiguous. A sound
  selector needs the generated member table to declare each struct's complete key set. That is new table output
  (broadening `structuralMemberTable`) or a runtime contract, not a local proof.
- **The catch branch writes the caught value into `error?: unknown`.** That is
  [caught-value-into-unknown-member.md](caught-value-into-unknown-member.md), and it blocks this function
  independently.

## Design analysis (no patch carried)

### The selection rule must be exact key-set equality

The literal has to become the alternative of `Result` whose complete key set equals the literal's keys. A
superset alternative that merely *admits* the literal (its extra keys are optional) is the wrong answer, because
the emitter lowers a member-presence test as an alternative test:
- `'error' in result` is `result.index() == 0`; see
  [narrowed-member-multi-alternative-selection.md](narrowed-member-multi-alternative-selection.md).
- `{ outcome: 'tray-destroyed' }` built as the `{ error?, outcome }` struct would answer `'error' in` with `true`.
- Node answers `false`.

Under exact equality, both of `invokeUpdate`'s literals select uniquely for `TrayImageUpdateResult`:
- `{ outcome }` selects the `{ outcome }` struct;
- `{ error, outcome }` selects the `{ error?, outcome }` struct.

Zero or several equal key sets must fail at instantiation.

### A struct's exact key set cannot be read from C++ itself

The selection happens at instantiation, where only the C++ types are available. Measured on the committed generated
tree, emitted object structs have these bases:

| Base | Structs |
| --- | ---: |
| `flight::ReferenceEnabled` (empty) | 3848 |
| `flight::types::NodeRuntime<...>` | 59 |
| `flight::types::Entity` | 32 |
| `flight::types::GlFullscreenProgram` | 7 |
| others | a handful |

Many bases carry data members, so a struct's keys include inherited ones. C++20 cannot enumerate a class's bases.
Aggregate field-count probes (`S{ {}, any... }`) count only the struct's own members when the base is braced. When
it is not braced, brace elision folds an aggregate base's members into the same count. Either way, inherited keys
cannot be told apart from own keys, so a probe would accept a struct whose inherited members the literal does not
have. Designated-initializer viability checks only that the literal's keys exist (in declaration order), not that
nothing else does. No pure C++ census is sound.

### Sound facilities, and why each is cross-cutting

1. **Emitter-side key metadata.** The emitter knows each struct's exact key set from the IR, inheritance included,
   and can emit it beside the struct as a trait specialization:
   `flight::detail::object_keys<S>` lists the TS names, each marked required or optional. A runtime
   `flight::construct_asserted<Result>(keys, assign)` then selects the unique alternative with an equal key set,
   assigns each member from values evaluated once in source order, and fails to compile on zero or several
   matches.

   This is sound. It adds text to every emitted object struct (about 3,960 at this pin), so it changes every generated row and
   only takes effect after a coordinated regeneration rooted at `@flighthq/types`.
2. **A census rendered into `flight/sdk/structural_members.hpp`** by parsing emitted and override headers for each
   struct's members and bases. It needs no row regeneration, but it broadens the shared table and depends on
   text-parsing C++ declarations, templates and inheritance chains. That is excluded here.

### Independent blocker

`invokeUpdate`'s catch branch writes the caught value into `error?: unknown`
([caught-value-into-unknown-member.md](caught-value-into-unknown-member.md)). That is not authorized, so
`invokeUpdate` does not emit even with facility 1 in place. Only its `{ outcome: 'tray-destroyed' } as Result`
branch, which is the one that currently emits code that cannot instantiate, would be fixed.

## Affected modules

`@flighthq/tray` `invokeUpdate` and its ten callers (`setTrayIcon`, `displayTrayBalloon`, `popupTrayContextMenu`,
…), whose emitted bodies call the omitted `invoke_update`. `invokeRead` has the same `as Result` shape. Its emitted
header also fails for an earlier reason: `Result['outcome']` is emitted as
`decltype(std::declval<Result&>().outcome)`, which is ill-formed for a variant `Result`.

## Carrying workaround

None. Rewriting each call site to build its declared arm directly would remove the generic helper upstream chose;
that is a source change made for the emitter.
