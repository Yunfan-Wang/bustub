#include <iostream>
#include "storage/page/b_plus_tree_leaf_page.h"

using namespace bustub;

int main() {
  using LeafPage = BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>, 2>;
  alignas(LeafPage) char data[BUSTUB_PAGE_SIZE] = {0};
  auto leaf = reinterpret_cast<LeafPage *>(data);
  leaf->Init(8);
  GenericKey<8> key;
  for (int number : {10, 20, 30, 40}) {
    key.SetFromInteger(number);
    leaf->InsertAt(leaf->GetSize(), key, RID(0, number));
  }
  std::cout << "initial: " << leaf->ToString() << '\n';
  leaf->DeleteAt(0);
  leaf->DeleteAt(2);
  std::cout << "del 10, 30: " << leaf->ToString() << '\n';
  leaf->DeleteAt(1);
  std::cout << "del 20, flush oldest: " << leaf->ToString() << '\n';

  leaf->Revive(0, RID(1, 20));
  std::cout << "revive 20: " << leaf->ToString() << '\n';
  std::cout << "revived RID: " << leaf->ValueAt(0) << '\n';
}
