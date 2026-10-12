#include <flight/abc/contract.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Native port of packages/abc/src/abcFile.test.ts and abcInstruction.test.ts (a627849), all seventeen cases,
// against the generated headers (no override). build_abc mirrors the upstream buildAbc() byte for byte.
// The reader depends on source-patches/compiler-constructor-parameter-property-assignment.patch: without
// it, AbcReader's `source` parameter property is never assigned and every read sees an empty buffer.

namespace {

using flight::String;
using flight::Uint8Array;
namespace abc = flight::abc;
using flight::types::abc_multiname_kind;
using flight::types::abc_opcode;
using flight::types::abc_trait_kind;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

struct Builder {
  std::vector<std::uint8_t> bytes;
  void u8(std::uint32_t value) { bytes.push_back(static_cast<std::uint8_t>(value & 0xff)); }
  void u16(std::uint32_t value) {
    u8(value);
    u8(value >> 8);
  }
  void u30(double value) {
    double remaining = value;
    do {
      const auto byte = static_cast<std::uint32_t>(std::fmod(remaining, 0x80));
      remaining = std::floor(remaining / 0x80);
      u8(remaining > 0 ? (byte | 0x80) : byte);
    } while (remaining > 0);
  }
  void str(const std::string& value) {
    u30(static_cast<double>(value.size()));
    for (const char c : value) u8(static_cast<std::uint8_t>(c));
  }
  void f64(double value) {
    std::uint8_t raw[8];
    std::memcpy(raw, &value, 8);  // little-endian host, as DataView.setFloat64(..., true) writes
    for (const auto byte : raw) u8(byte);
  }
};

Uint8Array build_abc() {
  Builder b;
  b.u16(16);
  b.u16(46);
  b.u30(2);
  b.u30(0xfffffff9);
  b.u30(2);
  b.u30(9);
  b.u30(2);
  b.f64(1.5);
  b.u30(4);
  b.str("Main");
  b.str("stop");
  b.str("flash.display");
  b.u30(2);
  b.u8(0x16);
  b.u30(3);
  b.u30(2);
  b.u30(1);
  b.u30(1);
  b.u30(3);
  b.u8(static_cast<std::uint32_t>(abc_multiname_kind->qname));
  b.u30(1);
  b.u30(1);
  b.u8(static_cast<std::uint32_t>(abc_multiname_kind->multiname));
  b.u30(2);
  b.u30(1);

  b.u30(2);
  b.u30(0);
  b.u30(0);
  b.u30(2);
  b.u8(0);
  b.u30(1);
  b.u30(0);
  b.u30(1);
  b.u30(1);
  b.u8(0x08 | 0x80);
  b.u30(1);
  b.u30(1);
  b.u8(0x03);
  b.u30(1);

  b.u30(0);

  b.u30(1);
  b.u30(1);
  b.u30(1);
  b.u8(0);
  b.u30(0);
  b.u30(1);
  b.u30(1);
  b.u30(2);
  b.u8(static_cast<std::uint32_t>(abc_trait_kind->method));
  b.u30(0);
  b.u30(0);
  b.u30(0);
  b.u30(0);

  b.u30(1);
  b.u30(0);
  b.u30(0);

  b.u30(1);
  b.u30(0);
  b.u30(2);
  b.u30(1);
  b.u30(1);
  b.u30(2);
  b.u30(3);
  b.u8(0xd0);
  b.u8(0x30);
  b.u8(0x47);
  b.u30(0);
  b.u30(0);

  Uint8Array out(b.bytes.size());
  for (std::size_t i = 0; i < b.bytes.size(); ++i) out[i] = b.bytes[i];
  return out;
}

bool doubles_equal(const flight::Array<double>& actual, std::initializer_list<double> expected) {
  if (actual.size() != expected.size()) return false;
  std::size_t index = 0;
  for (const double value : expected)
    if (actual[index++] != value) return false;
  return true;
}

void test_read_abc_file() {
  const auto file = abc::read_abc_file(build_abc());
  if (!file.has_value()) {
    check(false, "readAbcFile reads the well-formed fixture");
    return;
  }
  const auto& pool = file.value()->constant_pool;
  check(file.value()->minor_version == 16.0 && file.value()->major_version == 46.0 && pool->strings[0] == String("") && pool->strings.size() == 4 &&
            pool->strings[1] == String("Main") && pool->strings[2] == String("stop") && pool->strings[3] == String("flash.display") &&
            doubles_equal(pool->integers, {0, -7}) && doubles_equal(pool->unsigned_integers, {0, 9}) && std::abs(pool->doubles[1] - 1.5) < 1e-9,
        "readAbcFile reads the version and the whole constant pool, keeping the reserved zero entry");

  const auto& qualified = pool->multinames[1];
  const auto& multiname = pool->multinames[2];
  check(pool->namespaces[1]->kind == 0x16 && pool->namespaces[1]->name == 3.0 && doubles_equal(pool->namespace_sets[1], {1}) &&
            qualified->kind == abc_multiname_kind->qname && qualified->namespace_ == 1.0 &&
            pool->strings[static_cast<std::size_t>(qualified->name)] == String("Main") && multiname->kind == abc_multiname_kind->multiname &&
            pool->strings[static_cast<std::size_t>(multiname->name)] == String("stop") && multiname->namespace_set == 1.0,
        "readAbcFile reads namespaces, namespace sets, and both multiname shapes");

  const auto& methods = file.value()->methods;
  check(methods.size() == 2 && methods[0]->name == 2.0 && methods[0]->parameter_types.size() == 0 && methods[0]->return_type == 0.0 &&
            methods[1]->optional_values.size() == 1 && methods[1]->optional_values[0]->kind == 0x03 && methods[1]->optional_values[0]->value == 1.0 &&
            doubles_equal(methods[1]->parameter_names, {1}),
        "readAbcFile reads methods with their optional defaults and parameter names");

  const auto& instances = file.value()->instances;
  const auto& classes = file.value()->classes;
  check(instances.size() == 1 && classes.size() == 1 && instances[0]->initializer == 1.0 && instances[0]->name == 1.0 && instances[0]->super_name == 1.0 &&
            instances[0]->traits[0]->kind == abc_trait_kind->method && instances[0]->traits[0]->method_index == 0.0 && classes[0]->initializer == 0.0,
        "readAbcFile reads instances and classes as parallel lists sharing one count");

  const auto& bodies = file.value()->method_bodies;
  check(file.value()->scripts[0]->initializer == 0.0 && bodies.size() == 1 && bodies[0]->local_count == 1.0 && bodies[0]->max_stack == 2.0 &&
            bodies[0]->method == 0.0 && bodies[0]->code.size() == 3 && bodies[0]->code[0] == 0xd0 && bodies[0]->code[1] == 0x30 &&
            bodies[0]->code[2] == 0x47 && bodies[0]->exceptions.size() == 0,
        "readAbcFile reads script and method-body tables, keeping instruction bytes unparsed");

  check(!abc::read_abc_file(Uint8Array()).has_value() && !abc::read_abc_file(Uint8Array{0x10, 0x00, 0x2e, 0x00}).has_value() &&
            !abc::read_abc_file(build_abc().subarray(0, 12)).has_value(),
        "readAbcFile returns null rather than throwing for input that is not a readable container");

  std::uint32_t seed = 0x2f6a51b3u;
  const auto next = [&]() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
  };
  int threw = 0;
  for (int i = 0; i < 500; ++i) {
    Uint8Array bytes(next() % 200);
    for (std::size_t b = 0; b < bytes.size(); ++b) bytes[b] = static_cast<std::uint8_t>(next() & 0xff);
    try {
      static_cast<void>(abc::read_abc_file(bytes));
    } catch (...) {
      ++threw;
    }
  }
  for (int i = 0; i < 500; ++i) {
    auto mutant = build_abc();
    for (int f = 0; f < 3; ++f) mutant[next() % mutant.size()] = static_cast<std::uint8_t>(next() & 0xff);
    try {
      static_cast<void>(abc::read_abc_file(mutant));
    } catch (...) {
      ++threw;
    }
  }
  check(threw == 0, "readAbcFile never throws on arbitrary bytes, whatever they happen to encode");

