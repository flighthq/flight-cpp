# A named re-export must retain its declaring namespace and allocated spelling

At Flight `a627849` and compiler `2b687ea`, Scene3D's `billboard.hpp` contains
`using flight::types::BillboardKind;`, but the declaring type header exports only
`inline const flight::String billboard_kind`. Ordinary imported reads already use
`flight::types::billboard_kind`; the named re-export takes a different path and guesses from the barrel.

## Reproduction

```ts
// types/owner.ts
export const BillboardKind = 'Billboard';
// types/contract.ts
export * from './owner.ts';
// scene3d/billboard.ts
export { BillboardKind } from '@flighthq/types/contract';
```

The backend emits the owner as `billboard_kind`, but emits the facade's named using-declaration
as `BillboardKind`. `emitReexportsCpp` asks `getCppResolvedExportTargetName` about the barrel,
which has no declaring binding. That helper falls back to `pascalCase(exportedName)`.
It also qualifies the result with the barrel package rather than the declaring package; a barrel
forwarding a third package therefore loses namespace provenance as well as allocated spelling.

## Required behavior

Resolve the exact exported declaration through the recorded graph, retain the namespace and
allocated target spelling of that declaration, and emit a using-declaration. Do not introduce a copied
constant, a replacement owner, a forwarding function, or a package-specific compatibility alias.
The same source declaration reached by a diamond is one origin. Distinct origins in one type/value
lane remain ambiguous and must be refused. Cycles must terminate; renamed value exports remain
refused unless a separate lowering represents their identity correctly.

The existing cross-package star-export implementation already collects exact origins, separates type
and value lanes, deduplicates declaration identities, and rejects ambiguity. The prepared patch reuses
that selection for an unrenamed, non-type-only named export across a package boundary. A chosen-name
parameter limits that walk to the selected export, leaving unrelated sibling namespace or renamed
exports alone; a full star-export walk retains its original checks. Type-only
named aliases and the existing renamed-value refusal remain on their existing paths.

For the reproduction the facade must emit `using flight::types::billboard_kind;`. For a value declared
in a third package it must qualify the third package's namespace, not the intermediate barrel's.
A using-declaration retains the original variable's storage and address; it has no runtime behavior.

## Prepared scoped evidence

The actual three-module baseline fails C++20 compilation with the absent `BillboardKind` and missing
facade `billboard_kind` diagnostics. An isolated compiler copy with the prepared patch builds and
emits the lowercase declaration's exact using-declaration; the generated three-header consumer
compiles and runs, comparing the facade and original constant addresses.

Twenty-four compiler controls pass: direct value export, same-package barrel, module-order independence,
third-package provenance, diamond selection and one-line deduplication, ambiguous origins, a cycle,
renamed-value refusal, non-type-only interface forwarding, unchanged type-only renaming, separate
type/value declarations sharing one source name, unresolved-name refusal, ignored unrelated namespace/renamed siblings, preserved full-star refusals,
an allocated spelling changed by a measured namespace collision, local-import forwarding (including an aliased local import), a diamond through local imports, type-only lane filtering, and existing direct function/class/enum declarations. These are compiler
fixtures, not a claim that all real Billboard or Scene3D functions compile.

The current Scene3D failure propagates through `billboard.hpp`, `billboard_camera.hpp`, the contract,
internal index, and guard aggregator. This unit must regenerate/compile only the owning package when
published; it must not move dependency rows to conceal their separate failures.

## Workaround

Prepared source patch: `source-patches/compiler-named-reexport-provenance.patch`.
It expires when named re-exports use the exact declaring origin's namespace and allocated C++ name.

`npm run named-reexport:oracle` applies the complete compiler patch stack, builds the pinned compiler,
runs all twenty-four controls and compiles/runs a generated constant-address plus mutable-storage
consumer. The compiler sources and pristine build are restored afterward. Set CXX to a C++20 driver.

A controlled one-line edit of the real current-pin `billboard.hpp` removes the absent-name diagnostic
from all five affected headers at `-j2`, but all five still fail: the next boundary is the nominal
Billboard argument to `get_node_signals`, with a merged-row owner projection failure; the camera
also reaches an undeclared `Traits`. These failures are independent of the re-export spelling.
Prepared one-module emission with the then-current 244-patch stack reproduced that controlled header byte for
byte (SHA256 `cd172718e8e41e569f9340b0ad65b917c20ab397b32193c319796f51a1e7fe5f`).
The five-header compilation therefore measures the exact generated header and unchanged transitive
inputs, without another identical C++ sweep. No dependency row was moved.

## Exact package payload

The prepared full `pkg:generate -- @flighthq/scene3d` run changed exactly three generated declarations:
BillboardKind, MeshKind and Node3DKind become their allocated lowercase using-declarations. The only
other generated path was `manifest.json`, which recorded this compiler patch. No dependency header or
row moved. The later Material/KindUsage correction leaves SceneDocument and the original
KindUsage assertion unsupplied (19/21 modules emitted). Publishing this unit requires fresh scoped
generation and native accounting against the delivered floor; it does not supply either omission.

The complete Node/Mesh overrides shadow two of those headers. Re-derivation verifies each generated
file differs solely in its using-declaration, preserves the existing implementation and restores that
public binding with the same declaration. Their recorded full-file hashes are updated to that exact
source. `flight_cpp.generated_scene3d_kind_reexports` checks both real-header constant addresses,
separately from the generic generated constant/mutable-storage oracle. This does not claim the
independent Scene3D owner, signals, transform or document boundaries are solved.
