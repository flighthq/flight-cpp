# Computed Record deletion must remove the property

At compiler `806d257` with the carried stack, `delete record[key]` for an actual
`Record<string, string>` emits a C++ delete of its lookup payload. The same failure
appears in the original `resolveSvgStyle` inheritance loop. Reading or destroying the
payload does not implement JavaScript property deletion.

The declared `compiler-record-computed-delete` patch emits one removal from the original
mutable Record and then `true`. Record entries and its shared symbol attachments are
configurable data properties. JavaScript returns true for a missing property as well as
one it removes; the runtime erase result instead reports whether it removed an entry.
The receiver and key are evaluated once, in source order, and the same shared owner is
mutated. No payload read, copied dictionary, manufactured readonly write or exception
translation is introduced. Aliases of actual Record storage participate; nominal index
signatures and named-property views remain outside this lane. Readonly Record deletion
has an explicit refusal.

Three focused groups pass, including emitted native compilation/link/execution of
present and missing string/numeric/UTF16/symbol keys, shared aliases, enumeration order,
last-owner release and receiver/key abrupt completion. The identical preceding stack
fails all three groups. The joint prototype also preserves the existing typed-Record
controls. Ordered declared-stack validation and original public cascade qualification
remain pending; this mechanism alone supplies no replacement style body.
