# An optional member must construct its declared Record value carrier

A `Record<string, Uint8Array>` is assignable to a readonly
`Record<string, ArrayLike<number>>` in TypeScript. The flight-cpp backend emits that conversion without
adapting it as a direct return and inside a required structural member, but refuses when the same Record
is used to fill an optional structural member.

## Minimal TypeScript

```ts
interface Options {
  externalBuffers?: Readonly<Record<string, ArrayLike<number>>>;
}

export function options(externalBuffers: Record<string, Uint8Array>): Readonly<Options> {
  return { externalBuffers };
}
```

With flight-compiler `2b687ea` and flight-cpp's pinned runtime bindings, this refuses:

```text
contextual union value type flight::Record<flight::String, flight::Uint8Array> is not a represented
runtime domain
```

The union is introduced by the optional outer member. The Record itself and both element carriers are
represented.

## Pinned boundary

| source and destination | result |
| --- | --- |
| optional member, `Record<string, Uint8Array>` to the exact same Record type | emits |
| optional member, source already `Record<string, ArrayLike<number>>` | emits |
| optional member, `Record<string, Uint8Array>` to `Record<string, ArrayLike<number>>` | refused |
| optional member, `Record<string, number[]>` to `Record<string, ArrayLike<number>>` | refused |
| required member with the same Uint8Array-to-ArrayLike widening | emits without adapting the Record |
| direct return with the same widening | emits `return v;`, which does not compile |

The array-valued control proves this is not specific to typed arrays. The exact-carrier controls prove
that optional structural members and Records are both otherwise represented. The required-member and
direct-return outputs are negative controls, not successful conversions: the direct fixture tries to
return `flight::Record<String, Uint8Array>` as `flight::Record<String, SequenceView<double>>`, for which
there is no C++ conversion.

## Required carrier behavior

The source Record already exists, so constructing a second Record would change JavaScript object
identity and split later writes. A correct lowering needs one of these two proofs:

- choose `Record<string, ArrayLike<number>>` when the Record is freshly allocated in that declared
  context, converting each inserted array value to the represented sequence view; or
- retain the existing Record identity through a readonly projected value view whose reads adapt
  `Uint8Array` to `ArrayLike<number>` without granting writes through the wider cell type.

The P-3D2 value is returned from a helper as an existing `Record<string, Uint8Array>`, so it needs the
second behavior unless the source contract is changed at the allocation boundary. The compiler must
not emit the currently invalid invariant C++ Record conversion, copy the entries, or erase values into
`Any`.

## P-3D2 reach and distinct requests

`packages/scene3d-resources/src/gltfLoad.ts` loads external `.bin` files into a
`Record<string, Uint8Array>` and supplies that existing Record to
`GltfImportOptions.externalBuffers?: Readonly<Record<string, ArrayLike<number>>>`. It is a committed
first refusal; the minimal optional-member fixture reproduces its exact diagnostic.

This is not [typed-array static `from`](typed-array-static-from-binding.md), which constructs a typed
array from a source sequence, and it is not [`Object.assign`](object-assign-onto-structural-record.md),
which mutates a target. It is a value-domain projection of one already allocated Record. The invalid
direct-return control should be fixed or truthfully refused with the same identity constraint.

No source rewrite or downstream Record copy is carried.

## G2 Rive rejection details widen numeric cells into diagnostic values

At Flight `a627849` / compiler `806d257`, no-viable-conversion-e1a24bd1 retains four failed
headers: scene2d_formats/{rive_document,rive_core_type_census,rive_requirements,rive_scene_2d}.
The recorded first failure is rive_document.hpp133, where report_rive_reject forwards detail:

```cpp
std::optional<flight::Record<flight::String, double>> detail;
// callee's fifth parameter and ImportDiagnostic.detail storage:
std::optional<flight::Record<flight::String, std::variant<bool, double, flight::String>>>
```

