# A joint initializer is called with the construction view instead of the owner it wraps

**Family.** Entity-construction view at a concrete (non-generic) call boundary. **Sites at this pin:** 8 headers
across physics2d, physics2d-abi and physics3d. **Route:** compiler.

This is NOT the generic case already filed in
[entity-construction-generic-call-inference.md](entity-construction-generic-call-inference.md). That one is a
generic helper's type parameter being specialized as the construction wrapper. This is a concrete call to a
non-generic function.

## Reproduction, from the emitted text

`flight/physics2d/joint_factories.hpp`:

```cpp
// line 44 -- the initializer wants the owner
inline void init_joint_base(flight::Ref<flight::types::Physics2DJoint> out,
                            flight::StructuralRef<flight::RowReadonly<flight::RowOf<
                                flight::Ref<flight::types::Physics2DJointOptions>>>> options) { ... }

// line 62 -- the caller holds the CONSTRUCTION VIEW
inline void initialize_physics_2d_distance_joint(
    flight::types::EntityConstruction<flight::Ref<flight::types::Physics2DDistanceJoint>> out, ...) {
  // line 65 -- and passes the view straight through
  init_joint_base(out, flight::structural_ref_cast<...>(options));
```

```
error: no matching function for call to 'init_joint_base'
```

The same shape repeats at lines 79, 129, 153 and 170 of that header, and in the physics3d joint factories; the
eight affected headers are those of `@flighthq/physics2d`, `@flighthq/physics2d-abi` and `@flighthq/physics3d`
that include them.

## Cause

`EntityConstruction<T>` is the source's assertion view of the object being constructed; it is not `T` and not
`Ref<T>`, and the emitter never materializes the owner from it at a call that wants the owner. The specialized
initializer holds the view, the base initializer's parameter is a plain `Ref<Physics2DJoint>`, and no conversion
exists between them, so overload resolution has no candidate. `Physics2DDistanceJoint` being a `Physics2DJoint`
structurally does not help: the mismatch is the wrapper, not the owner.

The faithful fix is the emitter materializing the construction view into the owner at that call boundary, since
that is the same object the view asserts. Nothing about absence, identity, equality or ordering changes: the
Ref is the object the caller already holds.

## What we do meanwhile

Nothing, and deliberately: no hand-written cast and no source patch. A cast at eight sites would be the
repository silently reinterpreting a construction view as an owner, which is exactly what the wrapper exists to
make explicit; a source patch would relocate the defect into flight. The sites stay failing and counted.
