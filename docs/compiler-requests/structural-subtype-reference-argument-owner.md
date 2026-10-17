# A structural subtype argument must retain an identity-preserving view

TypeScript accepts a reference to a structurally wider interface where a readonly smaller interface
is required. The C++ emitter instead gives both named interfaces unrelated owning reference types and
passes one directly as the other, which does not compile.

Measured with flight-compiler `2b687ea` and Flight `a627849`.

## Minimal TypeScript

```ts
interface Position {
  readonly offset: number;
  readonly line: number;
  readonly column: number;
}

interface Token {
  readonly kind: string;
  readonly offset: number;
  readonly line: number;
  readonly column: number;
}

function positionLine(position: Position): number {
  return position.line;
}

function tokenLine(token: Token): number {
  return positionLine(token);
}
```

## Observed C++

```cpp
double position_line(flight::Ref<Position> position);

double token_line(flight::Ref<Token> token) {
  return position_line(token);
}
```

`flight::Ref<Token>` and `flight::Ref<Position>` are sibling `shared_ptr` owners, so Clang rejects the
call even though every member the callee may read is present on the source object.

## Required behavior

A readonly structural parameter must receive an identity-preserving view over the original owner, or
both compatible declarations must share a carrier that preserves that owner. The compiler must not
silently copy the visible fields: identity and later reads of the original object can be observable in
the general case.

## Affected module

`@flighthq/scene-document`, `sceneDocumentYamlSubset.ts`: the parser passes its private token record to
`createSceneDocumentYamlSubsetRefusal`, whose position parameter reads only the token's `offset`,
`line`, and `column`. The generated call passes `Ref<Token>` to `Ref<Position>` and prevents the parser
and every header including it from compiling.

## Carrying workaround

At this private, read-only boundary the package constructs the declared position record from the same
three token fields. The callee immediately copies those scalar fields into the published refusal and
does not expose or compare the position object's identity, so the fresh record preserves all observable
results. The workaround should expire when readonly structural subtype arguments use an
identity-preserving view.

## G2 Scene3D reveal optional ease owner projection

At Flight a627849 / compiler806d257, revealScene3DResourcesOnResolve forms
`tweenOptions = options?.ease !== undefined ? { ease: options.ease } : undefined` and passes it as
createTween's optional readonly TweenOptions. The generated local is optional<Ref<ease-record>>;
the argument instead constructs RowWritable<ease-record> directly from the **whole optional** before
casting to the readonly TweenOptions row. StructuralRef accepts a supported present owner/view,
not an optional container, so actual unused-header compilation after the scratch collector declaration
fails there (candidate183/original181). See the
[retained-callee measurement](retained-declaration-calls-omitted-callee.md#g2-scene3d-resource-reveal-collector-declaration-fails-before-link).

This extends the structural argument request to a compatible optional projection: absence must stay
std::nullopt; only a present payload may become a readonly view of the same anonymous ease owner,
with the same EasingFunction and all other optional TweenOptions fields absent. Unconditional value(),
constructing a default owner for absence, copying the original options or granting writable authority
is not that handoff. The runtime's rejection is correct; no permissive optional-to-row constructor,
source patch or override is carried. The independent manager/target union-order failure remains.

## G2 SWF shape edge reader: a writable structural parameter

At Flight `a627849` / compiler `806d257`, `packages/swf/src/swfShape.ts` calls
`readSwfShapeEdgeRecord(reader, state)` at source line 585. Here `state` is the private
`SwfShapeState` (`fill0`, `fill0Segment`, `fill1`, ... plus `x` and `y`), and the callee parameter is the
anonymous `pen: { x: number; y: number }`. The other caller, at line 318, passes a real pen literal.

The emitted call at `flight/swf/swf_shape.hpp:241` passes `Ref<SwfShapeState>` to
`Ref<x_y_8365950bd60f783f>` (GCC 15.2, one worker: `could not convert 'state' from
'shared_ptr<flight::swf::SwfShapeState>' to 'shared_ptr<flight::types::x_y_8365950bd60f783f>'`; Clang
reports it as no matching `read_swf_shape_edge_record`).

This case is stricter than the readonly one above. The callee **writes** the pen: `pen.x +=`, `pen.y +=`,
`pen.x = controlX + ...`. Those writes must advance the caller's own `SwfShapeState` position, because the
next edge record and the segment builder read `state.x` and `state.y`. A copied `{ x, y }` record would
drop every update. So the required view must be a **writable** alias over the original owner's `x` and
`y` cells. Even the scalar-copy workaround that is valid for a readonly parameter is not valid here.

Four more SWF headers reach `swf_shape.hpp` through their includes: `swf_text`, `swf_font_handler`,
`swf_static_text_handler` and `swf_font_tag_family`. They were not recompiled. The later `swf_shape.hpp`
errors (segment array carrier at `:540`, record union `index()` at `:588`) were noted but not analysed in
this unit. Nothing is carried: no copied pen, no write-back shim, and no narrowed shape domain.

## G2 Scene3D present: native override carrier drift

At Flight a627849 / compiler806d257, `present_gl_scene_3d.hpp:46` already passes the correct
readonly `GlTextureRenderTarget` row retaining the caller's target. Both the pinned source and
generated `presentGlRenderTarget` take that readonly view, with an optional/null readonly
`GlRenderTarget` destination. The effective partial native present and linear-to-sRGB overrides
instead take nominal `Ref` targets. This is override ABI drift, not an emitted argument-owner
error, missing heritage or omitted function. Do not repair the caller with a cast or copied owner.

Aligning those two native signatures cleared the one-header compile (1/1, 176.6 s, one worker),
but the new actual SDL/Mesa test failed 0/1 (0.36 s) on its first explicit destination bind:
`row_get<"framebuffer">` requested `optional<WebGlFramebuffer>` from the same concrete texture
owner's present `WebGlFramebuffer` cell and threw `std::bad_cast`. The candidate and test were
rejected; the unchanged inventory retains original0/1. The distinct runtime read-projection gap,
measured stack, evidence limits and full pass/present semantics are filed in
[readonly structural member carrier widening](readonly-structural-member-carrier-widening.md).
No native implementation, generated/source patch or successful present is carried.
