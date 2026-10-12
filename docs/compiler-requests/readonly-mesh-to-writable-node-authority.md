# A readonly `Mesh` cannot assert writable `NodeAny` authority

The P-3D2 picking path receives a `Readonly<Mesh>` and asserts it to writable `NodeAny` only to call
a readonly node-runtime accessor. That assertion claims two things the source boundary did not prove:
write authority and the wider target owner's cells. The compiler correctly refuses instead of
laundering either claim through a cast.

## Minimal TypeScript

```ts
const RuntimeKey = Symbol.for('Runtime');

interface Node<Traits extends object = object> {
  [RuntimeKey]: number;
  enabled: boolean;
  traits: Traits;
}
type NodeAny = Node<any>;

interface Mesh {
  enabled: boolean;
  geometry: number;
}

function getNodeRuntime<Traits extends object = object>(source: Readonly<Node<Traits>>): number {
  return source[RuntimeKey];
}

export function readBounds(mesh: Readonly<Mesh>): number {
  return getNodeRuntime(mesh as NodeAny);
}
```

At flight-compiler `2b687ea` and flight `a627849`, this refuses with rule
`cpp-structural-assertion-writable-capability-unproven`:

```text
a structural assertion from the readonly row Readonly<Mesh> to the writable row NodeAny claims
mutation the source refused
```

The exact Flight graph adds `Node3D` traits and the computed `EntityRuntimeKey`; the committed picking
diagnostic names that key as the target cell its retained `Mesh` owner does not prove.

## Pinned boundary

| source and assertion | result |
| --- | --- |
| `Readonly<Mesh>` as writable `NodeAny`, then call the generic readonly accessor | refuses the writable-capability rule |
| `Readonly<Mesh>` as `Readonly<NodeAny>` | refuses adjacent `cpp-structural-assertion-owner-unproven` |
| writable `Mesh` as writable `NodeAny` | refuses adjacent `cpp-reference-assertion-without-heritage` |
| `Readonly<NodeAny>` as the same exact `NodeAny` owner | emits `structural_ref_cast<Ref<NodeAny>>` |
| read a member declared directly by `Readonly<Mesh>` | emits |

The first three rows are important negative controls. Removing `Readonly` from only the parameter
does not create C++ heritage between independent flattened interface owners. Changing only the target
to readonly removes the false authority claim but still cannot add cells or recover a wider owner.
The exact-owner control emits because the row already retains the `NodeAny` owner; it does not infer a
new one from a narrower `Mesh` declaration.

## Required source authority

No compiler relaxation is correct for the expression as written. A readonly row never becomes a
writable row merely because an assertion requests it. The picking operation only reads
`deformedLocalBounds`, so it needs a readonly, Mesh-typed runtime access path whose storage declaration
proves the Mesh runtime cells. If another path really mutates those cells, that path needs a separate
mutable source and accessor established at the owning storage boundary.

Either design must retain the exact traits specialization and object identity. A native pointer cast,
row materialization, copied replacement, registry tag, or side table would not prove that the retained
object owns the requested cells. Replacing the assertion with `Readonly<NodeAny>` alone is therefore
not a workaround, as the measured adjacent refusal shows.

## Existing coverage and distinct requests

The pinned compiler already tests the general invariant with readonly runtime and node shapes, exact
same-owner controls, typed mutable accessors, and package/path-independent diagnostics. The current
generated inventory contains 17 direct refusals under this general rule; they include both runtime
accessors that later mutate and node assertions used only for reads. This page narrows the P-3D2
instance rather than asking for another broad diagnostic test.

[Imported owner recovery from readonly views](compiler-imported-readonly-view-owner-recovery.md) is
different: that request already has a unique exact owner and fails because imported spellings are not
canonicalized. [Interface heritage owner widening](compiler-interface-heritage-owner-widening.md) is
also adjacent but answers nominal widening across independent flattened owners; it does not grant
writes through a readonly row.

## P-3D2 reach and carrying status

`packages/picking/src/pickScene3D.ts`, inside `intersectMeshTriangles`, evaluates:

```ts
(getNodeRuntime(mesh as NodeAny) as MeshRuntime).deformedLocalBounds
```

The committed inventory records this as the module's direct refusal. The read-only use does not make
the writable assertion safe, and the second assertion to `MeshRuntime` is a separate owner question
that has not been reached by this reduction. No source rewrite, compiler patch, or generated repair is
carried while Scene3D remains render-gated.

## Second measured site: `@flighthq/updater`

Same rule, same pin, another package -- and here the refusal is exactly right while the faithful lowering is a
construction rather than a cast. `packages/updater/src/updater.ts` line 11:

```ts
const INSTALL_OK = Object.freeze({ reason: 'ok' }) as AppUpdateInstallOutcome;
```

```
cpp-structural-assertion-writable-capability-unproven [source-portability]: a structural assertion from the
readonly row Readonly<object> to the writable row AppUpdateInstallOutcome claims mutation the source refused
```

`AppUpdateInstallOutcome` is emitted as a nominal `ReferenceEnabled` struct with a mutable `reason` cell, so
asserting a frozen literal to it does claim write authority the source withheld. By this request's own
distinction -- ask the value's role, not its spelling -- the answer here is the second repair: the constant *is*
an outcome the module owns and hands out, so it is declared as that carrier and constructed directly
(`flight::make_ref<AppUpdateInstallOutcome>(AppUpdateInstallOutcome{.reason = ...})`), which is what
`@flighthq/updater`'s override does.

Worth recording against this rule: the slice committed before this one lowered the same line by *casting* into
the nominal (`structural_ref_cast<flight::Ref<AppUpdateInstallOutcome>>(make_ref<reason_5d202cf6c17c5b52>(...))`),
so the stricter reading is the newer behaviour -- and a frozen literal is not what a writable nominal's own
construction should be built out of.
