# A fresh RichText loses its runtime before the caller can read it

The existing partial RichText override returns a fresh node whose first public runtime read throws:

```cpp
auto node = flight::text::create_rich_text();
(void)flight::text::get_rich_text_runtime(node);
// flight::Error: RichText runtime is unavailable
```

The [rich-text B receipt](../../artifacts/rich-text-data-b-coherence.json) preserves the first compiled public consumer's uncaught failure (signal 6 / shell exit 134), its exact initial fixture, and the final complete-B observation. The final 26-check probe explicitly catches this error as a remaining partial boundary. It does not qualify runtime recovery or normalize that failure into source behavior.

At raw Flight pin `a62784923f3be814463286c9fb28edfe6ab15789`, `richText.ts:createRichText` passes `createRichTextRuntime` to the original `createNode2D`. The original `initializeNode` stores the returned runtime on the node's `EntityRuntimeKey`, and `getRichTextRuntime` returns that retained runtime through `getNode2DRuntime`. The complete original-module oracle calls the public factory, reads its runtime twice, checks exact object identity, and observes both selection indices as zero. That case passes; all 1,285 loaded sources are byte-verified against the raw pin.

The native override instead stores a generic `create_node_2d_runtime()` result in the node's physical runtime slot, allocates a separate `RichTextRuntime`, and registers only a `weak_ptr` to that separate object. The local strong reference dies when construction returns. The getter then finds an expired weak entry and throws. This construction/recovery mechanism predates the seven inherited-designator/default fixes and is unchanged by them.

The exact default source/override/fixture inputs and failed/successful probe logs are in `builder3-rich-text-b-qualification.tgz`, SHA256 `3c2ef523f7c4f5d4f28506a06179080bdea4724884b2404afaa4de21261f3f29` (1,055,761 bytes). The final complete-B probe uses held Builder2 head `5f79108f` plus the declared current native floor; its `legacyRuntimeUnavailable: true` flag is an explicit failure boundary, while the source observation has `sameRuntimeOwner: true`.

A repair must preserve one source runtime owner in the actual node slot and recover that same owner on repeated public reads. A stronger registry reference alone would retain a second runtime alongside the generic physical one. Copying base fields, fabricating a replacement runtime in the getter, or writing an incompatible symbol value into shadow cells would also split the source identity. The relevant native carrier/base-view representation must be made coherent before claiming the original default factory/runtime consumer. Foreman has queued this separate defect to Builder6 after the general imported-call guard work; no runtime or Types repair is implemented here.
