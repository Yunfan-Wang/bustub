//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree.cpp
//
// Identification: src/storage/index/b_plus_tree.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/index/b_plus_tree.h"
#include "buffer/traced_buffer_pool_manager.h"
#include "storage/index/b_plus_tree_debug.h"

namespace bustub {

FULL_INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : bpm_(std::make_shared<TracedBufferPoolManager>(buffer_pool_manager)),
      index_name_(std::move(name)),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  auto header = bpm_->ReadPage(header_page_id_);
  return header.template As<BPlusTreeHeaderPage>()->root_page_id_ == INVALID_PAGE_ID;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t {
  auto header = bpm_->ReadPage(header_page_id_);
  return header.template As<BPlusTreeHeaderPage>()->root_page_id_;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  auto header = bpm_->ReadPage(header_page_id_);
  auto root = header.template As<BPlusTreeHeaderPage>()->root_page_id_;
  if (root == INVALID_PAGE_ID) {
    return false;
  }
  auto guard = bpm_->ReadPage(root);
  header.Drop();
  while (!guard.template As<BPlusTreePage>()->IsLeafPage()) {
    const auto *page = guard.template As<InternalPage>();
    auto child = bpm_->ReadPage(page->ValueAt(page->Lookup(key, comparator_)));
    guard.Drop();
    guard = std::move(child);
  }
  const auto *leaf = guard.template As<LeafPage>();
  int index = leaf->LowerBound(key, comparator_);
  if (index == leaf->GetSize() || comparator_(leaf->KeyAt(index), key) != 0 || leaf->IsTombstone(index)) {
    return false;
  }
  result->push_back(leaf->ValueAt(index));
  return true;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::LockLeaf(const KeyType &key, bool *is_root) -> std::optional<WritePageGuard> {
  auto header = bpm_->ReadPage(header_page_id_);
  auto root = header.template As<BPlusTreeHeaderPage>()->root_page_id_;
  if (root == INVALID_PAGE_ID) {
    return std::nullopt;
  }
  auto guard = bpm_->ReadPage(root);
  *is_root = guard.template As<BPlusTreePage>()->IsLeafPage();
  if (*is_root) {
    guard.Drop();
    auto leaf = bpm_->WritePage(root);
    header.Drop();
    return leaf;
  }
  header.Drop();
  while (true) {
    const auto *page = guard.template As<InternalPage>();
    auto id = page->ValueAt(page->Lookup(key, comparator_));
    auto child = bpm_->ReadPage(id);
    if (child.template As<BPlusTreePage>()->IsLeafPage()) {
      child.Drop();
      auto leaf = bpm_->WritePage(id);
      guard.Drop();
      return leaf;
    }

    guard.Drop();
    guard = std::move(child);
  }
}

FULL_INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ReleasePath(Context *ctx) {
  ctx->header_page_.reset();
  while (!ctx->write_set_.empty()) {
    ctx->write_set_.pop_front();
  }
}

FULL_INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::LockPath(const KeyType &key, bool insert, Context *ctx) {
  ctx->header_page_ = bpm_->WritePage(header_page_id_);
  ctx->root_page_id_ = ctx->header_page_->template As<BPlusTreeHeaderPage>()->root_page_id_;
  if (ctx->root_page_id_ == INVALID_PAGE_ID) {
    return;
  }
  auto guard = bpm_->WritePage(ctx->root_page_id_);
  while (true) {
    const auto *page = guard.template As<BPlusTreePage>();
    bool safe = insert ? page->GetSize() < page->GetMaxSize() - (page->IsLeafPage() ? 1 : 0)
                       : page->GetSize() > page->GetMinSize();
    if (!insert && guard.GetPageId() == ctx->root_page_id_) {
      safe = page->GetSize() > (page->IsLeafPage() ? 1 : 2);
    }
    if (safe) {
      ReleasePath(ctx);
    }
    ctx->write_set_.push_back(std::move(guard));
    if (page->IsLeafPage()) {
      return;
    }
    const auto *internal = ctx->write_set_.back().template As<InternalPage>();
    guard = bpm_->WritePage(internal->ValueAt(internal->Lookup(key, comparator_)));
  }
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  bool root_leaf = false;
  auto optimistic = LockLeaf(key, &root_leaf);
  if (optimistic.has_value()) {
    auto *leaf = optimistic->template AsMut<LeafPage>();
    int index = leaf->LowerBound(key, comparator_);
    if (index < leaf->GetSize() && comparator_(leaf->KeyAt(index), key) == 0) {
      if (!leaf->IsTombstone(index)) {
        return false;
      }
      leaf->Revive(index, value);
      return true;
    }
    if (leaf->GetSize() < leaf->GetMaxSize() - 1) {
      leaf->InsertAt(index, key, value);
      return true;
    }
    optimistic.reset();
  }
  Context ctx;
  LockPath(key, true, &ctx);
  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    auto id = bpm_->NewPage();
    auto guard = bpm_->WritePage(id);
    auto *leaf = guard.template AsMut<LeafPage>();
    leaf->Init(leaf_max_size_);
    leaf->InsertAt(0, key, value);
    ctx.header_page_->template AsMut<BPlusTreeHeaderPage>()->root_page_id_ = id;
    ReleasePath(&ctx);
    return true;
  }
  auto *leaf = ctx.write_set_.back().template AsMut<LeafPage>();
  int index = leaf->LowerBound(key, comparator_);
  if (index < leaf->GetSize() && comparator_(leaf->KeyAt(index), key) == 0) {
    bool inserted = leaf->IsTombstone(index);
    if (inserted) {
      leaf->Revive(index, value);
    }
    ReleasePath(&ctx);
    return inserted;
  }
  leaf->InsertAt(index, key, value);
  if (leaf->GetSize() < leaf->GetMaxSize()) {
    ReleasePath(&ctx);
    return true;
  }
  auto right_id = bpm_->NewPage();
  auto right_guard = bpm_->WritePage(right_id);
  auto *right = right_guard.template AsMut<LeafPage>();
  right->Init(leaf_max_size_);
  int middle = leaf->GetSize() / 2;
  leaf->MoveTo(right, middle, leaf->GetSize() - middle, comparator_);
  right->SetNextPageId(leaf->GetNextPageId());
  leaf->SetNextPageId(right_id);
  KeyType separator = right->KeyAt(0);
  auto left_id = ctx.write_set_.back().GetPageId();
  right_guard.Drop();
  ctx.write_set_.pop_back();
  while (!ctx.write_set_.empty()) {
    auto *parent = ctx.write_set_.back().template AsMut<InternalPage>();
    int position = parent->ValueIndex(left_id) + 1;
    if (parent->GetSize() < parent->GetMaxSize()) {
      parent->InsertAt(position, separator, right_id);
      ReleasePath(&ctx);
      return true;
    }
    auto new_id = bpm_->NewPage();
    auto new_guard = bpm_->WritePage(new_id);
    auto *new_page = new_guard.template AsMut<InternalPage>();
    new_page->Init(internal_max_size_);
    int size = parent->GetSize();
    int split = (size + 1) / 2;
    for (int i = split; i <= size; i++) {
      int old_index = i < position ? i : i - 1;
      new_page->InsertAt(new_page->GetSize(), i == position ? separator : parent->KeyAt(old_index),
                         i == position ? right_id : parent->ValueAt(old_index));
    }
    if (position < split) {
      parent->SetSize(split - 1);
      parent->InsertAt(position, separator, right_id);
    } else {
      parent->SetSize(split);
    }
    separator = new_page->KeyAt(0);
    left_id = ctx.write_set_.back().GetPageId();
    right_id = new_id;
    new_guard.Drop();
    ctx.write_set_.pop_back();
  }
  auto new_root = bpm_->NewPage();
  auto guard = bpm_->WritePage(new_root);
  auto *page = guard.template AsMut<InternalPage>();
  page->Init(internal_max_size_);
  page->InsertAt(0, KeyType{}, left_id);
  page->InsertAt(1, separator, right_id);
  ctx.header_page_->template AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root;
  ReleasePath(&ctx);
  return true;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  bool root_leaf = false;
  auto optimistic = LockLeaf(key, &root_leaf);
  if (!optimistic.has_value()) {
    return;
  }
  auto *target = optimistic->template AsMut<LeafPage>();
  int position = target->LowerBound(key, comparator_);
  if (position == target->GetSize() || comparator_(target->KeyAt(position), key) != 0 ||
      target->IsTombstone(position)) {
    return;
  }
  if (target->GetSize() > (root_leaf ? 1 : target->GetMinSize())) {
    target->DeleteAt(position);
    return;
  }
  optimistic.reset();
  Context ctx;
  LockPath(key, false, &ctx);
  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    ReleasePath(&ctx);
    return;
  }
  auto *leaf = ctx.write_set_.back().template AsMut<LeafPage>();
  position = leaf->LowerBound(key, comparator_);
  if (position == leaf->GetSize() || comparator_(leaf->KeyAt(position), key) != 0 || leaf->IsTombstone(position)) {
    ReleasePath(&ctx);
    return;
  }
  leaf->DeleteAt(position);
  while (!ctx.write_set_.empty()) {
    auto &current_guard = ctx.write_set_.back();
    auto current_id = current_guard.GetPageId();
    auto *current = current_guard.template AsMut<BPlusTreePage>();
    if (current_id == ctx.root_page_id_) {
      page_id_t replacement = current_id;
      if (current->IsLeafPage() && current->GetSize() == 0) {
        replacement = INVALID_PAGE_ID;
      }
      if (!current->IsLeafPage() && current->GetSize() == 1) {
        replacement = current_guard.template As<InternalPage>()->ValueAt(0);
      }
      if (replacement != current_id) {
        ctx.header_page_->template AsMut<BPlusTreeHeaderPage>()->root_page_id_ = replacement;
        ReleasePath(&ctx);
        bpm_->DeletePage(current_id);
        return;
      }
      ReleasePath(&ctx);
      return;
    }
    if (current->GetSize() >= current->GetMinSize()) {
      ReleasePath(&ctx);
      return;
    }
    auto *parent = ctx.write_set_[ctx.write_set_.size() - 2].template AsMut<InternalPage>();
    int child_index = parent->ValueIndex(current_id);
    int sibling_index = child_index > 0 ? child_index - 1 : child_index + 1;
    auto sibling_id = parent->ValueAt(sibling_index);
    auto sibling_guard = bpm_->WritePage(sibling_id);
    bool from_left = sibling_index < child_index;
    if (current->IsLeafPage()) {
      auto *node = current_guard.template AsMut<LeafPage>();
      auto *other = sibling_guard.template AsMut<LeafPage>();
      while (node->GetSize() < node->GetMinSize() && other->GetSize() > other->GetMinSize()) {
        other->MoveTo(node, from_left ? other->GetSize() - 1 : 0, 1, comparator_);
      }
      if (node->GetSize() >= node->GetMinSize()) {
        parent->SetKeyAt(from_left ? child_index : sibling_index, from_left ? node->KeyAt(0) : other->KeyAt(0));
        sibling_guard.Drop();
        ReleasePath(&ctx);
        return;
      }
      auto *left = from_left ? other : node;
      auto *right = from_left ? node : other;
      right->MoveTo(left, 0, right->GetSize(), comparator_);
      left->SetNextPageId(right->GetNextPageId());
    } else {
      auto *node = current_guard.template AsMut<InternalPage>();
      auto *other = sibling_guard.template AsMut<InternalPage>();
      if (other->GetSize() > other->GetMinSize()) {
        if (from_left) {
          node->SetKeyAt(0, parent->KeyAt(child_index));
          int last = other->GetSize() - 1;
          node->InsertAt(0, KeyType{}, other->ValueAt(last));
          parent->SetKeyAt(child_index, other->KeyAt(last));
          other->RemoveAt(last);
        } else {
          node->InsertAt(node->GetSize(), parent->KeyAt(sibling_index), other->ValueAt(0));
          parent->SetKeyAt(sibling_index, other->KeyAt(1));
          other->RemoveAt(0);
        }
        sibling_guard.Drop();
        ReleasePath(&ctx);
        return;
      }
      auto *left = from_left ? other : node;
      auto *right = from_left ? node : other;
      left->InsertAt(left->GetSize(), parent->KeyAt(from_left ? child_index : sibling_index), right->ValueAt(0));
      for (int i = 1; i < right->GetSize(); i++) {
        left->InsertAt(left->GetSize(), right->KeyAt(i), right->ValueAt(i));
      }
    }
    parent->RemoveAt(from_left ? child_index : sibling_index);
    auto removed_id = from_left ? current_id : sibling_id;
    sibling_guard.Drop();
    ctx.write_set_.pop_back();
    bpm_->DeletePage(removed_id);
  }
  ReleasePath(&ctx);
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::ScanStart(const KeyType *key) -> INDEXITERATOR_TYPE {
  auto header = bpm_->ReadPage(header_page_id_);
  auto root = header.template As<BPlusTreeHeaderPage>()->root_page_id_;
  if (root == INVALID_PAGE_ID) {
    return End();
  }
  auto guard = bpm_->ReadPage(root);
  header.Drop();
  while (!guard.template As<BPlusTreePage>()->IsLeafPage()) {
    const auto *page = guard.template As<InternalPage>();
    auto child = bpm_->ReadPage(page->ValueAt(key == nullptr ? 0 : page->Lookup(*key, comparator_)));
    guard.Drop();
    guard = std::move(child);
  }
  int index = key == nullptr ? 0 : guard.template As<LeafPage>()->LowerBound(*key, comparator_);
  return INDEXITERATOR_TYPE(bpm_, std::move(guard), index, header_page_id_, comparator_, key);
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE { return ScanStart(nullptr); }

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE { return ScanStart(&key); }

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 3>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 2>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 1>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, -1>;

template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
