// The generated @flighthq/skeleton2d package at a627849, exercised natively. Expectations are ported from
// packages/skeleton2d/src/skeleton2d.test.ts and slotDeform2D.test.ts: construction and buffer sizing, the
// world-transform walk and its inherit modes, bone lookup, cloning (bones deep-copied, slots copied, skins
// shared), equality, setup reset and its aliasing guard, bind pose and the skin palette, skins, validation,
// disposal, and the slot deform record.
//
// Three of those paths run through this repository's source patches and are checked here directly:
// createSkeleton2D's default slots (skeleton2d-create-two-state-slots-default) must store null, not
// undefined; cloneSkeleton2D's slot copy (skeleton2d-clone-slot-copy-declared-type) must copy each slot and
// keep its attachment; and the slot deform copy (skeleton2d-slot-deform-offsets-slice) must own its offsets.
//
// TransformMode2D's presets are emitted as an anonymous struct rather than TransformInherit2D, so the
// inherit modes are built here from the same four flags the presets declare.
#include <flight/geometry/matrix.hpp>
#include <flight/skeleton2d/skeleton2d.hpp>
#include <flight/skeleton2d/slot_deform2_d.hpp>

#include <cmath>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

// Vitest's toBeCloseTo(expected, 5).
bool close(double actual, double expected) { return std::abs(actual - expected) < 0.5e-5; }

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace sk = flight::skeleton2d;
using flight::types::Bone2D;
using flight::types::Skeleton2D;
using flight::types::Slot2D;
using flight::types::TransformInherit2D;
using Mode = flight::Ref<TransformInherit2D>;

Mode mode(bool reflection, bool rotation, bool scale, bool translation) {
  return flight::make_ref<TransformInherit2D>(
      TransformInherit2D{.reflection = reflection, .rotation = rotation, .scale = scale, .translation = translation});
}
Mode normal() { return mode(true, true, true, true); }
Mode onlyTranslation() { return mode(false, false, false, true); }
Mode noScale() { return mode(true, true, false, true); }

struct BoneSpec {
  double x = 0, y = 0, rotation = 0;
  double parent = -1;
  double scaleX = 1, scaleY = 1;
  const char* name = nullptr;
  Mode transform = nullptr;
};

flight::Ref<Bone2D> bone(BoneSpec spec) {
  auto b = flight::make_ref<Bone2D>();
  b->length = 0;
  if (spec.name != nullptr) b->name = flight::String(spec.name);
  else b->name = flight::null;
  b->parent_index = spec.parent;
  b->rotation = spec.rotation;
  b->scale_x = spec.scaleX;
  b->scale_y = spec.scaleY;
  b->shear_x = 0;
  b->shear_y = 0;
  b->transform_mode = spec.transform ? spec.transform : normal();
  b->x = spec.x;
  b->y = spec.y;
  return b;
}

flight::Array<flight::Ref<Bone2D>> bones(std::vector<BoneSpec> specs) {
  flight::Array<flight::Ref<Bone2D>> out;
  for (const auto& spec : specs) out.push(bone(spec));
  return out;
}

flight::Ref<flight::types::Matrix> world(const flight::Ref<Skeleton2D>& s, double index) {
  auto out = flight::geometry::create_matrix();
  sk::get_skeleton2_dbone_world_matrix(out, view(s), index);
  return out;
}

bool slotsAreNull(const flight::Ref<Skeleton2D>& s) { return std::holds_alternative<flight::Null>(s->slots); }

void construction() {
  auto s = sk::create_skeleton2_d(bones({{}, {.parent = 0}}));
  check(s->world_matrices.size() == 12 && s->inverse_bind_matrices.size() == 12 && s->bone_matrices.size() == 12,
        "buffers are sized 6 floats per bone");
  check(slotsAreNull(s), "a pure bone rig stores null slots, not undefined (defaulted slots)");
  check(std::holds_alternative<flight::Null>(s->skins), "skins start null");

  flight::Array<flight::Ref<Slot2D>> slots;
  auto slot = flight::make_ref<Slot2D>();
  slot->bone_index = 0;
  slots.push(slot);
  auto drawable = sk::create_skeleton2_d(bones({{}}), slots);
  check(std::holds_alternative<flight::Array<flight::Ref<Slot2D>>>(drawable->slots) &&
            std::get<flight::Array<flight::Ref<Slot2D>>>(drawable->slots).element(0) == slot,
        "passed slots are stored with their identity");
  auto explicitNull = sk::create_skeleton2_d(bones({{}}), std::optional<flight::Array<flight::Ref<Slot2D>>>{});
  check(slotsAreNull(explicitNull), "explicit null slots store null");
}