Source reportRiveReject accepts detail?: Readonly<Record<string, number>> and forwards that existing
object to reportImportDiagnostic's detail?: Readonly<Record<string, boolean | number | string>>.
The collector returns immediately when no sink was supplied; otherwise it pushes a diagnostic
retaining the **same** detail object, Reject severity, original kind and origin parseRiveDocument.
ImportDiagnostic declares the same wider readonly Record as the collector parameter. No parser,
RiveDocument or import-context owner is being converted by this failing argument.

This is the existing Record value-projection request at an optional parameter, rather than an optional
structural member. Both values and the wider bool/number/string variant are already represented.
Record<String,double> and Record<String,variant<...>> are different native storage types; their shared
handle behavior within one specialization does not supply a cross-value readonly view. Wrapping an
existing double in the variant is easy; preserving its source Record identity, keys/order, absence
and alias-visible later reads across that wider view is the missing container contract. The outer
optional must remain absent when detail is omitted, rather than becoming an engaged empty Record.

For an already allocated detail, retain its owner through readonly projected reads that select the
double variant arm without coercion or wider writes. At a genuinely fresh allocation, the existing
request's other allowed route is choosing the destination value carrier in its declared context.
A private helper annotation alone should not be described as a new generic Record conversion; no
such source rewrite is measured or carried here. Copying entries, stringifying numbers, substituting
bool/zero/default data, erasing to Any or adding a permissive converting constructor with new storage
is not the existing-object handoff. Disabling the optional collector to skip this call changes the
API and diagnostics, even though ordinary valid parses do not reach rejection branches.

The source's adjacent rejection calls also pass bare numeric detail literals to the optional helper
parameter (unsupported-version major/supported and unknown-width propertyKey, emitted280/295).
Their [optional Record construction request](optional-record-literal-not-materialized.md) is separate:
that materializes a newly written literal, while line133 must preserve an already received Record.
Those later sites are inspected source/emission facts, not newly measured next compiler diagnostics
or evidence that a wider private annotation makes any complete header pass. All functions in this
parser module are emitted; there is no omitted callee whose declaration repairs the conversion.

The parser and its consumers still require the full source boundary:

- Read the exact supplied readonly Uint8Array view, preserving length/cursor/overflow behavior. Require
  the RIVE fingerprint and supported major7; retain minor version/fileId and the file's width table.
  The table uses two bits per key but only eight low bits of each little-endian uint32 word, four keys
  per word. Do not reinterpret it as sixteen keys or add a new canonical-header/version policy.
- Keep object/property stream order and numeric keys. A built-in property width wins over the file
  table; use the file table only as fallback. If neither knows the width, stop at that property and
  return null with rive.unknown-property-width/propertyKey. Never guess, skip arbitrary payload bytes
  or return a partial/empty document as a successful parse. Cursor overflow instead reports
  rive.truncated-object-stream; invalid/truncated headers report rive.invalid-header, and a different
  major reports rive.unsupported-version with original major and supported7. Report only the actual
  branch's diagnostic, then return null; preserve opt-in sink/no-sink behavior and detail presence.
- Preserve unsigned LEB128's multiply-by-powers accumulation beyond31 bits, little-endian uint32/color
  and float32 reads, length-prefixed strings/raw bytes and the source's custom UTF-8 decoding. String
  fields known to be bytes use slice copies as the source specifies. Do not add a host decoder,
  resource fetch/decode, stricter input validation, integer truncation or blanket numeric coercion to
  make diagnostic types fit. Unknown field codes follow the existing Uint fallback.
- Successful parsing allocates the real RiveDocument header plus ordered core-object/property owners,
  including a valid empty stream. RiveValue already carries Uint8Array, number and String. Failure is
  null, distinct from success with no objects. The type census reuses this same parser and returns
  null on failure versus an empty Map on a valid empty stream; requirements and readability preserve
  that distinction rather than returning universal readable/empty success.
