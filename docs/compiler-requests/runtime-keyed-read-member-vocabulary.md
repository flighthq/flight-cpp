# A runtime-keyed read needs its keys in the structural member vocabulary

`for (const flag of FLAGS) if (!explanation[flag]) …`, where `FLAGS` is an `as const` list of the row's own member
names, emits `flight::row_get<bool>(explanation, flag)`. The runtime answers that string-keyed read through the
row's named view, and the named view knows a member only when its name is in
`flight/sdk/structural_members.hpp`. `scripts/structuralMembers.mjs` builds that vocabulary from the
`flight::RowKey<"…">` spellings in emitted and override headers. A member that is only ever read by a runtime key
appears there as a string literal, not as a `RowKey`, so it is not in the vocabulary. The read then sees
`undefined` and the checked `row_get` throws `BadAnyAccess`.

## Measured site

`@flighthq/physics2d` `enablePhysics2DGuards.ts` `getFailingPhysics2DPreconditions` reads nine boolean flags of
`Physics2DStepExplanation` (`bodyStateValid`, `contactStateValid`, `gravityValid`, …) from
`PHYSICS2D_PRECONDITION_FLAGS`. At a627849 none of the nine is in the vocabulary, so with the guards enabled, a
step that the explanation declines throws `BadAnyAccess` out of `stepPhysics2D` instead of the guard's own error.
The header compiles. The two upstream cases that reach this are written in `tests/generated_physics2d_world_test.cpp`
and not run.

`stepValidation.ts`'s `rigidBodyFiniteKeys` read works only because every one of those names is also read by a
`RowKey` somewhere else in the SDK.

## Expected behavior

The names a runtime-keyed read can take are known statically when the key ranges over a closed `as const` list.
The generator should add them to the vocabulary, as it does for `RowKey` spellings, or emit the read as a
switch over that list's members.

## Not carried

Adding the nine names would broaden the shared structural member table, whether by a generator change or by an
override that mentions them, and this arc was asked not to do that. The guards module is reported as compiling
but behaviour-blocked.
