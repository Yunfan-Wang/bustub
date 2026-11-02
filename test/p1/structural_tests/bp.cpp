#include <cstring>
#include <iostream>
#include <memory>

#include "buffer/buffer_pool_manager.h"

using namespace bustub;

class BpmTD : public DiskManager {
 public:
  int reads = 0;
  int writes = 0;
  void ReadPage(page_id_t page, char *data) override {
    ++reads;
    std::memset(data, 0, BUSTUB_PAGE_SIZE);
    std::cout << "disk read: " << page << '\n';
  }
  void WritePage(page_id_t, const char *) override { ++writes; }
};

int main() {
  auto disk_manager = std::make_shared<BpmTD>();
  auto bpm = std::make_shared<BufferPoolManager>(2, disk_manager.get());
  auto first = bpm->NewPage();
  auto second = bpm->NewPage();
  std::cout << "new page IDs: " << first << ' ' << second << '\n';
  std::cout << "resident before read: " << bpm->GetPinCount(first).has_value() << '\n';
  std::cout << "disk reads before fetch: " << disk_manager->reads << '\n';
  {
    auto guard = bpm->ReadPage(first);
    std::cout << "pin while guarded: " << bpm->GetPinCount(first).value_or(0) << '\n';
    std::cout << "first 8 bytes:";
    for (int i = 0; i < 8; i++) {
      std::cout << ' ' << static_cast<int>(guard.GetData()[i]);
    }
    std::cout << '\n';
  }
  std::cout << "pin after scope: " << bpm->GetPinCount(first).value_or(0) << '\n';
  std::cout << "disk reads/writes: " << disk_manager->reads << '/' << disk_manager->writes << '\n';
}
