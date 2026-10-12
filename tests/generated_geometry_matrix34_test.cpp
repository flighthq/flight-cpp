#include <flight/geometry/matrix.hpp>
#include <flight/geometry/matrix3.hpp>
#include <flight/geometry/matrix4.hpp>
#include <flight/geometry/quaternion.hpp>
#include <flight/geometry/transform3d.hpp>
#include <flight/geometry/typedarray.hpp>
#include <flight/geometry/vector3.hpp>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

// Native port of packages/geometry/src/{matrix4,matrix3,typedarray,transform3d}.test.ts at a627849 against
// the generated headers (geometry has no override). Each check names the upstream case it ports.
//
// Translation notes, not exclusions:
// - A TypeScript matrix-like or vector-like object literal ({ m: new Float32Array(...) }, { x, y, z })
//   becomes the exact generated owner built from the same fields; the functions take readonly rows of
//   that owner, so the literal and a created instance reach them the same way.
// - `typeof initializeX === 'function'` is established by the C++ declaration and is a static_assert.
// - `toBeInstanceOf(Float32Array)` is the declared member type, also a static_assert.
// - `toStrictEqual` / `toEqual` on typed arrays compare type and contents (not identity), so they are
//   ported as an element-wise Object.is comparison of equal-length arrays.
//
// Excluded: none.

namespace {

namespace geo = flight::geometry;
namespace types = flight::types;
using M3 = flight::Ref<types::Matrix3>;
using M4 = flight::Ref<types::Matrix4>;
using V3 = flight::Ref<types::Vector3>;
using V4 = flight::Ref<types::Vector4>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// vitest's toBe: Object.is, so -0 is not 0 and NaN is NaN.
bool same(double actual, double expected) {
  if (std::isnan(actual) || std::isnan(expected)) return std::isnan(actual) && std::isnan(expected);
  return actual == expected && std::signbit(actual) == std::signbit(expected);
}

// vitest's toBeCloseTo(expected, digits): |actual - expected| < 10^-digits / 2.
bool close(double actual, double expected, int digits = 2) {
  if (actual == expected) return true;
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

// expect(() => ...).toThrow(RangeError): the call must throw, and what it throws must be a RangeError.
template <typename Call>
bool throws_range_error(Call call) {
  try {
    call();
  } catch (const flight::RangeError&) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

// expect(() => ...).toThrow(): any throw.
template <typename Call>
bool throws_anything(Call call) {
  try {
    call();
  } catch (...) {
    return true;
  }
  return false;
}

const double pi = M_PI;

// Array.from(typedArray): each stored element read back as a number.
template <typename Array>
std::vector<double> from(const Array& array) {
  std::vector<double> out;
  for (std::size_t i = 0; i < array.size(); ++i) out.push_back(static_cast<double>(array[i]));
  return out;
}

// toEqual over number arrays: same length, each element Object.is-equal.
bool equal(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (!same(a[i], b[i])) return false;
  return true;
}

std::vector<double> slice(const std::vector<double>& values, std::size_t begin, std::size_t end) {
  return std::vector<double>(values.begin() + begin, values.begin() + end);
}

flight::Float32Array f32(std::initializer_list<float> values) { return flight::Float32Array(values); }
flight::Float32Array f32(std::size_t length) { return flight::Float32Array(length); }

// The { m: new Float32Array([...]) } matrix-like literals.
M3 m3like(std::initializer_list<float> values) {
  return flight::make_ref<types::Matrix3>(types::Matrix3{.entity_runtime_key = std::nullopt, .m = f32(values)});
}
M4 m4like(std::initializer_list<float> values) {
  return flight::make_ref<types::Matrix4>(types::Matrix4{.entity_runtime_key = std::nullopt, .m = f32(values)});
}

// The { x, y, z } and { x, y, z, w } vector-like literals.
V3 v3(double x, double y, double z) {
  return flight::make_ref<types::Vector3>(types::Vector3{.entity_runtime_key = std::nullopt, .x = x, .y = y, .z = z});
}
V4 v4(double x, double y, double z, double w) {
  return flight::make_ref<types::Vector4>(types::Vector4{.entity_runtime_key = std::nullopt, .x = x, .y = y, .z = z, .w = w});
}

bool vec4_equal(const V4& v, double x, double y, double z, double w) { return same(v->x, x) && same(v->y, y) && same(v->z, z) && same(v->w, w); }
bool vec3_equal(const V3& v, double x, double y, double z) { return same(v->x, x) && same(v->y, y) && same(v->z, z); }

V4 x_axis() { return v4(1, 0, 0, 0); }
V4 y_axis() { return v4(0, 1, 0, 0); }
V4 z_axis() { return v4(0, 0, 1, 0); }

M4 discriminating_matrix4() {
  auto m = geo::create_matrix4();
  geo::set_matrix4(m, 0, 2, 0, 0, -3, 0, 0, 0, 0, 0, 4, 0, 7, 8, 9, 1);
  return m;
}

bool matrix4_close(const M4& actual, const M4& expected, int digits = 2) {
  bool ok = true;
  for (std::size_t i = 0; i < 16; ++i) ok = ok && close(actual->m[i], expected->m[i], digits);
  return ok;
}

// ---------------------------------------------------------------------------------------------- matrix4

void test_append_matrix4() {
  {
    const auto source = discriminating_matrix4();
    const auto other = geo::create_matrix4();
    geo::set_matrix4(other, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, other, source);
    const auto out = geo::create_matrix4();
    geo::append_matrix4(out, source, other);
    check(equal(from(out->m), from(expected->m)), "appendMatrix4 composes other after source: out = other * source");
  }
  {
    const auto source = discriminating_matrix4();
    const auto other = geo::create_matrix4();
    geo::set_matrix4(other, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1);
    const auto transposed_order = geo::create_matrix4();
    geo::multiply_matrix4(transposed_order, source, other);
    const auto out = geo::create_matrix4();
    geo::append_matrix4(out, source, other);
    check(!equal(from(out->m), from(transposed_order->m)), "appendMatrix4 is not the other operand order");
  }
  {
    const auto source = discriminating_matrix4();
    const auto other = geo::create_matrix4();
    geo::scale_matrix4(other, other, 2, 3, 4);
    const auto expected = geo::create_matrix4();
    geo::append_matrix4(expected, source, other);
    geo::append_matrix4(source, source, other);
    check(equal(from(source->m), from(expected->m)), "appendMatrix4 supports out === source");
  }
}

void test_append_rotation_matrix4() {
  {
    const auto m = geo::create_matrix4();
    geo::append_rotation_matrix4(m, m, pi / 2, z_axis());
    check(close(m->m[0], 0) && close(m->m[1], 1) && close(m->m[4], -1) && close(m->m[5], 0),
          "appendRotationMatrix4 rotates identity around Z axis by 90 degrees");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 10, 0, 0);
    geo::append_rotation_matrix4(m, m, pi / 2, z_axis());
    check(close(m->m[12], 0, 6) && close(m->m[13], 10, 6), "appendRotationMatrix4 sweeps the existing translation around the origin");
  }
  {
    const auto source = discriminating_matrix4();
    const auto rotation = geo::create_matrix4();
    geo::append_rotation_matrix4(rotation, rotation, pi / 3, z_axis());
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, rotation, source);
    const auto out = geo::create_matrix4();
    geo::append_rotation_matrix4(out, source, pi / 3, z_axis());
    check(matrix4_close(out, expected, 5), "appendRotationMatrix4 applies the rotation after source: out = R * source");
  }
  {
    const auto source = geo::create_matrix4();
    geo::translate_matrix4(source, source, 10, 0, 0);
    const auto pivot = v4(5, 0, 0, 1);
    const auto out = geo::create_matrix4();
    geo::append_rotation_matrix4(out, source, pi / 2, z_axis(), pivot);
    const auto lands_on_pivot = geo::create_vector3(-5, 0, 0);
    const auto before = geo::create_vector3();
    geo::matrix4_transform_point(before, source, lands_on_pivot);
    const bool before_ok = close(before->x, pivot->x, 6);
    const auto after = geo::create_vector3();
    geo::matrix4_transform_point(after, out, lands_on_pivot);
    check(before_ok && close(after->x, pivot->x, 6) && close(after->y, pivot->y, 6) && close(after->z, pivot->z, 6),
          "appendRotationMatrix4 holds the pivot point fixed");
  }
  {
    const auto source = discriminating_matrix4();
    const auto world = geo::create_matrix4();
    const auto local = geo::create_matrix4();
    geo::append_rotation_matrix4(world, source, pi / 3, z_axis());
    geo::prepend_rotation_matrix4(local, source, pi / 3, z_axis());
    check(!equal(from(world->m), from(local->m)), "appendRotationMatrix4 differs from the local prependRotationMatrix4 on a translated matrix");
  }
  {
    const auto source = discriminating_matrix4();
    const auto distinct = geo::create_matrix4();
    const auto aliased = geo::clone_matrix4(source);
    geo::append_rotation_matrix4(distinct, source, pi / 3, z_axis(), v4(1, 2, 3, 1));
    geo::append_rotation_matrix4(aliased, aliased, pi / 3, z_axis(), v4(1, 2, 3, 1));
    check(equal(from(aliased->m), from(distinct->m)), "appendRotationMatrix4 writes the same values whether out aliases source or not");
  }
}

void test_append_scale_matrix4() {
  {
    const auto source = discriminating_matrix4();
    const auto scale = geo::create_matrix4();
    geo::set_matrix4(scale, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, scale, source);
    const auto out = geo::create_matrix4();
    geo::append_scale_matrix4(out, source, 2, 3, 4);
    check(equal(from(out->m), from(expected->m)), "appendScaleMatrix4 applies the scale after source: out = S * source");
  }
  {
    const auto source = discriminating_matrix4();
    const auto world = geo::create_matrix4();
    const auto local = geo::create_matrix4();
    geo::append_scale_matrix4(world, source, 2, 3, 4);
    geo::scale_matrix4(local, source, 2, 3, 4);
    check(same(world->m[12], static_cast<double>(source->m[12]) * 2) && same(local->m[12], source->m[12]) && !equal(from(world->m), from(local->m)),
          "appendScaleMatrix4 scales the translation, unlike the local scaleMatrix4");
  }
  {
    const auto source = discriminating_matrix4();
    const auto expected = geo::create_matrix4();
    geo::append_scale_matrix4(expected, source, 2, 3, 4);
    geo::append_scale_matrix4(source, source, 2, 3, 4);
    check(equal(from(source->m), from(expected->m)), "appendScaleMatrix4 supports out === source");
  }
}

void test_append_translation_matrix4() {
  {
    const auto source = discriminating_matrix4();
    const auto out = geo::create_matrix4();
    geo::append_translation_matrix4(out, source, 1, 2, 3);
    check(same(out->m[12], static_cast<double>(source->m[12]) + 1) && same(out->m[13], static_cast<double>(source->m[13]) + 2) &&
              same(out->m[14], static_cast<double>(source->m[14]) + 3) && equal(slice(from(out->m), 0, 12), slice(from(source->m), 0, 12)),
          "appendTranslationMatrix4 adds the offset to the translation and leaves the basis alone");
  }
  {
    const auto source = discriminating_matrix4();
    const auto translation = geo::create_matrix4();
    geo::set_matrix4(translation, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 1);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, translation, source);
    const auto out = geo::create_matrix4();
    geo::append_translation_matrix4(out, source, 1, 2, 3);
    check(equal(from(out->m), from(expected->m)), "appendTranslationMatrix4 equals composing a translation matrix after source");
  }
  {
    const auto source = discriminating_matrix4();
    const auto world = geo::create_matrix4();
    const auto local = geo::create_matrix4();
    geo::append_translation_matrix4(world, source, 1, 2, 3);
    geo::prepend_translation_matrix4(local, source, 1, 2, 3);
    check(!equal(from(world->m), from(local->m)), "appendTranslationMatrix4 differs from the local prependTranslationMatrix4 on a rotated matrix");
  }
  {
    const auto source = discriminating_matrix4();
    const auto before = from(source->m);
    const auto out = geo::create_matrix4(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
    const auto aliased = geo::clone_matrix4(source);
    geo::append_translation_matrix4(out, source, 1, 2, 3);
    geo::append_translation_matrix4(aliased, aliased, 1, 2, 3);
    check(equal(from(out->m), from(aliased->m)) && equal(from(source->m), before),
          "appendTranslationMatrix4 copies the whole source matrix when out is a distinct object");
  }
}

void test_clone_matrix4() {
  {
    const auto source = geo::create_matrix4();
    const M4 clone = geo::clone_matrix4(source);
    check(clone != nullptr && clone != source, "cloneMatrix4 creates a createMatrix4 instance");
  }
  {
    const auto source = geo::create_matrix4(2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53);
    const auto clone = geo::clone_matrix4(source);
    check(equal(from(clone->m), from(source->m)), "cloneMatrix4 copies all values from the source matrix");
  }
  {
    const auto source = geo::create_matrix4();
    const auto clone = geo::clone_matrix4(source);
    clone->m[5] = 42;
    check(same(source->m[5], 1) && same(clone->m[5], 42), "cloneMatrix4 does not share internal storage");
  }
}

void test_compose_matrix4() {
  {
    const auto out = geo::create_matrix4();
    geo::compose_matrix4(out, geo::create_vector3(10, 20, 30), geo::create_quaternion(), geo::create_vector3(2, 3, 4));
    check(close(out->m[0], 2, 6) && close(out->m[5], 3, 6) && close(out->m[10], 4, 6) && close(out->m[12], 10, 6) && close(out->m[13], 20, 6) &&
              close(out->m[14], 30, 6) && close(out->m[15], 1, 6),
          "composeMatrix4 composes translation, identity rotation and scale");
  }
  {
    const auto position = geo::create_vector3(1, -2, 3);
    const auto rotation = geo::create_quaternion();
    geo::set_quaternion_from_axis_angle(rotation, geo::create_vector3(0, 1, 0), pi / 5);
    const auto scale = geo::create_vector3(2, 2, 2);
    const auto m = geo::create_matrix4();
    geo::compose_matrix4(m, position, rotation, scale);
    const auto p = geo::create_vector3();
    const auto r = geo::create_quaternion();
    const auto s = geo::create_vector3();
    geo::decompose_matrix4(p, r, s, m);
    const double dot = r->x * rotation->x + r->y * rotation->y + r->z * rotation->z + r->w * rotation->w;
    check(close(p->x, 1, 5) && close(p->y, -2, 5) && close(p->z, 3, 5) && close(s->x, 2, 5) && close(s->y, 2, 5) && close(s->z, 2, 5) &&
              close(std::abs(dot), 1, 5),
          "composeMatrix4 round-trips with decomposeMatrix4");
  }
}

void test_copy_matrix4() {
  {
    const auto source = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto out = geo::create_matrix4();
    geo::copy_matrix4(out, source);
    check(equal(from(out->m), from(source->m)), "copyMatrix4 copies all values from source into out");
  }
  {
    const auto source = geo::create_matrix4();
    const auto out = geo::create_matrix4();
    geo::copy_matrix4(out, source);
    out->m[0] = 99;
    check(same(source->m[0], 1) && same(out->m[0], 99), "copyMatrix4 does not share the underlying Float32Array");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4(m, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1);
    geo::copy_matrix4(m, m);
    check(equal(from(m->m), {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1}), "copyMatrix4 is a no-op when out aliases source");
  }
}

void test_copy_matrix4_columns_and_rows() {
  {
    const auto m = geo::create_matrix4();
    geo::copy_matrix4_column_from_vector4(m, 0, v4(1, 2, 3, 4));
    check(same(m->m[0], 1) && same(m->m[1], 2) && same(m->m[2], 3) && same(m->m[3], 4), "copyMatrix4ColumnFromVector4 copies values into column 0");
  }
  {
    const auto m = geo::create_matrix4();
    geo::copy_matrix4_column_from_vector4(m, 2, v4(5, 6, 7, 8));
    check(same(m->m[8], 5) && same(m->m[9], 6) && same(m->m[10], 7) && same(m->m[11], 8), "copyMatrix4ColumnFromVector4 copies values into column 2");
  }
  {
    const auto m = geo::create_matrix4();
    const auto v = v4(0, 0, 0, 0);
    check(throws_range_error([&] { geo::copy_matrix4_column_from_vector4(m, -1, v); }) &&
              throws_range_error([&] { geo::copy_matrix4_column_from_vector4(m, 4, v); }),
          "copyMatrix4ColumnFromVector4 throws a RangeError for an invalid column index");
  }
  {
    bool ok = true;
    for (int column = 0; column < 4; ++column) {
      const auto m = geo::create_matrix4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      geo::copy_matrix4_column_from_vector4(m, column, v4(1, 2, 3, 4));
      std::vector<double> expected(16, 0.0);
      expected[column * 4] = 1;
      expected[column * 4 + 1] = 2;
      expected[column * 4 + 2] = 3;
      expected[column * 4 + 3] = 4;
      ok = ok && equal(from(m->m), expected);
    }
    check(ok, "copyMatrix4ColumnFromVector4 writes column c into elements 4c..4c+3 for every column");
  }

  {
    const auto m = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto out = v4(0, 0, 0, 0);
    geo::copy_matrix4_column_to_vector4(out, 1, m);
    check(vec4_equal(out, 5, 6, 7, 8), "copyMatrix4ColumnToVector4 copies values from column 1 into a vector");
  }
  {
    const auto m = geo::create_matrix4();
    const auto out = v4(0, 0, 0, 0);
    check(throws_range_error([&] { geo::copy_matrix4_column_to_vector4(out, 99, m); }) &&
              throws_range_error([&] { geo::copy_matrix4_column_to_vector4(out, -1, m); }),
          "copyMatrix4ColumnToVector4 throws a RangeError for an invalid column index");
  }
  {
    const auto m = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    bool ok = true;
    for (int column = 0; column < 4; ++column) {
      const auto out = v4(0, 0, 0, 0);
      geo::copy_matrix4_column_to_vector4(out, column, m);
      ok = ok && vec4_equal(out, column * 4 + 1, column * 4 + 2, column * 4 + 3, column * 4 + 4);
    }
    check(ok, "copyMatrix4ColumnToVector4 reads column c from elements 4c..4c+3 for every column");
  }

  {
    const auto m = geo::create_matrix4();
    geo::copy_matrix4_row_from_vector4(m, 0, v4(1, 2, 3, 4));
    check(same(m->m[0], 1) && same(m->m[4], 2) && same(m->m[8], 3) && same(m->m[12], 4), "copyMatrix4RowFromVector4 copies values into row 0");
  }
  {
    const auto m = geo::create_matrix4();
    geo::copy_matrix4_row_from_vector4(m, 3, v4(9, 8, 7, 6));
    check(same(m->m[3], 9) && same(m->m[7], 8) && same(m->m[11], 7) && same(m->m[15], 6), "copyMatrix4RowFromVector4 copies values into row 3");
  }
  {
    const auto m = geo::create_matrix4();
    const auto v = v4(0, 0, 0, 0);
    check(throws_range_error([&] { geo::copy_matrix4_row_from_vector4(m, -1, v); }) && throws_range_error([&] { geo::copy_matrix4_row_from_vector4(m, 4, v); }),
          "copyMatrix4RowFromVector4 throws a RangeError for an invalid row index");
  }
  {
    bool ok = true;
    for (int row = 0; row < 4; ++row) {
      const auto m = geo::create_matrix4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      geo::copy_matrix4_row_from_vector4(m, row, v4(1, 2, 3, 4));
      std::vector<double> expected(16, 0.0);
      expected[row] = 1;
      expected[row + 4] = 2;
      expected[row + 8] = 3;
      expected[row + 12] = 4;
      ok = ok && equal(from(m->m), expected);
    }
    check(ok, "copyMatrix4RowFromVector4 writes row r with a stride of four for every row");
  }

  {
    const auto m = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto out = v4(0, 0, 0, 0);
    geo::copy_matrix4_row_to_vector4(out, 2, m);
    check(vec4_equal(out, 3, 7, 11, 15), "copyMatrix4RowToVector4 copies values from row 2 into a vector");
  }
  {
    const auto m = geo::create_matrix4();
    const auto out = v4(0, 0, 0, 0);
    check(throws_range_error([&] { geo::copy_matrix4_row_to_vector4(out, 42, m); }) && throws_range_error([&] { geo::copy_matrix4_row_to_vector4(out, -1, m); }),
          "copyMatrix4RowToVector4 throws a RangeError for an invalid row index");
  }
  {
    const auto m = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    bool ok = true;
    for (int row = 0; row < 4; ++row) {
      const auto out = v4(0, 0, 0, 0);
      geo::copy_matrix4_row_to_vector4(out, row, m);
      ok = ok && vec4_equal(out, row + 1, row + 5, row + 9, row + 13);
    }
    check(ok, "copyMatrix4RowToVector4 reads row r with a stride of four for every row");
  }
}

void test_create_matrix4() {
  check(equal(from(geo::create_matrix4()->m), {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}), "createMatrix4 creates an identity matrix when called with no arguments");
  static_assert(std::is_same_v<decltype(types::Matrix4::m), flight::Float32Array>, "createMatrix4 creates a Float32Array of length 16");
  check(geo::create_matrix4()->m.size() == 16, "createMatrix4 creates a Float32Array of length 16");
  {
    const auto m = geo::create_matrix4(2, std::nullopt, std::nullopt, std::nullopt, std::nullopt, 3);
    check(equal(from(m->m), {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}), "createMatrix4 overrides only the provided constructor values");
  }
  {
    const auto m = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    check(equal(from(m->m), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}), "createMatrix4 maps constructor arguments to correct column-major indices");
  }

