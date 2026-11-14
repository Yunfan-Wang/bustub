//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// aggregation_executor.cpp
//
// Identification: src/execution/aggregation_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>
#include "common/macros.h"

#include "execution/executors/aggregation_executor.h"

namespace bustub {

/**
 * Construct a new AggregationExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The insert plan to be executed
 * @param child_executor The child executor from which inserted tuples are pulled (may be `nullptr`)
 */
AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                         std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),
    plan_(plan),
    child_executor_(std::move(child_executor)),
    aht_(plan->GetAggregates(), plan->GetAggregateTypes()),
    aht_iterator_(aht_.Begin()) {
}

/** Initialize the aggregation */
void AggregationExecutor::Init() {
    child_executor_->Init();
  aht_.Clear();
  std::vector<Tuple> tuples;
  std::vector<RID> rids;
  while (child_executor_->Next(&tuples, &rids, BUSTUB_BATCH_SIZE)) {
    for (const auto &tuple : tuples) {
      aht_.InsertCombine(MakeAggregateKey(&tuple), MakeAggregateValue(&tuple));
    }
  }
  if (plan_->GetGroupBys().empty() && aht_.Begin() == aht_.End()) {
    aht_.InsertEmptyGroup();
  }
  aht_iterator_ = aht_.Begin();
}

/**
 * Yield the next tuple batch from the aggregation.
 * @param[out] tuple_batch The next batch of tuples produced by the aggregation
 * @param[out] rid_batch The next batch of tuple RIDs produced by the aggregation
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if any tuples were produced, `false` if there are no more tuples
 */

auto AggregationExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                               size_t batch_size) -> bool {
  tuple_batch->clear();
  rid_batch->clear();
  auto &it = aht_iterator_;
  const auto end = aht_.End();
  while (it != end && tuple_batch->size() < batch_size) {
    auto values = it.Key().group_bys_;
    const auto &aggregates = it.Val().aggregates_;
    values.insert(values.end(), aggregates.begin(), aggregates.end());
    tuple_batch->emplace_back(values, &GetOutputSchema());
    rid_batch->emplace_back();
    ++it;
  }
  return !tuple_batch->empty();
}

/** Do not use or remove this function; otherwise, you will get zero points. */
auto AggregationExecutor::GetChildExecutor() const -> const AbstractExecutor * { return child_executor_.get(); }

}  // namespace bustub
