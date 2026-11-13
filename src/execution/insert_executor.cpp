//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// insert_executor.cpp
//
// Identification: src/execution/insert_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>
#include "common/macros.h"

#include "execution/executors/insert_executor.h"
#include <utility>

#include "type/value_factory.h"

namespace bustub {

/**
 * Construct a new InsertExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The insert plan to be executed
 * @param child_executor The child executor from which inserted tuples are pulled
 */
InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {
}

/** Initialize the insert */
void InsertExecutor::Init() {
  child_executor_->Init();
  finished_ = false;
}

/**
 * Yield the number of rows inserted into the table.
 * @param[out] tuple_batch The tuple batch with one integer indicating the number of rows inserted into the table
 * @param[out] rid_batch The next tuple RID batch produced by the insert (ignore, not used)
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 *
 * NOTE: InsertExecutor::Next() does not use the `rid_batch` out-parameter.
 * NOTE: InsertExecutor::Next() returns true with the number of inserted rows produced only once.
 */
auto InsertExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
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
  int32_t inserted = 0;

  while (child_executor_->Next(&tuples, &rids, batch_size)) {
    for (const auto &tuple : tuples) {
      auto rid = table->table_->InsertTuple({0, false}, tuple, exec_ctx_->GetLockManager(), txn, table->oid_);
      if (!rid.has_value()) {
        continue;
      }

      for (const auto &index : indexes) {
        auto key = tuple.KeyFromTuple(table->schema_, index->key_schema_, index->index_->GetKeyAttrs());
        index->index_->InsertEntry(key, *rid, txn);
      }
      inserted++;
    }
  }

  tuple_batch->emplace_back(std::vector<Value>{ValueFactory::GetIntegerValue(inserted)}, &GetOutputSchema());
  finished_ = true;
  return true;
}

}  // namespace bustub
