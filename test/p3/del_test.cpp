#include <memory>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "execution/executors/delete_executor.h"
#include "execution/executors/seq_scan_executor.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "type/value_factory.h"

namespace bustub {
// logically speaking, del should be cascading
TEST(DeleteLocalTest, index) {
  DiskManagerUnlimitedMemory disk;
  BufferPoolManager bpm(8, &disk);
  Catalog catalog(&bpm, nullptr, nullptr);
  ExecutorContext context(nullptr, &catalog, &bpm, nullptr, nullptr, false);
  
  auto schema = std::make_shared<Schema>(std::vector<Column>{Column("id", TypeId::INTEGER)});
  auto count_schema = std::make_shared<Schema>(std::vector<Column>{Column("count", TypeId::INTEGER)});
  auto table = catalog.CreateTable(nullptr, "items", *schema);
  Tuple old_key({ValueFactory::GetIntegerValue(1)}, schema.get());
  Tuple other_key({ValueFactory::GetIntegerValue(2)}, schema.get());
  auto old_rid = table->table_->InsertTuple({0, false}, old_key);
  auto other_rid = table->table_->InsertTuple({0, false}, other_key);
  ASSERT_TRUE(old_rid.has_value());
  ASSERT_TRUE(other_rid.has_value());
  auto index = catalog.CreateIndex<GenericKey<4>, RID, GenericComparator<4>>(
      nullptr, "items_id", "items", *schema, *schema, {0}, 4, HashFunction<GenericKey<4>>{});
  ASSERT_NE(nullptr, index);

  auto column = std::make_shared<ColumnValueExpression>(0, 0, schema->GetColumn(0));
  auto one = std::make_shared<ConstantValueExpression>(ValueFactory::GetIntegerValue(1));
  auto predicate = std::make_shared<ComparisonExpression>(column, one, ComparisonType::Equal);
  auto scan = std::make_shared<SeqScanPlanNode>(schema, table->oid_, table->name_, predicate);
  
  DeletePlanNode plan(count_schema, scan, table->oid_);
  DeleteExecutor executor(&context, &plan, std::make_unique<SeqScanExecutor>(&context, scan.get()));

  std::vector<Tuple> tuples;
  std::vector<RID> rids;
  executor.Init();
  ASSERT_TRUE(executor.Next(&tuples, &rids, 1));
  ASSERT_EQ(1, tuples.size());
  EXPECT_EQ(1, tuples[0].GetValue(count_schema.get(), 0).GetAs<int32_t>());
  EXPECT_FALSE(executor.Next(&tuples, &rids, 1));

  EXPECT_TRUE(table->table_->GetTupleMeta(*old_rid).is_deleted_);
  index->index_->ScanKey(old_key, &rids, nullptr);
  EXPECT_TRUE(rids.empty());
  index->index_->ScanKey(other_key, &rids, nullptr);
  EXPECT_EQ(std::vector<RID>{*other_rid}, rids);
}

}
