# Video constructor readonly host and partial option rows

At Flight `a62784923f3be814463286c9fb28edfe6ab15789`, the public constructor accepts:

```ts
createVideoTexture(
  hostVideo: Readonly<HostVideoCapability>,
  source: VideoResource,
  opts?: Readonly<Partial<TextureLike>>,
): Texture2D
```

`TextureLike` has four concrete arms: 2D, cube, 2D array and 3D. The current emitter
refuses the factory as `cpp-partial-shape-unresolvable`; the existing native
`CreateTexture2DOptions` convenience overload does not represent that source signature.
The named mapped-union request remains [open](named-union-partial-shape.md).

The bounded constructor supply names each arm as a readonly partial row and retains
their union in `VideoTextureOptions`. `ReadonlyVideoHost` reads the existing callback
cells. Both the canonical optional-union function signature and direct row convenience
calls are available; there is no row-to-native owner recovery or cast.

The source creates an image before spreading options. Therefore its width callback
may change the height callback and option values before either is subsequently read.
The constructor reads width, writes it, then reads height. Only afterward does it copy
the declared option fields in the represented owner's string-key order, followed by
`EntityRuntimeKey`. The local typed snapshot retains source/source-list/runtime
references even though the forced 2D constructor does not consume them. It then writes
`dimension`, `source` and `version` in the source's overriding order. This is a fresh
option object, rather than a retyped input texture.

The 2D constructor applies the existing defaults and independently clones sampler and
UV owners. A supplied empty string, false or zero remains supplied. Array/cube source
lists cannot replace the new video image, input dimensions and versions cannot change
the forced result, and none of those forced writes mutates the input option owner.
The image retains the exact borrowed host element, straight alpha, sRGB gamut and
initial `0xffffffff` revision. Missing host callbacks give zero dimensions; a missing
borrowed element suppresses both callbacks while still applying options. Exceptions
from a host callback or option read propagate before later operations.

`videoTextureOptionsOracle.mjs` obtains the exact pinned Git declarations for the
constructor and all exercised entity/texture/sampler/vector helpers, plus their source
constants. It selects the actual overloaded function implementation through the
TypeScript parser already installed for the pinned compiler input, then erases only
types. It adds no runtime dependency. The native regression checks the same ownership,
default/override, callback/property/symbol order and exception cases, as well as the
canonical function-value surface and all four represented option arms.

The video override remains **partial**. Other video helpers still lack their emitted
readonly host/texture-row callable carriers even though their native convenience
operations have executable behavior. This unit supplies the four declared closed
option shapes; generic mapped-union planning and open dynamic own-property spreads
outside those declared shapes are not supplied. See `remaining` in the override
manifest for exact API names. The original native Texture target and syntax gate do
not certify those missing surfaces. The separate cube setter and render factory gaps
are unchanged.
