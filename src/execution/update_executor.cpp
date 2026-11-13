//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// update_executor.cpp
//
// Identification: src/execution/update_executor.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <memory>
#include "common/macros.h"

#include "execution/executors/update_executor.h"
#include "type/value_factory.h"

namespace bustub {

/**
 * Construct a new UpdateExecutor instance.
 * @param exec_ctx The executor context
 * @param plan The update plan to be executed
 * @param child_executor The child executor that feeds the update
 */
UpdateExecutor::UpdateExecutor(ExecutorContext *exec_ctx, const UpdatePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {
}

/** Initialize the update */
void UpdateExecutor::Init() {
  child_executor_->Init();
  finished_ = false;
}

/**
 * Yield the number of rows updated in the table.
 * @param[out] tuple_batch The tuple batch with one integer indicating the number of rows updated in the table
 * @param[out] rid_batch The next tuple RID batch produced by the update (ignore, not used)
 * @param batch_size The number of tuples to be included in the batch (default: BUSTUB_BATCH_SIZE)
 * @return `true` if a tuple was produced, `false` if there are no more tuples
 *
 * NOTE: UpdateExecutor::Next() does not use the `rid_batch` out-parameter.
 * NOTE: UpdateExecutor::Next() returns true with the number of updated rows produced only once.
 */
auto UpdateExecutor::Next(std::vector<bustub::Tuple> *tuple_batch, std::vector<bustub::RID> *rid_batch,
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
    for (const auto &rid : rids) {
      auto [meta, tuple] = table->table_->GetTuple(rid);
      if (meta.is_deleted_) {
        continue;
      }

      std::vector<Value> values;
      for (const auto &expr : plan_->target_expressions_) {
        values.push_back(expr->Evaluate(&tuple, table->schema_));
      }
      Tuple replacement(values, &table->schema_);
      table->table_->UpdateTupleMeta({meta.ts_, true}, rid);
      auto nrid = table->table_->InsertTuple({0, false}, replacement, exec_ctx_->GetLockManager(), txn, table->oid_);
      if (!nrid.has_value()) {
        table->table_->UpdateTupleMeta(meta, rid);
        continue;
      }

      for (const auto &index : indexes) {
        const auto &attrs = index->index_->GetKeyAttrs();
        auto okey = tuple.KeyFromTuple(table->schema_, index->key_schema_, attrs);
        auto nkey = replacement.KeyFromTuple(table->schema_, index->key_schema_, attrs);
        
        index->index_->DeleteEntry(okey, rid, txn);
        index->index_->InsertEntry(nkey, *nrid, txn);
      }
      affected++;
    }
  }

  tuple_batch->emplace_back(std::vector<Value>{ValueFactory::GetIntegerValue(affected)}, &GetOutputSchema());
  finished_ = true;
  return true;
}

}  // namespace bustub
