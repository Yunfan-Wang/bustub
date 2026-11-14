#pragma once

#include "storage/table/tuple.h"
#include "type/value_factory.h"

// created to address jointuple action type
namespace bustub {

inline auto JoinTuple(const Tuple &left, const Schema &left_schema, const Tuple *right, const Schema &right_schema,
                      const Schema &output_schema) -> Tuple {
  std::vector<Value> values;
  
  for (uint32_t i = 0; i < left_schema.GetColumnCount(); i++) {
    values.push_back(left.GetValue(&left_schema, i));
  }
  for (uint32_t i = 0; i < right_schema.GetColumnCount(); i++) {
    values.push_back(right == nullptr ? ValueFactory::GetNullValueByType(right_schema.GetColumn(i).GetType())
                                      : right->GetValue(&right_schema, i));
  }

  return Tuple(std::move(values), &output_schema);
}

}
