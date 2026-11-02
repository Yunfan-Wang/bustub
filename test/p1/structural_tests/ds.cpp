#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "storage/disk/disk_scheduler.h"

using namespace bustub;

class SchedulerTestDisk : public DiskManager {
 public:
  char data[BUSTUB_PAGE_SIZE] = {0};
  void ReadPage(page_id_t, char *output) override {
    std::memcpy(output, data, BUSTUB_PAGE_SIZE);
    std::cout << "disk read\n";
  }
  void WritePage(page_id_t, const char *input) override {
    std::memcpy(data, input, BUSTUB_PAGE_SIZE);
    std::cout << "disk write\n";
  }
};

int main() {
  auto disk_manager = std::make_unique<SchedulerTestDisk>();
  auto disk_scheduler = std::make_unique<DiskScheduler>(disk_manager.get());
  char data[BUSTUB_PAGE_SIZE] = "injected page";
  char output[BUSTUB_PAGE_SIZE] = {0};
  auto promise1 = disk_scheduler->CreatePromise();
  auto future1 = promise1.get_future();
  auto promise2 = disk_scheduler->CreatePromise();
  auto future2 = promise2.get_future();
  std::vector<DiskRequest> requests;
  requests.push_back({true, data, 0, std::move(promise1)});
  requests.push_back({false, output, 0, std::move(promise2)});



  // Scheduler
  disk_scheduler->Schedule(requests);
  bool written = future1.get();
  bool read = future2.get();
  std::cout << "wr completed: " << written << '/' << read << '\n';
  std::cout << "input: " << data << "\noutput: " << output << '\n';
}
