# A non-empty tuple spread is emitted with the Array carrier

`[first, ...files.slice(1)]` for a member declared `readonly [ShareFile, ...ShareFile[]]` is emitted as a
`flight::Array` — the right elements in the right order, in the wrong carrier — so the aggregate
initializer cannot convert to the member's declared `std::tuple<ShareFile, Array<ShareFile>>`. The
declaration is emitted, not refused; the module simply does not compile.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/share`.

## Observed

Source, `packages/share/src/share.ts:110`:

```ts
function filesContent(files: readonly ShareFile[]): ShareFilesContent | null {
  const first = files[0];
  return first === undefined || !files.every(isShareFileValid) ? null : { files: [first, ...files.slice(1)] };
}
```

Emission, `generated/include/flight/share/share.hpp:137` (the single line, wrapped):

```cpp
  return ((!first.has_value() || !files.every(is_share_file_valid)) ? std::nullopt
    : std::optional<flight::Ref<flight::types::ShareFilesContent>>{flight::make_ref<flight::types::ShareFilesContent>(
        flight::types::ShareFilesContent{.files = ([&]() {
          flight::Array<flight::Ref<flight::types::ShareFile>> array_spread_result;
          array_spread_result.push(first.value());
          for (const auto& array_spread_item : files.slice(1.0)) { array_spread_result.push(array_spread_item); }
          return array_spread_result;
        }())})});
```

with the member declared as the non-empty tuple:

```cpp
struct ShareFilesContent : public flight::ReferenceEnabled {
  std::tuple<flight::Ref<flight::types::ShareFile>, flight::Array<flight::Ref<flight::types::ShareFile>>> files;
```

```
no viable conversion from 'flight::Array<flight::Ref<flight::types::ShareFile>>' to
'std::tuple<flight::Ref<flight::types::ShareFile>, flight::Array<flight::Ref<flight::types::ShareFile>>>'
```

## Required C++ shape

The elements the emitter computed are already correct — the head, then the tail — so the carrier is the
only thing to change:

```cpp
  .files = [&]() {
    flight::Array<flight::Ref<flight::types::ShareFile>> array_spread_rest;
    for (const auto& array_spread_item : files.slice(1.0)) { array_spread_rest.push(array_spread_item); }
    return std::tuple<flight::Ref<flight::types::ShareFile>,
                      flight::Array<flight::Ref<flight::types::ShareFile>>>{first.value(), array_spread_rest};
  }()
```

The type is already generated — this is a spread that knows its tuple's tuple-ness and emits the Array
anyway.

## Where it fires

| site | source line | what is lost |
| --- | --- | --- |
| `filesContent` | `share.ts:110` | the one place a `ShareFilesContent` is built, so all three file-sharing exports go with it |

## Distinguish from the requests already filed

[`unbounded-spread-into-rest-call.md`](unbounded-spread-into-rest-call.md) is about spreading into a
*call*'s rest parameter. [`corpus-context-structural-row-record-spread.md`](corpus-context-structural-row-record-spread.md)
is about spreading a structural row into a record. This one is an array literal spread into a declared
non-empty tuple member, where the element computation is right and only the carrier is wrong.

## Carrying workaround

`overrides/include/flight/share/share.hpp` carries the corrected carrier, marked in the file; the package
is `assisted` and its behavior is exercised natively by `tests/generated_share_test.cpp`.
