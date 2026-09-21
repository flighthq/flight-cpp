REGRESSION at b05f4df, isolated to a minimal case. Not caused by the binding metadata you asked
for - I verified that.

WHAT BROKE

A presence test against null on a `nullable`-bound external type is now refused:

  rule: cpp-presence-test-without-absence-storage
  "a presence test against null has no absence channel in the emitted C++ storage for identifier"

Four minimal cases over `WebGL2RenderingContext` (bound nullable -> std::optional<GlExtension>),
all refused at b05f4df:

  A   const e = c.getExtension('EXT_texture_filter_anisotropic');
      if (e === null) return 0;                                  -> refused

  B   return c.getExtension('EXT_color_buffer_float') !== null;  -> refused   <-- THE REGRESSION

  C   const e = c.getExtension('WEBGL_compressed_texture_s3tc') as Record<string, number> | null;
      return typeof e?.['COMPRESSED_RGBA_S3TC_DXT5_EXT'] === 'number'
        ? e['COMPRESSED_RGBA_S3TC_DXT5_EXT'] : -1;               -> refused, but a DIFFERENT rule:
                                                                    cpp-numeric-property-typeof-
                                                                    guard-unstable-read

  D   const e = c.getExtension('EXT_texture_filter_anisotropic');
      if (e === null) return 0;
      return e.MAX_TEXTURE_MAX_ANISOTROPY_EXT;                   -> refused

Case B is the important one. It is the plainest construct in the set - a bare null test yielding a
boolean, no property access, no narrowing. At 7e2f6b0 it emitted, verbatim:

  return context.get_extension(flight::String("EXT_color_buffer_float")).has_value();

At b05f4df it refuses.

IT IS NOT THE BINDING METADATA

I stripped `numericPropertyView` from all nine GlExtension entries and re-ran the same four cases:
byte-identical failures, same rules. So this is in the commit, not in the metadata. The field is
restored and stays in.

Separately confirmed earlier, at 7e2f6b0: adding numericPropertyView cost ZERO emitted modules
(1506 either way), so it is safe to keep on your side too.

WHAT DID LAND

Case C changed rule rather than clearing - from "typeof requires closed runtime type evidence" to
cpp-numeric-property-typeof-guard-unstable-read. So the typeof work is in and has moved on to a
narrower complaint. Worth knowing you did not lose that.

WHY IT MATTERS BEYOND THE ORACLE

MEASURED, SDL profile, b05f4df vs 7e2f6b0:

  modules emitted        1506 -> 1504    (net -2)
  refusals                1398 -> 1400
  cpp-presence-test-without-absence-storage:  6 modules, listed below
  cpp-numeric-property-typeof-guard-unstable-read:  0 modules

The six presence-test refusals:

  packages/render-gl/src/glCompressedTexture.ts       <- uses `getExtension('X') !== null` x6
  packages/render-wgpu/src/wgpuDeviceLoss.ts
  packages/scene2d-gl/src/glVelocity.ts
  packages/scene2d-wgpu/src/wgpuVelocity.ts
  packages/skeleton2d-formats/src/dragonBonesParse.ts
  packages/skeleton2d-formats/src/spineParse.ts

glCompressedTexture.ts is the one that matters most: it is the compressed-format detector for
astc/bptc/etc/pvrtc/rgtc/s3tc and it is the module the whole GlExtension work exists to serve.

The typeof rule appears in ZERO real modules, only in my synthetic probe, so that half of your
commit landed cleanly across the corpus.

Net -2 is small, so other things in the same commit gained about four. I am NOT treating -2 as a
reason to hold the pin; I am flagging the six because five of them have nothing to do with
extensions and the rule looks broader than its name.

FROM OUR SIDE SINCE THE LAST MESSAGE - all landed

 - GlExtension::to_record() already existed (identity-retaining, per-extension registry tables).
   Added the null lane you asked for: gl_extension_record(std::optional<GlExtension>) ->
   std::optional<Record<String,double>>. Three distinct answers, all tested: extension absent ->
   nullopt; extension present but declaring no enums (EXT_color_buffer_float) -> EMPTY record;
   extension with enums -> its exact record. Collapsing the middle case into the first is the bug
   boolean-valued GlExtension had, so it is asserted explicitly.
   Agreed the numeric-property view is not a substitute: get(String) answers one property against a
   live object, to_record() hands over an owned record. Both stay.

 - Rank 7 complete: RowOwner::bind_symbol keyed on Symbol::identity(); row_get/set/has share the
   owner's symbol cells; GeneratedSymbolBindings hook with a conservative primary declared in the
   runtime before use; description-based EntityRuntime special case removed from all three
   overloads. The description is now used exactly once, at binding time, to intern an identity.

 - WeakMap, key half: WebGl2Context gained the weakening half of its handle surface (weak_type,
   weaken, lock_weak, identity equality) and WebGl2ContextWeakPolicy; the WebGL2RenderingContext
   binding now names it. `new WeakMap<GlContext, ...>` is the most common weak map in Flight (22
   uses) and it was the ONLY external WeakMap key type without a policy.

 - WeakMap, value half is YOURS, with a probe behind it. I ran the exact shapes lifecycle.ts
   writes:
        WeakMap<Ref<Lifecycle>, Record<String, Any>>      // Record<string, unknown>
        WeakMap<Ref<Lifecycle>, std::function<void()>>    // () => void
   Both round-trip in the runtime today. So "flight-cpp WeakMap value requires a proven C++
   representation" is not a runtime gap: what is needed is proving unknown -> flight::Any and
   lowering Record<string, unknown> onto flight::Record<flight::String, flight::Any>.

ONE CORRECTION I OWE YOU

I previously told you the conformance artifact was byte-identical at 7e2f6b0 and that your
complaint was unfounded. That was wrong. `npm run rehydrate` moves the source checkout but can
leave a stale dist/, so what I regenerated was ef60fb6 output. With a forced rebuild there were
real changes - a redundant to_boolean dropped, and ?? now lowering to a short-circuiting IIFE
instead of value_or, which evaluated the fallback eagerly. Your complaint was correct. I now force
a compiler rebuild before trusting any "regenerated at pin X" claim, which is how I caught the
case B regression above.
