#include <memory>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"

#include "execution/executors/insert_executor.h"
#include "execution/executors/values_executor.h"

#include "execution/expressions/constant_value_expression.h"
#include "gtest/gtest.h"

#include "storage/disk/disk_manager_memory.h"
#include "type/value_factory.h"

namespace bustub {

TEST(InsertLocalTest, index3) {
  DiskManagerUnlimitedMemory disk;
  BufferPoolManager bpm(8, &disk);
  
  Catalog catalog(&bpm, nullptr, nullptr);
  ExecutorContext context(nullptr, &catalog, &bpm, nullptr, nullptr, false);
  
  auto schema = std::make_shared<Schema>(std::vector<Column>{Column("id", TypeId::INTEGER)});
  auto count_schema = std::make_shared<Schema>(std::vector<Column>{Column("count", TypeId::INTEGER)});
  auto table = catalog.CreateTable(nullptr, "items", *schema);
  auto index = catalog.CreateIndex<GenericKey<4>, RID, GenericComparator<4>>(
      nullptr, "items_id", "items", *schema, *schema, {0}, 4, HashFunction<GenericKey<4>>{});
  ASSERT_NE(nullptr, index);

  std::vector<std::vector<AbstractExpressionRef>> rows;
  for (int id : {1, 2, 3}) {
    rows.push_back({std::make_shared<ConstantValueExpression>(ValueFactory::GetIntegerValue(id))});
  }
  
  
  // don't need more tests.
  auto values = std::make_shared<ValuesPlanNode>(schema, rows);
  InsertPlanNode plan(count_schema, values, table->oid_);
  InsertExecutor insert(&context, &plan, std::make_unique<ValuesExecutor>(&context, values.get()));

  std::vector<Tuple> tuples;
  std::vector<RID> rids;
  
  insert.Init();
  ASSERT_TRUE(insert.Next(&tuples, &rids, 2));
  //value test v1
  ASSERT_EQ(1, tuples.size());
  EXPECT_EQ(3, tuples[0].GetValue(count_schema.get(), 0).GetAs<int32_t>());
  EXPECT_FALSE(insert.Next(&tuples, &rids, 2));

  for (int id : {1, 2, 3}) {
    Tuple key({ValueFactory::GetIntegerValue(id)}, schema.get());
    index->index_->ScanKey(key, &rids, nullptr);
    
    ASSERT_EQ(1, rids.size());
    auto [meta, tuple] = table->table_->GetTuple(rids[0]);
    EXPECT_FALSE(meta.is_deleted_);
    EXPECT_EQ(id, tuple.GetValue(schema.get(), 0).GetAs<int32_t>());
    
    rids.clear();
  }
}

}
