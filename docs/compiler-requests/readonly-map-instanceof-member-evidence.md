# A readonly Map arm must retain its Map constructor evidence

At Flight `a627849` / compiler `806d257`, the current `scene3d-gl/glShadedPrelude.ts` carries both
registry sources explicitly, but refuses its private `isGlModifierSnippetTable` guard:

```ts
type GlModifierSnippetSource =
  | Readonly<ModifierRegistry>
  | Readonly<ReadonlyMap<Kind, GlModifierSnippet>>;

function isGlModifierSnippetTable(
  registry: GlModifierSnippetSource,
): registry is Readonly<ReadonlyMap<Kind, GlModifierSnippet>> {
  return registry instanceof Map;
}
```

The rule is `cpp-instanceof-member-unresolved [source-portability]`:

```text
an instanceof test against Map needs the operand's union to name it or to carry the erased arm;
this union does neither, so nothing tells the target which kind is being asked about
```

The generated source carrier already has the concrete Map arm:

```cpp
using GlModifierSnippetSource = std::variant<
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<ModifierRegistry>>>>,
    flight::Map<flight::String, flight::Ref<GlModifierSnippet>>>;
```

Lower the source constructor question against that same represented carrier, reconciling the readonly
wrappers and `ReadonlyMap` spelling with the actual Map arm. For this two-arm carrier, the result is
an exact Map-arm question; it is false for the registry owner, not a test of whether an object happens
to expose `get`. Do not replace the registry with a copied Map or discard either source alternative.

The resolver's current emitted body already retains a registry's three `ModifierDefinitionValue`
arms, selects the exact GL snippet owner through checked union recovery, and returns the same Map
entry reference in the other branch. Keep the eight built-in modifier owners and open vendor arm,
neutral definitions, GL/WGPU snippet arms, null lookup, callbacks, ordering, keys and shader source.
The carried [concrete-value source patch](modifier-concrete-value-path.md) is applied at this pin;
the older native override was still shadowing these owner flows with prior emission.

This differs from [`instanceof` emitted verbatim](instanceof-lowered-verbatim.md): this guard is
refused before any C++ operator is emitted. It also differs from
[lost property-union evidence](property-union-evidence-lost.md): the current C++ alias visibly retains
the concrete Map arm rather than an unknown-only carrier. The missing step is recognizing its
constructor evidence through the readonly collection spelling.

The existing `declare-refused-function` mechanism can add only
`bool is_gl_modifier_snippet_table(GlModifierSnippetSource registry);` under the exact refusal/direct-call
witnesses. An unused current-emission header with that declaration and the previously accepted native
color-feature optional projection compiled, linked and ran. Such a declaration supplies no guard,
shader snippet or rendering behavior; a real resolver call must still fail to link on the exact missing
guard until this request is implemented.
