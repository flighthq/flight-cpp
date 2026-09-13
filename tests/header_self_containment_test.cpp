#ifndef FLIGHT_CPP_HEADER_SELECTOR
#error "FLIGHT_CPP_HEADER_SELECTOR must name one public header"
#elif FLIGHT_CPP_HEADER_SELECTOR == 1
#include <flight/array.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 2
#include <flight/contract.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 3
#include <flight/date.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 4
#include <flight/equality.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 5
#include <flight/error.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 6
#include <flight/executor.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 7
#include <flight/host.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 8
#include <flight/map.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 9
#include <flight/math.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 10
#include <flight/presence.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 11
#include <flight/rejection.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 12
#include <flight/runtime.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 13
#include <flight/set.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 14
#include <flight/string.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 15
#include <flight/task.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 16
#include <flight/typed_array.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 17
#include <flight/version.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 18
#include <flight/reference.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 19
#include <flight/number.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 20
#include <flight/object.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 21
#include <flight/symbol.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 22
#include <flight/text_decoder.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 23
#include <flight/url.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 24
#include <flight/array_buffer.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 25
#include <flight/data_view.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 26
#include <flight/regexp.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 27
#include <flight/intl.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 28
#include <flight/callable.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 29
#include <flight/conditional_facet_ref.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 30
#include <flight/json.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 31
#include <flight/structural_ref.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 32
#include <flight/weak_map.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 33
#include <flight/array_buffer_view.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 34
#include <flight/sequence_view.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 35
#include <flight/record.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 36
#include <flight/weak_set.hpp>
#elif FLIGHT_CPP_HEADER_SELECTOR == 37
#include <flight/abort.hpp>
#else
#error "FLIGHT_CPP_HEADER_SELECTOR does not name a public header"
#endif

int main() { return 0; }
