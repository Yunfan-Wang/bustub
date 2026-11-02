#include <iostream>
#include "catalog/schema.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"

using namespace bustub;

int main() {
  using LeafPage = BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>;
  using InternalPage = BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
  alignas(LeafPage) char leaf_data[BUSTUB_PAGE_SIZE] = {0};
  auto leaf = reinterpret_cast<LeafPage *>(leaf_data);
  leaf->Init(4);
  
  std::cout << "leaf size/max/min: " << leaf->GetSize() << '/' << leaf->GetMaxSize() << '/' << leaf->GetMinSize() << '\n';
  GenericKey<8> key;
  for (int number : {10, 30}) {
    key.SetFromInteger(number);
    leaf->InsertAt(leaf->GetSize(), key, RID(0, number));
  }

  key.SetFromInteger(20);
  leaf->InsertAt(1, key, RID(0, 20));
  //bruh1
  std::cout << "ins 20 bet 10 and 30: " << leaf->ToString() << '\n';
  for (int i = 0; i < leaf->GetSize(); i++) {
    std::cout << leaf->KeyAt(i).GetAsInteger() << ':' << leaf->ValueAt(i) << '\n';
  }
  leaf->SetNextPageId(7);
  std::cout << "next leaf: " << leaf->GetNextPageId() << '\n';

  alignas(InternalPage) char internal_data[BUSTUB_PAGE_SIZE] = {0};
  auto internal = reinterpret_cast<InternalPage *>(internal_data);
  internal->Init(4);
  Schema schema({Column("a", TypeId::BIGINT)});
  GenericComparator<8> comparator(&schema);
  
  key.SetFromInteger(999);
  internal->InsertAt(0, key, 10);
  key.SetFromInteger(20);
  internal->InsertAt(1, key, 11);
  key.SetFromInteger(40);
  internal->InsertAt(2, key, 12);
  std::cout << "internal seps: " << internal->ToString() << '\n';
  
  for (int query : {0, 19, 20, 39, 40, 99}) {
    key.SetFromInteger(query);
    int index = internal->Lookup(key, comparator);
    std::cout << "query " << query << " -> slot " << index << " c " << internal->ValueAt(index) << '\n';
  }

  internal->RemoveAt(1);
  
  std::cout << "remove c 11: " << internal->ToString() << '\n';
  std::cout << "cs: " << internal->ValueAt(0) << ' ' << internal->ValueAt(1) << '\n';
}
