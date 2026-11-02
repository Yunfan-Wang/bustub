#include <memory>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/index/b_plus_tree.h"

namespace bustub {

TEST(P2Tombstone, reuse80) {
  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(32, disk_manager.get());
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 1> tree("test", bpm->NewPage(), bpm.get(), comparator, 4, 3);
  GenericKey<8> key;
  for (int number = 0; number < 80; ++number) {
    key.SetFromInteger(number);
    ASSERT_TRUE(tree.Insert(key, RID(0, number)));
  }
  for (int number = 0; number < 72; ++number) {
    key.SetFromInteger(number);
    tree.Remove(key);
  }
  {
    auto iterator = tree.Begin();
    for (int number = 72; number < 80; ++number) {
      ASSERT_FALSE(iterator.IsEnd());
      EXPECT_EQ(number, (*iterator).first.GetAsInteger());
      ++iterator;
    }
    EXPECT_TRUE(iterator.IsEnd());
  }
  for (int number = 0; number < 72; ++number) {
    key.SetFromInteger(number);
    std::vector<RID> found;
    EXPECT_FALSE(tree.GetValue(key, &found));
    ASSERT_TRUE(tree.Insert(key, RID(1, number)));
  }
  for (int number = 0; number < 80; ++number) {
    key.SetFromInteger(number);
    std::vector<RID> found;
    ASSERT_TRUE(tree.GetValue(key, &found));
    ASSERT_EQ(1, found.size());
    EXPECT_EQ(RID(number < 72 ? 1 : 0, number), found[0]);
    tree.Remove(key);
  }
  EXPECT_TRUE(tree.Begin().IsEnd());
}

}
