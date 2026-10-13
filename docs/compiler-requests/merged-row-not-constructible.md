# A merged row must be constructible and readable as the one object it describes

A runtime gap, not a compiler defect: the emitter's spelling is sound, and the runtime could not represent it. It is
resolved in the runtime, and no compiler change is requested.

Independent Host and App probes agreed on the pre-fix behavior: writes into the constructed merge failed schema
resolution, while projection into the merge succeeded but the narrowed member read threw `std::bad_cast`.

## Upstream shapes

`@flighthq/host` `host.ts` builds a Host over an intersection:

```ts
export function createHost<Capabilities extends Partial<EntityWithoutRuntime<Host>>>(
  capabilities: Readonly<Capabilities> = {} as Capabilities,
): Host & Capabilities {
  const out = allocateEntity<Host & Capabilities>();   // {} as EntityConstruction<Host & Capabilities>
  initializeHost(out, capabilities);                     // out.audio = ...; once per group
  return finishEntity(out);
}
```

`@flighthq/app` `appWindow.ts` reads a capability narrowed to have an optional member:

```ts
export function centerWindow(
  hostWindowGeometry: Readonly<HostWindowGeometryCapability & Required<Pick<HostWindowGeometryCapability, 'center'>>>,
  win: AppWindow,
): void {
  hostWindowGeometry.center(win);
}
```

`@flighthq/node` `createNode` has the Host shape (`EntityConstruction<Node<Traits> & Traits>`). It was not measured here.

## Emitted (flight-compiler 2b687ea with the carried patches, flight a627849)

```cpp
flight::types::EntityConstruction<flight::StructuralRef<flight::RowMerge<
    flight::RowOf<flight::Ref<flight::types::Host>>, flight::RowOf<Capabilities>>>> out = /* allocate_entity */;
flight::row_set<flight::RowKey<"audio">>(out, /* group */);
```

```cpp
inline void center_window(flight::StructuralRef<flight::RowReadonly<flight::RowMerge<
    flight::RowOf<flight::Ref<flight::types::HostWindowGeometryCapability>>,
    flight::RowRequired<flight::RowOf<flight::Ref<center_90209bd6a085864a>>>>>> host_window_geometry,
    flight::Ref<flight::types::AppWindow> win) {
  flight::row_get<flight::RowKey<"center">>(host_window_geometry)(win);
}
```

Three runtime failures, each measured against the committed `include/flight/structural_ref.hpp`:

1. **A row of a row resolves no members.** `EntityConstruction<T>` is `StructuralRef<RowWritable<RowOf<T>>>`, and
   here `T` is itself a `StructuralRef` over a merge. `RowOf<StructuralRef<...>>` resolved nothing, so every
   `row_set` into `out` failed at instantiation with `structural row schema does not contain the requested
   property`, even at `Capabilities = Ref<Host>`.
2. **A merge could not be built over its object, and construction never made one.** A merge has no `object_type`.
   So `StructuralRef<RowMerge<...>>(make_ref<Host>())` has no matching constructor, and
   `{} as EntityConstruction<Host & ...>` stayed the anonymous empty bag `allocate_entity` starts from. No native
   Host stood behind the finished entity, so recovering one (`structural_ref_cast<Ref<Host>>` after projection)
   threw `std::bad_cast`.
3. **A member the merge narrows to present was read at the wrong type.** The merged type of `center` is the bare
   `std::function`, because `Required<...>` wins. The capability stores it as `std::optional<std::function>`. The
   read asked the owner for the bare type, and `named_value` threw `std::bad_cast`. This is reproduced by calling
   the generated `center_window` with a row over a capability whose `center` is set: it prints `bad_cast`.

## Required behavior (TypeScript semantics)

- `createHost` returns one object: a Host carrying the groups `initializeHost` wrote, the caller's own group objects
  where given (identity), and fresh empty groups where omitted.
- A narrowed view is the same object. `centerWindow` calls the capability's own `center` with the window, and a
  write through the view is a write to the capability.
- Nothing is invented for an intersection of unrelated or ambiguous types.

## Runtime change (`include/flight/structural_ref.hpp`)

- **`RowOf<StructuralRef<S>>` is S** for `schema_source`, `schema_member` and the partial, readonly and required
  flags.
- **`detail::merge_subject_t`** names the merge's one native subject. A constituent subject S qualifies when every
  constituent is a row over S, or over a subject the generated table already proves S answers for (the existing
  widening rule, with computed-cell agreement). The result is void when none qualifies, when two distinct subjects
  qualify, or when a constituent has no subject. Nothing is added to the structural member table.
- **With a subject:**
  - a merged row has a constructor from a pointer to that subject (one owner, shared identity);
  - `structural_ref_cast` from an empty bag into a merge mints that subject through the existing
    `materialize_row` rule, so the bag's writes land on the subject's own members.
- **Without a subject:** neither of those exists, and the merge stays schema-only as before.
- **Narrowed members:** a merged member that is non-optional while some constituent declares it optional is read,
  tested and written with the required-row rule for that member only. A read accepts the bare value or the engaged
  optional and throws `std::out_of_range` on an empty one. A write engages the subject's own optional.

`merge_subject_t` changes no conversion rule: a merge's `object_type` stays void, so `row_convertible_to` answers
exactly as before.

## Verification

`tests/merged_row_test.cpp` (CTest `flight_cpp.merged_row`, Bazel `merged_row_test`) covers:
- the Host factory shape at `Capabilities = Host` and at `Capabilities = Readonly<Host>`;
- identity, write visibility in both directions, native recovery and lifetime;
- the generated `center_window` with `center` present and absent, a member the merge does not narrow, present and
  absent optional members, and a write through the narrowed member;
- compile-time negatives: unrelated, ambiguous and unanswered merges have no subject and no pointer constructor,
  and a narrowed view is not constructible from the picked type or an unrelated one;
- the capability merge built from `{}` (builder7's probe A) and asserted from a row over the capability (probe B):
  both call the capability's `center`;
- a runtime negative: an unrelated merge built from `{}` keeps its cells and refuses native recovery.

Writing an optional into the member the merge declares present (`out.center = cap.center`, with `center?:` on `cap`)
does not compile. TypeScript rejects the same assignment, so this is the intended refusal, not a gap.

Mutants killed:
- dropping the narrowing gives `bad_cast`;
- dropping the minting aborts the Host case;
- the old header does not compile the test.

A scratch regeneration of `@flighthq/host` with every carried patch was used to instantiate the generated
`create_host` natively, at `Capabilities = Ref<Host>` and at a `Partial<Host>` row. Both pass the same 6 checks:
- a given group is installed as the same object;
- an omitted group becomes a fresh empty group;
- each call gets its own fallback group;
- each call is a new Host;
- the result is recoverable as a native Host carrying the groups;
- the generated `get_host_audio_device` reads through it.

The committed `host.hpp` is not regenerated here; it changes when its owner regenerates.
