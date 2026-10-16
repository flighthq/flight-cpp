# 806d257 lowers twenty modules into headers that do not compile

## Reproduction

Regenerate any of the modules below with the pinned compiler and compile the header it writes, on its own,
against the installed runtime:

```sh
FLIGHT_CPP_COMPILE_JOBS=8 npm run sdk:compile:sdl
```

Every header named in [compiler-806d257.md](../compiler-806d257.md)'s carry table failed at `2b687ea`'s
lowering of the same source and fails at `806d257`'s. The failure is in what the emitter produces, not in
the source: the same Flight revision compiled from the previous pin.

## What is wrong, by family

| modules | diagnostic |
| --- | --- |
| `@flighthq/image-codec` decodeImage/detectImageMimeType, `@flighthq/scene2d-resources` loadScene2DImageResources | `no matching conversion for functional-style cast from 'std::variant<flight::ArrayBuffer, flight::Uint8Array>'` |
| `@flighthq/text` (contract, native_text, text_label_layout, the index and guard headers) | `no matching function for call to 'get_text_layout'`, and `use of undeclared identifier 'Traits'` in native_text.hpp |
| `@flighthq/intl` collator, plural | `use of undeclared identifier 'get_collator'` / `'get_plural_rules'` |
| `@flighthq/media` audio_mixer, enable_audio_mixer_guards | `use of undeclared identifier 'register_bus_in_reverse_map'` |
| `@flighthq/effects-wgpu` wgpu_effect_blit_shader, wgpu_effect_box_blur, wgpu_effect_tint_shader | `no viable conversion from 'optional<std::shared_ptr<flight::types::WgpuEffectPipeline>>'` |
| `@flighthq/shape` scale9_shape | `no matching function for call to 'structural_ref_cast'` |
| `@flighthq/tray` tray | `use of undeclared identifier 'destroy_tray_runtime'` |
| `@flighthq/tokens` flight_document_scene_tokens | `no viable conversion from 'std::optional<std::variant<flight::Array<flight::types::FlightDocumentValue>…` |

## What we carry meanwhile

Each of those headers ships its **prior emission** — the same source lowered by `2b687ea`, taken from this
repository's rename commit — as a `partial` override, so the header still compiles and the package stays out
of the shippable tier rather than reporting itself as done. The overrides are listed under
`<package>-prior-emission-<module>` in `overrides/manifest.json`, each with `remaining` naming the module the
pinned compiler still owes.

The carry is deliberate debt, not a fork: it is the emission this repository already measured, and each entry
expires when the pinned compiler emits the module so that its header compiles on its own.