- Scene import consumes the original document objects through graph construction, the supplied
  registry, document handlers/assets/font names and per-artboard contexts. Document/artboard context
  initializers retain those objects, diagnostics, registry and original display roots; their separate
  owner/sequence refusals are not this Record error. Do not fabricate a diagnostic-only document,
  copied context/owner graph, empty scene/artboard list or narrowed registered-type domain to avoid
  the shared bedrock parser. The existing invalid-parse scene branch remains its own source behavior.

Pinned source tests inspect valid/no-diagnostic parsing, bad/short fingerprints, header fields,
float/color/string/mixed streams, built-in/file widths and precedence, unsupported generation,
truncation and varuint-valued fields. They were read as requirements, not executed as native parser
results in this unit. The recorded **0/4** remains; per task, unchanged C++ is not rerun. Only this
existing request appendix, four failed rows/their root and worklist45 gain precise evidence/links.
No C++/runtime/binding/source patch/repair/override/manifest, generation, parser/context functionality
or global/package/G1 count changes are carried. No fake Rive document/parser/empty success, copied
owner or narrowed accepted document/diagnostic domain is supplied.

## G2 Rive object graph chooses the diagnostic carrier at fresh detail allocation

At Flight `a627849` / compiler `806d257`, `no-viable-conversion-9695423c` is the single
`scene2d_formats/rive_object_graph.hpp` failure at line 39. `report_rive_graph_drop`
receives a required `Record<String,double>` and forwards that existing value into
`report_import_diagnostic`'s optional `Record<String,variant<bool,double,String>>`.
The collector and ImportDiagnostic.detail agree on that wider readonly source domain;
it is the numeric detail Record that fails to cross, not a graph or document owner.

The module-private `reportRiveGraphDrop` has exactly three callsites, all fresh literals:
`{ index }` for a missing parent or cycle, and `{ index, parent }` for an unresolved parent.
It never receives an already allocated numeric Record from a public caller. The carried
[type-only source patch](../../source-patches/rive-object-graph-fresh-diagnostic-record.patch)
changes only its required detail annotation:

```ts
// Before:
detail: Readonly<Record<string, number>>,
// After, identical to the collector's value domain:
detail: Readonly<Record<string, boolean | number | string>>,
```

The annotation erases in TypeScript. It gives each literal the destination Record carrier at
its first allocation, selecting numeric double alternatives without coercion. The same fresh
object then passes through the helper and optional collector into diagnostic detail. No entries
are copied, no wider writes are granted to an existing numeric Record, and no Record owner or
graph object is replaced. The required helper argument matters: these literals are not bare
objects being passed directly to an optional Record parameter. This takes the fresh-allocation
route already allowed above, rather than adding generic existing-Record covariance.