  check(geo::create_matrix4_from2_d(1, 0, 0, 1, 10, 20) != nullptr, "createMatrix4From2D creates a Matrix4 instance");
  check(equal(from(geo::create_matrix4_from2_d(1, 2, 3, 4, 5, 6)->m), {1, 2, 0, 0, 3, 4, 0, 0, 0, 0, 1, 0, 5, 6, 0, 1}),
        "createMatrix4From2D initializes the matrix using set2D semantics");
  {
    const auto a = geo::create_matrix4_from2_d(1, 0, 0, 1, 0, 0);
    const auto b = geo::create_matrix4_from2_d(1, 0, 0, 1, 0, 0);
    b->m[0] = 42;
    check(same(a->m[0], 1) && same(b->m[0], 42), "createMatrix4From2D does not share internal storage with other matrices");
  }

  {
    const auto m1 = geo::create_orthographic_matrix4(-1, 1, -1, 1, 0.1, 100);
    const auto m2 = geo::create_matrix4();
    geo::set_orthographic_matrix4(m2, -1, 1, -1, 1, 0.1, 100);
    check(geo::equals_matrix4(m1, m2) == true, "createOrthographicMatrix4 returns a Matrix4 equivalent to setOrtho");
  }
  {
    const auto m1 = geo::create_perspective_matrix4(0.5, 1.6, 0.1, 1000);
    const auto m2 = geo::create_matrix4();
    geo::set_perspective_matrix4(m2, 0.5, 1.6, 0.1, 1000);
    check(geo::equals_matrix4(m1, m2) == true, "createPerspectiveMatrix4 returns a Matrix4 equivalent to setPerspective");
  }
}

void test_decompose_matrix4() {
  {
    const auto m = geo::create_matrix4();
    const auto rotation = geo::create_quaternion();
    geo::set_quaternion_from_axis_angle(rotation, geo::create_vector3(0, 0, 1), pi / 2);
    geo::compose_matrix4(m, geo::create_vector3(5, 6, 7), rotation, geo::create_vector3(2, 3, 4));
    const auto p = geo::create_vector3();
    const auto r = geo::create_quaternion();
    const auto s = geo::create_vector3();
    geo::decompose_matrix4(p, r, s, m);
    const double dot = r->x * rotation->x + r->y * rotation->y + r->z * rotation->z + r->w * rotation->w;
    check(close(p->x, 5, 5) && close(p->y, 6, 5) && close(p->z, 7, 5) && close(s->x, 2, 5) && close(s->y, 3, 5) && close(s->z, 4, 5) &&
              close(std::abs(dot), 1, 5),
          "decomposeMatrix4 extracts translation, rotation and scale from a TRS matrix");
  }
  {
    const auto p = geo::create_vector3();
    const auto r = geo::create_quaternion();
    const auto s = geo::create_vector3();
    geo::decompose_matrix4(p, r, s, geo::create_matrix4());
    check(close(p->x, 0, 6) && close(p->y, 0, 6) && close(p->z, 0, 6) && close(s->x, 1, 6) && close(s->y, 1, 6) && close(s->z, 1, 6) && close(r->w, 1, 6),
          "decomposeMatrix4 identity matrix decomposes to origin, identity rotation, unit scale");
  }
  const std::vector<std::pair<std::string, V3>> axes = {
      {"x", geo::create_vector3(1, 0, 0)}, {"y", geo::create_vector3(0, 1, 0)}, {"z", geo::create_vector3(0, 0, 1)}};
  for (const auto& [name, axis] : axes) {
    const auto rotation = geo::create_quaternion();
    geo::set_quaternion_from_axis_angle(rotation, axis, pi);
    const auto original = geo::create_matrix4();
    geo::compose_matrix4(original, geo::create_vector3(5, 6, 7), rotation, geo::create_vector3(2, 3, 4));
    const auto p = geo::create_vector3();
    const auto r = geo::create_quaternion();
    const auto s = geo::create_vector3();
    geo::decompose_matrix4(p, r, s, original);
    const auto recomposed = geo::create_matrix4();
    geo::compose_matrix4(recomposed, p, r, s);
    check(matrix4_close(recomposed, original, 5), "decomposeMatrix4 round-trips a half turn about " + name);
  }
  {
    const auto original = geo::create_matrix4();
    geo::compose_matrix4(original, geo::create_vector3(0, 0, 0), geo::create_quaternion(), geo::create_vector3(-2, 3, 4));
    const auto p = geo::create_vector3();
    const auto r = geo::create_quaternion();
    const auto s = geo::create_vector3();
    geo::decompose_matrix4(p, r, s, original);
    const bool scale_ok = close(s->x, -2, 5) && close(s->y, 3, 5) && close(s->z, 4, 5);
    const auto recomposed = geo::create_matrix4();
    geo::compose_matrix4(recomposed, p, r, s);
    check(scale_ok && matrix4_close(recomposed, original, 5), "decomposeMatrix4 folds a mirrored basis into a negative x scale");
  }
  {
    const auto original = geo::create_matrix4();
    geo::compose_matrix4(original, geo::create_vector3(1, 2, 3), geo::create_quaternion(), geo::create_vector3(0, 0, 0));
    const auto p = geo::create_vector3();
    const auto r = geo::create_quaternion();
    const auto s = geo::create_vector3();
    geo::decompose_matrix4(p, r, s, original);
    check(same(s->x, 0) && same(s->y, 0) && same(s->z, 0) && std::isfinite(r->x) && std::isfinite(r->y) && std::isfinite(r->z) && std::isfinite(r->w) &&
              same(p->x, 1),
          "decomposeMatrix4 reports a collapsed axis as zero scale without producing NaN");
  }
}

