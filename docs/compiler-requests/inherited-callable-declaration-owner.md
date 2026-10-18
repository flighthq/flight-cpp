# Inherited callable declaration identity

The pinned Flight program declares `RenderStateRuntime.registryMiss` once and inherits that callback
object in GL and WGPU runtimes. Inheritance introduces neutral import bindings for its enum argument
and named `signals` field. The C++ backend used those importer IDs in the callable object's identity,
creating unrelated native helper types for the same inherited source field. It also planned the
inherited enum through a synthesized module the reference planner had never indexed, turning an enum
value into `Ref<RenderRegistryTable>`. The native heritage gate correctly rejected that storage.

`compiler-inherited-callable-declaration-owner.patch` resolves named references in the callable identity
key to their original declarations. Transparent aliases substitute their exact generic arguments before
resolution. An object-bearing alias keeps its own original declaration and instantiation: two named
native records remain different even when their fields match. Distinct interfaces, classes, enums and
generic arguments stay distinct. Nested anonymous records retain their raw identity and package scope,
because this fix establishes no cross-owner equivalence for those records. Source IR and initializer
contents are untouched; no generated hash-name comparison or storage-gate exception is introduced.

Inherited enum representation now resolves the original enum owner before consulting the existing
planner. This gives the callback its declared value argument. The correction applies even when another
field prevents native inheritance, so sharing a base cannot hide a wrong enum representation.

## Forward runtime declarations

The actual GL source defines its runtime owner helpers before defining `GlRenderStateRuntime`. For an
explicit shared-cell relation and a statically declared nominal source, the compiler emits the shared
view's existing nominal constructor. That retains the supplied owner and defers the base conversion
until the runtime declaration is complete. Values narrowed from a base retain `::of` and its lazy checked
recovery. Ordinary WGPU carriers have no `cells` declaration and retain their existing wrapping.

No runtime API, allocation descriptor or `ReferenceEnabled` layout changes in this patch. Builder2 owns
the separately approved recording-deleter provenance implementation.

## Qualification

Compiler pin: `806d257d2f4100de745cc97340e61c2f301b0f24`; frozen ordered baseline: 105 compiler patches;
candidate: 106. Patch application reproduces all three changed compiler files exactly, and reversal
restores both original files and removes the added test. The complete ordered stack applies.

The strict compiler build passes. The five focused regression files pass **26/26 with zero skipped**:

- Twelve callable-owner cases cover direct imports, barrels, transparent generic aliases and import
  rebinding; distinct interfaces, object aliases and generic instantiations; four fully emitted native
  consumers; and the enum-value witness on a deliberately flattened owner.
- Five GL cell cases include fully emitted native consumers with both complete and forward runtime
  declarations. The native consumers exercise the two physical cells, legal base replacement, lazy
  reads, checked wrong-owner errors, presence, symbols, owner/control-block identity and lifetime.
- Nine existing enum, inherited-alias and callable-initializer cases retain their behavior.

The original ten-case owner fixture on baseline105 has eight failures and two passes; all four native
consumers fail there. Removing only the enum-owner fix from the candidate fails the independent
flattened-owner enum witness. The distinct object-alias negative also rejected an earlier overbroad
alias normalization. These countertests distinguish declaration identity from equal field shapes.

Existing callable ABI, arity, identity, dependent-pack and reference-planner controls pass 53/53.
The backend callable/enum/heritage/inherited selection has the same 71 passes and 13 failures as
baseline105; all selected and unselected case statuses match. These existing failures remain recorded,
not accepted as new successes.

A compiler-only check of Flight pin `a62784923f3be814463286c9fb28edfe6ab15789` plus 201 Flight patches
uses the same 1,032 input modules and two pre-existing semantic diagnostics. It emits the actual
`RenderState`, `GlRenderState` and `WgpuRenderState` modules with the two fixture GL cell declarations.
GL state, registries and runtime retain their three native base relations. State's runtime cell and
runtime's registries cell are inherited, without duplicate fields. The actual early owner read/write
helpers use the common cell and existing nominal constructor.

This proves the compiler prerequisite and bounded fixtures. Normal GL profile activation, generated
header regeneration, override rederivation and an installed public consumer remain separate work.
The callable helper identity changes emitted ABI spelling, so regenerated modules and their overrides
must be qualified together. The separately held GL six-header target-copy task is not activated.
WGPU's original public registry consumer, backend execution, historical eight-header results and whole
package counts receive no completion claim from this check. Canvas/DOM implementation is outside scope.