void transforms() {
  {
    auto s = sk::create_skeleton2_d(bones({{.x = 10, .rotation = 90}}));
    sk::compute_skeleton2_dworld_transforms(view(s));
    const auto& w = s->world_matrices;
    check(close(w[0], 0) && close(w[1], 1) && close(w[2], -1) && close(w[3], 0) && close(w[4], 10) && close(w[5], 0),
          "a root rotated 90 degrees at (10, 0)");
  }
  {
    auto s = sk::create_skeleton2_d(bones({{.x = 10, .rotation = 90}, {.x = 2, .parent = 0}}));
    sk::compute_skeleton2_dworld_transforms(view(s));
    auto m = world(s, 1);
    check(close(m->tx, 10) && close(m->ty, 2), "a child at parent x local");
  }
  {
    auto s = sk::create_skeleton2_d(bones({{.rotation = 90}, {.x = 3, .parent = 0, .transform = onlyTranslation()}}));
    sk::compute_skeleton2_dworld_transforms(view(s));
    auto m = world(s, 1);
    check(close(m->a, 1) && close(m->b, 0) && close(m->c, 0) && close(m->d, 1), "OnlyTranslation keeps its own basis");
    check(close(m->tx, 0) && close(m->ty, 3), "OnlyTranslation follows the parent position");
  }
  {
    auto s = sk::create_skeleton2_d(bones({{.rotation = 90, .scaleX = 2, .scaleY = 2}, {.x = 1, .parent = 0, .transform = noScale()}}));
    sk::compute_skeleton2_dworld_transforms(view(s));
    auto m = world(s, 1);
    check(close(std::hypot(m->a, m->b), 1) && close(m->a, 0) && close(m->b, 1), "NoScale keeps the parent rotation, strips its scale");
  }
  {
    auto s = sk::create_skeleton2_d(bones({{}}));
    auto out = flight::geometry::create_matrix(7, 7, 7, 7, 7, 7);
    check(!sk::get_skeleton2_dbone_world_matrix(out, view(s), 3) && out->a == 7, "an out-of-range bone returns false and leaves out alone");
    sk::compute_skeleton2_dbone_world_transform(view(s), 5);
    check(s->world_matrices.size() == 6, "an out-of-range index writes nothing past the buffer");
  }
}

void lookup_clone_equality() {
  auto s = sk::create_skeleton2_d(bones({{.name = "root"}, {.parent = 0, .name = "arm"}}));
  check(sk::get_skeleton2_dbone_index_by_name(view(s), flight::String("arm")) == 1, "finds a named bone");
  check(sk::get_skeleton2_dbone_index_by_name(view(s), flight::String("leg")) == -1, "-1 for a miss");

  flight::Array<flight::Ref<Slot2D>> slots;
  auto slot = flight::make_ref<Slot2D>();
  slot->bone_index = 1;
  slot->color = 0x11223344;
  slots.push(slot);
  auto rig = sk::create_skeleton2_d(bones({{.x = 1}, {.x = 2, .parent = 0}}), slots);
  flight::Array<flight::Ref<flight::types::AttachmentSkin2D>> skins;
  auto skin = flight::make_ref<flight::types::AttachmentSkin2D>();
  skin->name = flight::String("goblin");
  skins.push(skin);
  rig->skins = skins;

  auto copy = sk::clone_skeleton2_d(view(rig));
  check(copy != rig && copy->bones.element(0) != rig->bones.element(0), "clone deep-copies bones");
  copy->bones.element(0)->x = 99;
  check(rig->bones.element(0)->x == 1, "the clone poses independently");
  const auto& copiedSlots = std::get<flight::Array<flight::Ref<Slot2D>>>(copy->slots);
  check(copiedSlots.size() == 1 && copiedSlots.element(0) != slot && copiedSlots.element(0)->bone_index == 1 &&
            copiedSlots.element(0)->color == slot->color,
        "clone copies each slot with its members (declared slot copy)");
  auto found = sk::get_skeleton2_dskin(view(copy), flight::String("goblin"));
  check(found.has_value() && found.value() == skin, "clone shares skins, so getSkin works on it");
  check(!sk::get_skeleton2_dskin(view(copy), flight::String("missing")).has_value(), "a missing skin is null");
  check(!sk::get_skeleton2_dskin(view(sk::create_skeleton2_d(bones({{}}))), flight::String("goblin")).has_value(),
        "a rig with no wardrobe has no skin");

  auto twin = sk::clone_skeleton2_d(view(s));
  check(sk::equals_skeleton2_d(view(s), view(twin)), "a fresh clone is equal");
  twin->bones.element(1)->rotation = 5;
  check(!sk::equals_skeleton2_d(view(s), view(twin)), "an edited bone is not equal");
}

