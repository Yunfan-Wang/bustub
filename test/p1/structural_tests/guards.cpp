#include <iostream>
#include <utility>

#include "storage/page/page_guard.h"
using namespace bustub;

int main() {
  ReadPageGuard read;
  WritePageGuard write;
  std::cout << "default guards\n";
  ReadPageGuard moved_read(std::move(read));
  WritePageGuard moved_write(std::move(write));
  std::cout << "move construction finished\n";
  
  read.Drop();
  write.Drop();
  moved_read.Drop();
  moved_write.Drop();
  std::cout << "dropping mv from and inv guards finished\n";
  read = std::move(moved_read);
  write = std::move(moved_write);
  
  
  read.Drop();
  write.Drop();
  std::cout << "move ass and repeat drops good\n";
}
