//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_iterator.cpp
//
// Identification: src/storage/index/index_iterator.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"

namespace bustub {

/**
 * @note you can change the destructor/constructor method here
 * set your own input parameters
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() = default;

FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;  // NOLINT

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool { return page_id_ == INVALID_PAGE_ID; }

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> std::pair<const KeyType &, const ValueType &> {
  const auto *leaf = guard_.template As<LeafPage>();
  return {leaf->KeyRefAt(index_), leaf->ValueAt(index_)};
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  if (!IsEnd()) {
    index_++;
    SkipDeleted();
  }
  return *this;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(std::shared_ptr<TracedBufferPoolManager> bpm, ReadPageGuard guard, int index,
                                  page_id_t header_page_id, const KeyComparator &comparator, const KeyType *start_key)
    : bpm_(std::move(bpm)),
      guard_(std::move(guard)),
      page_id_(guard_.GetPageId()),
      index_(index),
      header_page_id_(header_page_id),
      comparator_(comparator),
      lower_bound_(start_key == nullptr ? std::nullopt : std::make_optional(*start_key)) {
  SkipDeleted();
}

FULL_INDEX_TEMPLATE_ARGUMENTS
void INDEXITERATOR_TYPE::SeekAfter(const KeyType &key) {
  using InternalPage = BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>;
  std::deque<ReadPageGuard> path;
  std::vector<int> positions;
  auto header = bpm_->ReadPage(header_page_id_);
  auto root = header.template As<BPlusTreeHeaderPage>()->root_page_id_;
  page_id_ = INVALID_PAGE_ID;
  index_ = 0;
  if (root == INVALID_PAGE_ID) {
    return;
  }
  auto guard = bpm_->ReadPage(root);
  header.Drop();
  while (!guard.template As<BPlusTreePage>()->IsLeafPage()) {
    const auto *page = guard.template As<InternalPage>();
    int position = page->Lookup(key, *comparator_);
    auto child = bpm_->ReadPage(page->ValueAt(position));
    path.push_back(std::move(guard));
    positions.push_back(position);
    guard = std::move(child);
  }
  const auto *leaf = guard.template As<LeafPage>();
  index_ = leaf->LowerBound(key, *comparator_);
  if (index_ < leaf->GetSize() && (*comparator_)(leaf->KeyAt(index_), key) == 0) {
    index_++;
  }
  if (index_ == leaf->GetSize()) {
    guard.Drop();
    while (!path.empty()) {
      const auto *parent = path.back().template As<InternalPage>();
      int next = positions.back() + 1;
      if (next < parent->GetSize()) {
        guard = bpm_->ReadPage(parent->ValueAt(next));
        break;
      }
      path.pop_back();
      positions.pop_back();
    }
    if (path.empty()) {
      index_ = 0;
      return;
    }
    while (!path.empty()) {
      path.pop_front();
    }
    while (!guard.template As<BPlusTreePage>()->IsLeafPage()) {
      auto child = bpm_->ReadPage(guard.template As<InternalPage>()->ValueAt(0));
      guard.Drop();
      guard = std::move(child);
    }
    index_ = 0;
  }
  while (!path.empty()) {
    path.pop_front();
  }
  page_id_ = guard.GetPageId();
  guard_ = std::move(guard);
}

FULL_INDEX_TEMPLATE_ARGUMENTS
void INDEXITERATOR_TYPE::SkipDeleted() {
  while (page_id_ != INVALID_PAGE_ID) {
    const auto *leaf = guard_.template As<LeafPage>();
    while (index_ < leaf->GetSize() &&
           (leaf->IsTombstone(index_) ||
            (lower_bound_.has_value() && (*comparator_)(leaf->KeyAt(index_), *lower_bound_) < 0))) {
      index_++;
    }
    if (index_ < leaf->GetSize()) {
      lower_bound_.reset();
      return;
    }
    if (leaf->GetNextPageId() == INVALID_PAGE_ID) {
      guard_.Drop();
      page_id_ = INVALID_PAGE_ID;
      index_ = 0;
      return;
    }
    auto last = leaf->KeyAt(leaf->GetSize() - 1);
    guard_.Drop();
    SeekAfter(last);
  }
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 3>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 2>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 1>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, -1>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
