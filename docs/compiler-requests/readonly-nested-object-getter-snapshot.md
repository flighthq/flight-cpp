# Preserve the owning result of a readonly nested object getter

The original Sprite initializer reads `obj?.data` through an optional readonly partial object
view. An erased row cell may hold a native reference, an anonymous structural row, or that exact
readonly row type. The existing `TypedRowCell::as_any()` does not represent structural rows, and
the generated accessor's reference-returning `get()` retains a cached result. Reading through that
cache can retain the last fresh object after the source options and its consumer have died.

The bounded proposal has a separate owning `readonly_object_value()` snapshot. Generated private
readonly accessor cells call the original getter once into a local owning value and return its
actual owner and native lifetime. The emitted source getter body, `get()`, `set()`, `as_any()` and
cache layout stay unchanged. Runtime dispatch uses this snapshot before exact typed-cell reads,
only for a readonly outer row's optional readonly partial object member. A non-null target rejects
source null; source undefined stays absent. JSON and external erased objects are excluded.

The runtime base default delegates to the existing erased read. The inherited typed default can
capture a structural value's actual `shared_owner()` directly, including a schema-only owner.
An empty owned optional has construction-value undefined semantics. The native-cell override
uses the declared member policy: RequiredNullable is null, SourceOptional is undefined, and an
ambiguous Required empty carrier refuses. Generated getters use the compiler's existing declared
null/undefined carrier analysis; the inherited erased read outside this domain is unchanged.

This deliberately leaves external caching subclasses' own retention behavior and ordinary typed
getter reads unchanged. It also leaves dynamic `NamedProperties` representation unchanged:
structural row cells remain unrepresented, on both marked and unmarked owners. The previous global
`as_any()` expansion was measured and rejected for changing those answers.

The inactive proposal and exact evidence are recorded in
`artifacts/sprite-readonly-object-snapshot-proof.json`. The generator hunk is based on compiler
`806d257d2f4100de745cc97340e61c2f301b0f24` plus the archived combined136 proof composition.
Its exact baseline backend hash and composition are recorded there; applying it to another
composition requires requalification. The proposal expires when the compiler owns this snapshot
lowering with matching runtime semantics, or when either pinned input invalidates its exact base.
It is not registered for generation or applied to product headers.

Adding the virtual method changes the public RowCell vtable. Manager3e1de4a2 and Foreman
f0914edc/be4966fe conditionally authorized implementation and proof of that specific ABI change.
Activation requires coherent runtime/compiler evidence and Builder6's actual-byte review; this
request neither activates it nor changes the advertised runtime ABI version.

The original whole Sprite factory remains separate: strict source checking still has 220
diagnostics with two owned errors, and the actual native factory/image39 is not qualified by these
bounded getter tests. No Types changes, native schema allocation or factory body substitution are
part of this proposal.
