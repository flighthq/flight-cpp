# An object spread of a call result must copy the returned fields

An object literal adds one field and spreads a freshly returned, closed interface object. The emitter
constructs the destination with only the spelled field and silently omits every field from the spread.
The module otherwise emits, so neither the refusal ledger nor the header warns that serialized array
frames have lost their geometry and flags.

Measured at Flight `a627849` / flight-compiler `2b687ea` in the Aseprite and TexturePacker serializers
from `@flighthq/spritesheet-formats`:

```ts
const frames: ArrayFrame[] = data.frames.map((frame) => ({
  filename: frame.name,
  ...frameToEntry(frame),
}));
```

`frameToEntry` returns a fresh, nominal hash-frame object with `frame`, `rotated`, `sourceSize`,
`spriteSourceSize`, and `trimmed`; Aseprite also has `duration`, and TexturePacker may have `pivot`.

## Observed C++

Both callbacks allocate only the destination's explicit filename:

```cpp
return flight::make_ref<flight::types::AsepriteArrayFrame>(
    flight::types::AsepriteArrayFrame{.filename = frame->name});
```

The call to `frameToEntry` is gone as well as its returned properties. This changes output and also
removes all reads performed by that helper. It is distinct from
`object-spread-of-a-parameter-dropped.md`: that request loses a structural parameter; these sites lose
a nominal object returned by a call inside a contextually typed object literal.

## Required lowering

Evaluate the spelled `filename` initializer first, evaluate `frameToEntry` once, then copy the returned
object's enumerable own fields in their insertion order. The resulting owner is the contextual
`ArrayFrame`, and the source object remains a separate fresh object exactly as in JavaScript.

## Carrying workaround

`source-patches/spritesheet-formats-serializer-existing-shapes.patch` names the filename before the
call, binds the returned entry once, and lists its closed fields in the same property order. The
TexturePacker branch lists `pivot` in its original position only when present. The patch expires when a
call-result spread contributes every returned field and preserves its evaluation order.
