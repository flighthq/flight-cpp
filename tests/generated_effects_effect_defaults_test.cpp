// Focused native behavior for @flighthq/effects normalizeEffect at Flight a627849, emitted through the
// compiler-membership-test-presence patch. Its carry-over loop is `if (!(key in entry))` over the per-kind
// defaults Record, which now lowers to the Record's own-entry presence plus Object.prototype's names.
// These fixtures pin the source contract: defaults fill unset fields, explicit values (including zero) win,
// a key the defaults table has is never re-copied from the effect, and an unknown kind leaves `out` unchanged.
#include <flight/effects/effect_defaults.hpp>
#include <flight/effects/vignette_effect.hpp>
#include <flight/types/vignette_effect.hpp>

#include <iostream>
#include <string>

namespace {

namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

using EffectRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::Effect>>>>;

EffectRow row_of(const flight::Ref<types::VignetteEffect>& effect) {
  return flight::structural_ref_cast<EffectRow>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<types::VignetteEffect>>>>(effect));
}

// Built through the package's own constructor, so the object's row owner is the one the SDK gives it.
flight::Ref<types::VignetteEffect> vignette() { return flight::effects::create_vignette_effect(); }

}  // namespace

int main() {
  // Defaults fill every unset field; the membership test keeps the second loop from overwriting a
  // defaulted key with the effect's unset value.
  {
    auto effect = vignette();
    effect->radius = 0.0;  // explicit zero is preserved, not replaced by the default 1.0
    auto out = vignette();
    const bool ok = flight::effects::normalize_effect(row_of(effect), out);
    check(ok, "a kind with a defaults entry normalizes");
    check(out->intensity == 0.5, "an unset field takes its default and is not re-copied as unset");
    check(out->softness == 0.5 && out->color == 255.0, "every defaulted field is filled");
    check(out->radius == 0.0, "an explicit zero survives normalization");
    check(out->kind == flight::String("VignetteEffect"), "a key outside the defaults table is carried over");
  }

  // Explicit values win over defaults.
  {
    auto effect = vignette();
    effect->intensity = 0.9;
    effect->color = 7.0;
    auto out = vignette();
    check(flight::effects::normalize_effect(row_of(effect), out), "normalizes with explicit values");
    check(out->intensity == 0.9 && out->color == 7.0, "explicit values are kept");
  }

  // The defaults Record answers `in` for its own entries and for Object.prototype's names, never for an
  // absent key.
  {
    const auto entry = flight::effects::get_effect_defaults(flight::String("VignetteEffect"));
    check(entry.has(flight::String("intensity")) && !entry.has(flight::String("kind")),
          "the defaults entry holds the documented keys only");
    check(flight::is_object_prototype_property(flight::String("toString")) &&
              !flight::is_object_prototype_property(flight::String("kind")),
          "inherited names are present on every ordinary object, ordinary keys are not");
  }

  // An unknown kind has no entry: false, and `out` is untouched.
  {
    auto effect = vignette();
    effect->kind = flight::String("NoSuchEffect");
    auto out = vignette();
    check(!flight::effects::normalize_effect(row_of(effect), out), "an unknown kind is refused");
    check(!out->intensity.has_value() && out->kind == flight::String("VignetteEffect"), "out is unchanged");
  }

  if (failures == 0) std::cout << "effects normalizeEffect membership: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
