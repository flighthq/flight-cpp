# A nullable generic value narrowed present must be unwrapped at calls

Control flow narrows a nullable generic value before it is passed to another generic function. The
emitter preserves the outer `std::optional` at the call instead of passing its present payload, so the
header parses but every instantiation fails.

## Minimal TypeScript

```ts
function has<T>(value: T): boolean { return true }
export function narrowed<T>(value: T | null): boolean {
  if (value === null) return false;
  return has(value);
}
```

The required call is `has<T>(value.value())`. Passing `value` asks a function taking `T` to accept
`std::optional<T>` and either fails deduction or invents a different `T`.

## Affected Flight module

At Flight `a627849` / flight-compiler `2b687ea`,
`packages/selection/src/pointerSelectionPolicy.ts` narrows `hit` with `!hit` in the early-return guard.
The emitted `apply_pointer_up_selection_policy` then calls both functions with the optional itself:

```cpp
is_node_selected<NodeType>(state, hit);
select_node<NodeType>(state, hit);
```

Its sibling pointer-down function uses `hit.value()` correctly after an equivalent guard, which is the
passing control. The focused header gate misses this because the function is a template; native selection
instantiation exposes it.

## Carrying workaround

The selection policy override preserves the emitted branches and passes `hit.value()` after the same
presence guard. It expires when narrowing consistently projects the present payload at generic calls.

## G2 Scene2D kind usage passes a guarded member as optional storage

At Flight `a627849` / compiler `806d257`, the one recorded header in
`call-has-no-matching-7f4ae181` remains failed at `scene2d/scene_kind_usage.hpp:95`:
`no matching function for call to 'add_scene_2d_used_kind'`. This is the same present-payload call
boundary as above, now a **non-generic scalar helper and a repeated structural member read**, not
the generic-local reduction. This docs-only audit retains **0/1** and the exact original diagnostic;
it does not rerun either that reduction or native/generation checks. No override supplies this module.

Source `packages/scene2d/src/sceneKindUsage.ts:37–39` already guards the nullable field:

```ts
if (node.blendMode !== null && node.blendMode !== BlendMode.Normal) {
  addScene2DUsedKind(out.blendModes, node.blendMode);
}
```

`HasBlendMode.blendMode` is `BlendMode | null`, where BlendMode is the open string type. Native
`Node2DTraits::blend_mode` and its row read use `optional<String>`. The emitted visitor is a readonly
merged `NodeOf<Ref<Node2DTraits>>` row over the original node; line94 tests the row read's
`has_value()` and compares it with Normal. Line95 nevertheless passes another complete optional
read to `add_scene_2d_used_kind(Array<String>, String)`. That helper is present at line64: it scans
the same array for strict string equality and appends only a new value. There is no missing overload
or refused helper to declare. Project the guarded string in the argument's actual carrier; accepting
arbitrary optional strings or choosing a default would invent an absent-argument result.

Keep the source's evaluation order and short-circuiting. BlendMode can be read three times on this
branch; material is read for the null guard and again for its kind. A cached first read, extra guard,
fallback Normal value or nonempty-string test is not established as equivalent for getters/live cells.
The material branch at native97–98 also retains a guarded optional member read; that inspected later
projection is not a newly measured next diagnostic. The source guard, not a permissive runtime
conversion, must supply the present-value evidence.

The output is the caller's exact `Ref<Scene2DKindUsage>`, retained in an emitted binding cell. Its four
Array<String> handles share their original storage; clear lengths in source order (blend, material,
node, command) rather than replacing arrays, then accumulate and finally sort each with default string
ordering. Initialization separately creates four fresh empty arrays. Preserve aliases to reused output
arrays and partial mutation before an exception; no copy of the usage record or transactional reset.

Scene2D.root is Node2D, the intersection `Node<Node2DTraits> & Node2DTraits`. Visit that same root first,
then all descendants in live child-array order, depth-first pre-order, without enabled/visible or
renderer filters. The existing node-exact-capability-runtimes patch carries NodeOf<Traits> through
traversal, and scene-nodeof-traversal-capabilities already removes the redundant visitor assertion.
Those patches remain unchanged. Native111 still selects `for_each_node_descendant<Ref<NodeTraits>>`
for the readonly Node2DTraits visitor, while NodeRuntime<Traits>::children stores NodeOf<Traits>.
This inspected later [node-trait deduction boundary](heritage-argument-generic-deduction.md) must
retain the concrete trait/runtime/child owner; the first scalar projection alone does not qualify the
whole module. No default-trait reinterpretation, copied graph, global trait alias or writable promotion.

Collect each node's actual kind, every explicit blend string except null and the case-sensitive
`Normal`, and the kind of each present material. Built-in and vendor strings remain accepted;
null materials do not synthesize a StandardMaterial requirement. No backend registry is consulted.
Command discovery is structural across all node/data kinds: read node.data and then its optional
commands property once, preserving inherited properties/getters. Skip only an undefined commands
result; a present null is not an empty stream and still reaches the source failure. Walk the existing
flat `[key,argCount,...args]` buffer by `argCount + 2`, collecting only keys, not string-valued arguments.
Keep the full ShapeCommandToken domain (matrix, texture family, boolean, number, number-array, string,
null), rather than filtering tokens or limiting custom command/node kinds to a built-in table.
Native100's partial data view still needs the actual retained data cells; no fabricated commands or
empty usage is supplied here. Deduplicate by string equality and sort all four lists after traversal;
texture-source kinds remain outside this API's reported fields.

The twelve inspected `sceneKindUsage.test.ts` source cases cover empty construction/initializer,
sorted/deduplicated kinds, nested descendants, command keys/deduplication/stride/missing stream,
nondefault blend modes, present/absent material kinds and repeated output reuse. They are not new
native results or complete getter/vendor coverage. `generated_render_scene2d_coverage_test.cpp`
constructs usage records directly for coverage decisions; it never calls get_scene_2d_kind_usage
and does not prove the walk. This disposition adds existing request links to the original row/cluster
and worklist101 only; C++/runtime/source/patch/repair/override/manifest/pins and foreign records stay
unchanged. No native kind-discovery, registration, rendering or header-availability gain is claimed.
