//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// topn_executor.cpp
//
// Identification: src/execution/topn_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/topn_executor.h"
#include <algorithm>


namespace bustub {

/**
 * Construct a new TopNExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The TopN plan to be executed
 */
TopNExecutor::TopNExecutor(ExecutorContext *exec_ctx, const TopNPlanNode *plan,
                           std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

/** Initialize the TopN */
void TopNExecutor::Init() {
  child_executor_->Init();
  
  top_entries_.clear();
  
  pos_ = 0;
  if (plan_->GetN() == 0) {
    return;
  }
  TupleComparator cmp(plan_->GetOrderBy());
  std::vector<Tuple> tuples;
  std::vector<RID> rids;
  
  while (child_executor_->Next(&tuples, &rids, 1)) {
    for (auto &tuple : tuples) {
      
      auto key = GenerateSortKey(tuple, plan_->GetOrderBy(), GetOutputSchema());
      SortEntry entry{std::move(key), std::move(tuple)};
      
      if (top_entries_.size() < plan_->GetN()) {
        
        top_entries_.push_back(std::move(entry));
        std::push_heap(top_entries_.begin(), top_entries_.end(), cmp);
      } else if (cmp(entry, top_entries_.front())) {
        
        std::pop_heap(top_entries_.begin(), top_entries_.end(), cmp);
        top_entries_.back() = std::move(entry);
        std::push_heap(top_entries_.begin(), top_entries_.end(), cmp);
      }
    }
  }
  std::sort_heap(top_entries_.begin(), top_entries_.end(), cmp);
}

/**
 * Yield the next tuple batch from the TopN.
 * @param[out] tuple_batch The next tuple batch produced by the TopN
 * @param[out] rid_batch The next tuple RID batch produced by the TopN
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 */
auto TopNExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch, size_t batch_size)
    -> bool {
  tuple_batch->clear();
  rid_batch->clear();
  
  while (pos_ < top_entries_.size() && tuple_batch->size() < batch_size) {
    tuple_batch->push_back(top_entries_[pos_++].second);
    rid_batch->emplace_back();
  }
  
  return !tuple_batch->empty();
}

auto TopNExecutor::GetNumInHeap() -> size_t { throw NotImplementedException("TopNExecutor is not implemented"); };

}  // namespace bustub
