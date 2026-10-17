# Readonly Record construction and entry enumeration need one retained owner

At compiler `806d257` / Flight `a627849`, the single header in
`missing-member-d7bf452c`, `scene_document/scene_document_interactive_state_bindings.hpp`, fails
inside `flight/object.hpp:45`: the generic `object_entries` asks a readonly structural Record view
for `begin()`. The completed one-worker production check is **0/1 pass, 1 fail, 0 unattempted**.

## Exact source and carriers

`sceneDocumentInteractiveStateBindings.ts:113` contains:

```ts
function cloneFields(fields: Readonly<FlightDocumentFields>): FlightDocumentFields {
  const out: FlightDocumentFields = {};
  for (const [name, value] of Object.entries(fields)) out[name] = cloneValue(value);
  return out;
}
```

`FlightDocumentFields` already emits as `Record<String, FlightDocumentValue>`.
`FlightDocumentValue` retains null plus the recursive array/Record, boolean, number and string
alternatives. The helper's input instead emits as
`StructuralRef<RowReadonly<RowOf<Record<String, FlightDocumentValue>>>>`.
The direct `Record` overload of `object_entries` returns `enumerable_entries()`; the structural input
selects the generic iterable overload, whose `entries.begin()` expression does not exist.

The retained callers create a writable structural Record view from an existing descriptor's `fields`
Record value, then project it readonly for `cloneFields`. `StructuralRef` supplies pointer, row and
represented-variant constructors, but no constructor from that Record value. The missing iterator is
therefore not the whole bridge between this generated call and the runtime.

## Measured runtime-only delegation probe

A scratch overlay changes only `object_entries`' generic body and adds its standard exception include.
When `entries.shared_object()->enumerable_entries()` exists, it retains that checked native owner,
refuses an absent owner and delegates to the existing typed Record snapshot; other iterable inputs
keep their original path. No production runtime or generated file is changed.

The actual same SDK header now passes the original `begin` error but still fails to compile at line152:

```text
no matching conversion for functional-style cast from 'flight::Record<flight::String, FlightDocumentValue>'
to 'flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Record<flight::String, flight::types::FlightDocumentValue>>>>'
```

The changed-input probe completed at one worker: **0/1 pass, 1 fail, 0 unattempted**. Its diagnostic
proves a separate construction barrier. Delegation also cannot assume every allowed readonly
structural dictionary has an exact native Record owner; `shared_object()` is not a general fallback
for an owner held behind another structural projection. This probe is rejected as a complete fix.
No enumeration behavior executable, unused-header link or cloning success is claimed.

## Required shared compiler/runtime contract

Construct the readonly dictionary view from the existing Record without replacing its identity,
copying its entries into a new owner, borrowing storage past its lifetime, or granting writable
access through the readonly boundary. Emitted call-site construction and runtime enumeration must
agree on that retained owner, including allowed structural projections.

`Object.entries` must produce the source's typed entry snapshot: own enumerable string keys in
Record order, integer-index keys first, symbols excluded, and each value captured before the clone
loop. This snapshot is the operation requested by the source; it does not justify copying the
original dictionary merely to manufacture a view. Reference-valued entries retain their owners.
Forwarding raw `begin/end` alone does not supply Record enumeration order or snapshot semantics.

Keep the full recursive `FlightDocumentValue` domain, source field order and one `cloneValue` call
per captured entry. `cloneFields` intentionally constructs its fresh output; that does not authorize
replacing the input dictionary. No permissive `Any`, empty result, skipped field, narrowed value
family or substitute cloning body is acceptable. The already-refused `cloneValue` body remains a
separate module limitation; this request does not claim to supply it.

This is distinct from [optional Record value projection](contextual-record-value-carrier-projection.md),
which changes `Uint8Array` values to `ArrayLike<number>`, and from
[Record spread construction](corpus-context-structural-row-record-spread.md), which constructs a fresh
object from a spread. Here both sides already name the same Record value domain; readonly
construction and `Object.entries` disagree with that represented carrier. No production workaround
or generation change is carried, and the original row retains its original failure.
