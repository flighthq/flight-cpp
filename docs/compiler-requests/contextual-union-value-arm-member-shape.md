# A constructed value must match the destination alternative's declared members

The emitter refuses an expression whose value is a reference to an existing owner while the destination
alternative declares a *different* member shape — a member the destination requires optional or absent, or
one given an incompatible type. Eleven declarations of `@flighthq/preferences`' storage module are refused
by this one rule, all of them outcome readers or writers that construct a result arm from a value already
in hand.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/preferences`.

## Observed

```
cpp emission failed for @flighthq/types/packages/types/src/Storage.ts: contextual union value type
flight::Ref<reason_value_8c1689e7820d42eb> is not a represented runtime domain: the value leaves a member
the destination requires optional or absent, or gives it an incompatible type, so it is not the shape an
alternative declares. This expression refers to an existing owner. Keep the value in the declared
destination arm from the point it is created, or explicitly rebuild the declared result in source where
changing identity is intended. The compiler will not retype, copy, or materialize the existing object into
a sibling owner.
```

Eleven declarations, all in `packages/preferences/src/storage.ts`:

| declaration | source line |
| --- | --- |
| `clearStorageNamespace` | 59 |
| `getNamespacedStorageEntries` | 109 |
| `getStorageBoolean` | 152 |
| `getStorageJSON` | 230 |
| `getStorageNumber` | 263 |
| `migrateStorage` | 293 |
| `removeStorageItems` | 349 |
| `setStorageItems` | 391 |
| `setStorageJSON` | 405 |
| `removeStorageItemFromBackend` | 467 |
| `setStorageItemOnBackend` | 481 |

The emitted module is therefore a 642-line partial with those eleven omissions; `@flighthq/preferences` is
`assisted` through `overrides/include/flight/preferences/storage.hpp`, which supplies the module and
declares one gap of its own (a generic JSON read outside the represented domains).

## What the rule is asking for

The message is unusually specific, and it is worth quoting for the fix rather than paraphrasing: keep the
value in the declared destination arm **from the point it is created**, or **rebuild** the declared result
in source where changing identity is intended. The sites above do neither: they read a value into a local
and then construct an outcome arm from it, so the arm's member shape and the local's type are decided in
two different places and the emitter will not reconcile them by copying, retyping, or materializing a
sibling owner.

## Distinguish from the requests already filed

[`task-of-union-domain-unrepresented.md`](task-of-union-domain-unrepresented.md) carries the same refusal
code on a `Task<…>` carrier. [`nullable-optional-presence.md`](nullable-optional-presence.md) is about an
absence channel a *partial row* needs. This one is the member-shape half: the destination arm declares the
member optional or absent, or at another type, and the value expression does not come from that arm.

## Carrying workaround

`overrides/include/flight/preferences/storage.hpp`, status `partial`, supplies the module with its own
outcome-variant vocabulary and its declared JSON gap. The copy predates this emission and is retained
rather than re-derived in this pass: re-deriving would replace a working copy with another copy of the same
eleven declarations carrying the *same* declared gap, because that gap is a runtime representation limit
around the same rule rather than a defect of the copy.
