//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// delete_executor.cpp
//
// Identification: src/execution/delete_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>
#include "common/macros.h"

#include "execution/executors/delete_executor.h"

#include <utility>
#include "type/value_factory.h"

namespace bustub {

/**
 * Construct a new DeleteExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The delete plan to be executed
 * @param child_executor The child executor that feeds the delete
 */
DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {
}

/** Initialize the delete */
void DeleteExecutor::Init() { 
  child_executor_->Init();
  finished_ = false;
}

/**
 * Yield the number of rows deleted from the table.
 * @param[out] tuple_batch The tuple batch with one integer indicating the number of rows deleted from the table
 * @param[out] rid_batch The next tuple RID batch produced by the delete (ignore, not used)
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 *
 * NOTE: DeleteExecutor::Next() does not use the `rid_batch` out-parameter.
 * NOTE: DeleteExecutor::Next() returns true with the number of deleted rows produced only once.
 */
auto DeleteExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
                          size_t batch_size) -> bool {
  tuple_batch->clear();
  
  rid_batch->clear();
  if (finished_ || batch_size == 0) {
    return false;
  }

  auto catalog = exec_ctx_->GetCatalog();
  auto table = catalog->GetTable(plan_->GetTableOid());
  auto indexes = catalog->GetTableIndexes(table->name_);
  auto txn = exec_ctx_->GetTransaction();
  
  std::vector<Tuple> tuples;
  std::vector<RID> rids;
  int32_t affected = 0;

  while (child_executor_->Next(&tuples, &rids, batch_size)) {
    // this section to be tested for performance later.
    for (const auto &rid : rids) {
      auto [meta, tuple] = table->table_->GetTuple(rid);
      if (meta.is_deleted_) {
        continue;
      }

      table->table_->UpdateTupleMeta({meta.ts_, true}, rid);
      for (const auto &index : indexes) {

        auto k = tuple.KeyFromTuple(table->schema_, index->key_schema_, index->index_->GetKeyAttrs());
        index->index_->DeleteEntry(k, rid, txn);
      }
      affected++;
    }
  }

  tuple_batch->emplace_back(std::vector<Value>{ValueFactory::GetIntegerValue(affected)}, &GetOutputSchema());
  finished_ = true;
  return true;
}

}  // namespace bustub
