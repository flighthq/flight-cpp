# Latest Flight runtime tranche — batch acceptance

Reports the tranche requested on 2026-09-19. Numbers are measured at the pins below, not carried
over from the request.

## Baseline

| | Requested | Adopted |
| --- | --- | --- |
| `flight` | `7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad` | same |
| `flight-compiler` | `66cad9ccda7e67a017d7c0ab880a2033047801fb` | `ef60fb6d3181ce5c12d5ce8d75c8ef6518c9b168` |
| comparison `flight-cpp` | `7107eee30583e10265e7acab1784edfae96bf07c` | same |

**The requested compiler commit does not exist on the remote.** `66cad9c` is not on `main` — whose
tip was `ef60fb6`, 53 commits past the previous pin `fbfcc11` — and not in any `refs/pull/*/head`.
The user chose `main`'s tip. Corroborating evidence for that being the intended counterpart:
`flight-compiler`'s own `dependencies.lock.json` at `main` pins `flight-cpp` to `7107eee`, exactly
the comparison revision the request names.

One consequence, stated rather than smoothed over: **the request's figures of 1,420 / 2,904 emitted
modules and a 59-failure generated-code baseline cannot be asserted as reproduced**, because they
were produced by a revision nobody outside the author's machine can check out. Everything below is
measured here.

SDL profiles used, in order: `runtime`, `headless`, `web-types`, `sdl-image`, `sdl-gl`, `sdl-wgpu`,
`sdl-app`. SDL3 3.4.2 and Vulkan 1.4.341 development headers were present for the final compile;
`CXXFLAGS` came from `pkg-config --cflags sdl3`, without which 44 SDL-dependent headers report
failures that are only a missing include path.

## Emitted modules

| Inventory | Emitted | Source modules |
| --- | --- | --- |
| SDL profiles (`out/sdk-sdl`) | **1435** | 2904 |
| Portable, committed (`generated/`) | **1138** | 2904 |

The committed portable inventory was 985 / 2900 at the previous pins.

## Refusals

1469 refusal records: **721 dependency cascades** and **748 direct refusals**.

Only **26 modules** are still blocked on an unbound external symbol, across 31 distinct symbols
(one of which is the test harness). Every remaining family is one this tranche classified as
deliberately unbound rather than missing:

| Symbols | Classification |
| --- | --- |
| `LocalesArgument`, `Intl.Segmenter`, `Intl.SegmentData`, `SegmenterOptions`, `Segments`, and the collator / date-time / number / plural / relative-time / list options | Intentional refusal — no Unicode segmentation tables and no host segmentation provider |
| `AudioContext` | Profile-specific unavailable capability — SDL's audio-device provider is not the browser decode/node graph |
| `Navigator`, `Node`, `Text`, `ChildNode`, `SVGDefsElement`, `SVGFilterElement`, `SVGSVGElement`, `OffscreenCanvasRenderingContext2D`, `PropertyDescriptor`, `PropertyDescriptorMap` | Intentional refusal — browser surfaces the SDL profile does not provide |
| `HTMLImageElement[value]`, `HTMLVideoElement[value]`, `OffscreenCanvas[value]`, `ImageBitmap[value]`, `VideoFrame[value]`, `DOMException[value]` | Types are bound as opaque capability handles; the *constructor values* stay unbound |
| `Function[value]`, `location[value]` | No target exists |

The largest direct-refusal groups are compiler emission limits, not binding gaps: contextual union
value types (112), variant-alternative type assertions (82), `optionalSingle` construction evidence
(40), `typeOf` type computation (40), and `typeof` runtime type evidence (33).

## Compiling headers

**1368 of 1435 generated headers compile independently**; 67 fail. 18 distinct diagnostic
fingerprints across 43 distinct raw diagnostics.

The previous report, at the previous pins, was 1131 of 1161 with 30 failures. The corpus grew by 274
modules, so the raw counts are not comparable and the fingerprints are what to read.

**Fingerprints gone since the baseline** — no remaining failure matches these:

- `'…' was not declared in this scope` (3)
- `'…' parameter not permitted in this context` (2)

