#include <array>
#include <cstring>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/disk/disk_scheduler.h"

namespace bustub {

TEST(P1Disk, drain32) {
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  std::vector<std::array<char, BUSTUB_PAGE_SIZE>> data(32);
  std::vector<std::future<bool>> futures;
  {
    auto disk_scheduler = std::make_unique<DiskScheduler>(disk_manager.get());
    std::vector<DiskRequest> requests;
    for (int i = 0; i < 32; ++i) {
      data[i].fill(static_cast<char>(i + 1));
      auto promise = disk_scheduler->CreatePromise();
      futures.push_back(promise.get_future());
      requests.push_back({true, data[i].data(), i, std::move(promise)});
    }
    disk_scheduler->Schedule(requests);
  }
  //??
  for (auto &completion : futures) {
    EXPECT_TRUE(completion.get());
  }
  for (int i = 0; i < 32; ++i) {
    std::array<char, BUSTUB_PAGE_SIZE> out{};
    disk_manager->ReadPage(i, out.data());
    EXPECT_EQ(out, data[i]);
  }
}

}