void test_equals_and_elements_matrix4() {
  {
    const auto m = geo::create_matrix4();
    check(geo::equals_matrix4(m, m) == true, "equalsMatrix4 returns true when comparing the same reference");
  }
  {
    const auto m = geo::create_matrix4();
    check(geo::equals_matrix4(m, flight::null) == false && geo::equals_matrix4(flight::undefined, m) == false && geo::equals_matrix4(flight::null, flight::null) == true,
          "equalsMatrix4 returns false if either argument is null or undefined");
  }
  {
    const auto a = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto b = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    check(geo::equals_matrix4(a, b) == true, "equalsMatrix4 returns true for two matrices with identical values");
  }
  {
    const auto a = geo::create_matrix4();
    const auto b = geo::create_matrix4();
    b->m[10] = 2;
    check(geo::equals_matrix4(a, b) == false, "equalsMatrix4 returns false if any value differs");
  }

  check(same(geo::get_matrix4_determinant(geo::create_matrix4()), 1), "getMatrix4Determinant returns 1 for the identity matrix");

  {
    const auto m = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    check(same(geo::get_matrix4_element(m, 0, 0), 1) && same(geo::get_matrix4_element(m, 1, 0), 2) && same(geo::get_matrix4_element(m, 0, 1), 5) &&
              same(geo::get_matrix4_element(m, 3, 3), 16),
          "getMatrix4Element returns the element at the given row and column");
  }

  {
    const auto m = geo::create_matrix4();
    m->m[12] = 10;
    m->m[13] = 20;
    m->m[14] = 30;
    const auto out = v3(0, 0, 0);
    geo::get_matrix4_position(out, m);
    check(vec3_equal(out, 10, 20, 30), "getMatrix4Position extracts translation components from the matrix");
  }
  {
    const auto m = geo::create_matrix4();
    const auto snapshot = from(m->m);
    const auto out = v3(0, 0, 0);
    geo::get_matrix4_position(out, m);
    check(equal(from(m->m), snapshot), "getMatrix4Position does not mutate the source matrix");
  }

  static_assert(std::is_function_v<decltype(geo::initialize_matrix4)>, "initializeMatrix4 is the construction initializer of createMatrix4");
}

void test_interpolate_matrix4() {
  {
    const auto a = geo::create_matrix4();
    geo::translate_matrix4(a, a, 0, 0, 0);
    const auto b = geo::create_matrix4();
    geo::translate_matrix4(b, b, 10, 0, 0);
    const auto out = geo::create_matrix4();
    geo::interpolate_matrix4(out, a, b, 0);
    check(geo::equals_matrix4(out, a) == true, "interpolateMatrix4 returns a at t=0");
  }
  {
    const auto a = geo::create_matrix4();
    const auto b = geo::create_matrix4();
    geo::translate_matrix4(b, b, 10, 0, 0);
    const auto out = geo::create_matrix4();
    geo::interpolate_matrix4(out, a, b, 1);
    check(geo::equals_matrix4(out, b) == true, "interpolateMatrix4 returns b at t=1");
  }
  {
    const auto a = geo::create_matrix4();
    const auto b = geo::create_matrix4();
    geo::translate_matrix4(b, b, 10, 0, 0);
    const auto out = geo::create_matrix4();
    geo::interpolate_matrix4(out, a, b, 0.5);
    check(close(out->m[12], 5), "interpolateMatrix4 returns midpoint at t=0.5");
  }
  {
    const auto a = geo::create_matrix4();
    const auto b = geo::create_matrix4();
    geo::translate_matrix4(b, b, 10, 20, 30);
    geo::interpolate_matrix4(a, a, b, 0.5);
    check(close(a->m[12], 5) && close(a->m[13], 10) && close(a->m[14], 15), "interpolateMatrix4 supports out === a");
  }
  {
    const auto a = geo::create_matrix4();
    const auto b = geo::create_matrix4();
    geo::translate_matrix4(b, b, 10, 20, 30);
    geo::interpolate_matrix4(b, a, b, 0.5);
    check(close(b->m[12], 5) && close(b->m[13], 10) && close(b->m[14], 15), "interpolateMatrix4 supports out === b");
  }
}

void test_inverse_matrix4() {
  {
    const auto m = geo::create_matrix4();
    const auto inv = geo::create_matrix4();
    geo::inverse_matrix4(inv, m);
    check(geo::equals_matrix4(inv, geo::create_matrix4()) == true, "inverseMatrix4 inverse of identity is identity");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, -3, 2);
    const auto inv = geo::create_matrix4();
    geo::inverse_matrix4(inv, m);
    check(close(inv->m[12], -5) && close(inv->m[13], 3) && close(inv->m[14], -2) && close(inv->m[0], 1) && close(inv->m[5], 1) && close(inv->m[10], 1),
          "inverseMatrix4 inverse of translation only negates translation");
  }
  {
    const auto m = geo::create_matrix4();
    geo::append_rotation_matrix4(m, m, pi / 2, z_axis());
    const auto inv = geo::create_matrix4();
    geo::inverse_matrix4(inv, m);
    const auto product = geo::create_matrix4();
    geo::multiply_matrix4(product, m, inv);
    check(matrix4_close(product, geo::create_matrix4()), "inverseMatrix4 inverse of rotation-only matrix is its transpose");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, 0, 0);
    geo::append_rotation_matrix4(m, m, pi / 2, z_axis());
    const auto inv = geo::create_matrix4();
    geo::inverse_matrix4(inv, m);
    const auto product = geo::create_matrix4();
    geo::multiply_matrix4(product, m, inv);
    check(matrix4_close(product, geo::create_matrix4()), "inverseMatrix4 inverse of rotation + translation");
  }
  {
    const auto matrix = geo::create_matrix4();
    geo::translate_matrix4(matrix, matrix, 5, -3, 2);
    geo::append_rotation_matrix4(matrix, matrix, pi / 6, z_axis());
    geo::scale_matrix4(matrix, matrix, 2, 3, 4);
    const auto expected = geo::create_matrix4();
    geo::inverse_matrix4(expected, matrix);
    geo::inverse_matrix4(matrix, matrix);
    check(matrix4_close(matrix, expected), "inverseMatrix4 supports out === source");
  }
  {
    const auto m = geo::create_matrix4();
    m->m[0] = 0;
    const auto inv = geo::create_matrix4();
    const bool ok = geo::inverse_matrix4(inv, m);
    const double det = geo::get_matrix4_determinant(m);
    check(close(det, 0) && ok == false && std::isnan(inv->m[0]) && std::isnan(inv->m[1]) && std::isnan(inv->m[2]),
          "inverseMatrix4 inverse of singular matrix should fail or produce NaN");
  }
  {
    const auto m = geo::create_matrix4();
    m->m[0] = 0;
    const bool ok = geo::inverse_matrix4(m, m);
    check(ok == false && std::isnan(m->m[0]), "inverseMatrix4 returns false for a singular matrix with out === source");
  }
}

void test_is_affine_matrix4() {
  check(geo::is_affine_matrix4(geo::create_matrix4()) == true, "isAffineMatrix4 returns true for the identity matrix");
  check(geo::is_affine_matrix4(geo::create_matrix4_from2_d(1, 0, 0, 1, 10, 20)) == true, "isAffineMatrix4 returns true for a 2D transform matrix");
  const std::vector<std::pair<int, float>> cases = {{3, 1}, {7, 1}, {11, 1}, {15, 0}};
  const std::vector<std::string> titles = {"returns false if m[3] is non-zero", "returns false if m[7] is non-zero", "returns false if m[11] is non-zero",
                                           "returns false if m[15] is not 1"};
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const auto m = geo::create_matrix4();
    m->m[cases[i].first] = cases[i].second;
    check(geo::is_affine_matrix4(m) == false, "isAffineMatrix4 " + titles[i]);
  }
}

void test_matrix4_transform() {
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, 10, 15);
    const auto out = v3(0, 0, 0);
    geo::matrix4_transform_point(out, m, v3(0, 0, 0));
    check(close(out->x, 5) && close(out->y, 10) && close(out->z, 15), "matrix4TransformPoint translates a point by the matrix translation");
  }
  {
    const auto m = geo::create_matrix4();
    geo::scale_matrix4(m, m, 2, 3, 4);
    const auto out = v3(0, 0, 0);
    geo::matrix4_transform_point(out, m, v3(1, 2, 3));
    check(same(out->x, 2) && same(out->y, 6) && same(out->z, 12), "matrix4TransformPoint scales a point correctly");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, 10, 15);
    const auto point = v3(1, 2, 3);
    geo::matrix4_transform_point(point, m, point);
    check(close(point->x, 6) && close(point->y, 12) && close(point->z, 18), "matrix4TransformPoint supports out === point");
  }

  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, 10, 15);
    const auto out = v4(0, 0, 0, 0);
    geo::matrix4_transform_vector(out, m, v4(1, 2, 3, 1));
    check(close(out->x, 6) && close(out->y, 12) && close(out->z, 18), "matrix4TransformVector transforms a Vector4 by the matrix");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, 10, 15);
    const auto out = v4(0, 0, 0, 0);
    geo::matrix4_transform_vector(out, m, v4(1, 2, 3, 0));
    check(close(out->x, 1) && close(out->y, 2) && close(out->z, 3) && close(out->w, 0), "matrix4TransformVector does not translate a direction vector with w = 0");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, 10, 15);
    const auto vector = v4(1, 2, 3, 1);
    geo::matrix4_transform_vector(vector, m, vector);
    check(close(vector->x, 6) && close(vector->y, 12) && close(vector->z, 18) && close(vector->w, 1), "matrix4TransformVector supports out === vector");
  }

  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 1, 2, 3);
    const auto vectors = f32({0, 0, 0, 1, 0, 0});
    const auto out = f32(std::size_t{6});
    geo::matrix4_transform_vectors(out, m, vectors);
    check(close(out[0], 1) && close(out[1], 2) && close(out[2], 3) && close(out[3], 2) && close(out[4], 2) && close(out[5], 3),
          "matrix4TransformVectors transforms a flat array of [x, y, z] triples");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 1, 2, 3);
    const auto vectors = f32({0, 0, 0, 1, 0, 0});
    geo::matrix4_transform_vectors(vectors, m, vectors);
    check(equal(from(vectors), {1, 2, 3, 2, 2, 3}), "matrix4TransformVectors supports out === vectors");
  }
}