**Fingerprints that persist**, baseline → now: `could not convert … from … to …` 5 → 15;
`no matching function for call to …` 4 → 9; `no match for call to …` 3 → 9;
`expected primary-expression before … token` 4 → 5;
`no match for 'operator…' (operand types are … and …)` 1 → 4 and 2 → 2 for the `{aka …}` variant;
`conversion from … to non-scalar type … requested` 3 → 3;
`request for member … in …, which is of non-class type …` 1 → 1;
`'…' is not a member of '…'` 1 → 1; `cannot convert … to …` 1 → 1.

**Fingerprints new since the baseline**, all from code paths the larger corpus reaches for the first
time: `'…' has no member named '…'` (8) and its `did you mean` variant (1);
`cannot convert … to … in initialization` (2); `operands to '…' have different types` (2);
`could not convert … from … {aka …} to …` (1); `'…' is not a member of '…'; did you mean '…'` (1);
`cannot convert … to … in return` (1); and the one runtime static assertion (1), which is the
runtime's own guard refusing a malformed write rather than a new defect.

Read by fingerprint rather than by count, the corpus did not regress: two groups closed outright,
none of the persisting groups changed character, and the growth in the others tracks the 274 extra
modules now being compiled at all.

## Failure classification

Each remaining failure is recorded as one of the five requested causes. **No failure is
runtime-owned.**

| Count | Group | Cause |
| --- | --- | --- |
| 30 | result / subtype / element-type conversion — `BitmapReadbackOutcome` to `BitmapReadbackResolution`, `Array<variant<…>>` cast to `Array<Any>`, `finish_entity` handed back as a synthesized struct name | compiler emitted the wrong type |
| 11 | member reached on the wrong receiver — `.trim` on `std::optional<String>`, `.delete_` on `std::optional<Ref<ShapedRun>>`, `.value` on `Array<double>`, `.process` on `Record<String, Any>`, `.begin` on `Ref<TextFormat>` | compiler lost evidence (an optional never unwrapped, or a member resolved against the wrong receiver) |
| 9 | callback and optional-callable invocation — an omitted argument against a `std::optional<…>` parameter, a settled-result variant invoked directly | compiler lost evidence |
| 8 | operator over mismatched domains — `&&` on `String` and `bool` without the truthiness lowering the backend already has, `[]` with a `String` on a nominal struct, `?:` mixing `std::nullopt` and a structural row | compiler lost evidence |
| 5 | emitted syntax — `expected primary-expression before '{'` | compiler emitted malformed code |
| 2 | `structural_ref_cast` applied to `std::optional<Ref<TextureSource>>` with a `Ref<Bitmap>` row as the target | compiler emitted the wrong type — an optional overload would not fix it, because the element types are unrelated |
| 1 | `row_set` of `SequenceView<StructuralRef<…RiveCoreObject…>>` into an `Array<Ref<RiveCoreObject>>` member | compiler emitted the wrong type — the runtime static assertion is the guard working |
| 1 | other | compiler emitted the wrong type |

Nothing here was accommodated by widening a runtime operator. Where the runtime genuinely lacked an
operation, it was added and tested (next section); where the generated code was malformed, it stays
refused.

## Runtime-owned failures closed by this tranche

| Was | Now |
| --- | --- |
| `emit_signal` rejected a nominal `Ref<T>` against a listener taking `Readonly<T>` | `callable_signature_v1::accepts` admits the row projection of the same object — and nothing wider |
| `flight::String` had no `codePointAt` | `String::code_point_at` returns `std::optional<double>`, surrogate-pair aware |
| `flight::Array` had identity but no equality, so an emitted `!==` between two arrays did not compile | `operator==` compares array identity, which is what `===` means for an object |
| `flight::host_sdl::DomStyle` had no `transform` | added; the DOM scene writes a CSS `matrix(...)` there |
| A `Partial<T>` probe of an unrelated subject was rejected by the new widening rule | a readonly partial target is admitted, because `Partial<T>` asks nothing of its subject |

## External binding additions, by profile

| Profile | Added |
| --- | --- |
| `runtime` | `ProxyHandler[type]` → a deliberately non-constructible `flight::ProxyHandler<Target>`; `FontFace[type]` and `FontFace[value]` → `flight::FontFace` |
| `sdl-gl` | `GLbitfield`, `GLboolean`, `GLbyte`, `GLclampf`, `GLfloat`, `GLint`, `GLint64`, `GLintptr`, `GLshort`, `GLsizei`, `GLsizeiptr`, `GLubyte`, `GLuint64`, `GLushort`; weak-key policies for the eight WebGL object handles |
| `web-types` | `GPUCanvasAlphaMode` → `flight::String` |