  auto bytes = build_abc();
  std::size_t index = 20;
  while (index < bytes.size() && bytes[index] != static_cast<std::uint8_t>(abc_multiname_kind->qname)) ++index;
  bytes[index] = 0x7f;
  check(!abc::read_abc_file(bytes).has_value(), "readAbcFile rejects a multiname whose kind the format does not define");
}

struct Expected {
  double offset;
  double opcode;
  std::vector<double> operands;
};

bool instructions_equal(const std::optional<flight::Array<flight::Ref<flight::types::AbcInstruction>>>& actual, const std::vector<Expected>& expected) {
  if (!actual.has_value() || actual->size() != expected.size()) return false;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const auto& instruction = (*actual)[i];
    if (instruction->offset != expected[i].offset || instruction->opcode != expected[i].opcode || instruction->operands.size() != expected[i].operands.size())
      return false;
    for (std::size_t o = 0; o < expected[i].operands.size(); ++o)
      if (instruction->operands[o] != expected[i].operands[o]) return false;
  }
  return true;
}

void test_read_abc_instructions() {
  check(instructions_equal(abc::read_abc_instructions(Uint8Array{0xd0, 0x30, 0x47}),
                           {{0, abc_opcode->get_local0, {}}, {1, abc_opcode->push_scope, {}}, {2, abc_opcode->return_void, {}}}),
        "readAbcInstructions decodes the constructor prologue every compiled method opens with");
  check(instructions_equal(abc::read_abc_instructions(Uint8Array{0x5d, 0x05, 0x24, 0x02, 0x4f, 0x05, 0x01}),
                           {{0, abc_opcode->find_prop_strict, {5}}, {2, abc_opcode->push_byte, {2}}, {4, abc_opcode->call_prop_void, {5, 1}}}),
        "readAbcInstructions reads the operand shapes a call site uses, including two-operand calls");
  check(instructions_equal(abc::read_abc_instructions(Uint8Array{0x2d, 0x81, 0x01}), {{0, abc_opcode->push_int, {129}}}),
        "readAbcInstructions widens a multi-byte variable-length operand");
  check(instructions_equal(abc::read_abc_instructions(Uint8Array{0x10, 0xfc, 0xff, 0xff}), {{0, abc_opcode->jump, {-4}}}),
        "readAbcInstructions sign-extends a backward branch target");
  check(instructions_equal(abc::read_abc_instructions(Uint8Array{0x1b, 0x07, 0x00, 0x00, 0x01, 0x0a, 0x00, 0x00, 0x0d, 0x00, 0x00}),
                           {{0, abc_opcode->lookup_switch, {7, 1, 10, 13}}}),
        "readAbcInstructions reads a lookupswitch, whose case count is one less than its target count");
  check(!abc::read_abc_instructions(Uint8Array{0xd0, 0xff, 0x47}).has_value(), "readAbcInstructions stops rather than guessing on an undefined opcode");
  check(!abc::read_abc_instructions(Uint8Array{0x4f, 0x05}).has_value() && !abc::read_abc_instructions(Uint8Array{0x10, 0xfc}).has_value(),
        "readAbcInstructions reports a truncated operand rather than reading past the body");
  check(instructions_equal(abc::read_abc_instructions(Uint8Array{0x81, 0x83, 0x84, 0x88, 0x89, 0x47}),
                           {{0, abc_opcode->coerce_boolean, {}}, {1, abc_opcode->coerce_int, {}}, {2, abc_opcode->coerce_double, {}},
                            {3, abc_opcode->coerce_uint, {}}, {4, abc_opcode->coerce_object, {}}, {5, abc_opcode->return_void, {}}}),
        "readAbcInstructions places the deprecated typed coerce forms compilers still emit");
  const auto empty = abc::read_abc_instructions(Uint8Array());
  check(empty.has_value() && empty->size() == 0, "readAbcInstructions decodes an empty body as no instructions");
}

}  // namespace

int main() {
  test_read_abc_file();
  test_read_abc_instructions();
  return failures == 0 ? 0 : 1;
}
