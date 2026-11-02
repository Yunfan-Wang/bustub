#include <memory>

#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/index/b_plus_tree.h"
#include "storage/index/index_iterator.h"

namespace bustub {

TEST(P2Iterator, lb) {
  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(32, disk_manager.get());
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 0> tree("test", bpm->NewPage(), bpm.get(), comparator, 3, 3);
  GenericKey<8> key;
  
  EXPECT_TRUE(tree.Begin().IsEnd());
  for (int number = 0; number <= 100; number += 10) {
    key.SetFromInteger(number);
    ASSERT_TRUE(tree.Insert(key, RID(0, number)));
  }
  
  for (int query = -1; query <= 101; ++query) {
    key.SetFromInteger(query);
    auto iterator = tree.Begin(key);
    if (query > 100) {
      EXPECT_TRUE(iterator.IsEnd());
    } else {
      int expected = query <= 0 ? 0 : ((query + 9) / 10) * 10;
      ASSERT_FALSE(iterator.IsEnd());
      EXPECT_EQ(expected, (*iterator).first.GetAsInteger());
      for (int number = expected; number <= 100; number += 10) {
        ASSERT_FALSE(iterator.IsEnd());
        EXPECT_EQ(number, (*iterator).first.GetAsInteger());
        ++iterator;
      }
      EXPECT_TRUE(iterator.IsEnd());
    }
  }
}

}
