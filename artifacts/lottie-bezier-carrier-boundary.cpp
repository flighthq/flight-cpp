// Counterexample for the existing native row ABI; this is a capability probe, not a Bezier implementation.
#include <flight/json_scalar.hpp>
#include <flight/types/lottie_document.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>
using Schema=flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::LottieShapePath>>>;
using Row=flight::StructuralRef<Schema>;
static_assert(!std::is_convertible_v<Row,flight::Ref<flight::types::LottieShapePath>>);
int main(){
 int checks=0,failures=0;auto check=[&](bool ok){++checks;if(!ok)++failures;};
 std::optional<Row> retained;std::weak_ptr<flight::JsonObject> lifetime;
 {
  auto parsed=flight::Json::parse(R"({"v":7})");auto object=parsed.object_owner();lifetime=object;
  auto row=flight::json_assertion_row<Schema>(flight::Any(parsed));retained=row;
  check(row.identity()==object.get());
  check(flight::json_row_member(row,"v").as_number()==7);
  bool nativeReadRejected=false;try{(void)flight::row_get<flight::RowKey<"v">>(row);}catch(const std::exception& error){nativeReadRejected=true;std::cout<<"number-v typed read: "<<error.what()<<'\n';}
  check(nativeReadRejected);
  object->set("v",flight::JsonValue(9.0));check(flight::json_row_member(row,"v").as_number()==9);
  auto valid=flight::Json::parse(R"({"v":[[1,2]]})");auto validRow=flight::json_assertion_row<Schema>(flight::Any(valid));
  auto raw=flight::json_row_member(validRow,"v");check(raw.array_get(0)->array_get(1)->as_number()==2);
  bool rawArrayRejected=false;try{(void)flight::row_get<flight::RowKey<"v">>(validRow);}catch(const std::exception& error){rawArrayRejected=true;std::cout<<"nested-json-v typed read: "<<error.what()<<'\n';}
  check(rawArrayRejected);
 }
 check(!lifetime.expired()&&flight::json_row_member(*retained,"v").as_number()==9);
 retained.reset();check(lifetime.expired());
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
