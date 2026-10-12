# Reading a member that each union arm declares required or optional needs per-arm narrowing

`hasShareContentFields` reads `title`, `text` and `url` off a `ShareContent` union whose three arms each
declare the three members, one of them required and the other two optional. The emitter refuses because it
reads the member across the union instead of per arm: there is no single storage shape to test, since the
member is a required `String` in one arm and an `std::optional<String>` in another.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/share`.

## Minimal TypeScript

`packages/share/src/share.ts:51`, against `packages/types/src/Share.ts:6`:

```ts
export type ShareContent =
  | { readonly text?: string; readonly title: string; readonly url?: string }
  | { readonly text: string; readonly title?: string; readonly url?: string }
  | { readonly text?: string; readonly title?: string; readonly url: string };

export function hasShareContentFields(content: Readonly<ShareContent>): boolean {
  if (content.title !== undefined && content.title !== '') return true;
  if (content.text !== undefined && content.text !== '') return true;
  if (content.url !== undefined && content.url !== '') return true;
  return false;
}
```

```
cpp emission failed for @flighthq/share/packages/share/src/share.ts: nullable union member access requires
presence-aware C++ narrowing
```

## Required C++ shape

The union is already emitted as a three-arm variant whose arms carry exactly the per-arm shapes:

```cpp
struct text_title_url_66ef499c1c8b6060 { flight::String text; std::optional<flight::String> title; std::optional<flight::String> url; };
struct text_title_url_0920bc2a3ce65b06 { std::optional<flight::String> text; flight::String title; std::optional<flight::String> url; };
struct text_title_url_178ed2aab7bfa756 { std::optional<flight::String> text; std::optional<flight::String> title; flight::String url; };
using ShareContent = std::variant<…>;
```

so visiting the elected arm is the narrowing, and the source's three tests are an OR over the arm's own
carriers — which is what the package's override supplies in place of the refused declaration. A required
member is present by construction, so only its emptiness can fail the test; an optional one fails when it
is absent or empty.

## Where it fires

| site | source line | what is lost |
| --- | --- | --- |
| `hasShareContentFields` | `share.ts:51` | the presence test two retained functions call (`canShareContent`, `shareContent`) |

Both retained callers are emitted and call the refused declaration, so the module does not compile until a
body exists — the shape filed as
[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md).

## Distinguish from the requests already filed

[`nullable-optional-presence.md`](nullable-optional-presence.md) is about a *partial row* needing two
absence channels for one member. Here the arms do keep their two channels; what is missing is narrowing
across them. [`reference-backed-outcome-variant-narrowing.md`](reference-backed-outcome-variant-narrowing.md)
is about reading a member on a variant the emitter treats as a record — the opposite failure.

## Carrying workaround

`overrides/include/flight/share/share.hpp` supplies this one declaration by visiting the elected arm;
`@flighthq/share` is `assisted` on that basis, and the supplied behavior is exercised natively by
`tests/generated_share_test.cpp`.