void test_multiply_matrix4() {
  {
    const auto identity = geo::create_matrix4();
    const auto t = geo::create_matrix4_from2_d(1, 0, 0, 1, 10, 20);
    const auto out = geo::create_matrix4();
    geo::multiply_matrix4(out, identity, t);
    check(geo::equals_matrix4(out, t) == true, "multiplyMatrix4 multiply (identity) returns the right-hand operand when left is identity");
  }
  {
    const auto identity = geo::create_matrix4();
    const auto s = geo::create_matrix4();
    geo::scale_matrix4(s, s, 2, 3, 4);
    const auto out = geo::create_matrix4();
    geo::multiply_matrix4(out, s, identity);
    check(geo::equals_matrix4(out, s) == true, "multiplyMatrix4 multiply (identity) returns the left-hand operand when right is identity");
  }
  {
    const auto a = geo::create_matrix4();
    geo::translate_matrix4(a, a, 1, 2, 3);
    const auto b = geo::create_matrix4();
    geo::scale_matrix4(b, b, 2, 2, 2);
    const auto a_before = from(a->m);
    const auto b_before = from(b->m);
    geo::multiply_matrix4(geo::create_matrix4(), a, b);
    check(equal(from(a->m), a_before) && equal(from(b->m), b_before), "multiplyMatrix4 multiply (identity) does not mutate inputs");
  }
  {
    const auto a = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto b = geo::create_matrix4(17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, a, b);
    geo::multiply_matrix4(a, a, b);
    check(geo::equals_matrix4(a, expected) == true, "multiplyMatrix4 multiply (identity) supports out === a");
  }
  {
    const auto a = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto b = geo::create_matrix4(17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, a, b);
    geo::multiply_matrix4(b, a, b);
    check(geo::equals_matrix4(b, expected) == true, "multiplyMatrix4 multiply (identity) supports out === b");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 1, 0, 0);
    const auto s = geo::create_matrix4();
    geo::scale_matrix4(s, s, 2, 2, 2);
    geo::multiply_matrix4(m, m, s);
    check(same(m->m[12], 1) && same(m->m[0], 2), "multiplyMatrix4 post-multiplies by the given matrix");
  }
  {
    const auto t = geo::create_matrix4();
    geo::translate_matrix4(t, t, 10, 0, 0);
    const auto s = geo::create_matrix4();
    geo::scale_matrix4(s, s, 2, 2, 2);
    const auto ts = geo::create_matrix4();
    geo::multiply_matrix4(ts, t, s);
    const auto st = geo::create_matrix4();
    geo::multiply_matrix4(st, s, t);
    check(same(ts->m[12], 10) && same(st->m[12], 20) && geo::equals_matrix4(ts, st) == false,
          "multiplyMatrix4 multiply (ordering) translation x scale != scale x translation");
  }
  {
    const auto m1 = geo::create_matrix4();
    geo::translate_matrix4(m1, m1, 1, 2, 3);
    const auto m2 = geo::create_matrix4();
    geo::translate_matrix4(m2, m2, 1, 2, 3);
    const auto t = geo::create_matrix4();
    geo::translate_matrix4(t, t, 4, 5, 6);
    geo::append_translation_matrix4(m1, m1, 4, 5, 6);
    geo::multiply_matrix4(m2, m2, t);
    check(geo::equals_matrix4(m1, m2) == true, "multiplyMatrix4 multiply equivalence appendTranslation equals multiply by translation matrix");
  }
  {
    const auto m1 = geo::create_matrix4();
    geo::translate_matrix4(m1, m1, 10, 0, 0);
    const auto m2 = geo::create_matrix4();
    geo::translate_matrix4(m2, m2, 10, 0, 0);
    const auto s = geo::create_matrix4();
    geo::scale_matrix4(s, s, 2, 2, 2);
    geo::prepend_scale_matrix4(m1, m1, 2, 2, 2);
    geo::multiply_matrix4(m2, m2, s);
    check(geo::equals_matrix4(m1, m2) == true, "multiplyMatrix4 multiply equivalence prependScale equals multiplying the scale on the right");
  }
}

void test_prepend_matrix4() {
  {
    const auto source = discriminating_matrix4();
    const auto other = geo::create_matrix4();
    geo::set_matrix4(other, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, source, other);
    const auto out = geo::create_matrix4();
    geo::prepend_matrix4(out, source, other);
    check(equal(from(out->m), from(expected->m)), "prependMatrix4 composes other before source: out = source * other");
  }
  {
    const auto source = discriminating_matrix4();
    const auto other = geo::create_matrix4();
    geo::set_matrix4(other, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1);
    const auto appended = geo::create_matrix4();
    geo::append_matrix4(appended, source, other);
    const auto prepended = geo::create_matrix4();
    geo::prepend_matrix4(prepended, source, other);
    check(!equal(from(appended->m), from(prepended->m)), "prependMatrix4 is the opposite composition from appendMatrix4");
  }
  {
    const auto source = discriminating_matrix4();
    const auto other = geo::create_matrix4();
    geo::scale_matrix4(other, other, 2, 3, 4);
    const auto expected = geo::create_matrix4();
    geo::prepend_matrix4(expected, source, other);
    geo::prepend_matrix4(source, source, other);
    check(equal(from(source->m), from(expected->m)), "prependMatrix4 supports out === source");
  }
}

void test_prepend_rotation_matrix4() {
  {
    const auto m = geo::create_matrix4();
    geo::prepend_rotation_matrix4(m, m, pi / 2, z_axis());
    check(close(m->m[0], 0) && close(m->m[1], 1), "prependRotationMatrix4 rotates identity around Z axis");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 10, 0, 0);
    geo::prepend_rotation_matrix4(m, m, pi / 2, z_axis());
    check(close(m->m[12], 10, 6) && close(m->m[13], 0, 6), "prependRotationMatrix4 leaves the existing translation in place");
  }
  {
    const auto source = discriminating_matrix4();
    const auto rotation = geo::create_matrix4();
    geo::prepend_rotation_matrix4(rotation, rotation, pi / 3, z_axis());
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, source, rotation);
    const auto out = geo::create_matrix4();
    geo::prepend_rotation_matrix4(out, source, pi / 3, z_axis());
    check(matrix4_close(out, expected, 5), "prependRotationMatrix4 applies the rotation before source: out = source * R");
  }
  {
    const auto source = discriminating_matrix4();
    const auto pivot = v4(5, 0, 0, 1);
    const auto before = geo::create_vector3();
    geo::matrix4_transform_point(before, source, geo::create_vector3(pivot->x, pivot->y, pivot->z));
    const auto out = geo::create_matrix4();
    geo::prepend_rotation_matrix4(out, source, pi / 2, z_axis(), pivot);
    const auto after = geo::create_vector3();
    geo::matrix4_transform_point(after, out, geo::create_vector3(pivot->x, pivot->y, pivot->z));
    check(close(after->x, before->x, 5) && close(after->y, before->y, 5) && close(after->z, before->z, 5), "prependRotationMatrix4 holds the pivot point fixed");
  }
  {
    const auto source = discriminating_matrix4();
    const auto distinct = geo::create_matrix4();
    const auto aliased = geo::clone_matrix4(source);
    geo::prepend_rotation_matrix4(distinct, source, pi / 3, z_axis(), v4(1, 2, 3, 1));
    geo::prepend_rotation_matrix4(aliased, aliased, pi / 3, z_axis(), v4(1, 2, 3, 1));
    check(equal(from(aliased->m), from(distinct->m)), "prependRotationMatrix4 writes the same values whether out aliases source or not");
  }
}

void test_prepend_scale_and_translation_matrix4() {
  {
    const auto source = discriminating_matrix4();
    const auto scale = geo::create_matrix4();
    geo::set_matrix4(scale, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, source, scale);
    const auto out = geo::create_matrix4();
    geo::prepend_scale_matrix4(out, source, 2, 3, 4);
    check(equal(from(out->m), from(expected->m)), "prependScaleMatrix4 applies the scale before source: out = source * S");
  }
  {
    const auto source = discriminating_matrix4();
    const auto out = geo::create_matrix4();
    geo::prepend_scale_matrix4(out, source, 2, 3, 4);
    check(same(out->m[12], source->m[12]) && same(out->m[13], source->m[13]) && same(out->m[14], source->m[14]),
          "prependScaleMatrix4 leaves the translation alone, unlike the world-frame appendScaleMatrix4");
  }
  {
    const auto source = discriminating_matrix4();
    const auto expected = geo::create_matrix4();
    geo::prepend_scale_matrix4(expected, source, 2, 3, 4);
    geo::prepend_scale_matrix4(source, source, 2, 3, 4);
    check(equal(from(source->m), from(expected->m)), "prependScaleMatrix4 supports out === source");
  }

  {
    const auto source = discriminating_matrix4();
    const auto translation = geo::create_matrix4();
    geo::set_matrix4(translation, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 1);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, source, translation);
    const auto out = geo::create_matrix4();
    geo::prepend_translation_matrix4(out, source, 1, 2, 3);
    check(equal(from(out->m), from(expected->m)), "prependTranslationMatrix4 applies the offset before source: out = source * T");
  }
  {
    const auto source = discriminating_matrix4();
    const auto out = geo::create_matrix4();
    geo::prepend_translation_matrix4(out, source, 1, 2, 3);
    const auto& s = source->m;
    check(same(out->m[12], double(s[0]) * 1 + double(s[4]) * 2 + double(s[8]) * 3 + double(s[12])) &&
              same(out->m[13], double(s[1]) * 1 + double(s[5]) * 2 + double(s[9]) * 3 + double(s[13])) &&
              same(out->m[14], double(s[2]) * 1 + double(s[6]) * 2 + double(s[10]) * 3 + double(s[14])),
          "prependTranslationMatrix4 carries the offset through the source basis");
  }
  {
    const auto source = discriminating_matrix4();
    const auto expected = geo::create_matrix4();
    geo::prepend_translation_matrix4(expected, source, 1, 2, 3);
    geo::prepend_translation_matrix4(source, source, 1, 2, 3);
    check(equal(from(source->m), from(expected->m)), "prependTranslationMatrix4 supports out === source");
  }
}

void test_rotate_matrix4() {
  {
    const auto a = geo::create_matrix4();
    const auto b = geo::create_matrix4();
    geo::rotate_matrix4(a, a, geo::create_vector3(0, 0, 1), pi / 2);
    geo::append_rotation_matrix4(b, b, pi / 2, z_axis());
    check(geo::equals_matrix4(a, b) == true, "rotateMatrix4 matches appendRotation on identity");
  }
  {
    const double inverse_length = 1 / std::sqrt(14.0);
    const auto axis = geo::create_vector3(2 * inverse_length, inverse_length, 3 * inverse_length);
    const double radians = pi / 3;
    const auto quaternion = geo::create_quaternion();
    geo::set_quaternion_from_axis_angle(quaternion, axis, radians);
    const auto expected = geo::create_matrix4();
    geo::set_matrix4_from_quaternion(expected, quaternion);
    const auto actual = geo::create_matrix4();
    geo::rotate_matrix4(actual, actual, axis, radians);
    check(matrix4_close(actual, expected, 6), "rotateMatrix4 matches the quaternion rotation path for an asymmetric unit axis");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 5, 0, 0);
    geo::rotate_matrix4(m, m, geo::create_vector3(0, 0, 1), pi / 2);
    check(same(m->m[12], 5) && same(m->m[13], 0), "rotateMatrix4 preserves translation when rotating locally");
  }
  {
    const auto m = geo::create_matrix4();
    geo::append_rotation_matrix4(m, m, pi / 2, x_axis());
    check(close(m->m[0], 1) && close(m->m[1], 0) && close(m->m[2], 0), "rotateMatrix4 rotation around X does not affect X axis basis vector");
  }
  {
    const auto m = geo::create_matrix4();
    geo::append_rotation_matrix4(m, m, pi / 2, y_axis());
    check(close(m->m[4], 0) && close(m->m[5], 1) && close(m->m[6], 0), "rotateMatrix4 rotation around Y does not affect Y axis basis vector");
  }
  {
    const auto source = geo::create_matrix4();
    geo::set_matrix4(source, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1);
    const auto distinct = geo::create_matrix4();
    const auto aliased = geo::clone_matrix4(source);
    geo::rotate_matrix4(distinct, source, geo::create_vector3(0, 0, 1), pi / 3);
    geo::rotate_matrix4(aliased, aliased, geo::create_vector3(0, 0, 1), pi / 3);
    check(equal(from(aliased->m), from(distinct->m)), "rotateMatrix4 writes the same values whether out aliases source or not");
  }
}

