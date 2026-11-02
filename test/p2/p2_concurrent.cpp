#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/index/b_plus_tree.h"

namespace bustub {


TEST(P2Concurrent, dup4) {

  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(32, disk_manager.get());
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("test", bpm->NewPage(), bpm.get(), comparator, 4, 3);
  std::promise<void> start;
  
  
  auto begin = start.get_future().share();
  std::atomic<int> inserted{0};
  std::vector<std::thread> threads;
  for (int worker = 0; worker < 4; ++worker) {
    threads.emplace_back([&] {
      begin.wait();
      for (int number = 0; number < 100; ++number) {
        GenericKey<8> key;
        key.SetFromInteger(number);
        if (tree.Insert(key, RID(0, number))) {
          inserted.fetch_add(1);
        }
      }
    });
  }
  
  start.set_value();
  for (auto &thread : threads) {
    thread.join();
  }
  EXPECT_EQ(100, inserted.load());
  auto iterator = tree.Begin();
  for (int number = 0; number < 100; ++number) {
    ASSERT_FALSE(iterator.IsEnd());
    EXPECT_EQ(number, (*iterator).first.GetAsInteger());
    EXPECT_EQ(RID(0, number), (*iterator).second);
    ++iterator;
  }
  EXPECT_TRUE(iterator.IsEnd());
}

TEST(P2Concurrent, mix4) {
  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(32, disk_manager.get());
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("test", bpm->NewPage(), bpm.get(), comparator, 4, 3);
  for (int number = 0; number < 200; ++number) {
    GenericKey<8> key;
    key.SetFromInteger(number);
    ASSERT_TRUE(tree.Insert(key, RID(0, number)));
  }
  std::promise<void> start;
  auto begin = start.get_future().share();
  std::vector<std::thread> threads;
  for (int worker = 0; worker < 2; ++worker) {
    threads.emplace_back([&, worker] {
      begin.wait();
      for (int round = 0; round < 5; ++round) {
        for (int number = worker * 2 + 1; number < 200; number += 4) {
          GenericKey<8> key;
          key.SetFromInteger(number);
          tree.Remove(key);
          EXPECT_TRUE(tree.Insert(key, RID(0, number)));
        }
      }
    });
  }
  for (int reader = 0; reader < 2; ++reader) {
    threads.emplace_back([&] {
      begin.wait();
      for (int round = 0; round < 5; ++round) {
        for (int number = 0; number < 200; number += 2) {
          GenericKey<8> key;
          key.SetFromInteger(number);
          std::vector<RID> found;
          ASSERT_TRUE(tree.GetValue(key, &found));
          ASSERT_EQ(1, found.size());
          EXPECT_EQ(RID(0, number), found[0]);
        }
      }
    });
  }
  start.set_value();
  for (auto &thread : threads) {
    thread.join();
  }
  auto iterator = tree.Begin();
  for (int number = 0; number < 200; ++number) {
    ASSERT_FALSE(iterator.IsEnd());
    EXPECT_EQ(number, (*iterator).first.GetAsInteger());
    EXPECT_EQ(RID(0, number), (*iterator).second);
    ++iterator;
  }
  EXPECT_TRUE(iterator.IsEnd());
}

}
