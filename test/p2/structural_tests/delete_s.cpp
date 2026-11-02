#include <iostream>
#include "storage/page/b_plus_tree_leaf_page.h"
using namespace bustub;

int main() {
  using LeafPage = BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>;
  alignas(LeafPage) char data[BUSTUB_PAGE_SIZE] = {0};
  auto leaf = reinterpret_cast<LeafPage *>(data);
  leaf->Init(4);
  GenericKey<8> key;
  for (int number : {10, 20, 30}) {
    key.SetFromInteger(number);
    leaf->InsertAt(leaf->GetSize(), key, RID(0, number));
  }
  std::cout << "before del: " << leaf->ToString() << '\n';
  leaf->DeleteAt(1);
  std::cout << "after del index 1: " << leaf->ToString() << '\n';
  for (int i = 0; i < leaf->GetSize(); i++) {
    std::cout << leaf->KeyAt(i).GetAsInteger() << ':' << leaf->ValueAt(i) << '\n';
  }
  leaf->DeleteAt(0);
  leaf->DeleteAt(0);
  std::cout << "size after del rem entries: " << leaf->GetSize() << '\n';
}
 