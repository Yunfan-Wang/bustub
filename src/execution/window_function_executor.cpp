//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// window_function_executor.cpp
//
// Identification: src/execution/window_function_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/window_function_executor.h"
#include <algorithm>
#include <numeric>
#include <unordered_map>

#include "execution/execution_common.h"
#include "execution/plans/aggregation_plan.h"
#include "execution/plans/window_plan.h"

#include "type/value_factory.h"
#include "storage/table/tuple.h"

namespace bustub {

/**
 * Construct a new WindowFunctionExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The window aggregation plan to be executed
 */
WindowFunctionExecutor::WindowFunctionExecutor(ExecutorContext *exec_ctx, const WindowFunctionPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

/** Initialize the window aggregation */
void WindowFunctionExecutor::Init() {
  child_executor_->Init();
  rows_.clear();
  pos_ = 0;
  std::vector<Tuple> input;
  std::vector<Tuple> batch;
  std::vector<RID> rids;
  while (child_executor_->Next(&batch, &rids, BUSTUB_BATCH_SIZE)) {
    for (auto &tuple : batch) {
      input.push_back(std::move(tuple));
    }
  }

  const auto &schema = child_executor_->GetOutputSchema();
  std::vector<std::vector<Value>> values(input.size(), std::vector<Value>(plan_->columns_.size()));
  std::vector<size_t> output_order(input.size());
  std::iota(output_order.begin(), output_order.end(), 0);
  bool ordered = false;

  for (uint32_t col = 0; col < plan_->columns_.size(); col++) {
    auto it = plan_->window_functions_.find(col);
    if (it == plan_->window_functions_.end()) {
      for (size_t i = 0; i < input.size(); i++) {
        values[i][col] = plan_->columns_[col]->Evaluate(&input[i], schema);
      }
      continue;
    }
    const auto &window = it->second;
    TupleComparator cmp(window.order_by_);
    std::vector<SortEntry> entries;
    for (const auto &tuple : input) {
      entries.emplace_back(GenerateSortKey(tuple, window.order_by_, schema), Tuple{});
    }
    std::vector<size_t> order(input.size());
    std::iota(order.begin(), order.end(), 0);
    if (!window.order_by_.empty()) {
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return cmp(entries[a], entries[b]); });
      if (!ordered) {
        output_order = order;
        ordered = true;
      }
    }

    std::unordered_map<AggregateKey, std::vector<size_t>> partitions;
    for (auto i : order) {
      AggregateKey key;
      for (const auto &expr : window.partition_by_) {
        key.group_bys_.push_back(expr->Evaluate(&input[i], schema));
      }
      partitions[key].push_back(i);
    }

    auto type = window.type_;
    bool count = type == WindowFunctionType::CountAggregate || type == WindowFunctionType::CountStarAggregate;
    for (const auto &pair : partitions) {
      const auto &part = pair.second;
      auto result = ValueFactory::GetNullValueByType(GetOutputSchema().GetColumn(col).GetType());
      if (count) {
        result = ValueFactory::GetIntegerValue(0);
      }
      size_t start = 0;
      while (start < part.size()) {
        size_t end = start + 1;
        while (end < part.size() && !cmp(entries[part[start]], entries[part[end]])) {
          end++;
        }
        if (type == WindowFunctionType::Rank) {
          result = ValueFactory::GetIntegerValue(start + 1);
        } else {
          for (size_t i = start; i < end; i++) {
            auto value = window.function_->Evaluate(&input[part[i]], schema);
            if (count) {
              if (type == WindowFunctionType::CountStarAggregate || !value.IsNull()) {
                result = ValueFactory::GetIntegerValue(result.GetAs<int32_t>() + 1);
              }
            } else if (!value.IsNull()) {
              if (result.IsNull()) {
                result = value;
              } else if (type == WindowFunctionType::SumAggregate) {
                result = result.Add(value);
              } else if (type == WindowFunctionType::MinAggregate) {
                result = result.Min(value);
              } else {
                result = result.Max(value);
              }
            }
          }
        }
        for (size_t i = start; i < end; i++) {
          values[part[i]][col] = result;
        }
        start = end;
      }
    }
  }
  for (auto i : output_order) {
    rows_.emplace_back(std::move(values[i]), &GetOutputSchema());
  }
}

/**
 * Yield the next tuple batch from the window aggregation.
 * @param[out] tuple_batch The next tuple batch produced by the window aggregation
 * @param[out] rid_batch The next tuple RID batch produced by the window aggregation
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 */
auto WindowFunctionExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                                  size_t batch_size) -> bool {
  tuple_batch->clear();
  rid_batch->clear();
  while (pos_ < rows_.size() && tuple_batch->size() < batch_size) {
    tuple_batch->push_back(rows_[pos_++]);
    rid_batch->emplace_back();
  }
  return !tuple_batch->empty();
}
}  // namespace bustub
