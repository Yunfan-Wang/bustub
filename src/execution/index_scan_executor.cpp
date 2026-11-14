//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.cpp
//
// Identification: src/execution/index_scan_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/index_scan_executor.h"
#include "common/macros.h"

#include <unordered_set>
#include "type/limits.h"
#include "type/value_factory.h"
#include "storage/index/b_plus_tree_index.h"


namespace bustub {
namespace {

template <size_t N>
void ScanTree(IndexInfo &info, const std::vector<Value> &prefix, std::vector<RID> *rids) {
  using Tree = BPlusTreeIndex<GenericKey<N>, RID, GenericComparator<N>>;
  auto *tree = static_cast<Tree *>(info.index_.get());
  auto it = tree->GetEndIterator();
  if (prefix.empty()) {
    it = tree->GetBeginIterator();
  } else {
    auto values = prefix;
    values.resize(info.key_schema_.GetColumnCount(), ValueFactory::GetIntegerValue(BUSTUB_INT32_MIN));
    GenericKey<N> lower;
    lower.SetFromKey(Tuple(values, &info.key_schema_));
    it = tree->GetBeginIterator(lower);
  }

  for (; !it.IsEnd(); ++it) {
    const auto [key, rid] = *it;
    for (size_t i = 0; i < prefix.size(); i++) {
      if (key.ToValue(&info.key_schema_, i).CompareEquals(prefix[i]) != CmpBool::CmpTrue) {
        return;
      }
    }
    rids->push_back(rid);
  }
}

}
/**
 * Creates a new index scan executor.
 * @param exec_ctx the executor context
 * @param plan the index scan plan to be executed
 */
IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {
}

void IndexScanExecutor::Init() {
  next_ = 0;
  rids_.clear();
  auto *catalog = exec_ctx_->GetCatalog();
  table_ = catalog->GetTable(plan_->table_oid_);
  auto index = catalog->GetIndex(plan_->GetIndexOid());
  std::vector<Value> keys;
  for (const auto &expr : plan_->pred_keys_) {
    keys.push_back(expr->Evaluate(nullptr, GetOutputSchema()));
  }
  const auto columns = index->key_schema_.GetColumnCount();
  if (!keys.empty() && columns == 1) {
    // if bugs, check here
    std::unordered_set<RID> seen;
    for (const auto &key : keys) {
      if (key.IsNull()) {
        continue;
      }
      
      std::vector<RID> matches;
      index->index_->ScanKey(Tuple({key}, &index->key_schema_), &matches, exec_ctx_->GetTransaction());
      for (const auto &rid : matches) {
        if (seen.insert(rid).second) {
          rids_.push_back(rid);
        }
      }
    }
    return;
  }
  for (const auto &key : keys) {
    if (key.IsNull()) {
      return;
    }
  }
  if (keys.size() == columns) {
    index->index_->ScanKey(Tuple(keys, &index->key_schema_), &rids_, exec_ctx_->GetTransaction());
    return;
  }
  switch (index->key_size_) {
    case 4:
      ScanTree<4>(*index, keys, &rids_);
      break;
    case 8:
      ScanTree<8>(*index, keys, &rids_);
      break;
    case 16:
      ScanTree<16>(*index, keys, &rids_);
      break;
    case 32:
      ScanTree<32>(*index, keys, &rids_);
      break;
    case 64:
      ScanTree<64>(*index, keys, &rids_);
      break;
    default:
      BUSTUB_ENSURE(false, "unsupported index key size");
  }
}

auto IndexScanExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                             size_t batch_size) -> bool {
  tuple_batch->clear();
  rid_batch->clear();
  while (next_ < rids_.size() && tuple_batch->size() < batch_size) {
    const auto rid = rids_[next_++];
    
    auto [meta, tuple] = table_->table_->GetTuple(rid);
    if (meta.is_deleted_) {
      continue;
    }
    //update value
    if (plan_->filter_predicate_ != nullptr) {
      auto value = plan_->filter_predicate_->Evaluate(&tuple, GetOutputSchema());
      if (value.IsNull() || !value.GetAs<bool>()) {
        continue;
      }
    }
    tuple_batch->push_back(std::move(tuple));
    rid_batch->push_back(rid);
  }
  return !tuple_batch->empty();
}

}  // namespace bustub
