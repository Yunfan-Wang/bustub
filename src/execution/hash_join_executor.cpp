//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// hash_join_executor.cpp
//
// Identification: src/execution/hash_join_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/hash_join_executor.h"
#include "common/macros.h"

#include "common/exception.h"
#include "execution/executors/join_tuple.h"
#include "storage/page/intermediate_result_page.h"

namespace bustub {

namespace {

auto JoinKey(const std::vector<AbstractExpressionRef> &exprs, const Tuple &tuple,
             const Schema &schema) -> AggregateKey {
  AggregateKey key;
  for (const auto &expr : exprs) {
    key.group_bys_.push_back(expr->Evaluate(&tuple, schema));
  }
  return key;
}

}

/**
 * Construct a new HashJoinExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The HashJoin join plan to be executed
 * @param left_child The child executor that produces tuples for the left side of join
 * @param right_child The child executor that produces tuples for the right side of join
 */
HashJoinExecutor::HashJoinExecutor(ExecutorContext *exec_ctx, const HashJoinPlanNode *plan,
                                   std::unique_ptr<AbstractExecutor> &&left_child,
                                   std::unique_ptr<AbstractExecutor> &&right_child)
    : AbstractExecutor(exec_ctx), plan_(plan), left_(std::move(left_child)), right_(std::move(right_child)) {
  if (plan->GetJoinType() != JoinType::LEFT && plan->GetJoinType() != JoinType::INNER) {
    // Note for Spring 2025: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

// destruction also needed
HashJoinExecutor::~HashJoinExecutor() {ClearPages();}

void HashJoinExecutor::ClearPages() {
  auto *bpm = exec_ctx_->GetBufferPoolManager();
  for (auto *partitions : {&left_pages_, &right_pages_}) {
    for (auto &pages : *partitions) {
      for (auto pid : pages) {
        bpm->DeletePage(pid);
      }
      pages.clear();
    }
  }
}

void HashJoinExecutor::PartitionInput(AbstractExecutor *child, const std::vector<AbstractExpressionRef> &keys,
                                      bool keep_nulls, Partitions *partitions) {
  auto *bpm = exec_ctx_->GetBufferPoolManager();
  std::vector<Tuple> tuples;
  std::vector<RID> rids;
  
  while (child->Next(&tuples, &rids, BUSTUB_BATCH_SIZE)) {
    
    for (const auto &tuple : tuples) {
      
      auto key = JoinKey(keys, tuple, child->GetOutputSchema());
      bool has_null = false;
      
      for (const auto &value : key.group_bys_) {
        if (value.IsNull()) {
          has_null = true;
          break;
        }
      }
      
      if (has_null && !keep_nulls) {
        continue;
      }
      
      size_t bucket = std::hash<AggregateKey>{}(key) % PARTITIONS;
      auto &pages = (*partitions)[bucket];
      
      if (!pages.empty()) {
        auto guard = bpm->WritePage(pages.back());
        if (guard.AsMut<IntermediateResultPage>()->Append(tuple)) {
          continue;
        }
      }
      
      pages.push_back(bpm->NewPage());
      
      auto guard = bpm->WritePage(pages.back());
      auto *page = guard.AsMut<IntermediateResultPage>();
      
      page->Init();
      page->Append(tuple);
    }
  }
}

/** Initialize the join */
void HashJoinExecutor::Init() {
  
  ClearPages();
  left_->Init();
  right_->Init();
  
  bool left_join = plan_->GetJoinType() == JoinType::LEFT;
  PartitionInput(left_.get(), plan_->LeftJoinKeyExpressions(), left_join, &left_pages_);
  PartitionInput(right_.get(), plan_->RightJoinKeyExpressions(), false, &right_pages_);
  
  left_rows_.clear();
  matched_.clear();
  build_.clear();
  
  
  matches_ = nullptr;
  partition_ = left_page_ = right_page_ = right_slot_ = left_row_ = match_row_ = 0;
  emit_unmatched_ = false;
}

auto HashJoinExecutor::LoadLeftPage() -> bool {
  
  while (partition_ < PARTITIONS) {
    bool has_left = left_page_ < left_pages_[partition_].size();
    bool has_right = !right_pages_[partition_].empty();
    if (has_left && (has_right || plan_->GetJoinType() == JoinType::LEFT)) {
      break;
    }

    partition_++;
    left_page_ = 0;
  }
  if (partition_ == PARTITIONS) {
    return false;
  }

  auto guard = exec_ctx_->GetBufferPoolManager()->ReadPage(left_pages_[partition_][left_page_++]);
  const auto *page = guard.As<IntermediateResultPage>();
  for (uint32_t i = 0; i < page->Size(); i++) {
    left_rows_.push_back(page->GetTuple(i));
  }
  matched_.assign(left_rows_.size(), false);
  right_page_ = right_slot_ = 0;
  return true;
}

auto HashJoinExecutor::BuildChunk() -> bool {
  build_.clear();
  size_t bytes = 0;
  const auto &pages = right_pages_[partition_];
  
  while (right_page_ < pages.size()) {
    
    auto guard = exec_ctx_->GetBufferPoolManager()->ReadPage(pages[right_page_]);
    const auto *page = guard.As<IntermediateResultPage>();
    
    while (right_slot_ < page->Size()) {
      auto tuple = page->GetTuple(right_slot_);
      const auto length = tuple.GetLength();
      if (bytes > 0 && bytes + length > BUILD_BYTES) {
        return true;
      }
      
      auto key = JoinKey(plan_->RightJoinKeyExpressions(), tuple, right_->GetOutputSchema());
      build_[std::move(key)].push_back(std::move(tuple));
      
      bytes += length;
      right_slot_++;
    }
    
    right_page_++;
    right_slot_ = 0;
  }
  return !build_.empty();
}

/**
 * Yield the next tuple batch from the hash join.
 * @param[out] tuple_batch The next tuple batch produced by the hash join
 * @param[out] rid_batch The next tuple RID batch produced by the hash join
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 */
auto HashJoinExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                            size_t batch_size) -> bool {
  tuple_batch->clear();
  rid_batch->clear();
  
  const auto &left_schema = left_->GetOutputSchema();
  const auto &right_schema = right_->GetOutputSchema();
  
  while (tuple_batch->size() < batch_size) {
    if (left_rows_.empty()) {
      if (!LoadLeftPage()) {
        break;
      }
      left_row_ = left_rows_.size();
      emit_unmatched_ = false;
    }
    
    if (left_row_ == left_rows_.size()) {
      if (emit_unmatched_) {
        left_rows_.clear();
        continue;
      }
      left_row_ = 0;
      
      if (!BuildChunk()) {
        if (plan_->GetJoinType() == JoinType::INNER) {
          left_rows_.clear();
          continue;
        }
        emit_unmatched_ = true;
      }
    }
    
    const auto &left = left_rows_[left_row_];
    if (emit_unmatched_) {
      if (!matched_[left_row_]) {
        tuple_batch->push_back(JoinTuple(left, left_schema, nullptr, right_schema, GetOutputSchema()));
      }
      
      left_row_++;
      continue;
    }
    
    if (matches_ == nullptr) {
      auto key = JoinKey(plan_->LeftJoinKeyExpressions(), left, left_schema);
      auto it = build_.find(key);
      if (it == build_.end()) {
        left_row_++;
        continue;
      }
      matches_ = &it->second;
      match_row_ = 0;
      matched_[left_row_] = true;
    }
    
    while (match_row_ < matches_->size() && tuple_batch->size() < batch_size) {
      const auto &right = (*matches_)[match_row_++];
      tuple_batch->push_back(JoinTuple(left, left_schema, &right, right_schema, GetOutputSchema()));
    }
    
    if (match_row_ == matches_->size()) {
      matches_ = nullptr;
      left_row_++;
    }
  }
  
  rid_batch->resize(tuple_batch->size());
  return !tuple_batch->empty();
}

}  // namespace bustub
