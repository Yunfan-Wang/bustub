#include <memory>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "execution/executors/seq_scan_executor.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "type/value_factory.h"

namespace bustub {

TEST(SeqScanLocalTest, batch) {
  DiskManagerUnlimitedMemory disk;
  BufferPoolManager bpm(8, &disk);
  
  Catalog catalog(&bpm, nullptr, nullptr);
  ExecutorContext context(nullptr, &catalog, &bpm, nullptr, nullptr, false);
  
  auto schema = std::make_shared<Schema>(std::vector<Column>{Column("id", TypeId::INTEGER)});
  auto table = catalog.CreateTable(nullptr, "scan", *schema);
  auto column = std::make_shared<ColumnValueExpression>(0, 0, schema->GetColumn(0));
  auto constant = std::make_shared<ConstantValueExpression>(ValueFactory::GetIntegerValue(1));
  auto predicate = std::make_shared<ComparisonExpression>(column, constant, ComparisonType::GreaterThan);
  
  
  
  SeqScanPlanNode plan(schema, table->oid_, table->name_, predicate);
  SeqScanExecutor scan(&context, &plan);
  
  std::vector<Tuple> tuples;
  std::vector<RID> rids;

  scan.Init();
  EXPECT_FALSE(scan.Next(&tuples, &rids, 2));

  auto null_rid = table->table_->InsertTuple({0, false}, Tuple({ValueFactory::GetNullValueByType(TypeId::INTEGER)}, schema.get()));
  ASSERT_TRUE(null_rid.has_value());

  std::vector<RID> inserted;

  for (int id = 1; id <= 5; id++) {
    auto rid = table->table_->InsertTuple({0, id == 2}, Tuple({ValueFactory::GetIntegerValue(id)}, schema.get()));
    
    ASSERT_TRUE(rid.has_value());
    inserted.push_back(*rid);
  }

  scan.Init();
  //deleted test1
  ASSERT_TRUE(scan.Next(&tuples, &rids, 2));
  ASSERT_EQ(2, tuples.size());
  EXPECT_EQ(3, tuples[0].GetValue(schema.get(), 0).GetAs<int32_t>());
  EXPECT_EQ(4, tuples[1].GetValue(schema.get(), 0).GetAs<int32_t>());
  EXPECT_EQ((std::vector<RID>{inserted[2], inserted[3]}), rids);

  ASSERT_TRUE(scan.Next(&tuples, &rids, 2));
  ASSERT_EQ(1, tuples.size());
  EXPECT_EQ(5, tuples[0].GetValue(schema.get(), 0).GetAs<int32_t>());
  EXPECT_EQ((std::vector<RID>{inserted[4]}), rids);

  EXPECT_FALSE(scan.Next(&tuples, &rids, 2));
  EXPECT_TRUE(tuples.empty());
  EXPECT_TRUE(rids.empty());

//   EXPECT_FALSE(scan.Next(&tuples, &rids, 1));
//   EXPECT_TRUE(tuples.empty());
//   EXPECT_EQ(6, tuples[0].GetValue(schema.get(), 0).GetAs<int32_t>());
//   EXPECT_TRUE(rids.empty());

  scan.Init();
  ASSERT_TRUE(scan.Next(&tuples, &rids, 1));
  EXPECT_EQ((std::vector<RID>{inserted[2]}), rids);
}

}
