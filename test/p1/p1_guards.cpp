#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <thread>
#include <utility>

#include "buffer/buffer_pool_manager.h"
#include "gtest/gtest.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/page/page_guard.h"

namespace bustub {

TEST(P1Guards, rw) {
  using namespace std::chrono_literals;
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(2, disk_manager.get());
  auto id = bpm->NewPage();
  auto reader = bpm->ReadPage(id);

  std::promise<void> reader_acquired, release_reader;
  auto acquired = reader_acquired.get_future();
  auto release = release_reader.get_future();
  std::thread other_reader([&] {
    auto guard = bpm->ReadPage(id);
    reader_acquired.set_value();
    release.wait();
  });
  EXPECT_EQ(std::future_status::ready, acquired.wait_for(2s));
  std::promise<void> started, written;
  auto writer_started = started.get_future();
  auto write_finished = written.get_future();
  std::thread writer([&] {
    started.set_value();
    auto guard = bpm->WritePage(id);
    std::strcpy(guard.GetDataMut(), "done");
    written.set_value();
  });
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
//   
  writer_started.wait();
  EXPECT_EQ(std::future_status::timeout, write_finished.wait_for(50ms));
  reader.Drop();
  EXPECT_EQ(std::future_status::timeout, write_finished.wait_for(50ms));
  release_reader.set_value();
  other_reader.join();
  EXPECT_EQ(std::future_status::ready, write_finished.wait_for(2s));
  writer.join();
  
  {
    auto guard = bpm->ReadPage(id);
    EXPECT_STREQ(guard.GetData(), "done");
  }
  EXPECT_EQ(0, bpm->GetPinCount(id));
}

}
