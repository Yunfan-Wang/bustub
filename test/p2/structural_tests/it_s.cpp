#include <iostream>
#include <memory>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "catalog/schema.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/index/b_plus_tree.h"

using namespace bustub;

int main() {
  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  auto disk_manager = std::make_shared<DiskManagerUnlimitedMemory>();
  auto bpm = std::make_shared<BufferPoolManager>(2, disk_manager.get());
  
  auto page_id = bpm->NewPage();
  GenericKey<8> key;
  //sep here
  {
    auto guard = bpm->WritePage(page_id);
    auto leaf = guard.AsMut<BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>, 2>>();
    leaf->Init(4);
    for (int number : {10, 20, 30}) {
      key.SetFromInteger(number);
      leaf->InsertAt(leaf->GetSize(), key, RID(0, number));
    }
    leaf->DeleteAt(1);
  }

  auto traced_bpm = std::make_shared<TracedBufferPoolManager>(bpm.get());
  IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 2> iterator(traced_bpm, bpm->ReadPage(page_id), 0, INVALID_PAGE_ID, comparator);

  std::cout << "leaf keys: 10 20 30; tbs 20\nvisible keys:";
  while (!iterator.IsEnd()) {
    std::cout << ' ' << (*iterator).first.GetAsInteger() << ':' << (*iterator).second;
    
    ++iterator;
  }
  std::cout << "\npin after scan: " << bpm->GetPinCount(page_id).value_or(0) << '\n';
}
