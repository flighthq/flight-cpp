# Mutable Node runtime access through its retained owner

The original mutable accessor reads `source[EntityRuntimeKey]!`. A widened DisplayObject or Node2D owner has that exact cell, but its native object can be an anonymous subject rather than a native `Node<Traits>`. Recovering a native Node reference would fail or require a different object.

The declared type-only patch changes the accessor parameter from `Node<Traits>` to the existing `EntityConstruction<Node<Traits>>` mapped writable family. The emitted accessor reads the original Symbol cell through its retained row owner and returns the original runtime reference. Its body and readonly counterpart remain unchanged. This bounded change was approved by Manager a4639758 and Foreman 944df1fd.

The complete TypeScript 5.9.3 program emits byte-identical JavaScript for the original module before and after the change. All 18 existing accessor calls retain their source text and resolved declaration, with no diagnostics at those calls; the program's 222 existing diagnostics are unchanged. Native controls exercise the whole original Node and revision modules, exact runtime identity, live writes, slot replacement, lifetime, readonly rejection, and denied native Node recovery.

The pinned emitter separately leaves a generic default callback unspecialized at a concrete nullable callable position. Its compiler patch uses exact contextual inference at the lazy default and preserves the original generic function and signature. The same error reproduces in the unmodified Node baseline.

Both patches expire when upstream emits the original accessor over retained writable owners and selects equivalent exact generic defaults, retaining these negative and ownership controls. Full public Lottie and installed SDK qualification remain separate from this preparation.
