#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include <flight/array_buffer.hpp>
#include <flight/error.hpp>
#include <flight/string.hpp>
#include <flight/task.hpp>

// The CSS Font Loading `FontFace`.
//
// Flight constructs one in PORTABLE code -- `new FontFace(family, source)` in
// `packages/font/src/_fontFaceLoad.ts` -- so this cannot be an opaque handle that only a host can
// mint: the runtime has to be able to make one. It is a value handle over shared state, like
// `flight::AbortSignal`, so copies are the same face and JavaScript reference identity survives.
//
// One gap is stated rather than hidden. This runtime has no font parser, so it cannot decide on its
// own whether a source is a usable face. `load()` settles through the font-face loader the profile
// installs; with NO loader installed it REJECTS, naming the missing capability, and the face's
// status becomes "error". It does not resolve: a face that was never parsed must not travel on as
// though it had been, because the next thing the caller does is hand it to
// `HostFontLoadingCapability::addFontFace`.
//
// `HostFontLoadingCapability` itself stays what it is in the source -- a plain object of four
// members, passed by the caller. A face carries no Entity runtime key and no identity-keyed side
// table; everything it knows is in the state below.
namespace flight {

// `FontFace.status` in the CSS Font Loading API. The strings are the observable values.
enum class FontFaceStatus : std::uint8_t { unloaded, loading, loaded, error };

[[nodiscard]] inline String font_face_status_name(FontFaceStatus status) {
  switch (status) {
    case FontFaceStatus::loading: return String("loading");
    case FontFaceStatus::loaded: return String("loaded");
    case FontFaceStatus::error: return String("error");
    case FontFaceStatus::unloaded: break;
  }
  return String("unloaded");
}

// `new FontFace(family, source)` takes `string | ArrayBuffer`: a CSS `src` descriptor such as
// `url(...) format('woff2')`, or the face's bytes.
//
// The ALTERNATIVE ORDER is the compiler's, not the source declaration's. A union's C++ spelling is
// part of the external binding contract: emitted code calls this constructor with the variant it
// built, and a variant whose alternatives are the same set in a different order is a different
// type, so the call simply does not match. The compiler canonicalises `string | ArrayBuffer` to
// `std::variant<flight::ArrayBuffer, flight::String>`; spelling it the other way round here cost
// seven headers. Nothing reaches these alternatives positionally -- every access is by type -- so
// following the compiler costs nothing and is the only spelling that links.
using FontFaceSource = std::variant<ArrayBuffer, String>;

class FontFace;

namespace detail {

struct FontFaceState final {
  String family;
  FontFaceSource source;
  FontFaceStatus status{FontFaceStatus::unloaded};
};

// The seam a profile installs to make `load()` mean something. It is deliberately one function
// rather than a registry: a face is loaded by the one font system the profile has.
using FontFaceLoader = std::function<Task<void>(const String& family, const FontFaceSource& source)>;

[[nodiscard]] inline std::optional<FontFaceLoader>& font_face_loader() {
  static std::optional<FontFaceLoader> loader;
  return loader;
}

} // namespace detail

// Installs the loader `FontFace::load` settles through, and returns the one it replaced. A profile
// with no font system installs nothing, and `load()` reports that rather than pretending.
inline std::optional<detail::FontFaceLoader> set_font_face_loader(
    std::optional<detail::FontFaceLoader> loader) {
  auto previous = detail::font_face_loader();
  detail::font_face_loader() = std::move(loader);
  return previous;
}

class FontFace final {
 public:
  using weak_type = std::weak_ptr<detail::FontFaceState>;

  FontFace(String family, FontFaceSource source)
      : state_(std::make_shared<detail::FontFaceState>(
            detail::FontFaceState{std::move(family), std::move(source), FontFaceStatus::unloaded})) {}

  FontFace(String family, String source) : FontFace(std::move(family), FontFaceSource(std::move(source))) {}

  FontFace(String family, ArrayBuffer source)
      : FontFace(std::move(family), FontFaceSource(std::move(source))) {}

  [[nodiscard]] const String& family() const { return state_->family; }
  [[nodiscard]] const FontFaceSource& source() const { return state_->source; }
  [[nodiscard]] FontFaceStatus status_kind() const noexcept { return state_->status; }
  [[nodiscard]] String status() const { return font_face_status_name(state_->status); }

  // `FontFace.load()`: resolves with this face once the source has been accepted. Calling it again
  // after it has loaded is a no-op that resolves, which is what the specification says.
  [[nodiscard]] Task<FontFace> load() const {
    if (state_->status == FontFaceStatus::loaded) return Task<FontFace>::ready(*this);
    auto& loader = detail::font_face_loader();
    if (!loader) {
      state_->status = FontFaceStatus::error;
      return Task<FontFace>::reject(
          Error(String("flight::FontFace has no font-face loader installed in this profile")));
    }
    state_->status = FontFaceStatus::loading;
    auto state = state_;
    const auto face = *this;
    auto loading = (*loader)(state_->family, state_->source);
    // The face records how the load ended and the settlement is passed along UNCHANGED. Rebuilding
    // the rejection here would replace the host's own reason -- a string, an Error, whatever it
    // threw -- with one this runtime invented, and the caller reads that reason.
    return Task<FontFace>::create([state, face, loading](auto resolve, auto reject) {
      static_cast<void>(loading
                            .then([state, face, resolve] {
                              state->status = FontFaceStatus::loaded;
                              resolve(face);
                            })
                            .catch_error([state, reject](const Rejection& rejection) {
                              state->status = FontFaceStatus::error;
                              reject(rejection);
                            }));
    });
  }

  [[nodiscard]] friend bool operator==(const FontFace& left, const FontFace& right) noexcept {
    return left.state_ == right.state_;
  }

  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<FontFace> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    if (!state) return std::nullopt;
    return FontFace(std::move(state));
  }

 private:
  explicit FontFace(std::shared_ptr<detail::FontFaceState> state) noexcept : state_(std::move(state)) {}

  std::shared_ptr<detail::FontFaceState> state_;
};

} // namespace flight
