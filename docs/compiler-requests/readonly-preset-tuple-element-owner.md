# A readonly preset tuple must retain its shared-owner element carrier

At compiler `806d257` / Flight `a627849`, `spineBinaryBonesHandler.ts` declares the five
`TransformMode2D` presets in a readonly `as const` tuple. The source helper selects a tuple element
for ordinals within its length and returns the original Normal preset otherwise. Every preset is
an existing shared object with reflection/rotation/scale/translation flags, not a numeric enum.

The generated `spine_binary_bones_handler.hpp` keeps these exact objects as `Any::object` cells in
`Array<Any>`, while its helper result and fallback use the common anonymous preset Ref carrier.
Line48 emits a raw C++ conditional between `Any` and that Ref. The original serial one-header check
completes **0/1 pass, 1 fail, 0 unattempted** at this incompatible operand pair. No declaration is
refused in this source module; the loss occurs in the element/conditional representation.

Preserve the checked common element carrier when lowering the readonly tuple, or recover its actual
stored owner through a checked, exact element read before selecting the conditional result. The
runtime already holds the preset references, so constructing new flag objects would lose preset
identity. An implicit or coercive Any-to-Ref conversion would claim recovery for unrelated erased
objects; making the Bone2D slot Any, picking Normal for all ordinals, narrowing accepted bytes to the
five known modes, or copying a structural view does not repair this source.

The carried source patch `spine-binary-transform-mode-element-union` spells the exact existing
preset element union as a readonly array annotation, replacing the erased tuple assertion. Whole
original/patched module JavaScript is byte-identical, so all reads, preset objects, allocation,
guards, fallback, diagnostics and binary operations are unchanged. No runtime Any conversion or new
owner is supplied.

Normal exact-pin generation emits the handler together with the unchanged canonical
`TransformMode2D` dependency. That dependency header is byte-identical to the committed header and
is not carried. The existing duplicate-struct alias engine gives the handler's exact anonymous shape
the canonical `flight::types` alias; its table then stores those original Ref owners directly.
Only this entire normal-generated handler and the patch provenance are carried. All function bodies,
including mode selection and every parser/diagnostic/skip operation, are byte-identical to the prior
header. The initial isolated one-module trial lacked the canonical definition, emitted a distinct
local twin and failed the one-header check at Array construction; it is not carried.


The known second owner boundary is recorded by
[the satisfies-table request](satisfies-record-member-carrier.md#g2-spine-binary-table-selection-and-bone-storage-are-distinct-boundaries).
The presets emit as anonymous flag owners, while `Bone2D.transform_mode` expects the unrelated nominal
`Ref<TransformInherit2D>`. Matching fields are not C++ inheritance. A local table fix cannot invent
that relation, and this single-header task does not rewrite the shared preset table for every package.
The invalid-parent diagnostic also visibly emits a bare key/value list into an optional Record slot;
[the existing optional Record request](optional-record-literal-not-materialized.md) covers that separate
construction. These are inspected follow-on boundaries, not extra winning header diagnostics.

## Binary reader, owner and failure contract

`parseSpineBinaryBones` uses the original ByteReader/view and reads the count varint first. Each
iteration checks sticky overrun, reads a length-prefixed UTF-8 name, then reads a parent index only
for non-root bones; root uses -1 and consumes no parent bytes. A parent outside the preceding bones
reports Drop `spine.binary-bone-parent-out-of-range` with bone name, declared index and current index,
marks overrun through the original skip helper and stops before reading its transform.

For a valid parent, the order is rotation, x, y, scaleX, scaleY, shearX, shearY and length as eight
big-endian floats; then the unsigned mode varint, one required boolean and, when nonessential is set,
four color bytes. Mode selection retains the five preset positions and Normal fallback for unknown
ordinals. Binary input remains the entire original byte/view domain, with no fixed mode whitelist.

Reader truncation returns its neutral value and parks the same cursor at byteLength+1. The bone parser
checks at the next loop boundary; it does not insert new per-field validation or silently replace its
current record behavior with an empty success. It constructs bones in wire order and the section handler
appends the returned list to the original context.bones. The skip entry runs the same parsing/consumption
without a diagnostic sink. Names/parents, all transform fields, original reader/view, diagnostic sink and
shared preset identities must survive. Existing reader/census tests do not establish native bone-handler
parity; none is claimed here.

## Current scoped result

The final changed one-header check completes at one worker: **0/1 pass, 1 fail, 0 unattempted**.
The original Any-versus-Ref conditional clears. Its next first error is the existing optional Record
diagnostic construction at `spine_binary_bones_handler.hpp:63`, under
[the existing request](optional-record-literal-not-materialized.md). The row remains failed, moves
into `other-40da43f7` and retains its previous cluster. Only that existing cluster's owned membership
count rises by one; the obsolete one-header cause is removed. Historical/G1 totals and foreign rows
remain unchanged.

Source JavaScript identity, exact normal generation/owner alias and final provenance/foreign-content
checks pass. No native bone/parser executable or link check is run, and no parsing behavior, complete
module or header gain is claimed. The optional Record and independent preset-to-Bone2D owner contracts
remain open. No fake bone/parser, empty success, copied view or narrowed input is supplied.
