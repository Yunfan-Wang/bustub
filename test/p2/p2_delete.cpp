#include <map>
#include <memory>
#include <random>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/index/b_plus_tree.h"

namespace bustub {

TEST(P2Delete, mix2500) {
  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(32, disk_manager.get());
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 0> tree("test", bpm->NewPage(), bpm.get(), comparator, 4, 3);
  GenericKey<8> key;
  std::map<int, RID> expected;
  std::mt19937 random(2026);
  
  for (int step = 0; step < 2500; ++step) {
    int number = static_cast<int>(random() % 300) - 150;
    key.SetFromInteger(number);
    if (random() % 2 == 0) {
      RID value(0, step);
      bool inserted = expected.emplace(number, value).second;
      ASSERT_EQ(inserted, tree.Insert(key, value));
    } else {
      tree.Remove(key);
      expected.erase(number);
    }
    std::vector<RID> found;
    ASSERT_EQ(expected.count(number) != 0, tree.GetValue(key, &found));
    if (!found.empty()) {
      ASSERT_EQ(1, found.size());
      EXPECT_EQ(expected.at(number), found[0]);
    }
    
    if (step % 50 == 0) {
      auto iterator = tree.Begin();
      for (const auto &entry : expected) {
        ASSERT_FALSE(iterator.IsEnd());
        EXPECT_EQ(entry.first, (*iterator).first.GetAsInteger());
        EXPECT_EQ(entry.second, (*iterator).second);
        ++iterator;
      }
      EXPECT_TRUE(iterator.IsEnd());
    }
  }
  for (const auto &entry : expected) {
    key.SetFromInteger(entry.first);
    tree.Remove(key);
  }

  EXPECT_TRUE(tree.IsEmpty());
  EXPECT_TRUE(tree.Begin().IsEnd());
  EXPECT_EQ(INVALID_PAGE_ID, tree.GetRootPageId());
}

}