void test_scale_matrix4() {
  {
    const auto m = geo::create_matrix4();
    geo::scale_matrix4(m, m, 2, 3, 4);
    check(same(m->m[0], 2) && same(m->m[5], 3) && same(m->m[10], 4), "scaleMatrix4 scales an identity matrix by (x, y, z)");
  }
  {
    const auto m = geo::create_matrix4();
    geo::scale_matrix4(m, m, 2, 3, 4);
    geo::scale_matrix4(m, m, 5, 6, 7);
    check(same(m->m[0], 10) && same(m->m[5], 18) && same(m->m[10], 28), "scaleMatrix4 accumulates scale multiplicatively");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 10, 20, 30);
    geo::scale_matrix4(m, m, 2, 3, 4);
    check(same(m->m[12], 10) && same(m->m[13], 20) && same(m->m[14], 30), "scaleMatrix4 does not modify translation when scaling locally");
  }
  {
    const auto rotated = geo::create_matrix4();
    geo::set_matrix4(rotated, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1);
    const auto scale = geo::create_matrix4();
    geo::set_matrix4(scale, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1);
    const auto expected = geo::create_matrix4();
    geo::multiply_matrix4(expected, rotated, scale);
    const auto result = geo::create_matrix4();
    geo::scale_matrix4(result, rotated, 2, 3, 4);
    check(equal(from(result->m), from(expected->m)) && same(result->m[1], 2) && same(result->m[4], -3),
          "scaleMatrix4 post-multiplies by diag(sx, sy, sz, 1) on a rotated basis");
  }
  {
    const auto projective = geo::create_matrix4();
    geo::set_matrix4(projective, 1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7, 0, 0, 0, 1);
    geo::scale_matrix4(projective, projective, 2, 3, 4);
    check(same(projective->m[3], 10) && same(projective->m[7], 18) && same(projective->m[11], 28) && same(projective->m[15], 1),
          "scaleMatrix4 scales the fourth row of each basis column for a non-affine matrix");
  }
  {
    const auto source = geo::create_matrix4();
    geo::set_matrix4(source, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1);
    const auto distinct = geo::create_matrix4();
    const auto aliased = geo::clone_matrix4(source);
    geo::scale_matrix4(distinct, source, 2, 3, 4);
    geo::scale_matrix4(aliased, aliased, 2, 3, 4);
    check(equal(from(aliased->m), from(distinct->m)), "scaleMatrix4 writes the same values whether out aliases source or not");
  }
}

void test_set_matrix4() {
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4(m, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    check(equal(from(m->m), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}), "setMatrix4 sets all 16 values in column-major order");
  }
  {
    const auto m = geo::create_matrix4(99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99);
    geo::set_matrix4(m, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    check(equal(from(m->m), {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}), "setMatrix4 overwrites existing matrix values completely");
  }

  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_element(m, 2, 3, 42);
    check(same(geo::get_matrix4_element(m, 2, 3), 42), "setMatrix4Element writes the value at the given row and column");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_element(m, 1, 2, 7);
    check(same(geo::get_matrix4_element(m, 0, 0), 1) && same(geo::get_matrix4_element(m, 3, 3), 1), "setMatrix4Element does not affect other elements");
  }

  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_from2_d(m, 1, 2, 3, 4, 5, 6);
    check(equal(from(m->m), {1, 2, 0, 0, 3, 4, 0, 0, 0, 0, 1, 0, 5, 6, 0, 1}), "setMatrix4From2D sets a 2D transform with translation");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_from2_d(m, 7, 8, 9, 10);
    check(same(m->m[12], 0) && same(m->m[13], 0), "setMatrix4From2D defaults tx and ty to 0 when omitted");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_from2_d(m, 1, 0, 0, 1, 0, 0);
    check(geo::is_affine_matrix4(m) == true, "setMatrix4From2D produces an affine matrix");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_from2_d(m, 1, 2, 3, 4);
    check(same(m->m[10], 1) && same(m->m[2], 0) && same(m->m[6], 0) && same(m->m[14], 0), "setMatrix4From2D sets Z axis to identity");
  }

  {
    auto arr = f32(std::size_t{16});
    for (int i = 0; i < 16; ++i) arr[i] = static_cast<float>(i + 1);
    const auto m = geo::create_matrix4();
    geo::set_matrix4_from_float32_array(m, 0, arr);
    bool ok = true;
    for (int i = 0; i < 16; ++i) ok = ok && same(m->m[i], i + 1);
    check(ok, "setMatrix4FromFloat32Array reads 16 elements at offset 0");
  }
  {
    auto arr = f32(std::size_t{20});
    for (int i = 0; i < 20; ++i) arr[i] = static_cast<float>(i + 1);
    const auto m = geo::create_matrix4();
    geo::set_matrix4_from_float32_array(m, 4, arr);
    bool ok = true;
    for (int i = 0; i < 16; ++i) ok = ok && same(m->m[i], i + 5);
    check(ok, "setMatrix4FromFloat32Array reads 16 elements at a non-zero offset");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 3, 4, 5);
    const auto arr = f32(std::size_t{16});
    geo::write_matrix4_to_float32_array(arr, 0, m);
    const auto m2 = geo::create_matrix4();
    geo::set_matrix4_from_float32_array(m2, 0, arr);
    check(geo::equals_matrix4(m, m2) == true, "setMatrix4FromFloat32Array round-trips with writeMatrix4ToFloat32Array");
  }

  {
    const auto mat2d = geo::create_matrix();
    const auto mat = geo::create_matrix4();
    geo::set_matrix4_from_matrix(mat, mat2d);
    check(equal(from(mat->m), from(f32({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}))), "setMatrix4FromMatrix should convert an Matrix to a Matrix4");
  }
  {
    const auto mat2d = geo::create_matrix(2, 0, 0, 3, 5, 10);
    const auto mat = geo::create_matrix4();
    geo::set_matrix4_from_matrix(mat, mat2d);
    const std::vector<double> rows = {2, 0, 0, 5, 0, 3, 0, 10, 0, 0, 1, 0, 0, 0, 0, 1};
    bool ok = true;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) ok = ok && same(geo::get_matrix4_element(mat, r, c), rows[r * 4 + c]);
    check(ok, "setMatrix4FromMatrix should handle scaling and translation");
  }

  {
    const auto mat3 = geo::create_matrix3();
    const auto mat = geo::create_matrix4();
    geo::set_matrix4_from_matrix3(mat, mat3);
    check(equal(from(mat->m), from(f32({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}))), "setMatrix4FromMatrix3 should convert a Matrix3x3 to a Matrix4");
  }
  {
    const auto mat3 = geo::create_matrix3(2, 0, 5, 0, 3, 10, 0, 0, 1);
    const auto mat = geo::create_matrix4();
    geo::set_matrix4_from_matrix3(mat, mat3);
    const std::vector<double> rows = {2, 0, 0, 5, 0, 3, 0, 10, 0, 0, 1, 0, 0, 0, 0, 1};
    bool ok = true;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) ok = ok && same(geo::get_matrix4_element(mat, r, c), rows[r * 4 + c]);
    check(ok, "setMatrix4FromMatrix3 should handle scaling and translation");
  }

  {
    const auto m = geo::create_matrix4(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2);
    geo::set_matrix4_from_quaternion(m, geo::create_quaternion());
    check(equal(from(m->m), {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}), "setMatrix4FromQuaternion identity quaternion writes the identity matrix");
  }
  {
    const auto q = geo::create_quaternion();
    geo::set_quaternion_from_axis_angle(q, geo::create_vector3(0, 0, 1), pi / 2);
    const auto m = geo::create_matrix4();
    geo::set_matrix4_from_quaternion(m, q);
    const auto out = geo::create_vector3();
    geo::matrix4_transform_point(out, m, geo::create_vector3(1, 0, 0));
    check(close(out->x, 0, 6) && close(out->y, 1, 6) && close(out->z, 0, 6), "setMatrix4FromQuaternion a 90-degree z rotation maps +x to +y");
  }

  {
    const auto m = geo::create_matrix4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    geo::set_matrix4_identity(m);
    check(equal(from(m->m), {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}), "setMatrix4Identity resets a matrix to identity");
  }
}

void test_look_at_and_position_matrix4() {
  {
    const auto m = geo::create_matrix4();
    const auto eye = geo::create_vector3(0, 0, 5);
    geo::set_matrix4_look_at(m, eye, geo::create_vector3(0, 0, 0), geo::create_vector3(0, 1, 0));
    const auto out = geo::create_vector3();
    geo::matrix4_transform_point(out, m, eye);
    check(close(out->x, 0, 6) && close(out->y, 0, 6) && close(out->z, 0, 6), "setMatrix4LookAt places the eye at the origin of view space");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_look_at(m, geo::create_vector3(0, 0, 5), geo::create_vector3(0, 0, 0), geo::create_vector3(0, 1, 0));
    const auto out = geo::create_vector3();
    geo::matrix4_transform_point(out, m, geo::create_vector3(0, 0, 0));
    check(close(out->z, -5, 6), "setMatrix4LookAt looks down -z so the target lands in front of the camera");
  }
  struct LookAtCase {
    std::string name;
    V3 eye, target, up;
  };
  const std::vector<LookAtCase> cases = {
      {"up parallel to the view direction", geo::create_vector3(0, 5, 0), geo::create_vector3(0, 0, 0), geo::create_vector3(0, 1, 0)},
      {"eye at the target", geo::create_vector3(2, 2, 2), geo::create_vector3(2, 2, 2), geo::create_vector3(0, 1, 0)},
      {"up parallel to a z-aligned view", geo::create_vector3(0, 0, 5), geo::create_vector3(0, 0, 0), geo::create_vector3(0, 0, 1)},
  };
  for (const auto& c : cases) {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_look_at(m, c.eye, c.target, c.up);
    bool ok = true;
    for (std::size_t i = 0; i < 16; ++i) ok = ok && std::isfinite(m->m[i]);
    const auto x = geo::create_vector3();
    const auto y = geo::create_vector3();
    const auto z = geo::create_vector3();
    geo::set_vector3(x, m->m[0], m->m[4], m->m[8]);
    geo::set_vector3(y, m->m[1], m->m[5], m->m[9]);
    geo::set_vector3(z, m->m[2], m->m[6], m->m[10]);
    ok = ok && close(geo::get_vector3_length(x), 1, 6) && close(geo::get_vector3_length(y), 1, 6) && close(geo::get_vector3_length(z), 1, 6) &&
         close(geo::get_vector3_dot(x, y), 0, 6) && close(geo::get_vector3_dot(x, z), 0, 6) && close(geo::get_vector3_dot(y, z), 0, 6) &&
         close(std::abs(geo::get_matrix4_determinant(m)), 1, 6);
    check(ok, "setMatrix4LookAt keeps the basis orthonormal when " + c.name);
  }

  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_position(m, v3(5, 6, 7));
    check(same(m->m[12], 5) && same(m->m[13], 6) && same(m->m[14], 7), "setMatrix4Position sets the translation components of the matrix");
  }
  {
    const auto m = geo::create_matrix4();
    const auto before = from(m->m);
    geo::set_matrix4_position(m, v3(1, 2, 3));
    check(same(m->m[0], before[0]) && same(m->m[5], before[5]) && same(m->m[10], before[10]) && same(m->m[15], before[15]),
          "setMatrix4Position does not modify other matrix values");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4_position(m, v3(1, 2, 3));
    geo::set_matrix4_position(m, v3(-1, -2, -3));
    check(same(m->m[12], -1) && same(m->m[13], -2) && same(m->m[14], -3), "setMatrix4Position can overwrite an existing translation");
  }
}

void test_projection_matrix4() {
  {
    const auto m = geo::create_matrix4();
    geo::set_orthographic_matrix4(m, -1, 1, -1, 1, 0.1, 100);
    check(close(m->m[0], 1), "setOrthographicMatrix4 sets m[0] to 2 / (right - left)");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_orthographic_matrix4(m, 0, 2, 0, 4, 0.1, 100);
    check(close(m->m[5], 0.5), "setOrthographicMatrix4 sets m[5] to 2 / (top - bottom)");
  }

  {
    const auto m = geo::create_matrix4();
    check(throws_anything([&] { geo::set_perspective_matrix4(m, 0.5, 0, 0.1, 1000); }), "setPerspectiveMatrix4 throws when aspect is 0");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_perspective_matrix4(m, 0.5, 1.6, 0.1, std::numeric_limits<double>::infinity());
    bool finite = true;
    for (std::size_t i = 0; i < 16; ++i) finite = finite && std::isfinite(m->m[i]);
    check(finite && same(m->m[10], -1) && close(m->m[14], -0.2), "setPerspectiveMatrix4 evaluates the infinite-far projection limit without NaN");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_perspective_matrix4(m, 0.5, 1.6, 0.1, 1000);
    check(same(m->m[11], -1), "setPerspectiveMatrix4 sets m[11] to -1");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_perspective_matrix4(m, 0.5, 1.6, 0.1, 1000);
    check(same(m->m[15], 0), "setPerspectiveMatrix4 sets m[15] to 0 so clip w is -z, not -z + w");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_perspective_matrix4(m, 0.5, 1.6, 0.1, 1000);
    const double z_eye = -5;
    const double w_clip = double(m->m[3]) * 2 + double(m->m[7]) * 3 + double(m->m[11]) * z_eye + double(m->m[15]) * 1;
    check(same(w_clip, 5), "setPerspectiveMatrix4 projects a point to clip w = -z_eye (the perspective divisor)");
  }
}

