# JSON.stringify must enumerate a nominal interface reference

`JSON.stringify` receives a freshly constructed document with a closed exported interface type. The
emitter passes the nominal `Ref<Document>` directly to the runtime. That runtime route has no named-row
enumeration and silently serializes the document as `{}`, even though every document field and nested
value is present.

Measured at Flight `a627849` / flight-compiler `2b687ea` in the Aseprite and TexturePacker serializers
from `@flighthq/spritesheet-formats`. Both generated headers compile and the refusal ledger is empty for
the serializer modules.

## Observed

Source:

```ts
const doc = dataToArrayDocument(data, existing?.meta);
return JSON.stringify(doc, null, 2);
```

Emission:

```cpp
auto doc = data_to_array_document(data, existing_meta);
return flight::Json::stringify(doc, nullptr, 2.0);
```

A native call returns the exact string `{}` for array and hash documents from both formats. The runtime
can enumerate a top-level `StructuralRef<RowReadonly<RowOf<Ref<T>>>>`, but that alone is insufficient:
an experimental readonly helper produced `{"frames": {}}` for array documents, still omitted metadata,
and left hash documents as `{}` because nested typed arrays, records, and nominal references also lack
complete JSON conversion. The defect therefore spans the whole closed schema graph rather than only the
top-level call carrier.

## Required lowering

When a nominal interface reference enters `JSON.stringify`, expose its readonly structural row to the
runtime while retaining the same owner. Enumeration must include required and present optional string
properties in declared JavaScript order, recurse through arrays, records, and nested interface owners,
and preserve the requested two-space indentation. It must not clone the document or treat symbol-backed
runtime attachments as string keys.

## Carrying workaround

None. A readonly helper at only the document boundary was measured and rejected because it merely moved
the failure into nested arrays, records, and nominal frame/metadata owners. Hand-writing JSON text would
duplicate the library serializer and risk property-order, escaping, omission, and numeric-format drift.
The serializers remain emitted and independently compilable, but end-to-end native JSON evidence stays
blocked until the runtime recursively enumerates these named values.

## Further sites: particles-formats Spine and Unity serializers (measured)

`@flighthq/particles-formats` `serializeSpineParticle` and `serializeUnityParticle` end the same way, with
`JSON.stringify(doc, null, 2)` of a freshly built `SpineParticleDocument` or `UnityParticleDocument`. Both headers
compile, and at flight `a627849` both return exactly `{}` natively for a default config. JavaScript returns the
full document. For Spine, built from `configToDocument` in the source:

```json
{
  "name": "",
  "maxParticles": 1000,
  "continuous": true,
  "duration": -1,
  "emission": {
    "low": 8,
    "high": 12
  },
  "life": {
    "low": 500,
    "high": 1000
  },
  "lifeOffset": {
    "low": 0,
    "high": 0
  },
  "x": {
    "low": 0,
    "high": 0
  },
  "y": {
    "low": 0,
    "high": 0
  },
  "spawnShape": "point",
  "spawnWidth": {
    "low": 0,
    "high": 0
  },
  "spawnHeight": {
    "low": 0,
    "high": 0
  },
  "velocity": {
    "low": 50,
    "high": 100
  },
  "angle": {
    "low": -90,
    "high": 270
  },
  "rotation": {
    "low": 0,
    "high": 0
  },
  "wind": {
    "low": 0,
    "high": 0
  },
  "gravity": {
    "low": 0,
    "high": 0
  },
  "scale": {
    "low": 1,
    "high": 1
  },
  "scaleEnd": {
    "low": 1,
    "high": 1
  },
  "tint": [
    {
      "time": 0,
      "color": "ffffff"
    },
    {
      "time": 1,
      "color": "ffffff"
    }
  ],
  "alpha": [
    {
      "time": 0,
      "alpha": 1
    },
    {
      "time": 1,
      "alpha": 0
    }
  ],
  "blendMode": "normal",
  "premultiplied": false,
  "images": []
}
```

`tests/generated_particles_formats_test.cpp` does not bless the `{}` text. It pins only the serializers'
warnings, which are computed from the config alone.
