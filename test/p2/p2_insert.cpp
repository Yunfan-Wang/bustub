#include <algorithm>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/index/b_plus_tree.h"

namespace bustub {

TEST(P2Insert, split300) {
  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(32, disk_manager.get());
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 0> tree("test", bpm->NewPage(), bpm.get(), comparator, 3, 3);
  GenericKey<8> key;
  std::vector<int> numbers(300);
  std::iota(numbers.begin(), numbers.end(), -150);
  std::mt19937 random(213);
  
  std::shuffle(numbers.begin(), numbers.end(), random);
  for (int number : numbers) {
    key.SetFromInteger(number);
    ASSERT_TRUE(tree.Insert(key, RID(0, number + 1000)));
  }
  for (int number : numbers) {
    key.SetFromInteger(number);
    std::vector<RID> found;
    ASSERT_TRUE(tree.GetValue(key, &found));
    ASSERT_EQ(1, found.size());
    EXPECT_EQ(RID(0, number + 1000), found[0]);
  }
  auto guard = bpm->ReadPage(tree.GetRootPageId());
  EXPECT_FALSE(guard.As<BPlusTreePage>()->IsLeafPage());
}

}
