#include <array>
#include <cstring>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/page/page_guard.h"

namespace bustub {

TEST(P1Bpm, evict20) {
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(2, disk_manager.get());
  std::vector<page_id_t> ids;
  for (int i = 0; i < 20; ++i) {
    auto id = bpm->NewPage();
    ids.push_back(id);
    auto guard = bpm->WritePage(id);
    std::memset(guard.GetDataMut(), i + 1, BUSTUB_PAGE_SIZE);
  }
  
  for (int i = 19; i >= 0; --i) {
    auto guard = bpm->ReadPage(ids[i]);
    for (size_t b = 0; b < BUSTUB_PAGE_SIZE; ++b) {
      ASSERT_EQ(guard.GetData()[b], i + 1) << "page " << i << " byte " << b;
    }
  }
}

}