The [Rive document rejection case](#g2-rive-rejection-details-widen-numeric-cells-into-diagnostic-values)
remains separate and open at recorded 0/4. Its optional numeric detail parameter can receive an
existing Record, and its adjacent bare literals cross an optional boundary. This graph-local
private annotation does not solve that API or the separate optional-literal construction gap.
No parser/document/registry/context owner, binary decoder or diagnostic capability is fabricated.

Normal pinned scoped generation exits 0: 1/1467 modules emitted across 30 packages; 0 refusals recorded; 255 source patch(es) applied; 1 header(s) repaired after emission. Only the own graph header and source-patch provenance record are carried. The complete emitted graph header is
byte-identical to the native-tested exact carrier candidate. Emission also constructs the four numeric values at the three fresh detail sites with explicit
`std::variant<bool,double,String>{std::in_place_type<double>, value}` alternatives. Removing only
those four wrappers and reversing the one private parameter carrier restores the original header
byte-for-byte; no graph operation, branch or caller expression is otherwise changed.
Final scoped original one: **1 attempted / 1 pass / 0 fail / 0 unattempted**. Actual whole-header graph probe
compile/link/run each exit 0 with Clang 22.1.8; it invokes the real generated graph functions,
not a duplicate graph implementation. Source-patch mechanism tests: **2 pass, 0 skip, 0 fail**.

The source and representation contract remains:

- A RiveDocument is the existing header plus ordered core-object/property owners, with numeric
  type/property keys and RiveValue number/String/Uint8Array alternatives. Graph construction consumes
  those same objects; it does not decode bytes, construct a substitute document or discard property
  metadata. Successful empty/no-artboard graphs are real freshly allocated graph results, distinct
  from a parser's null failure. `createRiveObjectGraph` allocates/initializes/finishes the existing
  entity owner; initialization replaces out.artboards with its fresh resolved list, keeping out.
- Each exact artboard type 1 opens a local numbering space with that artboard at index 0. Components
  before an artboard are ignored. Thereafter only objects derived from Component type 10 enter that
  board's objects, using the complete existing core-type ancestry table. Animations, keyframes,
  assets, state machines and unknown non-components still occupy positions in the original stream
  but never shift component indices. Do not copy a smaller type table, renumber the board from its
  first child or keep only a preselected Shape/Ellipse subset.
- Each board's streamStart is its original artboard index; streamEnd is the next artboard index or
  the complete document length, retaining ignored objects in the span. The newly created component
  lists retain original object references and order, not cloned core objects. Parent indices are
  positions within that board list, not original stream indices or another board's namespace.
- The artboard always gets parent -1. For a component, find the first property with key 5; missing
  parent reports `rive.component-without-parent` with index and returns -1. The source asserts its
  value as number and rejects exactly self, less than zero or greater/equal to count, reporting
  `rive.unresolved-parent` with original index/parent. Forward in-range parents are allowed, so
  do not require a previously encountered parent. Keep first-property selection, numeric values,
  property metadata and original comparison/exception behavior; no new integer/finite/type/field-code
  validation, default-to-artboard parent or narrowed accepted document is introduced by this patch.
- Populate every parent entry before scanning cycles from index 1 upwards. The existing slow/fast
  walk stops at -1 or detects equal cursors; on a detected cycle append `rive.parent-cycle`, origin
  `resolveRiveParentIndices`, detail index, then change only that entry to -1. Later checks see the
  updated same parent array. Preserve deterministic break order and still-valid descendants, rather
  than dropping every participant, sorting parents or fabricating a replacement hierarchy.
- All three graph diagnostics use Drop severity. Missing/unresolved parents originate at
  `readRiveParentIndex`; cycles originate at `resolveRiveParentIndices`, not the wrapper's name.
  Diagnostics append to the caller's existing sink in source order and preserve prior entries.
  Omitted sink remains a no-op in the collector while graph construction/rooting still completes.
  Supplied detail remains present with its exact numeric keys; missing parent is not filled with
  a synthetic parent value. No aggregation, disabled collector, success crumb, Reject severity,
  stringified number or default detail substitutes for the real drop branch.

The native probe covers no-artboard/fresh result, multiple local spaces, pre-artboard/animation/
unknown filtering and stream spans, original object identity and independent component lists,
missing/self/out-of-range parents, the 2↔1 ring's deterministic break, an allowed forward parent,
absent/present sink, exact Drop kinds/origins/numeric details and retained prior diagnostics.
A direct helper-to-collector detail check verifies Record identity and a later alias-visible update;
a static assertion confirms numeric Record does not acquire an implicit generic wider conversion.
The existing source tests' corpus-based numbering comments are requirements, not a new corpus run.
No byte parser, scene/artboard importer, asset registry, animation, skeleton or rendering executes.
Source assertions and untested malformed-value/exception paths remain their original semantics;
the native cases do not certify every possible forged RiveValue or the whole Rive pipeline.

Only this pinned private source annotation/record, whole normal header/own provenance entry and
owned request/worklist/product slice change. The passing graph row retains its prior failure id;
its resolved cluster is removed. Historical/global/package counters and all foreign compact rows,
records and prose remain intact. The generated header is carried whole from normal emission.
Runtime, bindings, emission repairs, overrides, document owners, public signatures and the graph
algorithm remain unchanged. Unchanged C++ is not rerun for final docs/base-only edits.
