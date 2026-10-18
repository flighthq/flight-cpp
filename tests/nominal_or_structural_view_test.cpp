// flight::NominalOrStructuralView: the carrier for a narrowed derived-interface value that may be any object
// declaring the derived members, not only one allocated as that type (render-wgpu's screen target binder).
//
// Semantics under test, against the source's JavaScript behavior:
//   * the SAME object is the carrier's identity, stored and returned unchanged;
//   * a write through the carrier is visible on the original object;
//   * a factory-made (nominal) owner and a caller-declared structural look-alike behave identically;
//   * an undeclared member reads as absence without throwing at the read;
//   * a native caller holding the nominal owner still passes it unchanged (implicit conversion).
#include <flight/nominal_or_structural_view.hpp>

#include <cstdio>
#include <optional>

namespace {

struct Target : flight::ReferenceEnabled {
  double width{0.0};
};
struct Screen : Target {
  using flight_native_base = Target;
  std::optional<double> presentation_view;
  double capture_width{0.0};
};
// The counterexample shape: declares every screen member but derives only from Target.
struct CallerScreen : Target {
  using flight_native_base = Target;
  std::optional<double> presentation_view;
  double capture_width{0.0};
};

using ScreenView = flight::NominalOrStructuralView<Screen, Target>;

// The base-reference owner case (VignetteEffect widened to Ref<Effect> before its first by-name use).
struct Effect : flight::ReferenceEnabled {
  double width{0.0};
};
struct Vignette : Effect {
  using flight_native_base = Effect;
  double intensity{0.0};
};
struct StrongVignette : Vignette {
  using flight_native_base = Vignette;
  double capture_width{0.0};
};
struct PlainVignette : Effect {
  double intensity{0.0};
};

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

// The behavior of the source binder, written once against the carrier: lazily set the presentation view,
// then return it. Both owners must produce the same observable results through it.
double bind_presentation(const ScreenView& target, double fresh) {
  if (!target.template read<flight::RowKey<"presentationView">, std::optional<double>, &Screen::presentation_view>().value()) {
    target.template write<flight::RowKey<"presentationView">, std::optional<double>, &Screen::presentation_view>(fresh);
  }
  return target.template read<flight::RowKey<"presentationView">, std::optional<double>, &Screen::presentation_view>().value().value();
}

}  // namespace