`ProxyHandler[type]` closes the named blocker on `@flighthq/entity/src/guards.ts`. `FontFace` closes
`types/HostFontLoading.ts` and `types/FontResource.ts`; `Host.ts` then advances to its next
dependency link rather than emitting, which is a chain rather than a single lever.

## ABI changes

**None.** `flight-runtime-contract/2` and C++ ABI 1 are unchanged, and no capability was claimed in
`contract.hpp`. Every addition is a header-level API with semantic tests.

## What the compiler still owns

Three findings are worth carrying upstream with their exact evidence.

1. **`guards.ts` needs two compiler changes beyond the binding.** With `ProxyHandler[type]` bound,
   the module moves off "binding plan is incomplete" and stops on
   `createGuardedEntityRuntime`: its `set` trap compares `prop === 'binding'`, a plain named key,
   while `getCppStructuralWriteProxyConstructionPlanCpp` requires a *computed symbol* key. The
   runtime primitive for the named case now exists —
   `make_structural_write_proxy(target, std::string intercepted_name, before_write)`, which
   intercepts named writes and leaves symbol writes alone. In the full package graph the module then
   reports `intersection types require C++ multiple-inheritance lowering`.

2. **Row widening is representable now; the union arm is not.** Passing
   `Readonly<GlTextureRenderTarget>` where `Readonly<GlRenderTarget>` is expected emits and converts
   under proof. Passing it where `Readonly<GlRenderTarget> | null` is expected still refuses with
   `contextual union value type … is not a represented runtime domain`, because a provable widening
   is not yet treated as reaching the union's domain.

3. **Entity construction relies on a cast the runtime had to be taught.** `allocate_entity` builds an
   empty `anonymous_…<Type>` bag and casts it to `EntityConstruction<Type>`; `finish_entity` then
   casts to `Type`. The bag has no members, so field writes landed in cell storage and the final cast
   found no object — `structural_ref_cast` now mints the object the row stands for. The cheaper fix
   is on the compiler side: emit the construction over the nominal type directly.

## Verification

- `npm run check`: **30 of 30 gates pass**, including two added by this tranche —
  `hostCapabilityOracle` and `examples:run`.
- `cmake --build --preset development` + `ctest --preset development`: 8/8.
- SDL host build (`FLIGHT_CPP_BUILD_HOST_SDL=ON`, warnings as errors, SDL 3.4.2) + `ctest`: 10/10,
  with `SDL_VIDEODRIVER=offscreen` and `SDL_AUDIODRIVER=dummy`.
- `npm run sdk:generate:sdl` then `npm run sdk:compile:sdl` once each, at the end, with
  `FLIGHT_CPP_COMPILE_JOBS=14` on a 16-thread machine.
- `out/sdk-sdl-header-compilation.json` is preserved.

Two gates are new because two things could previously pass without being true. `examples:run` builds
**and runs** the portable examples: `sdk_math` had been segfaulting at `main` — `finish_entity`
returned a null reference that the example dereferenced — while every gate stayed green, because
`examples:check` only diffs generated headers against the compiler and `npm run check` never built
the runtime at all. `hostCapabilityOracle` compiles and runs the pinned compiler's own output for
the plain-capability shape, so that contract is held by a test rather than by assertion.

## Left undone, and why

**SDL window visibility.** Upstream deleted `HostApplicationVisibilityProvider`, whose `isVisible()`
the SDL adapter answered from window flags. The successor `HostWindowVisibilityCapability` is a
different contract — `show(AppWindow)` / `hide(AppWindow)`, commands addressed by app window rather
than a query about this one — and binding it needs an `AppWindow`-to-SDL-window registry the adapter
does not have. The member was removed rather than guessed at. Every other part of the upstream
`Host*Provider` → `Host*Capability` reshape mapped cleanly, including
`HostFullscreenProvider` → `HostElementFullscreenCapability`, and the capability structs no longer
carry an entity runtime key — which is the same finding as the plain-object capability work below.
