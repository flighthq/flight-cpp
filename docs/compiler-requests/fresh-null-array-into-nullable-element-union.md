# A fresh array of `null` assigned to a nullable element union

```ts
export interface Table<T> { entries: (T | null)[]; vocabulary: readonly string[] }
export function create<T>(vocabulary: readonly string[]): Table<T> {
  return { entries: vocabulary.map(() => null), vocabulary };
}
```

```text
an array of [null] cannot be passed where the element union is [T, null]: the target's container is
parameterized by its element type, so this would need a different container, and building one would
copy the array and change the identity the source passes.
```

TypeScript infers `null[]` for `vocabulary.map(() => null)`, and accepts it as `(T | null)[]` because
arrays are covariant. In C++ those are two container types, and the emitter correctly refuses to copy an
array whose identity the source might share.

Here the array is a fresh temporary: the `map` result has no other holder, so no array identity can be
observed. Building it at the target element type from the start, so that `map` produces
`flight::Array<std::optional<T>>`, changes nothing the program can see. The request is to elect the
contextual element type for an array created in the same expression it is assigned from. Arrays that
already have a holder stay refused.

Measured at flight-compiler `2b687ea`, with every carried patch applied, against flight `a627849`. The
repro above fails as shown. With the callback's return annotated, `(): T | null => null`, it lowers.

## Affected

`@flighthq/registry` `registryTable.ts` `createOrdinalTable` (line 11), the module's only refusal at
`a627849`; `contract` and `index` cascade from it.

## Carrying workaround

The source patch [`registry-ordinal-table-nullable-entries`](../../source-patches/registry-ordinal-table-nullable-entries.patch)
annotates the callback's return:

```ts
return { entries: vocabulary.map((): T | null => null), vocabulary };
```

The annotation is type-only: the callback, the fresh array (one `null` per vocabulary entry) and the returned
object are the same at runtime. It is carried with the registry renderer group's coordinated regeneration. On
the live base, `@flighthq/registry` is 3/3 modules emitted with no refusal records: `registryTable.ts`, `contract`
and `index` are all emitted, and all 3 headers compile.

## Scoped optional-map owner correction at compiler 806d257

The same allocation boundary appears in the original `parseSvgGradient` at Flight
`a62784923f3be814463286c9fb28edfe6ab15789`:

```ts
const resolvedStops = stops.length === 0
  ? (inherited?.stops.map((stop) => ({ color: { ...stop.color }, offset: stop.offset })) ?? [])
  : stops;
```

The compiler already gives a direct fresh concise object `map` its unique contextual element owner.
At compiler `806d257d2f4100de745cc97340e61c2f301b0f24`, the optional property-call emitter drops that
context, and the nullish wrapper names the inferred anonymous array while its empty fallback names
`SvgGradientStop[]`. This is an emitted native diagnostic at `svg_gradient.hpp:174`, independently of
the preceding optional Record diagnostic at line 106.

The declared compiler patch
[`compiler-contextual-optional-map-owner`](../../source-patches/compiler-contextual-optional-map-owner.patch)
threads the existing unique fresh-object allocation proof through those two wrappers. The original map
allocates its array, each stop and each authored color spread directly with the declared native owner.
The supplied nonempty `stops` array and callbacks returning existing objects keep their original owners.
This introduces no conversion or copied replacement for an existing array. A held array, a block callback
without the existing concise allocation proof, competing array owners or incompatible existing nested
owners retains its prior boundary.

The exact assigned subset is `svg_container_element.hpp`, `svg_document.hpp`,
`svg_element_handlers.hpp`, `svg_text_element.hpp` and `svg_use_element.hpp`. All five belong to
`@flighthq/scene2d-formats`; its pinned package declares no host environment, so all five apply to the
native profile. The direct gradient header is a supporting root, not a sixth assigned row. The separate
SVG bounds callers remain outside this subset.

Code-first checks: seven focused groups pass, including compilation, linking and execution of the
unannotated conditional/local shape above, fresh versus retained array/stop/color identity, live aliases,
missing and empty inheritance, receiver evaluation once, callback/property order and exact thrown owner.
On the identical preceding 72-patch stack, the two positive groups fail at the anonymous array boundary
and the five refusal controls remain unchanged. Production compiler build and touched-file lint pass.
The normal declared 73-patch stack applies, builds and reverses to a clean pinned checkout; all seven
focused groups and 17 selected existing map/optional-chain/nullish groups pass. The full compiler check
retains its four known failing gates (format, lint, import order and five foreign fixture type errors).
A joint 75-patch stack subsequently applies, builds, passes the seven focused groups and reverses cleanly.
The one normal `scene2d-formats` generation exits zero on that recorded stack: 48 of 80 target modules
emit completely and 32 retain refusals. It changes 27 headers and four metadata files. Every existing
function definition and refusal marker survives; two additional original Lottie property definitions
replace their old refusal markers. Those additional bodies have no new native qualification claim.

The final standalone survey attempts all six selected headers with Clang 22.1.8 at two jobs. The direct
`svg_gradient.hpp` support root passes. All five assigned callers fail at the later
`svg_document.hpp:70` nested default/nullable readonly Matrix payload; none is promoted. The registered
original public `resolve_svg_gradient` consumer builds, links and passes CTest (1/1), checking 14 URL
syntax cases, original cached owner identity, live color mutation, map replacement and deletion.
An independent execution of the original pinned TypeScript import graph (1,128 modules) also passes
those cases plus inherited stable sorting and fresh stop/color identities, live inherited mutations,
actual authored parent style/currentColor/opacity/offset, defaults and reference diagnostics.

The whole original `parse_svg_gradient` native consumer compiles but fails to link: both
`resolve_svg_style` and `parse_svg_color` remain declaration-only refused helpers. No replacement
helper, body stub or narrower parse domain is used. The approved style cascade follow-on continues;
this array unit claims the shared allocation correction and measured support-root/getter result,
with no five-caller, package or end-to-end parser completion. The original broader nullable-element
request and registry callback annotation remain open pending their own expiry audit.
