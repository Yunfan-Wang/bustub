//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_index_join_executor.cpp
//
// Identification: src/execution/nested_index_join_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_index_join_executor.h"
#include "common/macros.h"
#include "common/exception.h"
#include "execution/execution_common.h"

namespace bustub {

/**
 * Creates a new nested index join executor.
 * @param exec_ctx the context that the nested index join should be performed in
 * @param plan the nested index join plan to be executed
 * @param child_executor the outer table
 */
NestedIndexJoinExecutor::NestedIndexJoinExecutor(ExecutorContext *exec_ctx, const NestedIndexJoinPlanNode *plan,
                                                 std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_(std::move(child_executor)) {
  if (plan->GetJoinType() != JoinType::LEFT && plan->GetJoinType() != JoinType::INNER) {
    // Note for Spring 2025: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void NestedIndexJoinExecutor::Init() {
  child_->Init();
  auto *catalog = exec_ctx_->GetCatalog();
  table_ = catalog->GetTable(plan_->GetInnerTableOid());
  index_ = catalog->GetIndex(plan_->GetIndexOid());
  
  outer_.clear();
  matches_.clear();
  
  outer_pos_ = match_pos_ = 0;
  probed_ = false;
  matched_ = false;
}

auto NestedIndexJoinExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                                   size_t batch_size) -> bool {
  tuple_batch->clear();
  rid_batch->clear();
  
  const auto &outer_schema = child_->GetOutputSchema();
  const auto &inner_schema = plan_->InnerTableSchema();
  std::vector<RID> child_rids;
  //
  while (tuple_batch->size() < batch_size) {
    if (outer_pos_ == outer_.size()) {
      outer_pos_ = 0;
      if (!child_->Next(&outer_, &child_rids, BUSTUB_BATCH_SIZE)) {
        break;
      }
    }
    
    const auto &outer = outer_[outer_pos_];
    if (!probed_) {
      auto key = plan_->KeyPredicate()->Evaluate(&outer, outer_schema);
      if (!key.IsNull()) {
        key = key.CastAs(index_->key_schema_.GetColumn(0).GetType());
        index_->index_->ScanKey(Tuple({key}, &index_->key_schema_), &matches_, exec_ctx_->GetTransaction());
      }
      probed_ = true;
    }
    
    while (match_pos_ < matches_.size() && tuple_batch->size() < batch_size) {
      auto [meta, inner] = table_->table_->GetTuple(matches_[match_pos_++]);
      if (meta.is_deleted_) {
        continue;
      }
      tuple_batch->push_back(JoinTuple(outer, outer_schema, &inner, inner_schema, GetOutputSchema()));
      matched_ = true;
    }
    
    if (match_pos_ < matches_.size()) {
      break;
    }
    
    if (!matched_ && plan_->GetJoinType() == JoinType::LEFT) {
      tuple_batch->push_back(JoinTuple(outer, outer_schema, nullptr, inner_schema, GetOutputSchema()));
    }
    outer_pos_++;
    matches_.clear();
    
    match_pos_ = 0;
    probed_ = false;
    matched_ = false;
  }
  
  rid_batch->resize(tuple_batch->size());
  return !tuple_batch->empty();
}

}  // namespace bustub