int main(int argc, char**) {
  // Nominal owner, passed by a native caller exactly as before.
  auto screen = flight::make_ref<Screen>(Screen{});
  const ScreenView from_native = screen;
  check(from_native.nominal() == screen && from_native.object() == screen, "a nominal owner converts implicitly as itself");
  check(bind_presentation(from_native, 7.0) == 7.0 && screen->presentation_view == 7.0,
        "a write through the nominal path lands in the owner's own cell");
  check(bind_presentation(from_native, 9.0) == 7.0, "the presentation view is set once, then reused");

  // The same nominal owner reached through its base, as the narrowed render-pass target reaches it.
  const auto narrowed_nominal = ScreenView::of(flight::Ref<Target>(screen));
  check(narrowed_nominal.nominal() == screen, "a narrowed base reference recovers the same nominal owner");

  // The caller-declared look-alike, reached through its base. Its members come from the generated row
  // table through its EXACT allocated type; nothing is bound by hand.
  auto caller = flight::make_ref<CallerScreen>(CallerScreen{});
  caller->capture_width = 4.0;
  const auto narrowed_caller = ScreenView::of(flight::Ref<Target>(caller));
  check(!narrowed_caller.nominal() && narrowed_caller.object() == caller,
        "a look-alike is carried as itself, with no nominal owner and no cast");
  check(narrowed_caller.template read<flight::RowKey<"captureWidth">, double, &Screen::capture_width>().value() == 4.0,
        "a by-name read sees the look-alike's real field");
  check(!flight::detail::find_row_owner(static_cast<const void*>(caller.get())) &&
            !flight::StructuralMemberView<Target>(flight::Ref<Target>(caller)).has_binding_metadata(),
        "a read registers no owner: it never mutates the registry");
  check(bind_presentation(narrowed_caller, 7.0) == 7.0 && caller->presentation_view == 7.0,
        "a by-name write is visible on the original look-alike's own field");
  check(flight::StructuralMemberView<Target>(flight::Ref<Target>(caller)).has_binding_metadata(),
        "the write, and only the write, created the object's registered owner");
  check(bind_presentation(narrowed_caller, 9.0) == 7.0, "the look-alike is set once, then reused, like the nominal owner");

  // An undeclared member written by name lives in the same object's named cells, readable by another view,
  // and fabricates no native member.
  check(narrowed_caller.template write<flight::RowKey<"lateMember">, double, &Screen::capture_width>(3.0), "an undeclared member write succeeds by name");
  const auto other_view = ScreenView::of(flight::Ref<Target>(caller));
  check(other_view.template read<flight::RowKey<"lateMember">, double, &Screen::capture_width>().value() == 3.0 && caller->capture_width == 4.0,
        "another view reads the by-name cell; the native captureWidth field is untouched");

  // Stored and returned identity: the frame keeps the carrier and later reads the same object.
  std::optional<ScreenView> frame_screen_target = narrowed_caller;
  check(frame_screen_target->object() == caller && *frame_screen_target == narrowed_caller,
        "storing the carrier keeps the same object");
  check(!(narrowed_caller == from_native), "two different objects are different carriers");

  // Exactness: an unregistered allocation (no native heritage marker) has no exact-type evidence and reads
  // absent; a look-alike is never recovered as the nominal owner; absence stays distinct from a stored 0.
  const auto plain = flight::make_ref<Target>(Target{});
  const auto plain_view = ScreenView::of(plain);
  check(!plain_view.nominal() && !plain_view.template read<flight::RowKey<"captureWidth">, double, &Screen::capture_width>().has_value(),
        "an unregistered object reads its members as absent");
  check(!flight::recover_owner<Screen>(flight::Ref<Target>(caller)), "a look-alike of another dynamic type is not recovered");
  caller->capture_width = 0.0;
  const auto zero = narrowed_caller.template read<flight::RowKey<"captureWidth">, double, &Screen::capture_width>();
  check(zero.has_value() && !flight::to_boolean(zero), "a stored 0 is present yet falsy");
  check(!narrowed_caller.template read<flight::RowKey<"missingMember">, double, &Screen::capture_width>().has_value(), "an undeclared member reads as absence");

  // A by-name read of a present value through the transient exact-type owner retains nothing: no owner is
  // registered, the object's reference count is unchanged, no cell is created, and the object expires as
  // soon as its external references drop.
  {
    auto reader = flight::make_ref<CallerScreen>(CallerScreen{});
    reader->capture_width = 6.0;
    std::weak_ptr<CallerScreen> watch = reader;
    const auto uses_before = reader.use_count();
    {
      const auto transient_view = ScreenView::of(flight::Ref<Target>(reader));
      check(transient_view.template read<flight::RowKey<"captureWidth">, double, &Screen::capture_width>().value() == 6.0,
            "a transient read returns the present native value");
    }
    check(!flight::detail::find_row_owner(static_cast<const void*>(reader.get())),
          "a present-value read registered no owner");
    check(reader.use_count() == uses_before, "a present-value read leaves the object's reference count unchanged");
    check(!flight::StructuralMemberView<Target>(flight::Ref<Target>(reader)).has_binding_metadata(),
          "the transient owner is not observable through any other view");
    reader.reset();
    check(watch.expired(), "the object expires once its external references drop");
  }

  // A make_ref allocation reached first through a BASE reference gets the owner of the type it was
  // allocated as: its derived member is present by name, and the owner is the one an exact-type access
  // sees too. Without a recorded allocation, the owner stays at the static type given -- never a guess.
  {
    auto vignette = flight::make_ref<Vignette>(Vignette{});
    vignette->intensity = 0.5;
    const flight::Ref<Effect> widened = vignette;
    const auto named = flight::named_properties(widened);
    check(named.has(flight::String("intensity")), "a widened make_ref allocation answers its derived member by name");
    check(named.has(flight::String("intensity")) && named.get(flight::String("intensity")).as_number() == 0.5,
          "the derived member reads its live value");
    check(flight::detail::owner_for(vignette) == flight::detail::owner_for(widened),
          "the base-reference and exact-type accesses share one owner");

    auto strong = flight::make_ref<StrongVignette>(StrongVignette{});
    strong->capture_width = 3.0;
    const flight::Ref<Effect> strong_widened = strong;
    const auto strong_named = flight::named_properties(strong_widened);
    check(strong_named.has(flight::String("intensity")) && strong_named.has(flight::String("captureWidth")),
          "a subclass allocation binds its own members and its parents'");

    const flight::Ref<Effect> unrecorded = std::make_shared<Vignette>();
    check(!flight::named_properties(unrecorded).has(flight::String("intensity")),
          "an object not made by make_ref keeps the static type's owner");
    const flight::Ref<Effect> plain = flight::make_ref<PlainVignette>(PlainVignette{});
    check(!flight::named_properties(plain).has(flight::String("intensity")),
          "a type with no native-base record keeps the static type's owner");
    const flight::Ref<Effect> base_only = flight::make_ref<Effect>(Effect{});
    check(!flight::named_properties(base_only).has(flight::String("intensity")),
          "a base allocation does not acquire a derived member");
  }

  if (argc > 1) check(caller->presentation_view == 9.0, "mutant: demand that the second bind overwrote");
  std::printf("%s (%d failures)\n", failures ? "FAIL" : "nominal-or-structural view ok", failures);
  return failures ? 1 : 0;
}
