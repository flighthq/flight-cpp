# Preserve authored generic callback evidence through mapped member reads

The original `createNodeRuntime<Traits>` accepts methods through
`Readonly<Partial<MethodsOf<NodeRuntime<Traits>>>>`. The declaration and reference planner retain
`NodeOf<Traits>` callback parameters, but the optional `methods?.canAddChild` expression lowers with
`NodeOf<unknown>`. The checker-derived member evidence has lost the written generic family.

The inactive semantic proposal reuses `getTypeScriptMappedSubjectSelection` while resolving an
alias's construction/member shape, before the existing `lowerType` fallback. Alias declaration
lowering already uses this helper. The existing substitution then resolves the callback through
the authored `NodeRuntime<Traits>` member, preserving both current type-parameter binding identity
and the optional undefined arm. This introduces no callback widening or new runtime carrier.

Actual lowering of the frozen original 1211-source input confirms that the expression type and
`optionalChain.valueType` now contain the function's own Traits binding. It produces the same two
unrelated frontend warnings and generates no whole-package headers. The source bodies and Types
files are unchanged; this is separate from the two invalid source contracts still reported by
strict checking.

The minimal two-module strict source emits real C++ in both variants. Baseline C++ compiles but
fails the default function-pointer identity check: its erased-family callable has been adapted.
Candidate C++ passes 30 checks covering the exact default/custom callback origin, original
anonymous argument owners and control blocks, replacement of the input callback, the original
exception and effect prefix, and release. The direct `Partial<Runtime<T>>` control retains its
original emitted signature. Identity across different generic instantiations is not qualified.

The inactive patch, exact compiler composition, native receipts, original expression IR and
focused semantic regression parity are in `artifacts/sprite-callback-member-evidence-proof.json`.
The patch is based on compiler `806d257d2f4100de745cc97340e61c2f301b0f24` plus the archived proof
composition identified by its exact baseline semantic file hash. It expires when upstream member
resolution preserves this mapped-subject evidence, or when the pin/composition changes and needs
requalification. It is not registered for SDK generation or applied to product inputs.

This proof does not fix the generic runtime-factory value asserted through unknown/Any, its
optional parameter representation, anonymous factory allocation ABI, Sprite's strict nullable
data/receiver contracts, or native factory/image39 execution. Those remain explicit independent
obligations.