void test_translate_and_transpose_matrix4() {
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 1, 2, 3);
    check(same(m->m[12], 1) && same(m->m[13], 2) && same(m->m[14], 3), "translateMatrix4 translates an identity matrix by (x, y, z)");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 1, 2, 3);
    geo::translate_matrix4(m, m, 4, 5, 6);
    check(same(m->m[12], 5) && same(m->m[13], 7) && same(m->m[14], 9), "translateMatrix4 accumulates translation when called multiple times");
  }
  {
    const auto source = geo::create_matrix4();
    geo::set_matrix4(source, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1);
    const auto out = geo::create_matrix4(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9);
    const auto aliased = geo::clone_matrix4(source);
    geo::translate_matrix4(out, source, 1, 2, 3);
    geo::translate_matrix4(aliased, aliased, 1, 2, 3);
    check(equal(from(out->m), from(aliased->m)) && equal(from(source->m), {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1}),
          "translateMatrix4 copies the whole source matrix when out is a distinct object");
  }

  {
    const auto m = geo::create_matrix4();
    const auto t = geo::create_matrix4();
    geo::transpose_matrix4(t, m);
    check(geo::equals_matrix4(t, m) == true, "transposeMatrix4 transpose of identity is identity");
  }
  {
    const auto m = geo::create_matrix4();
    geo::scale_matrix4(m, m, 2, 3, 4);
    const auto t = geo::create_matrix4();
    geo::transpose_matrix4(t, m);
    check(geo::equals_matrix4(t, m) == true, "transposeMatrix4 transpose of diagonal matrix is itself");
  }
  {
    const auto m = geo::create_matrix4();
    geo::append_rotation_matrix4(m, m, pi / 2, z_axis());
    const auto t = geo::create_matrix4();
    geo::transpose_matrix4(t, m);
    const auto inv = geo::create_matrix4();
    geo::inverse_matrix4(inv, m);
    check(matrix4_close(t, inv), "transposeMatrix4 transpose of rotation matrix equals its inverse");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4(m, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto t = geo::create_matrix4();
    geo::transpose_matrix4(t, m);
    check(equal(from(t->m), {1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16}), "transposeMatrix4 transpose of arbitrary matrix swaps rows and columns");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4(m, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto t = geo::create_matrix4();
    geo::transpose_matrix4(t, m);
    const auto t2 = geo::create_matrix4();
    geo::transpose_matrix4(t2, t);
    check(geo::equals_matrix4(t2, m) == true, "transposeMatrix4 transpose twice returns original matrix");
  }
  {
    const auto matrix = geo::create_matrix4();
    geo::set_matrix4(matrix, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto expected = geo::create_matrix4();
    geo::transpose_matrix4(expected, matrix);
    geo::transpose_matrix4(matrix, matrix);
    check(geo::equals_matrix4(matrix, expected) == true, "transposeMatrix4 supports out === source");
  }
  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4(m, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53);
    geo::transpose_matrix4(m, m);
    check(equal(from(m->m), {2, 11, 23, 41, 3, 13, 29, 43, 5, 17, 31, 47, 7, 19, 37, 53}), "transposeMatrix4 supports out === source with non-symmetric values");
  }

  {
    const auto m = geo::create_matrix4();
    geo::set_matrix4(m, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const auto arr = f32(std::size_t{16});
    geo::write_matrix4_to_float32_array(arr, 0, m);
    bool ok = true;
    for (std::size_t i = 0; i < 16; ++i) ok = ok && same(arr[i], m->m[i]);
    check(ok, "writeMatrix4ToFloat32Array writes 16 elements at offset 0");
  }
  {
    const auto m = geo::create_matrix4();
    geo::translate_matrix4(m, m, 1, 2, 3);
    const auto arr = f32(std::size_t{20});
    geo::write_matrix4_to_float32_array(arr, 4, m);
    bool ok = true;
    for (std::size_t i = 0; i < 16; ++i) ok = ok && same(arr[4 + i], m->m[i]);
    check(ok && same(arr[0], 0) && same(arr[3], 0), "writeMatrix4ToFloat32Array writes 16 elements at a non-zero offset");
  }
}

// ---------------------------------------------------------------------------------------------- matrix3

bool elements3(const M3& m, std::initializer_list<double> rows) {
  std::vector<double> expected(rows);
  bool ok = true;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) ok = ok && same(geo::get_matrix3_element(m, r, c), expected[r * 3 + c]);
  return ok;
}

void test_matrix3_clone_copy() {
  check(elements3(geo::clone_matrix3(geo::create_matrix3(2, 3, 4, 5, 6, 7, 8, 9, 10)), {2, 3, 4, 5, 6, 7, 8, 9, 10}), "cloneMatrix3 should clone the matrix correctly");
  check(elements3(geo::clone_matrix3(m3like({2, 5, 8, 3, 6, 9, 4, 7, 10})), {2, 3, 4, 5, 6, 7, 8, 9, 10}), "cloneMatrix3 should also clone matrix-like objects");
  {
    const M3 m2 = geo::clone_matrix3(m3like({2, 5, 8, 3, 6, 9, 4, 7, 10}));
    check(m2 != nullptr, "cloneMatrix3 should return a matrix instance");
  }

  {
    const auto m1 = geo::create_matrix3(2, 3, 4, 5, 6, 7, 8, 9, 10);
    const auto m2 = geo::create_matrix3();
    geo::copy_matrix3(m2, m1);
    check(elements3(m2, {2, 3, 4, 5, 6, 7, 8, 9, 10}), "copyMatrix3 should copy matrix values from another matrix");
  }
  {
    const auto matrix = geo::create_matrix3(2, 3, 4, 5, 6, 7, 8, 9, 10);
    geo::copy_matrix3(matrix, matrix);
    check(equal(from(matrix->m), {2, 5, 8, 3, 6, 9, 4, 7, 10}), "copyMatrix3 supports out === source");
  }
}

void test_matrix3_columns_rows() {
  {
    const auto m = geo::create_matrix3();
    geo::copy_matrix3_column_from_vector3(m, 0, geo::create_vector3(1, 2, 3));
    check(same(geo::get_matrix3_element(m, 0, 0), 1) && same(geo::get_matrix3_element(m, 1, 0), 2) && same(geo::get_matrix3_element(m, 2, 0), 3),
          "copyMatrix3ColumnFromVector3 should copy column from a Vector3 to a Matrix3");
  }
  {
    const auto m = geo::create_matrix3();
    geo::copy_matrix3_column_from_vector3(m, 1, geo::create_vector3(3, 4, 5));
    check(same(geo::get_matrix3_element(m, 0, 1), 3) && same(geo::get_matrix3_element(m, 1, 1), 4) && same(geo::get_matrix3_element(m, 2, 1), 5),
          "copyMatrix3ColumnFromVector3 should copy column 1 (c, d)");
  }
  {
    const auto m = geo::create_matrix3();
    geo::copy_matrix3_column_from_vector3(m, 2, geo::create_vector3(5, 6, 7));
    check(same(geo::get_matrix3_element(m, 0, 2), 5) && same(geo::get_matrix3_element(m, 1, 2), 6) && same(geo::get_matrix3_element(m, 2, 2), 7),
          "copyMatrix3ColumnFromVector3 should copy column 2 (tx, ty)");
  }
  {
    const auto m = geo::create_matrix3();
    const auto v = geo::create_vector3();
    check(throws_range_error([&] { geo::copy_matrix3_column_from_vector3(m, -1, v); }) && throws_range_error([&] { geo::copy_matrix3_column_from_vector3(m, 3, v); }),
          "copyMatrix3ColumnFromVector3 throws when column is outside 0 through 2");
  }

  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const auto v = geo::create_vector3();
    geo::copy_matrix3_column_to_vector3(v, 0, m);
    check(vec3_equal(v, 1, 4, 7), "copyMatrix3ColumnToVector3 should copy column to a Vector3 from a Matrix3");
  }
  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const auto v = geo::create_vector3();
    geo::copy_matrix3_column_to_vector3(v, 1, m);
    check(vec3_equal(v, 2, 5, 8), "copyMatrix3ColumnToVector3 should copy column 1 into Vector3");
  }
  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const auto v = geo::create_vector3();
    geo::copy_matrix3_column_to_vector3(v, 2, m);
    check(vec3_equal(v, 3, 6, 9), "copyMatrix3ColumnToVector3 should copy column 2 into Vector3");
  }
  {
    const auto m = geo::create_matrix3();
    const auto v = geo::create_vector3();
    check(throws_range_error([&] { geo::copy_matrix3_column_to_vector3(v, -1, m); }) && throws_range_error([&] { geo::copy_matrix3_column_to_vector3(v, 3, m); }),
          "copyMatrix3ColumnToVector3 throws when column is outside 0 through 2");
  }
  {
    const auto m = m3like({1, 4, 7, 2, 5, 8, 3, 6, 9});
    const auto v = v3(0, 0, 0);
    geo::copy_matrix3_column_to_vector3(v, 0, m);
    check(vec3_equal(v, 1, 4, 7), "copyMatrix3ColumnToVector3 should allow matrix-like and vector-like objects");
  }

  {
    const auto m = geo::create_matrix3();
    geo::copy_matrix3_row_from_vector3(m, 0, geo::create_vector3(1, 2, 3));
    check(same(geo::get_matrix3_element(m, 0, 0), 1) && same(geo::get_matrix3_element(m, 0, 1), 2) && same(geo::get_matrix3_element(m, 0, 2), 3),
          "copyMatrix3RowFromVector3 should copy row from a Vector3 to a Matrix3");
  }
  {
    const auto m = geo::create_matrix3();
    geo::copy_matrix3_row_from_vector3(m, 1, geo::create_vector3(2, 4, 6));
    check(same(geo::get_matrix3_element(m, 1, 0), 2) && same(geo::get_matrix3_element(m, 1, 1), 4) && same(geo::get_matrix3_element(m, 1, 2), 6),
          "copyMatrix3RowFromVector3 should copy row 1 (b, d, ty)");
  }
  {
    const auto m = geo::create_matrix3();
    const auto v = geo::create_vector3();
    check(throws_range_error([&] { geo::copy_matrix3_row_from_vector3(m, -1, v); }) && throws_range_error([&] { geo::copy_matrix3_row_from_vector3(m, 3, v); }),
          "copyMatrix3RowFromVector3 throws when row is outside 0 through 2");
  }
  {
    const auto m = m3like({1, 0, 0, 0, 1, 0, 0, 0, 1});
    geo::copy_matrix3_row_from_vector3(m, 0, v3(1, 2, 3));
    check(same(m->m[0], 1) && same(m->m[3], 2) && same(m->m[6], 3), "copyMatrix3RowFromVector3 should allow matrix-like and vector-like objects");
  }

  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const auto v = geo::create_vector3();
    geo::copy_matrix3_row_to_vector3(v, 0, m);
    check(vec3_equal(v, 1, 2, 3), "copyMatrix3RowToVector3 should copy row to a Vector3 from a Matrix3");
  }
  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const auto v = geo::create_vector3();
    geo::copy_matrix3_row_to_vector3(v, 1, m);
    check(vec3_equal(v, 4, 5, 6), "copyMatrix3RowToVector3 should copy row 1 (b, d, ty)");
  }
  {
    const auto m = geo::create_matrix3();
    const auto v = geo::create_vector3();
    geo::copy_matrix3_row_to_vector3(v, 2, m);
    check(vec3_equal(v, 0, 0, 1), "copyMatrix3RowToVector3 should return (0, 0, 1) for row 2");
  }
  {
    const auto m = geo::create_matrix3();
    const auto v = geo::create_vector3();
    check(throws_range_error([&] { geo::copy_matrix3_row_to_vector3(v, -1, m); }) && throws_range_error([&] { geo::copy_matrix3_row_to_vector3(v, 3, m); }),
          "copyMatrix3RowToVector3 throws when row is outside 0 through 2");
  }
  {
    const auto m = m3like({1, 4, 7, 2, 5, 8, 3, 6, 9});
    const auto v = v3(0, 0, 0);
    geo::copy_matrix3_row_to_vector3(v, 0, m);
    check(vec3_equal(v, 1, 2, 3), "copyMatrix3RowToVector3 should allow matrix-like and vector-like objects");
  }
}

