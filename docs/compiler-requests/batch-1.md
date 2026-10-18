# Compiler request batch 1 (2026-10-17)

For flight-compiler, against compiler `806d257` / flight `a627849`. Ranked by public headers each
fix unblocks in flight-cpp (`generated/header-status.json`, G1 sweep: 2,339 of 3,152 headers compile,
813 fail; 350 of the failures are routed to the compiler). Each file has the minimal TypeScript, the
emitted C++, the expected C++ and the affected modules.

| Headers | Request | Defect |
| ---: | --- | --- |
| 116 | [external-descriptor-literal-construction](external-descriptor-literal-construction.md) | object literal passed to a bound WebGPU method is emitted as a bare braced list; it should construct the bound descriptor type |
| 34 | [local-reexport-using-before-declaration](local-reexport-using-before-declaration.md) | `using` of a local re-export is emitted before the declaration it names (scene2d-canvas) |
| 31 | [membership-test-lowered-literally](membership-test-lowered-literally.md), [record-in-operator-lowered-verbatim](record-in-operator-lowered-verbatim.md) | TypeScript `in` is emitted verbatim as C++ `in` |
| 16 | [optional-record-literal-not-materialized](optional-record-literal-not-materialized.md) | shared compiler correction and original 3DS consumer qualified; 9 of 17 historical headers pass, eight have later failures; SVG results counted separately |
| 15 | [json-parse-assertion-static-cast](json-parse-assertion-static-cast.md) | `JSON.parse(...) as T` is emitted as `static_cast` from `JsonValue` to an unrelated `Ref<T>` |
| 13 | [array-pair-literal-deduction](array-pair-literal-deduction.md) | a tuple-typed pair literal is emitted as `flight::Array{...}` |
| 9 | [builtin-numeric-predicate-callback](builtin-numeric-predicate-callback.md) | `isFinite` used as a callback is emitted as the overloaded `std::isfinite` name |

About 234 headers in total. The 116-header WGPU item is judged half binding, half compiler: the
runtime side can add typed descriptor overloads, but the emitter should still construct the bound
type when the binding declares it.

Still to come from G2: the `Traits` leak (29 headers, being handled inside E0a), and the
remaining compiler-routed clusters (~115 headers, each 14 or fewer).
