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
  auto bpm = std::make_shared<BufferPoolManager>(8, disk_manager.get());
  auto header_id = bpm->NewPage();
  auto root_id = bpm->NewPage();
  page_id_t leaves[] = {bpm->NewPage(), bpm->NewPage()};
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("test", header_id, bpm.get(), comparator, 4, 3);
  GenericKey<8> key;

  for (int side = 0; side < 2; side++) {
    auto guard = bpm->WritePage(leaves[side]);
    auto leaf = guard.AsMut<BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>>();
    leaf->Init(4);
    for (int index = 0; index < 2; index++) {
      int number = 10 + side * 20 + index * 10;
      key.SetFromInteger(number);
      leaf->InsertAt(index, key, RID(0, number));
    }
    leaf->SetNextPageId(side == 0 ? leaves[1] : INVALID_PAGE_ID);
  }
  {
    auto guard = bpm->WritePage(root_id);
    auto root = guard.AsMut<BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>>();
    root->Init(3);
    key.SetFromInteger(999);
    root->InsertAt(0, key, leaves[0]);
    key.SetFromInteger(30);
    root->InsertAt(1, key, leaves[1]);
  }
  {
    auto guard = bpm->WritePage(header_id);
    guard.AsMut<BPlusTreeHeaderPage>()->root_page_id_ = root_id;
  }

  //->
  std::cout << "root sep: 30; l keys: 10 20; r keys: 30 40\n";
  for (int number : {0, 10, 20, 25, 30, 40, 50}) {
    key.SetFromInteger(number);
    std::vector<RID> found;
    bool exists = tree.GetValue(key, &found);
    std::cout << "lookup " << number << ": " << exists;
    for (const auto &rid : found) {
      std::cout << ' ' << rid;
    }
    std::cout << '\n';
  }
}
