#include <iostream>

#include "buffer/arc_replacer.h"

using namespace bustub;

int main() {
  ArcReplacer arc(3);
  std::cout << "empty size: " << arc.Size() << '\n';
  std::cout << "empty victim: " << arc.Evict().value_or(-1) << '\n';
  arc.RecordAccess(0, 10);
  
  std::cout << "pinned size: " << arc.Size() << '\n';
  arc.SetEvictable(0, true);
  arc.SetEvictable(0, true);
  std::cout << "evictable size: " << arc.Size() << '\n';
  arc.SetEvictable(0, false);

  std::cout << "pinned victim:" << arc.Evict().value_or(-1) << '\n';
  arc.SetEvictable(0, true);
  std::cout << "victim: " << arc.Evict().value_or(-1) << '\n';
  arc.RecordAccess(0, 20);
  arc.SetEvictable(0, true);
  arc.Remove(0);
  arc.Remove(0);

  std::cout << "size after remove 2: " << arc.Size() << '\n';
}