void setup_and_bind() {
  auto setup = sk::create_skeleton2_d(bones({{.x = 1, .rotation = 10}, {.x = 2, .parent = 0}}));
  auto pose = sk::clone_skeleton2_d(view(setup));
  pose->bones.element(0)->x = 50;
  pose->bones.element(1)->rotation = 45;
  sk::reset_skeleton2_dto_setup(view(setup), pose);
  check(pose->bones.element(0)->x == 1 && pose->bones.element(1)->rotation == 0, "reset copies the setup locals");

  bool threwAliased = false;
  try {
    sk::reset_skeleton2_dto_setup(view(setup), setup);
  } catch (...) {
    threwAliased = true;
  }
  check(threwAliased, "reset rejects an aliased setup and pose");
  bool threwShape = false;
  try {
    sk::reset_skeleton2_dto_setup(view(setup), sk::create_skeleton2_d(bones({{}})));
  } catch (...) {
    threwShape = true;
  }
  check(threwShape, "reset rejects a different bone count");

  auto s = sk::create_skeleton2_d(bones({{.x = 3, .rotation = 30}, {.x = 2, .parent = 0}}));
  sk::compute_skeleton2_dworld_transforms(view(s));
  sk::set_skeleton2_dbind_pose(view(s));
  sk::compute_skeleton2_dbone_matrices(view(s));
  bool identity = true;
  for (int b = 0; b < 2; ++b) {
    const auto& p = s->bone_matrices;
    identity = identity && close(p[b * 6 + 0], 1) && close(p[b * 6 + 1], 0) && close(p[b * 6 + 2], 0) &&
               close(p[b * 6 + 3], 1) && close(p[b * 6 + 4], 0) && close(p[b * 6 + 5], 0);
  }
  check(identity, "the palette is identity at the captured bind pose");
  s->bones.element(1)->rotation = 90;
  sk::compute_skeleton2_dworld_transforms(view(s));
  sk::compute_skeleton2_dbone_matrices(view(s));
  check(!close(s->bone_matrices[6], 1), "the palette moves off identity once posed");
}

void validation_and_disposal() {
  check(!sk::validate_skeleton2_d(view(sk::create_skeleton2_d(bones({{}, {.parent = 0}})))).has_value(),
        "a parent-before-child skeleton is valid");
  auto bad = sk::create_skeleton2_d(bones({{.parent = 1}, {}}));
  auto message = sk::validate_skeleton2_d(view(bad));
  check(message.has_value() && message.value().includes(flight::String("is not < its own index")), "reports a forward parent");

  flight::Array<flight::Ref<Slot2D>> slots;
  slots.push(flight::make_ref<Slot2D>());
  auto s = sk::create_skeleton2_d(bones({{}, {.parent = 0}}), slots);
  sk::dispose_skeleton2_d(s);
  check(s->bones.size() == 0 && slotsAreNull(s), "dispose clears bones and slots");
}

void slot_deform() {
  auto slot = flight::make_ref<Slot2D>();
  slot->bone_index = 0;
  auto attachment = flight::make_ref<flight::types::Attachment2D>();
  slot->attachment = attachment;

  flight::Float32Array source{1.5f, -2.0f, 3.25f};
  auto record = sk::set_skeleton2_dslot_deform(slot, attachment, source);
  check(record.has_value() && record.value()->offsets.size() == 3 && record.value()->offsets[1] == -2.0f, "stores the offsets");
  check(!(record.value()->offsets == source), "the stored offsets are a copy, not the caller's array");
  source[0] = 100.0f;
  check(record.value()->offsets[0] == 1.5f, "a later write to the source does not reach the record");

  auto offsets = sk::get_skeleton2_dslot_deform_offsets(view(slot));
  check(offsets.has_value() && offsets.value()[2] == 3.25f, "offsets read back for the attachment shown");

  flight::Float32Array next{4.0f, 5.0f, 6.0f};
  auto reused = sk::set_skeleton2_dslot_deform(slot, attachment, next);
  check(reused.has_value() && reused.value() == record.value() && reused.value()->offsets[0] == 4.0f,
        "an equal-length write reuses the record and copies in");

  slot->attachment = flight::make_ref<flight::types::Attachment2D>();
  check(!sk::get_skeleton2_dslot_deform_offsets(view(slot)).has_value(), "offsets authored for other art read as null");

  check(!sk::set_skeleton2_dslot_deform(slot, attachment, std::nullopt).has_value() &&
            std::holds_alternative<flight::Null>(slot->deform),
        "null offsets clear the deform");
}

}  // namespace

int main() {
  construction();
  transforms();
  lookup_clone_equality();
  setup_and_bind();
  validation_and_disposal();
  slot_deform();
  return failures == 0 ? 0 : 1;
}
