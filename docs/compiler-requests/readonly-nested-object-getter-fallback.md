# Preserve existing fallback reads in the readonly object snapshot

Builder6's review of the first actual snapshot candidate found a compatibility regression before
activation. Its unconditional unsupported-value exception rejected an exact typed StructuralRef
over a JsonObject that the floor returned with the same owner. It also threw for scalar and raw
JSON cells behind a partial outer row, where the floor returned absence. The old candidate and
its original check47 remain immutable evidence in archive `a386ad11`.

Removing that exception alone is insufficient. A generated exact JSON-backed getter would run
once for the snapshot and again through the existing exact typed read. The correction captures
the actual owner of every already typed StructuralRef, including a JSON-backed view, so that
read completes from its one original getter result. Raw Any(JsonObject/JsonArray) storage remains
unrepresented by the new projection and takes the existing fallback. Both bounded blocks retain
their original exact/has-cell/absence behavior for unsupported snapshots.

An empty exact StructuralRef also keeps the floor's engaged optional with an empty row. Its cell
type witnesses the existing exact value/present match, allowing that same result without another
getter call. An empty value with a different stored type still falls through. This adds no cell
field, snapshot payload, source rewrite or generated method change.

The corrected metadata62 fixture changes only the JSON identity assertion at check47. Separate
controls compare the floor and candidate for full and partial reads, scalar and raw JSON object
and array absence/refusal, exact empty rows, native nullable empty storage, and original getter
errors and effect order. A fresh JSON-backed typed getter also proves one invocation, exact owner
and control block retention, and final release without filling the generated get cache.

The inactive proof and exact reviewed inputs are recorded in
`artifacts/sprite-readonly-object-fallback-proof.json`. The original
[snapshot compiler request](readonly-nested-object-getter-snapshot.md) supplies the unchanged
generated method, exact compiler base/hash, equivalence and expiry. This correction requires no
additional compiler generation. Its runtime hunk must be requalified when the exact runtime base
changes. It expires with the upstream snapshot implementation and its matching fallback tests.

Manager3e1de4a2's conditional RowCell ABI authorization and Foreman01203c55/8ba09f0f cover this
bounded correction. Product activation still requires coherent runtime/compiler parity and
Builder6's review of the updated actual bytes. The advertised runtime ABI, product headers,
Types and SDK are unchanged by this proof. General typed/external cache retention remains an
explicit limit. Neither this correction nor the earlier component proofs qualify the original
native Sprite factory, image39, or the two invalid source contracts.
