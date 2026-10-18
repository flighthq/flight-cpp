# Decision requested: readonly automatic particle parser entrypoints

This proposal is **inactive**. Its patch is stored under `docs/compiler-requests/proposals/` and is absent from the active source-patch manifest. Manager must decide whether this bounded additional entrypoint pattern is approved before activation.

The original `parseParticleConfig` and `parseParticleConfigDocument` accept optional nominal `ParseParticleConfigOptions`. Each forwards that binding once to the corresponding codec callback, whose declared parameter is already `Readonly<ParseParticleConfigOptions>`. The options interface inherits two optional numeric fields. A public caller with a schema-only readonly view currently reaches the nominal C++ ABI, which cannot recover a native object that the view never owned.

The previous approved proof covers one optional numeric field read directly through optional access. These functions use a different pattern: forwarding the complete optional binding into an already-readonly callable slot. The concrete decision is whether to permit a second, uniquely named readonly entrypoint for this exact forwarding pattern while preserving the original native entrypoints and function-pointer ABI.

The proposed recognition requires a synchronous, nongeneric function with a required string parameter and one optional nominal options parameter, exactly two optional numeric fields, and exactly one reference to that options binding. That reference must be the second argument of an ordinary two-argument property call whose semantic parameter is a readonly view over the same canonical options declaration. Capture, aliasing, rebinding, extra reads, identity observation, multiple forwarding uses, mutable destinations and different canonical declarations fail the proof. Existing one-field direct-read handling stays separate.

The additional helper emits the original IR body with readonly parameter storage. It does not copy options fields, manufacture a native owner, insert a cast or replace the native function. A direct call carrying a proven compatible readonly row selects the helper; a nominal caller retains the original name. The proposal deliberately does not add named-callback adaptation for the new forwarding shape. It does not address readonly `Record` value-domain widening; builder5 owns that separate boundary.

Scratch qualification on the active 119-compiler-patch/321-total floor:

- Production strict build passes. Twelve focused source tests pass; ten exclusion cases retain byte-identical output against the active compiler.
- The complete original automatic-parser module and prepared original public consumer emit without refused declarations from the saved normal source graph. Exactly two readonly helpers are added. Both original native signatures match the preceding compiler byte-for-byte, and both public readonly calls select their corresponding helper. The original module IR is unchanged.
- The synthetic emitted native fixture compiles, links and passes 27 checks. Static assertions preserve the unambiguous original function pointer and its nominal ABI. Schema-only readonly callers preserve the same owner, deferred accessor reads, missing/default behavior, live writes, exact getter/callback exceptions and retained lifetime. The original nominal pointer remains callable.
- Retained callable/JSON/readonly source controls and the integrated empty-fallback source/parity controls are rerun separately. This is preparation, not installed public-native qualification.

The script `scripts/particleReadonlyForwardingProposalOracle.mjs` requires explicit scratch candidate, active baseline, TypeScript, C++ compiler, saved graph and output paths. It asserts that the proposal is absent from the active manifest. See `artifacts/particle-readonly-forwarding-proposal.json` for hashes and results.

No SDK was generated. The eventual authorized single particles run, all eight original built-in native config/document callbacks, original JSON helper/detector, public consumer and exact four headers remain to be qualified on a coherent shared floor. Shared B commit `6e1aa571` has been received provisionally; builder2's scene2d-canvas coherence generation and accepted floor receipt are still pending.
