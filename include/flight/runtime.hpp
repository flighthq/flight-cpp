#pragma once

#include <flight/array.hpp>
#include <flight/array_buffer.hpp>
#include <flight/callable.hpp>
#include <flight/conditional_facet_ref.hpp>
#include <flight/contract.hpp>
#include <flight/date.hpp>
#include <flight/data_view.hpp>
#include <flight/equality.hpp>
#include <flight/error.hpp>
#include <flight/executor.hpp>
#include <flight/host.hpp>
#include <flight/intl.hpp>
#include <flight/json.hpp>
#include <flight/map.hpp>
#include <flight/math.hpp>
#include <flight/number.hpp>
#include <flight/object.hpp>
#include <flight/presence.hpp>
#include <flight/reference.hpp>
#include <flight/regexp.hpp>
#include <flight/rejection.hpp>
#include <flight/set.hpp>
#include <flight/string.hpp>
#include <flight/structural_ref.hpp>
#include <flight/symbol.hpp>
#include <flight/task.hpp>
#include <flight/text_decoder.hpp>
#include <flight/typed_array.hpp>
#include <flight/url.hpp>
#include <flight/version.hpp>
#include <flight/weak_map.hpp>

template <typename Value>
using FlightTask = flight::Task<Value>;

using FlightDate = flight::Date;
