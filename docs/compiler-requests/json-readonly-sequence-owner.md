# Readonly sequences over retained JSON array owners

`Array.isArray(document.layers)` must inspect the original member value before requesting an array with a native element representation. A parsed JSON array has its original JSON owner; asking its member cell for `Array<Ref<LottieLayer>>` throws before the source predicate can run.

The compiler patch inspects the raw member in the existing JSON projection lane. A call whose parameter is a readonly array of readonly structural rows uses `json_readonly_sequence<Row>` to enter the existing `SequenceView<Row>` carrier. The bridge retains the original erased array, reads size and elements live, and projects only the element requested. It neither copies the array nor creates native layer objects. Mutable parameters and owning array storage retain their existing paths.

The runtime factory retains the array identity and shared lifetime through its accessor closures. Original element replacement and appends remain observable; an escaped element keeps its original referent after replacement. Null and primitive elements enter the existing assertion-row lane without field validation. Element reads and iteration retain deferred errors; an ignored readonly parameter does not inspect its elements or start iteration.

Foreman fe7cf6c6 approved this bounded extension of the existing sequence and JSON row family. Builder4 and Builder6 confirmed no overlapping unmerged implementation. No scalar ABI, source body, schema, or input domain changes are part of the patch.

The compiler patch is pinned to `806d257d2f4100de745cc97340e61c2f301b0f24` and expires when upstream emits equivalent raw predicates and original-owner readonly sequence arguments. Qualification covers native identity, live mutation, release, readonly and native escape controls, element guards, error timing, and the complete original public Lottie 32-case consumer. Installed SDK generation remains a separate coordinated step.
