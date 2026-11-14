//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_loop_join_executor.cpp
//
// Identification: src/execution/nested_loop_join_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_loop_join_executor.h"
#include "binder/table_ref/bound_join_ref.h"
#include "common/exception.h"
#include "common/macros.h"
#include "execution/executors/join_tuple.h"

namespace bustub {

/**
 * Construct a new NestedLoopJoinExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The nested loop join plan to be executed
 * @param left_executor The child executor that produces tuple for the left side of join
 * @param right_executor The child executor that produces tuple for the right side of join
 */
NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&left_executor,
                                               std::unique_ptr<AbstractExecutor> &&right_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), left_(std::move(left_executor)), right_(std::move(right_executor)) {
  if (plan->GetJoinType() != JoinType::LEFT && plan->GetJoinType() != JoinType::INNER) {
    // Note for Spring 2025: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

/** Initialize the join */
void NestedLoopJoinExecutor::Init() {
  left_->Init();
  left_batch_.clear();
  right_batch_.clear();
  
  left_pos_ = right_pos_ = 0;
  matched_ = false;
}

/**
 * Yield the next tuple batch from the join.
 * @param[out] tuple_batch The next tuple batch produced by the join
 * @param[out] rid_batch The next tuple RID batch produced by the join
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 */
auto NestedLoopJoinExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                                  size_t batch_size) -> bool {
  tuple_batch->clear();
  rid_batch->clear();
  
  const auto &left_schema = left_->GetOutputSchema();
  const auto &right_schema = right_->GetOutputSchema();
  
  std::vector<RID> child_rids;
  while (tuple_batch->size() < batch_size) {
    if (left_pos_ == left_batch_.size()) {
      left_pos_ = 0;
      if (!left_->Next(&left_batch_, &child_rids, BUSTUB_BATCH_SIZE)) {
        break;
      }
      right_->Init();
    }
    
    const auto &left = left_batch_[left_pos_];
    if (right_pos_ == right_batch_.size()) {
      right_pos_ = 0;
      if (!right_->Next(&right_batch_, &child_rids, BUSTUB_BATCH_SIZE)) {
        if (!matched_ && plan_->GetJoinType() == JoinType::LEFT) {
          tuple_batch->push_back(JoinTuple(left, left_schema, nullptr, right_schema, GetOutputSchema()));
        }
        left_pos_++;
        right_batch_.clear();
        matched_ = false;
        if (left_pos_ < left_batch_.size()) {
          right_->Init();
        }
        continue;
      }
    }
    
    const auto &right = right_batch_[right_pos_++];
    if (plan_->Predicate() != nullptr) {
      auto value = plan_->Predicate()->EvaluateJoin(&left, left_schema, &right, right_schema);
      if (value.IsNull() || !value.GetAs<bool>()) {
        continue;
      }
    }
    
    tuple_batch->push_back(JoinTuple(left, left_schema, &right, right_schema, GetOutputSchema()));
    matched_ = true;
  }
  
  rid_batch->resize(tuple_batch->size());
  return !tuple_batch->empty();
}

}  // namespace bustub
