# Activate the reviewed readonly nested object snapshot

Foreman `ffccf8e4` authorizes this paired runtime and managed compiler change after
Builder6 final actual-byte gate `30b10878` passed. The immutable corrected proof archive
`9489b25917bc77cac66f083a890ffdaae087458075e16f47507dc48d987330b8`
remains the historical implementation evidence; its inactive state describes that proof,
not this later activation. The old JSON regression and old expectations remain retained.

The product `include/flight/structural_ref.hpp` is byte-identical to reviewed v3 SHA256
`152217d57e6555aa91dd09c504c71074e09d97e0df078231776b39a0548923d5`.
The new managed patch `compiler-readonly-object-value-snapshot` carries exactly the reviewed
accessor block, with context for the current composition of 143 active compiler patches.
Compiler baseCommit is `806d257d2f4100de745cc97340e61c2f301b0f24`;
the manifest records equivalence, the upstream request and expiry.

Only a readonly outer row's optional readonly Partial<object> read uses the owning snapshot.
The original getter runs once, and its actual owner/control block, source sentinel and
exception prefix survive without filling the generated reference-returning get cache.
Typed JSON-backed rows keep their exact owner. Raw erased JSON and unsupported values take
the existing fallback; exact empty rows stay present without a second getter. Dynamic
NamedProperties and the existing get/as_any/set/cache implementation remain unchanged.

One RowCell virtual changes the vtable under Manager3e1de4a2 and Foreman authorization.
The advertised ABI version is unchanged. General typed and external subclass cache retention
is still outside this bounded read path. Full original Sprite construction, source220's two
owned contract errors and native image39 remain unqualified. No SDK was regenerated.

The [snapshot request](readonly-nested-object-getter-snapshot.md) and
[fallback correction](readonly-nested-object-getter-fallback.md) retain the original evidence.
Retire this managed patch when upstream supplies matching snapshot and fallback semantics;
requalify when either pin, the composed accessor block or exact runtime base changes.

Current-composition baseline backend SHA256: `17f5daacd5f9244f3ef8dec021a99e8b0d65ac2311c222c9447850a191b770d5`.
Candidate backend SHA256: `f1a884280267a8a68a413677e62e5a9e1b874bb39dadf833f1f82fcb1b855f38`.
Managed patch SHA256: `b6e8fbf2b6f6ce094c09f75a788ec2e32c7041541833f59e472e8a247f1d6e2e`.
The current-composition build, focused tests and four runtime project targets are checked
after the product commit; their results are recorded in the activation receipt.
