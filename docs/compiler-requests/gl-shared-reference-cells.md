# GL shared reference cells

The approved bounded capability covers exactly two relations:

| Physical storage | Derived value view | Native escape |
| --- | --- | --- |
| `GlRenderState` inherits the base `EntityRuntimeKey` cell, `optional<Ref<RenderStateRuntime>>` | `optional<NominalOrStructuralView<GlRenderStateRuntime, RenderStateRuntime>>` | Partial, tested |
| `GlRenderStateRuntime` inherits `RenderStateRuntime.registries`, `Ref<RenderRegistries>` | `NominalOrStructuralView<GlRenderRegistries, RenderRegistries>` | Partial, tested |

`compiler-shared-reference-cells.patch` extends the existing structural value carrier family with
explicit `cells` declarations. It checks the original source inheritance before choosing storage, requires
native heritage for the payload, and compares the resulting physical storage through the existing
heritage gates. An undeclared refinement remains flattened; a false payload-base declaration is refused.
The derived struct declares no second field. The field address, owning allocation and control block
remain the base cell's. A value read is a source snapshot of its object; reading the cell again observes
replacement, while a local already read retains its original identity.

Base and derived writes use that cell. Computed writes record written-undefined presence under the
original symbol identity. Construction-row writes project the value to the base cell's declared type
before `row_set`, preventing a derived-reference mismatch from landing in an attachment. Row reads use
that same native member's storage type. Nested runtime-view access uses a member pointer applicable to
the base itself, so reading the guaranteed `registries` cell requires no guess about a derived allocation.

A base writer may legally put a base-only object in either cell. Reading or storing the value view does
not validate a GL owner. Its absent derived member reads as absence through the existing by-name API.
Only `native()` or an emitted typed GL native escape demands the GL owner and throws the shared
`flight::NativeScalarEscapeError` on failure. It never reports a wrong object as an empty GL reference.
An allocation without recoverable provenance also remains partial at this escape; no global registration
or `ReferenceEnabled` layout change is introduced here. Builder2 owns the separately approved allocation
primitive.

The native-escape sites are explicitly **partial**: the runtime slot's typed GL runtime return (including
`readGlRenderStateRuntimeOwner`) and typed GL registry return/argument uses require a recoverable GL
allocation. The emitted native fixture tests both failures after legal base writes, alongside valid
same-owner returns, readonly reads, nested writes, clear/presence and lifetime controls.

## Activation and qualification

The two actual declarations are recorded in `tests/fixtures/gl_cell_views/carriers.json`. They remain
**held** from the normal `structural-value-carriers.json` profile pending compatible normal generation
and override rederivation. The inherited `registryMiss` callable prerequisite is now qualified by
[original declaration identity](inherited-callable-declaration-owner.md): the actual pinned three-module
compiler check retains GL state/registries/runtime native bases and the two inherited physical cells.
No heritage gate is bypassed. Callable helper ABI spelling changes require generated modules and their
overrides to be rederived and tested together before an installed GL claim.

The emitted owner-helper fixture also places reads/writes before the complete GL runtime declaration,
as the actual source does. Known nominal values use the existing view constructor; narrowed base values
retain lazy `::of` recovery. Native escapes remain partial and tested.
The direct C++ test and complete compiler-emitted native fixture cover the two physical cells. The
original source fixture supplies 26 source observables. These are preparation controls, not normal GL SDK,
backend execution, public registry consumer, or package qualification. Existing GL target-copy work
remains separately held; Canvas and DOM backend work is outside this scope.
