// Focused native behavior for the compiler-erased-member-query patch's class prototype registration at
// Flight a627849. An emitted class carries its methods as C++ member functions that no row cell reports, so
// the emitter registers each non-generic class's prototype members. These fixtures pin what JavaScript
// answers for a real emitted class instance reached as an erased value: `'readUint8' in reader` and
// `typeof reader.remaining` see the prototype method, Object.prototype names stay visible, and a name the
// runtime cannot place (an unbound own field, a foreign method) is refused rather than answered absent --
// without calling any member.
#include <flight/abc/abc_file.hpp>
#include <flight/swf/swf_reader.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

} // namespace

int main() {
  // new AbcReader(bytes): `in` and typeof over own fields, prototype methods, and Object.prototype.
  {
    const auto reader = flight::make_ref<flight::abc::AbcReader>(flight::Uint8Array{0x01, 0x02});
    const auto value = flight::Any::object(reader);
    check(flight::has_property(value, flight::String("readUint8")) && flight::has_property(value, flight::String("remaining")),
          "`in` sees AbcReader's prototype methods");
    check(flight::member_typeof(value, flight::String("readUint8")) == flight::String("function"),
          "typeof a prototype method is function");
    // AbcReader's own fields are not names the generated member table binds, so a key that is neither a
    // bound cell nor on the registered chain may still be a declared field: it is refused, never answered
    // absent. (`'pos' in reader` is true in JavaScript; answering false would be a false negative.)
    const auto refused = [&](const char* key) {
      try {
        static_cast<void>(flight::has_property(value, flight::String(key)));
      } catch (const flight::OutOfProfileError&) {
        return true;
      }
      return false;
    };
    check(refused("pos") && refused("readFixed8"), "an unbound own field or foreign name is refused, not answered");
    check(flight::has_property(value, flight::String("toString")), "Object.prototype names stay visible");
    check(reader->pos == 0.0 && reader->valid, "no query ran a reader method");
  }

  // new SwfReader(bytes, start, end): a second class's table is its own.
  {
    const auto reader = flight::make_ref<flight::swf::SwfReader>(flight::Uint8Array{0x00}, 0.0, 1.0);
    const auto value = flight::Any::object(reader);
    check(flight::has_property(value, flight::String("readFixed8")) &&
              flight::member_typeof(value, flight::String("alignToByte")) == flight::String("function"),
          "SwfReader's prototype methods are visible");
    check(flight::member_typeof(value, flight::String("readUint16")) == flight::String("function"),
          "each class reports its own registered methods");
  }

  if (failures != 0) return 1;
  std::cout << "class prototype presence behaves as specified\n";
  return 0;
}