void test_matrix3_create_equals() {
  check(elements3(geo::create_matrix3(2, 3, 4, 5, 6, 7, 8, 9, 10), {2, 3, 4, 5, 6, 7, 8, 9, 10}), "createMatrix3 should initialize matrix with provided values");
  check(equal(from(geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9)->m), {1, 4, 7, 2, 5, 8, 3, 6, 9}), "createMatrix3 stores its (row, column) arguments column-major");
  check(elements3(geo::create_matrix3(), {1, 0, 0, 0, 1, 0, 0, 0, 1}), "createMatrix3 should default to identity matrix when no values are provided");

  check(geo::equals_matrix3(geo::create_matrix3(), flight::null) == false, "equalsMatrix3 should return false if one matrix is null and the other is not");
  check(geo::equals_matrix3(geo::create_matrix3(), flight::undefined) == false, "equalsMatrix3 should return false if one matrix is undefined and the other is not");
  check(geo::equals_matrix3(flight::null, flight::null) == true, "equalsMatrix3 should return true if both matrix objects are null");
  check(geo::equals_matrix3(flight::undefined, flight::undefined) == true, "equalsMatrix3 should return true if both matrix objects are undefined");
  // Upstream's body for this title compares undefined with undefined, not undefined with null; ported as written.
  check(geo::equals_matrix3(flight::undefined, flight::undefined) == true, "equalsMatrix3 should return true if one matrix object is undefined and the other is null");
  {
    const auto mat1 = geo::create_matrix3();
    const auto mat2 = geo::create_matrix3();
    mat2->m[0] = 2;
    check(geo::equals_matrix3(mat1, mat2) == false, "equalsMatrix3 should return false if both matrix objects are defined and have different values");
  }
  check(geo::equals_matrix3(geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9), m3like({1, 4, 7, 2, 5, 8, 3, 6, 9})) == true,
        "equalsMatrix3 should return true if one object is matrix-like and one is not");
  check(geo::equals_matrix3(m3like({1, 2, 3, 4, 5, 6, 7, 8, 9}), m3like({1, 2, 3, 4, 5, 6, 7, 8, 9})) == true, "equalsMatrix3 should return true if both object are matrix-like");

  check(close(geo::get_matrix3_determinant(geo::create_matrix3()), 1, 6), "getMatrix3Determinant returns 1 for the identity matrix");
  check(close(geo::get_matrix3_determinant(geo::create_matrix3(1, 2, 3, 1, 2, 3, 4, 5, 6)), 0, 6), "getMatrix3Determinant returns 0 for a singular matrix");
  check(close(geo::get_matrix3_determinant(geo::create_matrix3(-1, 0, 0, 0, 1, 0, 0, 0, 1)), -1, 6),
        "getMatrix3Determinant returns a negative value when a row is negated (sign flip)");

  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    check(same(geo::get_matrix3_element(m, 0, 0), 1) && same(geo::get_matrix3_element(m, 0, 1), 2) && same(geo::get_matrix3_element(m, 0, 2), 3) &&
              same(geo::get_matrix3_element(m, 1, 0), 4) && same(geo::get_matrix3_element(m, 2, 2), 9),
          "getMatrix3Element returns the element at the given row and column");
  }
  static_assert(std::is_function_v<decltype(geo::initialize_matrix3)>, "initializeMatrix3 is the construction initializer of createMatrix3");
}

void test_matrix3_inverse() {
  {
    const auto m = geo::create_matrix3(2, 0, 5, 0, 2, 3, 0, 0, 1);
    const auto out = geo::create_matrix3();
    geo::inverse_matrix3(out, m);
    check(close(out->m[0], 0.5) && close(out->m[1], 0) && close(out->m[3], 0) && close(out->m[4], 0.5) && close(out->m[6], -2.5) && close(out->m[7], -1.5),
          "inverseMatrix3 should invert the matrix correctly");
  }
  {
    const auto source = geo::create_matrix3(2, 1, 5, 3, 4, 6, 0, 0, 1);
    const auto out = geo::create_matrix3(9, 9, 9, 9, 9, 9, 0, 0, 1);
    geo::inverse_matrix3(out, source);
    const auto result = geo::create_matrix3();
    geo::multiply_matrix3(result, source, out);
    check(close(geo::get_matrix3_element(result, 0, 0), 1) && close(geo::get_matrix3_element(result, 0, 1), 0) && close(geo::get_matrix3_element(result, 1, 0), 0) &&
              close(geo::get_matrix3_element(result, 1, 1), 1),
          "inverseMatrix3 should not depend on initial out matrix values");
  }
  {
    const auto m = m3like({2, 0, 0, 0, 2, 0, 5, 3, 1});
    const auto out = m3like({0, 0, 0, 0, 0, 0, 0, 0, 0});
    geo::inverse_matrix3(out, m);
    check(close(out->m[0], 0.5) && close(out->m[1], 0) && close(out->m[3], 0) && close(out->m[4], 0.5) && close(out->m[6], -2.5) && close(out->m[7], -1.5),
          "inverseMatrix3 should should allow matrix-like objects");
  }
  {
    const auto matrix = geo::create_matrix3(2, 1, 5, 3, 4, 6, 0, 0, 1);
    const auto expected = geo::create_matrix3();
    geo::inverse_matrix3(expected, matrix);
    geo::inverse_matrix3(matrix, matrix);
    check(geo::equals_matrix3(matrix, expected) == true, "inverseMatrix3 supports out === source for affine matrices");
  }
  {
    const auto matrix = geo::create_matrix3(1, 2, 3, 0, 1, 4, 5, 6, 0);
    const auto expected = geo::create_matrix3();
    geo::inverse_matrix3(expected, matrix);
    geo::inverse_matrix3(matrix, matrix);
    check(geo::equals_matrix3(matrix, expected) == true, "inverseMatrix3 supports out === source for non-affine matrices");
  }
  {
    const auto m = geo::create_matrix3(2, 0, 5, 0, 2, 3, 0, 0, 1);
    const auto out = geo::create_matrix3();
    check(geo::inverse_matrix3(out, m) == true, "inverseMatrix3 returns true on success");
  }
  {
    const auto m = geo::create_matrix3(0, 0, 5, 0, 0, 3, 0, 0, 1);
    const auto out = geo::create_matrix3();
    check(geo::inverse_matrix3(out, m) == false && std::isnan(out->m[0]) && std::isnan(out->m[4]), "inverseMatrix3 returns false and fills NaN for a singular affine matrix");
  }
  {
    const auto m = geo::create_matrix3(1, 2, 3, 2, 4, 6, 1, 1, 1);
    const auto out = geo::create_matrix3();
    check(geo::inverse_matrix3(out, m) == false && std::isnan(out->m[0]), "inverseMatrix3 returns false and fills NaN for a singular non-affine matrix");
  }
  {
    const auto m = geo::create_matrix3(0, 0, 5, 0, 0, 3, 0, 0, 1);
    check(geo::inverse_matrix3(m, m) == false && std::isnan(m->m[0]), "inverseMatrix3 returns false for a singular matrix with out === source");
  }

  check(geo::is_affine_matrix3(geo::create_matrix3()) == true, "isAffineMatrix3 returns true for identity matrix");
  check(geo::is_affine_matrix3(geo::create_matrix3(1, 0, 0, 0, 1, 0, 1, 0, 1)) == false, "isAffineMatrix3 returns false when last row is not (0, 0, 1)");
  check(geo::is_affine_matrix3(geo::create_matrix3(1, 0, 5, 0, 1, 10, 0, 0, 1)) == true, "isAffineMatrix3 returns true for a matrix with translation");
}

void test_matrix3_multiply_rotate_scale() {
  {
    const auto a = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 10);
    const auto b = geo::create_matrix3(2, 0, 1, 1, 3, 0, 0, 2, 4);
    const auto expected = geo::create_matrix3(4, 12, 13, 13, 27, 28, 22, 44, 47);
    const auto out = geo::create_matrix3();
    geo::multiply_matrix3(out, a, b);
    const bool distinct = geo::equals_matrix3(out, expected);
    geo::multiply_matrix3(a, a, b);
    check(distinct && geo::equals_matrix3(a, expected), "multiplyMatrix3 multiplies a non-affine matrix into distinct and aliased outputs");
  }
  {
    const auto a = geo::create_matrix3(2, 3, 4, 5, 7, 8, 0, 0, 1);
    const auto b = geo::create_matrix3(11, 13, 17, 19, 23, 29, 0, 0, 1);
    const auto expected = geo::create_matrix3();
    geo::multiply_matrix3(expected, a, b);
    geo::multiply_matrix3(a, a, b);
    check(geo::equals_matrix3(a, expected) == true, "multiplyMatrix3 should support out === a");
  }
  {
    const auto a = geo::create_matrix3(2, 3, 4, 5, 7, 8, 0, 0, 1);
    const auto b = geo::create_matrix3(11, 13, 17, 19, 23, 29, 0, 0, 1);
    const auto expected = geo::create_matrix3();
    geo::multiply_matrix3(expected, a, b);
    geo::multiply_matrix3(b, a, b);
    check(geo::equals_matrix3(b, expected) == true, "multiplyMatrix3 should support out === b");
  }
  {
    const auto a = geo::create_matrix3();
    const auto b = geo::create_matrix3(2, 3, 4, 5, 6, 7);
    const auto out = geo::create_matrix3();
    geo::multiply_matrix3(out, a, b);
    check(geo::equals_matrix3(out, b) == true, "multiplyMatrix3 should multiply identity correctly");
  }
  {
    const auto a = geo::create_matrix3(2, 0, 0, 0, 2, 0, 0, 0, 1);
    const auto b = geo::create_matrix3(1, 0, 5, 0, 1, 5, 0, 0, 1);
    geo::multiply_matrix3(a, a, b);
    check(same(geo::get_matrix3_element(a, 0, 2), 10) && same(geo::get_matrix3_element(a, 1, 2), 10),
          "multiplyMatrix3 composes an affine scale then translate (row/column semantics)");
  }
  {
    const auto a = m3like({2, 0, 0, 0, 2, 0, 0, 0, 1});
    const auto b = m3like({1, 0, 0, 0, 1, 0, 5, 5, 1});
    geo::multiply_matrix3(a, a, b);
    check(same(a->m[6], 10) && same(a->m[7], 10), "multiplyMatrix3 should allow matrix-like objects");
  }

  {
    const auto m = geo::create_matrix3(1, 0, 0, 0, 1, 0, 0, 0, 1);
    const auto out = geo::create_matrix3();
    geo::rotate_matrix3(out, m, pi / 2);
    check(close(geo::get_matrix3_element(out, 0, 0), 0) && close(geo::get_matrix3_element(out, 0, 1), -1) && close(geo::get_matrix3_element(out, 1, 0), 1) &&
              close(geo::get_matrix3_element(out, 1, 1), 0),
          "rotateMatrix3 should rotate the matrix correctly");
  }
  {
    const auto m = m3like({1, 0, 0, 0, 1, 0, 0, 0, 1});
    const auto out = m3like({1, 0, 0, 0, 1, 0, 0, 0, 1});
    geo::rotate_matrix3(out, m, pi / 2);
    check(close(out->m[0], 0) && close(out->m[1], 1) && close(out->m[3], -1) && close(out->m[4], 0), "rotateMatrix3 should allow a matrix-like object");
  }
  {
    const auto matrix = geo::create_matrix3(2, 3, 4, 5, 7, 8, 11, 13, 17);
    const auto expected = geo::create_matrix3();
    geo::rotate_matrix3(expected, matrix, pi / 2);
    geo::rotate_matrix3(matrix, matrix, pi / 2);
    check(geo::equals_matrix3(matrix, expected) == true, "rotateMatrix3 supports out === source");
  }

  {
    const auto m = geo::create_matrix3();
    const auto out = geo::create_matrix3();
    geo::scale_matrix3(out, m, 2, 3);
    check(same(geo::get_matrix3_element(out, 0, 0), 2) && same(geo::get_matrix3_element(out, 1, 1), 3), "scaleMatrix3 should scale the matrix correctly");
  }
  {
    const auto m = m3like({1, 0, 0, 0, 1, 0, 0, 0, 1});
    const auto out = geo::create_matrix3();
    geo::scale_matrix3(out, m, 2, 3);
    check(same(out->m[0], 2) && same(out->m[4], 3), "scaleMatrix3 should allow a matrix-like object");
  }
  {
    const auto matrix = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    geo::scale_matrix3(matrix, matrix, 2, 3);
    check(equal(from(matrix->m), {2, 8, 14, 6, 15, 24, 3, 6, 9}), "scaleMatrix3 supports out === source");
  }
}

void test_matrix3_set() {
  {
    const auto m = geo::create_matrix3();
    geo::set_matrix3(m, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    bool ok = true;
    for (int row = 0; row < 3; ++row)
      for (int col = 0; col < 3; ++col) ok = ok && same(geo::get_matrix3_element(m, row, col), row * 3 + col + 1);
    check(ok, "setMatrix3 sets all 9 elements by (row, column)");
  }
  {
    const auto m = geo::create_matrix3();
    geo::set_matrix3(m, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    check(equal(from(m->m), {1, 4, 7, 2, 5, 8, 3, 6, 9}), "setMatrix3 stores its arguments column-major");
  }

  {
    const auto m = geo::create_matrix3();
    geo::set_matrix3_element(m, 1, 2, 42);
    check(same(geo::get_matrix3_element(m, 1, 2), 42), "setMatrix3Element writes the value at the given row and column");
  }
  {
    const auto m = geo::create_matrix3();
    geo::set_matrix3_element(m, 0, 1, 7);
    check(same(geo::get_matrix3_element(m, 0, 0), 1) && same(geo::get_matrix3_element(m, 1, 1), 1), "setMatrix3Element does not affect other elements");
  }

  {
    const auto data = f32({99, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    const auto m = geo::create_matrix3();
    geo::set_matrix3_from_float32_array(m, 1, data);
    check(equal(from(m->m), {1, 2, 3, 4, 5, 6, 7, 8, 9}), "setMatrix3FromFloat32Array reads 9 values starting at the given offset");
  }
  {
    const auto data = f32({1, 2, 3, 4, 5, 6, 7, 8, 9});
    const auto m = geo::create_matrix3();
    geo::set_matrix3_from_float32_array(m, 0, data);
    check(equal(from(m->m), {1, 2, 3, 4, 5, 6, 7, 8, 9}), "setMatrix3FromFloat32Array defaults offset to 0");
  }

  {
    const auto mat = geo::create_matrix3();
    geo::set_matrix3_from_matrix(mat, geo::create_matrix());
    check(equal(from(mat->m), from(f32({1, 0, 0, 0, 1, 0, 0, 0, 1}))), "setMatrix3FromMatrix should convert a Matrix3x2 to a Matrix3");
  }
  {
    const auto mat = geo::create_matrix3();
    geo::set_matrix3_from_matrix(mat, geo::create_matrix(2, 0, 0, 3, 5, 10));
    check(equal(from(mat->m), from(f32({2, 0, 0, 0, 3, 0, 5, 10, 1}))), "setMatrix3FromMatrix should handle scaling and translation");
  }

  {
    const auto mat = geo::create_matrix3();
    geo::set_matrix3_from_matrix4(mat, m4like({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 4, 5, 1}));
    check(equal(from(mat->m), from(f32({1, 0, 0, 0, 1, 0, 0, 0, 1}))), "setMatrix3FromMatrix4 should convert an identity Matrix4x4 to a Matrix3");
  }
  {
    const auto mat = geo::create_matrix3();
    geo::set_matrix3_from_matrix4(mat, m4like({2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 5, 6, 7, 1}));
    check(equal(from(mat->m), from(f32({2, 0, 0, 0, 3, 0, 0, 0, 4}))), "setMatrix3FromMatrix4 should handle non-identity Matrix4x4 correctly");
  }
  {
    const auto mat = geo::create_matrix3();
    geo::set_matrix3_from_matrix4(mat, m4like({1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9, 0, 0, 0, 0, 1}));
    check(equal(from(mat->m), {1, 2, 3, 4, 5, 6, 7, 8, 9}) && same(geo::get_matrix3_element(mat, 0, 1), 4) && same(geo::get_matrix3_element(mat, 1, 0), 2),
          "setMatrix3FromMatrix4 copies the upper-left 3x3 column-major (no transpose)");
  }

  {
    const auto m = geo::create_matrix3(2, 3, 4, 5, 6, 7, 8, 9, 10);
    geo::set_matrix3_identity(m);
    check(same(geo::get_matrix3_element(m, 0, 0), 1) && same(geo::get_matrix3_element(m, 1, 1), 1) && same(geo::get_matrix3_element(m, 2, 2), 1) &&
              same(geo::get_matrix3_element(m, 0, 1), 0) && same(geo::get_matrix3_element(m, 1, 0), 0),
          "setMatrix3Identity resets a matrix to the identity");
  }

  {
    const auto m4 = geo::create_matrix4(0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    const auto out = geo::create_matrix3();
    geo::set_matrix3_normal_from_matrix4(out, m4);
    check(close(geo::get_matrix3_element(out, 0, 0), 0, 5) && close(geo::get_matrix3_element(out, 0, 1), -1, 5) && close(geo::get_matrix3_element(out, 1, 0), 1, 5) &&
              close(geo::get_matrix3_element(out, 1, 1), 0, 5) && close(geo::get_matrix3_element(out, 2, 2), 1, 5),
          "setMatrix3NormalFromMatrix4 equals the rotation basis for an orthonormal matrix");
  }
  {
    const auto m4 = geo::create_matrix4(0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    const auto out = geo::create_matrix3();
    geo::set_matrix3_normal_from_matrix4(out, m4);
    check(close(out->m[0], 0, 5) && close(out->m[1], 1, 5) && close(out->m[3], -1, 5) && close(out->m[4], 0, 5),
          "setMatrix3NormalFromMatrix4 uploads column-major so a GL upload needs no transpose");
  }
  {
    const auto m4 = geo::create_matrix4(2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    const auto normal = geo::create_matrix3();
    geo::set_matrix3_normal_from_matrix4(normal, m4);
    check(close(geo::get_matrix3_element(normal, 0, 0), 0.5, 5) && close(geo::get_matrix3_element(normal, 1, 1), 1, 5) && close(geo::get_matrix3_element(normal, 2, 2), 1, 5),
          "setMatrix3NormalFromMatrix4 keeps a normal perpendicular under non-uniform scale");
  }
}

void test_matrix3_translate_transpose_write() {
  {
    const auto m = geo::create_matrix3();
    const auto out = geo::create_matrix3();
    geo::translate_matrix3(out, m, 2, 3);
    check(same(geo::get_matrix3_element(out, 0, 2), 2) && same(geo::get_matrix3_element(out, 1, 2), 3), "translateMatrix3 should translate the matrix correctly");
  }
  {
    const auto m = m3like({1, 0, 0, 0, 1, 0, 0, 0, 1});
    const auto out = geo::create_matrix3();
    geo::translate_matrix3(out, m, 2, 3);
    check(same(out->m[6], 2) && same(out->m[7], 3), "translateMatrix3 should allow a matrix-like object");
  }
  {
    const auto matrix = geo::create_matrix3(1, 2, 3, 4, 5, 6, 0, 0, 1);
    geo::translate_matrix3(matrix, matrix, 2, 3);
    check(equal(from(matrix->m), {1, 4, 0, 2, 5, 0, 11, 29, 1}), "translateMatrix3 supports out === source");
  }

  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const auto out = geo::create_matrix3();
    geo::transpose_matrix3(out, m);
    check(equal(from(out->m), {1, 2, 3, 4, 5, 6, 7, 8, 9}) && same(geo::get_matrix3_element(out, 0, 1), 4) && same(geo::get_matrix3_element(out, 1, 0), 2),
          "transposeMatrix3 swaps rows and columns");
  }
  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    geo::transpose_matrix3(m, m);
    check(equal(from(m->m), {1, 2, 3, 4, 5, 6, 7, 8, 9}), "transposeMatrix3 supports out === source");
  }
  {
    const auto m = geo::create_matrix3();
    const auto out = geo::create_matrix3();
    geo::transpose_matrix3(out, m);
    check(equal(from(out->m), {1, 0, 0, 0, 1, 0, 0, 0, 1}), "transposeMatrix3 transpose of identity is identity");
  }

  {
    const auto m = geo::create_matrix3(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const auto data = f32(std::size_t{10});
    geo::write_matrix3_to_float32_array(data, 1, m);
    check(equal(from(data), {0, 1, 4, 7, 2, 5, 8, 3, 6, 9}), "writeMatrix3ToFloat32Array writes 9 values starting at the given offset");
  }
  {
    const auto m = geo::create_matrix3(9, 8, 7, 6, 5, 4, 3, 2, 1);
    const auto data = f32(std::size_t{9});
    geo::write_matrix3_to_float32_array(data, 0, m);
    const auto m2 = geo::create_matrix3();
    geo::set_matrix3_from_float32_array(m2, 0, data);
    check(geo::equals_matrix3(m2, m) == true, "writeMatrix3ToFloat32Array round-trips through setMatrix3FromFloat32Array");
  }
}

// ------------------------------------------------------------------------------------------- typedarray

template <typename Array>
bool strict_equal(const Array& a, const Array& b) {
  return equal(from(a), from(b));
}

template <typename Array, typename Reserve>
void test_reserve(const std::string& name, Reserve reserve) {
  {
    const Array array(std::size_t{100});
    const auto out = reserve(array, 100);
    check(strict_equal(out, array), name + " returns the same array if the capacity is equal");
  }
  {
    const Array array(std::size_t{100});
    const auto out = reserve(array, 10);
    check(strict_equal(out, array) && out.size() == 100, name + " returns the same array if the capacity is less than existing length");
  }
  {
    const Array array(std::size_t{100});
    const auto out = reserve(array, 1000);
    check(!strict_equal(out, array) && out.size() == 1000, name + " allocates a new array if the capacity is larger");
  }
  {
    const Array array{1, 2};
    const auto out = reserve(array, 4);
    check(equal(from(out), {1, 2, 0, 0}), name + " leaves extended capacity zero-filled");
  }
  {
    Array array(std::size_t{10});
    for (std::size_t i = 0; i < 10; ++i) array[i] = static_cast<typename std::remove_reference_t<decltype(array[0])>>(i);
    const auto out = reserve(array, 100);
    bool ok = true;
    for (std::size_t i = 0; i < 10; ++i) ok = ok && same(out[i], array[i]);
    check(ok, name + " copies the existing array values if it allocates");
  }
}

void test_typedarray() {
  test_reserve<flight::Float32Array>("reserveFloat32Array", [](flight::Float32Array a, double c) { return geo::reserve_float32_array(a, c); });
  test_reserve<flight::Int16Array>("reserveInt16Array", [](flight::Int16Array a, double c) { return geo::reserve_int16_array(a, c); });
  test_reserve<flight::Uint16Array>("reserveUint16Array", [](flight::Uint16Array a, double c) { return geo::reserve_uint16_array(a, c); });
}

// ------------------------------------------------------------------------------------------ transform3d

void test_transform3d() {
  {
    const auto t = geo::create_transform3_d();
    t->position = geo::create_vector3(4, 5, 6);
    t->scale = geo::create_vector3(2, 2, 2);
    const auto out = geo::create_matrix4();
    geo::compose_matrix4_from_transform3_d(out, t);
    check(close(out->m[0], 2) && close(out->m[5], 2) && close(out->m[10], 2) && close(out->m[12], 4) && close(out->m[13], 5) && close(out->m[14], 6),
          "composeMatrix4FromTransform3D composes translation, rotation, and scale into a matrix");
  }
  {
    const auto t = geo::create_transform3_d();
    check(vec3_equal(t->position, 0, 0, 0) && same(t->rotation->x, 0) && same(t->rotation->y, 0) && same(t->rotation->z, 0) && same(t->rotation->w, 1) &&
              vec3_equal(t->scale, 1, 1, 1),
          "createTransform3D defaults to identity: zero translation, identity rotation, unit scale");
  }
  {
    const auto source = geo::create_transform3_d();
    source->position = geo::create_vector3(1, 2, 3);
    source->rotation = geo::create_quaternion(0, 0, 0, 1);
    source->scale = geo::create_vector3(2, 3, 4);
    const auto m = geo::create_matrix4();
    geo::compose_matrix4_from_transform3_d(m, source);
    const auto out = geo::create_transform3_d();
    geo::decompose_matrix4_to_transform3_d(out, m);
    check(vec3_equal(out->position, 1, 2, 3) && close(out->scale->x, 2) && close(out->scale->y, 3) && close(out->scale->z, 4),
          "decomposeMatrix4ToTransform3D round-trips a shear-free compose");
  }
  static_assert(std::is_function_v<decltype(geo::initialize_transform3_d)>, "initializeTransform3D is the construction initializer of createTransform3D");
}

}  // namespace

int main() {
  test_append_matrix4();
  test_append_rotation_matrix4();
  test_append_scale_matrix4();
  test_append_translation_matrix4();
  test_clone_matrix4();
  test_compose_matrix4();
  test_copy_matrix4();
  test_copy_matrix4_columns_and_rows();
  test_create_matrix4();
  test_decompose_matrix4();
  test_equals_and_elements_matrix4();
  test_interpolate_matrix4();
  test_inverse_matrix4();
  test_is_affine_matrix4();
  test_matrix4_transform();
  test_multiply_matrix4();
  test_prepend_matrix4();
  test_prepend_rotation_matrix4();
  test_prepend_scale_and_translation_matrix4();
  test_rotate_matrix4();
  test_scale_matrix4();
  test_set_matrix4();
  test_look_at_and_position_matrix4();
  test_projection_matrix4();
  test_translate_and_transpose_matrix4();
  test_matrix3_clone_copy();
  test_matrix3_columns_rows();
  test_matrix3_create_equals();
  test_matrix3_inverse();
  test_matrix3_multiply_rotate_scale();
  test_matrix3_set();
  test_matrix3_translate_transpose_write();
  test_typedarray();
  test_transform3d();
  return failures == 0 ? 0 : 1;
}
